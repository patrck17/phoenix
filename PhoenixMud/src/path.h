/***************************************************************************
 *  File: path.h                                        Part of PhoenixMud *
 *  Usage: Definitions needed for the path system that allows builders     *
 *         to schedule mob actions. (like the mayor)                       *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *  AAM Apr 98                                                             *
 *                                                                         *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 *  PhoenixMUD is based on CircleMUD, Copyright (C) 1996-98.               *
 ***************************************************************************/

struct path_action {
   char command;		/* G(o) or C(ommand) */
   int  cmd_num;		/* which cmd line this is */
   int  dest_vnum;		/* for G where to go - virtual */
   int  dest_rnum;		/* for G where to go - real */
   char *command_line;		/* for C what to force the mob to do */
   int  hrmin;			/* 0-900 Sec with 1hr==75sec */
   int  afternoon;		/* 0 - AM  1 - PM */
   int  day;			/* 1 - 7 */
   int  week;			/* 1 - 5 */
   int  month;			/* 1 - 17 */
   long bitvector;		/* well, if we need bits */
   int  group_number;		/* active only if this group is active   */
  				/*    used for pirate raids and the like */
};

struct path_data {
   char *name;			/* label of the path */
   char *descr;			/* descripton of the path */
   int  number;			/* vnum of the path */
   int  num_commands;		/* number of commands in this path */
   long bitvector;		/* bits and bits and bits  */
   struct path_action *action;	/* point to cmd array */
};


#ifndef __PATH_C__
extern struct path_data *path_index;
extern int top_path;
#endif
