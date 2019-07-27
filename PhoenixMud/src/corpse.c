/***********************************************************************
*  File: corpse.c Version 1.1                                          *
*  Usage: Corpse Saving over reboots/crashes/copyovers                 *
*                                                                      *
*  By: Michael Cunningham (Romulus) IMP of Legends of The Phoenix MUD  *
*  Permission is granted to use and modify this software as long as    *
*  credits are given in the credits command in the game.               *
*  Built for circle30bpl15                                             *
*  Bunch of code reused from the XAP object patch by Patrick Dughi     *
*                                                  <dughi@IMAXX.NET>   *
*  Thankyou Patrick!                                                   *
*  Some functions have been renamed to protect the innocent            *
*  All Rights Reserved, Copyright (C) 1999                             *
***********************************************************************/

/* The standard includes */
#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"
#include "structs.h"
#include "buffer.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "interpreter.h"
#include "utils.h"
#include "spells.h"
#include "constants.h"

/* Set this define to wherever you want to save your corpses */
#define CORPSE_FILE "misc/corpse.save" 

/* External Structures / Variables */
extern struct obj_data *object_list;
extern struct room_data *world;
extern struct index_data *obj_index;   /* index table for object file   */
extern room_vnum frozen_start_room;


/* Local Function Declerations */
void save_corpses(void);
void load_corpses(void);
int corpse_save(struct obj_data * obj, FILE * fp, int location, 
		bool recurse_this_tree); 
int my_obj_save_to_disk(FILE *fp, struct obj_data *obj, int locate);
int parse_xap_obj(char *filename, struct obj_data **obj,char *line,
                  FILE *fl, int version, int *locate);

/* Tada! THE FUNCTIONS ! Yaaa! */


int corpse_save(struct obj_data * obj, FILE * fp, int location, 
		bool recurse_this_tree) 
{
  /* This function basically is responsible for taking the    */
  /* supplied obj and figuring out if it has any contents. If */
  /* it does then we write those to disk.. Ad Nasum.          */

/*    struct obj_data *tmp; */
   int result;

   if (obj)  /* a little recursion (can be a dangerous thing:) */
      {
     /* recurse_this_tree causes the recursion to branch only
	down the corpses content's tree and not the contents of the
	room. obj->next_content points to the rooms contents
	the first time this function is called from save_corpses
	hence we avoid going down there otherwise we will save
	the rooms contents as well as the corpses contents in the 
	corpse.save file. 
    */

      if (recurse_this_tree != FALSE)
	 {
	 corpse_save(obj->next_content, fp, location, recurse_this_tree);
	 }
      recurse_this_tree = TRUE;
      corpse_save(obj->contains, fp, MIN(0, location) - 1, recurse_this_tree);
      result = my_obj_save_to_disk(fp, obj, location); 

    /* readjust the wieght while we do this */
/*      for (tmp = obj->in_obj; tmp; tmp = tmp->in_obj)
	 {
	 GET_OBJ_WEIGHT(tmp) -= GET_OBJ_WEIGHT(obj);
	 }
	 */
      if (!result)
	 return (0);
      }
   return (TRUE);
}



void save_corpses(void)
{
  /* This is basically the mother of all the save corpse functions */
  /* You can call it from anywhere in the game with no arguments */
  /* Basically any time a corpse is manipulated in any way..either */
  /* directly or indirectly you need to call this function */

   FILE *fp;
   struct obj_data *i, *next;
   int location = 0;

  /* Open corpse file */
   if (!(fp = fopen(CORPSE_FILE, "wb")))
      {
      if (errno != ENOENT)   /* if it fails, NOT because of no file */   
	 log("SYSERR: checking for corpse file %s : %s", CORPSE_FILE, 
	     strerror(errno));
      return;
      }

   if(fprintf(fp,"@Version: %d\n",CUR_POBJ_VER)<1)
      {
      perror("SYSERR: Writing rent version");
      log("SYSERR OBJSAVE: Error writing rent version for corpse %s %d",
          CORPSE_FILE,errno);
      return ;
      }
  /* Scan the object list */
   for (i = object_list; i; i = next) 
      {
      next = i->next;      

     /* Check if its a players corpse */
      if (IS_OBJ_STAT(i, ITEM_PC_CORPSE)) 
	 {
	/* It is, so save it to a file */
         if (!corpse_save(i, fp, location, FALSE))
	    {           
	    log("SYSERR: A corpse didnt save for some reason");
	    fclose(fp);
	    return;
	    }
	 }
      }   
  /* Close the corpse file */
   fclose(fp);
} 

void load_corpses(void) 
{
  /* Ahh load corpses.. it was cake to write them out to a file      */
  /* it was a pain to load them back up through without a character. */
  /* Because I dont have a character I couldnt figure out how to     */
  /* put objects back into the corpse the exact way they came out..  */
  /* like objects back in their container, etc. So I just decided to */
  /* Dump it all in the corpse and let the character sort it out.    */
  /* If they dont like it, screwum. They are lucky I coded this:)    */
  /* Oh, and a bunch of this code is from Patricks XAP obj's code    */

   FILE *fp;
   char *line=get_buffer(256);
   char *buf1=get_buffer(2048);
   char *buf2=get_buffer(2048);
   int locate=0, num_objs=0;
   int version;
   struct obj_data *temp = NULL, *obj = NULL, *next_obj = NULL;

   if (!(fp = fopen(CORPSE_FILE, "r+b"))) 
      {
      if (errno != ENOENT) 
	 { 
	 sprintf(buf1, "SYSERR: READING CORPSE FILE %s in load_corpses", 
		 CORPSE_FILE);
	 perror(buf1);
	 }
      return;
      }

   if(!feof(fp))
      get_line(fp, line);
   if(*line == '@')
      {
      if(sscanf(line,"@Version: %d",&version)!=1)
	 {
	 mudlogf(CMP,LVL_IMMORT,TRUE,
		 "SYSERR OBJLOAD: Format error in %s with line: %s",
		CORPSE_FILE,line);
	 exit(1);
	 }
      if(!feof(fp))
	 get_line(fp,line);
      }
   else
      {
      version=1;
      }

   while (!feof(fp)) 
      {
      temp=NULL; 
     /* first, we get the number. Not too hard. */
      if(parse_xap_obj(CORPSE_FILE,&temp,line,fp,version,&locate)) 
	 {
	 if(temp != NULL) 
	    {
	    num_objs++;
	   /* Check if our object is a corpse */
	    if (IS_OBJ_STAT(temp, ITEM_PC_CORPSE))
	       {    /* scan our temp room for objects */
	       for (obj = world[real_room(frozen_start_room)].contents; obj ; obj = next_obj) 
		  {
		  next_obj = obj->next_content;
		  if (obj)
		     {            
		     obj_from_room(obj); /* get those objs from that room */       
		     obj_to_obj(obj, temp);  /* and put them in the corpse */
		     }
		  } /* exit the room scanning loop */
	       if (temp)
		  { /* put the corpse in the right room */
		  obj_to_room(temp, real_room(GET_OBJ_VROOM(temp)));
		  }
	       }
	    else 
	       { 
	      /* just a plain obj..send it to a temp room until we load a corpse */
	       obj_to_room(temp, real_room(frozen_start_room)); 
	       }
	    }
	 }
      else
	 {
	 if(!feof(fp))
	    get_line(fp,line);
	 else
	    break;
	 }
      
      }
   release_buffer(buf2);
   release_buffer(buf1);
   release_buffer(line);
   fclose(fp); 
}

