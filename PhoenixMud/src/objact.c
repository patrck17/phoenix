/***************************************************************************
 *  File: objact.c                                      Part of PhoenixMud *
 *  Usage: Functions for generating interesting object behaviour           *
 *                                                                         *
 *  All rights reserved.  See license.doc for complete information.        *
 *  AAM Apr 98                                                             *
 *  PhoenixMud is based on CircleMud                                       *
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
 *  PhoenixMud changes by Angus A. Mezick Copyright (C) 1998               *
 ***************************************************************************/

#include "../localHeader/conf.h" 
#include "../localHeader/sysdep.h" 
 
 
#include "structs.h" 
#include "buffer.h"
#include "utils.h" 
#include "db.h" 
#include "comm.h" 
#include "interpreter.h" 
#include "handler.h" 
#include "queue.h" 
#include "screen.h" 
#include "spells.h" 

extern struct char_data *character_list;
extern struct index_data *mob_index;
extern struct room_data *world;

void object_activity(void)
{
   struct char_data *ch;
   struct obj_data  *obj;

   for(ch = character_list; ch; ch = ch->next)
      {
      if(!IS_MOB(ch))
	 {
	 for(obj=world[IN_ROOM(ch)].contents;obj; obj = obj->next_content)
	    {
	    if(IS_OBJ_STAT(obj,ITEM_DO_ACT) && 
	       obj->action_description &&
	       !number(0,6) && (GET_OBJ_TYPE(obj)!=ITEM_NOTE))
	       {
	       act(obj->action_description, FALSE, ch, obj, obj, TO_CHAR);
	       }
	    }
	 }
      }
}
