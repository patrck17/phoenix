/***************************************************************************
 *  File: path.c                                        Part of PhoenixMud *
 *  Usage: Code needed to load the path system that allows builders        *
 *         to schedule mob actions. (like the mayor)                       *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *  AAM Apr 98                                                             *
 *                                                                         *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 *  PhoenixMUD is based on CircleMUD, Copyright (C) 1996-98.               *
 ***************************************************************************/

#define __PATH_C_

#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"

#include "structs.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "interpreter.h"
#include "buffer.h"
#include "utils.h"
#include "path.h"


/* Local variables */
struct path_data *path_index;
int top_path = 0;
#define PTH path_index[i]
#define PCMD path_index[i].action[j]

long asciiflag_conv(char *flag);

void parse_path(FILE * path_f, int nr, int version)
{
   static int i =0;
   char *buf2 = get_buffer(SMALL_BUFSIZE);
   char *f1   = get_buffer(128);
   char *line = get_buffer(256); 
   char *ptr;
   int j,t[10];
   

   
   PTH.number = nr;

   sprintf(buf2, "path vnum %d", nr);

  /*
   * String Data
   */
   PTH.name  = fread_string(path_f,buf2);
   PTH.descr = fread_string(path_f,buf2);

   if((!get_line(path_f,line))||(sscanf(line, " %d %s",t,f1)!=2))
      {
      log("SYSERR: Format error in path #%d, Flag line\n"
	  "SYSERR: ...expected line of form 'int str'\n"
	  "SYSERR: ...got: %s",nr,(line?line:""));
      exit(1);
      }
   PTH.num_commands = t[0];
   PTH.bitvector    = asciiflag_conv(f1);
   
   CREATE(PTH.action,struct path_action,PTH.num_commands);

   for(j=0;j<PTH.num_commands;j++)
      {
      if(!get_line(path_f,line))
	 {
	 log("SYSERR: Format error in path %d - premature end of file!",nr);
	 exit(1);
	 }
      
      ptr=line;
      skip_spaces(&ptr);
      if(*ptr=='*')
	 {
	 j--;
	 continue;
	 }

      PCMD.command = *ptr;
      PCMD.cmd_num = j;
      ptr++;

      if(PCMD.command=='!')
	 break;
      else if(PCMD.command=='G')
	 {
	 if(sscanf(ptr, " %d %d %d %d %d %s %d %d", &PCMD.month, &PCMD.week,
		   &PCMD.day, &PCMD.afternoon, &PCMD.hrmin, f1, 
		   &PCMD.group_number, &PCMD.dest_vnum)!=8)
	    {
	    log("SYSERR: Format error in path #%d, G line %d\n"
		"SYSERR: ...expected line of form 'int int int int int str int int'\n"
		"SYSERR: ...got: %s",nr,j,(line?line:""));
	    exit(1);
	    }
	 PCMD.bitvector=asciiflag_conv(f1);
	 }
      else if(PCMD.command=='C')
	 {
	 if(sscanf(ptr, " %d %d %d %d %d %s %d", &PCMD.month, &PCMD.week,
		   &PCMD.day, &PCMD.afternoon, &PCMD.hrmin, f1, 
		   &PCMD.group_number)!=7)
	    {
	    log("SYSERR: Format error in path #%d, C line %d\n"
		"SYSERR: ...expected line of form 'int int int int int str int'\n"
		"SYSERR: ...got: %s",nr,j,(line?line:""));
	    exit(1);
	    }
	 PCMD.bitvector=asciiflag_conv(f1);
	 PCMD.command_line = fread_string(path_f,buf2);

	 }
      else
	 {
	 log("SYSERR: Format error in path %d - missing command!",nr);
	 exit(1);
	 }

      }
   if(!get_line(path_f,line)||(*line!='!'))
      {
      log("SYSERR: Format error at end of path #%d - missing terminator!",nr);
      exit(1);
      }
   release_buffer(buf2);
   release_buffer(f1);
   release_buffer(line);
   top_path = i++;
}


int is_active_group(struct path_action *path_cmd)
{
  /* search the active path group list and return 1 if this command's */
  /* group number is present. */
   return 1;
}

