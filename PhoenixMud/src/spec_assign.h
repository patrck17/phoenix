/***************************************************************************
 *  File: spec_assign.h                                 Part of PhoenixMud *
 *  Usage: Definitions needed by spec_assign.c and OLC for dynamically     *
 *         assigned spec_procs.                                            *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *  AAM Mar 98                                                             *
 *                                                                         *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 *  PhoenixMUD is based on CircleMUD, Copyright (C) 1996-98.               *
 ***************************************************************************/

#ifndef SPECIAL
#error structs.h must be included!!
#else
typedef SPECIAL(*proctype);
char *get_mob_spec_name(SPECIAL(func));
int get_mob_spec_num(SPECIAL(func));
proctype get_mob_spec_proc(char *name);
void list_mob_spec_procs(struct char_data *ch,char *arg);
#endif
#define NUM_SPECS  47

#define ARCHER_SP      0
#define CITYGUARD_SP   2
#define GUILDGUARD_SP  5
#define HOMETOWN_SP   15
#define SAGE_SP       44
#define RECHARGE_SP   45


