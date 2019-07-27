/* ************************************************************************ 
*   File: ban.c                                         Part of CircleMUD * 
*  Usage: banning/unbanning/checking sites and player names               * 
*                                                                         * 
*  All rights reserved.  See license.doc for complete information.        * 
*                                                                         * 
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University * 
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               * 
************************************************************************ */ 
 
#include "../localHeader/conf.h" 
#include "../localHeader/sysdep.h" 
 
 
#include "structs.h" 
#include "buffer.h"
#include "utils.h" 
#include "comm.h" 
#include "interpreter.h" 
#include "handler.h" 
#include "db.h" 
 
extern int top_of_mobt;                 /* mob name prevention */ 
extern struct char_data *mob_proto; 
#define MAX_INVALID_NAMES 500
char *invalid_list[MAX_INVALID_NAMES];
 
 
struct ban_list_element *ban_list = NULL; 
 
 
char *ban_types[] = 
{ 
  "no", 
  "new", 
  "select", 
  "all", 
  "ERROR" 
}
; 
 
void load_banned(void) 
{ 
  FILE *fl; 
  int i, date; 
  char *site_name=get_buffer(BANNED_SITE_LENGTH + 1);
  char *ban_type=get_buffer(100); 
  char *name=get_buffer(MAX_NAME_LENGTH + 1); 
  struct ban_list_element *next_node; 
 
  ban_list = 0; 
 
  if (!(fl = fopen(BAN_FILE, "r"))) 
     { 
     if (errno != ENOENT) 
	{
	log("SYSERR: Unable to open banfile '%s': %s.",BAN_FILE,
	    strerror(errno));
	}
     else
	 log("SYSERR: Ban file '%s' doesn't exist.", BAN_FILE);
     } 
  else
     {
     while (fscanf(fl, " %s %s %d %s ", ban_type, site_name, &date, name) == 4)
	{ 
	CREATE(next_node, struct ban_list_element, 1); 
	strncpy(next_node->site, site_name, BANNED_SITE_LENGTH); 
	next_node->site[BANNED_SITE_LENGTH] = '\0'; 
	strncpy(next_node->name, name, MAX_NAME_LENGTH); 
	next_node->name[MAX_NAME_LENGTH] = '\0'; 
	next_node->date = date; 
	
	for (i = BAN_NOT; i <= BAN_ALL; i++) 
	   if (!strcmp(ban_type, ban_types[i])) 
	      next_node->type = i; 
	
	next_node->next = ban_list; 
	ban_list = next_node; 
	} 
     
     fclose(fl); 
     } 
  release_buffer(name);
  release_buffer(site_name);
  release_buffer(ban_type);
}
 
int isbanned(char *hostname) 
{ 
   int i; 
   struct ban_list_element *banned_node; 
   char *nextchar; 
 
   if (!hostname || !*hostname) 
      return (0); 
 
   i = 0; 
   for (nextchar = hostname; *nextchar; nextchar++) 
      *nextchar = LOWER(*nextchar); 
 
   for (banned_node = ban_list; banned_node; banned_node = banned_node->next) 
      if (strstr(hostname, banned_node->site)) /* if hostname is a substring */ 
	 i = MAX(i, banned_node->type); 
 
   return (i); 
} 
 
 
void _write_one_node(FILE * fp, struct ban_list_element * node) 
{ 
   if (node) 
      { 
      _write_one_node(fp, node->next); 
      fprintf(fp, "%s %s %ld %s\n", ban_types[node->type], 
	      node->site, (long) node->date, node->name); 
      } 
} 
 
 
 
void write_ban_list(void) 
{ 
   FILE *fl; 
 
   if (!(fl = fopen(BAN_FILE, "w"))) 
      { 
      perror("SYSERR: unable to open write_ban_list"); 
      return; 
      } 
   _write_one_node(fl, ban_list);/* recursively write from end to start */ 
   fclose(fl); 
   return; 
} 
 
 
ACMD(do_ban) 
{ 
   char *flag=get_buffer(MAX_INPUT_LENGTH);
   char *site=get_buffer(MAX_INPUT_LENGTH);
   char *format="%-25.25s  %-8.8s  %-15.15s  %-16.16s\r\n";
   char *nextchar, *timestr; 
   int i; 
   struct ban_list_element *ban_node; 
 
 
   if (!*argument) 
      { 
      if (!ban_list) 
	 { 
	 send_to_char(ch,"No sites are banned.\r\n"); 
	 release_buffer(site);
	 release_buffer(flag);
	 return; 
	 } 
      send_to_char(ch,  format, 
	      "Banned Site Name", 
	      "Ban Type", 
	      "Banned On", 
	      "Banned By"); 
      send_to_char(ch, format, 
	      "---------------------------------", 
	      "---------------------------------", 
	      "---------------------------------", 
	      "---------------------------------"); 
 
      for (ban_node = ban_list; ban_node; ban_node = ban_node->next) 
	 { 
	 if (ban_node->date) 
	    { 
	    timestr = asctime(localtime(&(ban_node->date))); 
/*	    *(timestr + 10) = 0;*/
            strcpy((timestr+11), (timestr+20));
	    strcpy(site, timestr); 
	    } 
	 else 
	    strcpy(site, "Unknown"); 
	 send_to_char(ch,format,ban_node->site,ban_types[ban_node->type], site,
		 ban_node->name); 
	 } 
      release_buffer(site);
      release_buffer(flag);
      return; 
      } 
   two_arguments(argument, flag, site); 
   if (!*site || !*flag) 
      { 
      send_to_char(ch,"Usage: ban {all | select | new} site_name\r\n"); 
      release_buffer(site);
      release_buffer(flag);
      return; 
      } 
   if (!(!str_cmp(flag, "select") || !str_cmp(flag, "all") || !str_cmp(flag, "new"))) 
      { 
      send_to_char(ch,"Flag must be ALL, SELECT, or NEW.\r\n"); 
      release_buffer(site);
      release_buffer(flag);
      return; 
      } 
   for (ban_node = ban_list; ban_node; ban_node = ban_node->next) 
      { 
      if (!str_cmp(ban_node->site, site)) 
	 { 
	 send_to_char(ch,"That site has already been banned -- unban it to change the ban type.\r\n"); 
	 release_buffer(site);
	 release_buffer(flag);
	 return; 
	 } 
      } 
 
   CREATE(ban_node, struct ban_list_element, 1); 
   strncpy(ban_node->site, site, BANNED_SITE_LENGTH); 
   for (nextchar = ban_node->site; *nextchar; nextchar++) 
      *nextchar = LOWER(*nextchar); 
   ban_node->site[BANNED_SITE_LENGTH] = '\0'; 
   strncpy(ban_node->name, GET_NAME(ch), MAX_NAME_LENGTH); 
   ban_node->name[MAX_NAME_LENGTH] = '\0'; 
   ban_node->date = time(0); 
 
   for (i = BAN_NEW; i <= BAN_ALL; i++) 
      if (!str_cmp(flag, ban_types[i])) 
	 ban_node->type = i; 
 
   ban_node->next = ban_list; 
   ban_list = ban_node; 
 
   
   mudlogf(NRM, MAX(LVL_DGOD, GET_INVIS_LEV(ch)), TRUE,
	   "Site-Ban: %s has banned %s for %s players.", GET_NAME(ch), site, 
	   ban_types[ban_node->type]); 
   send_to_char(ch,"Site banned.\r\n"); 
   write_ban_list(); 
   release_buffer(site);
   release_buffer(flag);
} 
 
 
ACMD(do_unban) 
{ 
   char *site=get_buffer(80);
   struct ban_list_element *ban_node, *temp; 
   int found = 0;  
 
   one_argument(argument, site); 
   if (!*site) 
      { 
      send_to_char(ch,"A site to unban might help.\r\n"); 
      release_buffer(site);
      return; 
      } 
   ban_node = ban_list; 
   while (ban_node && !found) 
      { 
      if (!str_cmp(ban_node->site, site)) 
	 found = 1; 
      else 
	 ban_node = ban_node->next; 
      } 
 
   if (!found) 
      { 
      send_to_char(ch,"That site is not currently banned.\r\n"); 
      release_buffer(site);
      return; 
      } 
   REMOVE_FROM_LIST(ban_node, ban_list, next); 
   send_to_char(ch,"Site unbanned.\r\n"); 
   mudlogf(NRM, MAX(LVL_DGOD, GET_INVIS_LEV(ch)), TRUE,
	   "Site-Ban: %s removed the %s-player ban on %s.", GET_NAME(ch), 
	   ban_types[ban_node->type], ban_node->site); 


   free(ban_node); 
   write_ban_list(); 
   release_buffer(site);
} 
 

/************************************************************************** 
 *  Code to check for invalid names (i.e., profanity, etc.)    * 
 *  Written by Sharon P. Goza        * 
 **************************************************************************/ 
 

int num_invalid = 0; 
 
int Valid_Name(char *newname) 
{ 
   int i; 
 
   char *tempname;

  /* return valid if list doesn't exist */ 
   if (!invalid_list || num_invalid < 1) 
      return (1); 
 
  /* change to lowercase */ 
   tempname = stolower(newname);

  /* Does the desired name contain a string in the invalid list? */ 
   for (i = 0; i < num_invalid; i++) 
      {
      if (strstr(tempname, invalid_list[i])) 
	 return (0);
      }

  /* The following added to prevent use of mob names */ 
   for (i = 0; i < top_of_mobt; i++) 
      { 
      if (isname(tempname, mob_proto[i].player.name))
	 return (0);
      }
   return (1); 
} 
 
 
void Read_Invalid_List(void) 
{ 
   FILE *fp; 
   char *temp;
 
   if (!(fp = fopen(XNAME_FILE, "r"))) 
      { 
      perror("SYSERR: Unable to open '" XNAME_FILE "' file for reading."); 
      return; 
      } 

   temp=get_buffer(256);
   num_invalid=0;
   while(get_line(fp,temp)&&(num_invalid<MAX_INVALID_NAMES))
      invalid_list[num_invalid++] = str_dup(temp);

   if (num_invalid >= MAX_INVALID_NAMES) 
      {
      log("SYSERR: Too many invalid names, change MAX_INVALID_NAMES in ban.c");
      exit(1);
      }
   
   release_buffer(temp);
   fclose(fp); 
} 

ACMD(do_add_xname)
{
   FILE *fp;
   int  i;
   char *buf;

   skip_spaces(&argument);
   
   if(*argument)
      {
      for (i = 0; argument[i]; i++) 
	 argument[i] = LOWER(argument[i]); 

      if(strlen(argument)<4)
	 {
	 send_to_char(ch,"To use this command, to name has to be at least 4 chars\r\nlong.  If you HAVE to add a smaller name, please post it to\r\nimmortal board for an implementor to add\r\n");
	 return;
	 }
      else if(!Valid_Name(argument))
	 {
	 send_to_char(ch,"That name is already on the xnames list.\r\nIf this is wrong please post to the immort board\r\nUse the command by itself to see the list\r\n--Masque\r\n");
	 return;
	 }
      /*
      else if(strstr("masque",argument))
	 {
	 send_to_char(ch,"Fuck YOU!\r\n");
	 mudlogf(CMP,LVL_IMMORT,TRUE,
		 "ALERT: %s just tried to xname %s(masque)",
		 GET_NAME(ch),argument);
	 return;
	 }
      else if(strstr("cymynedd",argument))
	 {
	 send_to_char(ch,"Fuck YOU!\r\n");
	 mudlogf(CMP,LVL_IMMORT,TRUE,
		 "ALERT: %s just tried to xname %s(cymynedd)",
		 GET_NAME(ch),argument);
	 return;
	 }
      else if(strstr("aleksandr",argument))
	 {
	 send_to_char(ch,"Fuck YOU!\r\n");
	 mudlogf(CMP,LVL_IMMORT,TRUE,
		 "ALERT: %s just tried to xname %s(aleksandr)",
		 GET_NAME(ch),argument);
	 return;
	 }
      else if(strstr("iluvatar",argument))
         {
         send_to_char(ch,"Fuck YOU!\r\n");
         mudlogf(CMP,LVL_IMMORT,TRUE,
                 "ALERT: %s just tried to xname %s(iluvatar)",
                 GET_NAME(ch),argument);
         return;
         }*/
      else
	 {
	 if((fp=fopen(XNAME_FILE,"a+"))==NULL)
	    {
	    mudlogf(CMP,LVL_IMMORT,TRUE,"Could not open %s for append!",
		    XNAME_FILE);
	    return;
	    }
	 fprintf(fp,"%s\n",argument);
	 fclose(fp);
	 
	 if(num_invalid<MAX_INVALID_NAMES)
	    invalid_list[num_invalid++]=str_dup(argument);
	 else
	    send_to_char(ch,"This name won't be added until the value of MAX_INVALID_NAMES is increases,\r\n post to the board\r\n");

	 
	 }
      }
   buf=get_buffer(MAX_STRING_LENGTH);
   for(i=0;i<num_invalid;i++)
      {
      sprintf(buf+strlen(buf),"%-17.17s ",invalid_list[i]);
      if((i>0)&&(((i+1)%4)==0))
	 strcat(buf,"\n");
      }
   
   if(ch->desc)
      page_string(ch->desc,buf,TRUE,"");
   
   release_buffer(buf);
   return;
}



