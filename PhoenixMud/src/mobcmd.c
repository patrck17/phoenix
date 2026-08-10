/***************************************************************************
 * MOBProgram ported for CircleMUD 3.0 by Mattias Larsson		   *
 * Traveller@AnotherWorld (ml@eniac.campus.luth.se 4000) 		   *
 **************************************************************************/

/***************************************************************************
 *  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
 *  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
 *                                                                         *
 *  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
 *  Chastain, Michael Quan, and Mitchell Tse.                              *
 *                                                                         *
 *  In order to use any part of this Merc Diku Mud, you must comply with   *
 *  both the original Diku license in 'license.doc' as well the Merc       *
 *  license in 'license.txt'.  In particular, you may not remove either of *
 *  these copyright notices.                                               *
 *                                                                         *
 *  Much time and thought has gone into this software and you are          *
 *  benefitting.  We hope that you share your changes too.  What goes      *
 *  around, comes around.                                                  *
 ***************************************************************************/

/***************************************************************************
 *  The MOBprograms have been contributed by N'Atas-ha.  Any support for   *
 *  these routines should not be expected from Merc Industries.  However,  *
 *  under no circumstances should the blame for bugs, etc be placed on     *
 *  Merc Industries.  They are not guaranteed to work on all systems due   *
 *  to their frequent use of strxxx functions.  They are also not the most *
 *  efficient way to perform their tasks, but hopefully should be in the   *
 *  easiest possible way to install and begin using. Documentation for     *
 *  such installation can be found in INSTALL.  Enjoy........    N'Atas-Ha *
 ***************************************************************************/

#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"
#include "structs.h"
#include "buffer.h"
#include "db.h"
#include "utils.h"
#include "handler.h"
#include "interpreter.h"
#include "comm.h"

extern struct index_data *mob_index;
extern struct room_data *world;
extern struct index_data *obj_index;
extern struct descriptor_data *descriptor_list;
extern struct zone_data *zone_table;

struct index_data *get_mob_index(int vnum);
struct index_data *get_obj_index(int vnum);

room_rnum find_target_room(struct char_data * ch, char *rawroomstr);
ACMD(do_trans);

#define bug(x, y) { log((x), (y)); }

/*
 * Local functions.
 */

char *			mprog_type_to_name	(int type);

/* This routine transfers between alpha and numeric forms of the
 *  mob_prog bitvector types. It allows the words to show up in mpstat to
 *  make it just a hair bit easier to see what a mob should be doing.
 */

char *mprog_type_to_name(int type)
{
   switch (type)
      {
       case IN_FILE_PROG:          return "in_file_prog";
       case ACT_PROG:              return "act_prog";
       case SPEECH_PROG:           return "speech_prog";
       case RAND_PROG:             return "rand_prog";
       case FIGHT_PROG:            return "fight_prog";
       case HITPRCNT_PROG:         return "hitprcnt_prog";
       case DEATH_PROG:            return "death_prog";
       case ENTRY_PROG:            return "entry_prog";
       case GREET_PROG:            return "greet_prog";
       case ALL_GREET_PROG:        return "all_greet_prog";
       case GIVE_PROG:             return "give_prog";
       case BRIBE_PROG:            return "bribe_prog";
       default:                    return "ERROR_PROG";
      }
}

/* string prefix routine */

bool str_prefix(const char *astr, const char *bstr)
{
   if (!astr) {
   log("Strn_cmp: null astr.");
   return TRUE;
   }
   if (!bstr) {
   log("Strn_cmp: null astr.");
   return TRUE;
   }
   for(; *astr; astr++, bstr++) 
      {
      if(LOWER(*astr) != LOWER(*bstr)) return TRUE;
      }
   return FALSE;
}

/* prints the argument to all the rooms aroud the mobile */

ACMD(do_mpasound)
{
   room_rnum was_in_room;
   int              door;
   char *arg;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   if (argument[0] == '\0')
      {
      bug("Mpasound - No argument: vnum %ld.", mob_index[ch->nr].vnum);
      return;
      }
 
   arg=get_buffer(MAX_STRING_LENGTH);
   one_argument(argument, arg);

   was_in_room = IN_ROOM(ch);
   for (door = 0; door <= 5; door++)
      {
      struct room_direction_data       *pexit;
      
      if ((pexit = world[was_in_room].dir_option[door]) != NULL
	  && pexit->to_room != NOWHERE
	  && pexit->to_room != was_in_room)
	 {
	 IN_ROOM(ch) = pexit->to_room;
	 MOBTrigger  = FALSE;
	 act(arg, FALSE, ch, NULL, NULL, TO_ROOM);
	 }
      }

   IN_ROOM(ch) = was_in_room;
   release_buffer(arg);
   return;

}

/* lets the mobile kill any player or mobile without murder*/

ACMD(do_mpkill)
{
   char *arg;
   struct char_data *victim;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   arg=get_buffer(MAX_STRING_LENGTH);
   one_argument(argument, arg);

   if (arg[0] == '\0')
      {
      bug("MpKill - no argument: vnum %ld.",
	  mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   if ((victim = get_char_room_vis(ch, arg)) == NULL)
      {
      bug("MpKill - Victim not in room: vnum %ld.",
	  mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   release_buffer(arg);
   if (victim == ch)
      {
      bug("MpKill - Bad victim to attack: vnum %ld.",
	  mob_index[ch->nr].vnum);
      return;
      }
 
   if (AFF_FLAGGED(ch, AFF_CHARM) && ch->master == victim)
      {
      bug("MpKill - Charmed mob attacking master: vnum %ld.",
	  mob_index[ch->nr].vnum);
      return;
      }

   if (ch->char_specials.position == POS_FIGHTING)
      {	
      bug("MpKill - Already fighting: vnum %ld",
	  mob_index[ch->nr].vnum);
      return;
      }

   hit(ch, victim, -1);
   return;
}

 
/* lets the mobile destroy an object in its inventory
   it can also destroy a worn object and it can destroy 
   items using all.xxxxx or just plain all of them */

ACMD(do_mpjunk)
{
   char *arg;
   int pos;
   struct obj_data *obj;
   struct obj_data *obj_next;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc) 
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   arg=get_buffer(MAX_STRING_LENGTH);
   one_argument(argument, arg);

   if (arg[0] == '\0') 
      {
      bug("Mpjunk - No argument: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }
 
   if (str_cmp(arg, "all") && str_prefix("all.", arg)) 
      {
      if ((obj=get_object_in_equip_vis(ch,arg,ch->equipment,&pos))!= NULL) 
	 {
	 unequip_char(ch, pos);
	 extract_obj(obj);
	 release_buffer(arg);
	 return;
	 }
      if ((obj = get_obj_in_list_vis(ch, arg, ch->carrying)) != NULL)
	 extract_obj(obj);
      release_buffer(arg);
      return;
      }
   else 
      {
      for (obj = ch->carrying; obj != NULL; obj = obj_next) 
	 {
	 obj_next = obj->next_content;
	 if (arg[3] == '\0' || isname(arg+4, obj->name)) 
	    {
	    extract_obj(obj);
	    }
	 }
      while((obj=get_object_in_equip_vis(ch,arg,ch->equipment,&pos))!=NULL)
	 {
	 unequip_char(ch, pos);
	 extract_obj(obj);
	 }   
      }
   release_buffer(arg);
   return;
}

/* prints the message to everyone in the room other than the mob and victim */

ACMD(do_mpechoaround)
{
   char *arg;
   struct char_data *victim;
   char *p;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   arg=get_buffer(MAX_STRING_LENGTH);
   p=one_argument(argument, arg);
   while(isspace((int)*p)) p++; /* skip over leading space */

   if (arg[0] == '\0')
      {
      bug("Mpechoaround - No argument:  vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   if (!(victim=get_char_room_vis(ch, arg)))
      {
      bug("Mpechoaround - victim does not exist: vnum %ld.",
	  mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   act(p, FALSE, ch, NULL, victim, TO_NOTVICT);
   release_buffer(arg);
   return;
}

/* prints the message to only the victim */

ACMD(do_mpechoat)
{
   char *arg;
   struct char_data *victim;
   char *p;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   arg=get_buffer(MAX_STRING_LENGTH);
   p = one_argument(argument, arg);
   while(isspace((int)*p)) p++; /* skip over leading space */

   if (arg[0] == '\0')
      {
      bug("Mpechoat - No argument:  vnum %ld.",
	  mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   if (!(victim = get_char_room_vis(ch, arg)))
      {
      bug("Mpechoat - victim does not exist: vnum %ld.",
	  mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   act(p,FALSE,  ch, NULL, victim, TO_VICT);
   release_buffer(arg);
   return;
}

/* prints the message to the room at large */

ACMD(do_mpecho)
{
   char *p;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   if (argument[0] == '\0')
      {
      bug("Mpecho - called w/o argument: vnum %ld.",
	  mob_index[ch->nr].vnum);
      return;
      }
   p = argument;
   while(isspace((int)*p)) p++;

   act(p,FALSE,  ch, NULL, NULL, TO_ROOM);
   return;

}
 
/* lets the mobile load an item or mobile.  All items
are loaded into inventory.  you can specify a level with
the load object portion as well. */

ACMD(do_mpmload)
{
   char *arg;
   struct index_data *pMobIndex;
   struct char_data      *victim;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   arg=get_buffer(MAX_STRING_LENGTH);
   one_argument(argument, arg);

   if (arg[0] == '\0' || !is_number(arg))
      {
      bug("Mpmload - Bad vnum as arg: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   if ((pMobIndex = get_mob_index(atoi(arg))) == NULL)
      {
      bug("Mpmload - Bad mob vnum: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }
 
   victim = read_mobile(atoi(arg), VIRTUAL);
   GET_MOB_VAL(victim,0)=GET_ROOM_VNUM(IN_ROOM(ch));
   victim->orig_room=IN_ROOM(ch);
   char_to_room(victim, IN_ROOM(ch));
   release_buffer(arg);
   return;
}

ACMD(do_mpoload)
{
   char *arg1;
   struct index_data *pObjIndex;
   struct obj_data       *obj;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   arg1=get_buffer(MAX_STRING_LENGTH);
   argument = one_argument(argument, arg1);
 
   if (arg1[0] == '\0' || !is_number(arg1))
      {
      bug("Mpoload - Bad syntax: vnum %ld.",
	  mob_index[ch->nr].vnum);
      release_buffer(arg1);
      return;
      }
  
   if ((pObjIndex = get_obj_index(atoi(arg1))) == NULL)
      {
      bug("Mpoload - Bad vnum arg: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg1);
      return;
      }

   obj = read_object(atoi(arg1), VIRTUAL);
   release_buffer(arg1);
   if (obj == NULL)
      return;
   if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) 
      {
      obj_to_char(obj, ch);
      }
   else 
      {
      obj_to_room(obj, IN_ROOM(ch));
      }

   return;
}

/* lets the mobile purge all objects and other npcs in the room,
   or purge a specified object or mob in the room.  It can purge
   itself, but this had best be the last command in the MOBprogram
   otherwise ugly stuff will happen */

ACMD(do_mppurge)
{
   struct char_data *victim;
   struct obj_data  *obj;
   char *arg;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      } 
 
   arg=get_buffer(MAX_STRING_LENGTH);
   one_argument(argument, arg);

   if (arg[0] == '\0')
      {
     /* 'purge' */
      struct char_data *vnext;
      struct obj_data  *obj_next;

      for (victim = world[IN_ROOM(ch)].people; victim != NULL; victim = vnext)
	 {
	 vnext = victim->next_in_room;
	 if (IS_NPC(victim) && victim != ch)
	    extract_char(victim);
	 }

      for (obj = world[IN_ROOM(ch)].contents; obj != NULL; obj = obj_next)
	 {
	 obj_next = obj->next_content;
	 extract_obj(obj);
	 }
 
      release_buffer(arg);
      return;
      }
 
   if (!(victim = get_char_room_vis(ch, arg)))
      {
      if ((obj = get_obj_vis(ch, arg))) 
	 {
	 extract_obj(obj);
	 }
      else 
	 {
	 bug("Mppurge - Bad argument: vnum %ld.",mob_index[ch->nr].vnum);
	 }
      release_buffer(arg);
      return;
      }
 
   if (!IS_NPC(victim))
      {
      bug("Mppurge - Purging a PC: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }
 
   extract_char(victim);
   release_buffer(arg);
   return;
}
 
 
/* lets the mobile goto any location it wishes that is not private */
 
ACMD(do_mpgoto)
{
   char *arg;
   room_rnum location;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }
 
   arg=get_buffer(MAX_STRING_LENGTH);
   one_argument(argument, arg);
   if (arg[0] == '\0')
      {
      bug("Mpgoto - No argument: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }
 
   if ((location = find_target_room(ch, arg)) < 0)
      {
      bug("Mpgoto - No such location: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }
 
   if (FIGHTING(ch) != NULL)
      stop_fighting(ch);
 
   char_from_room(ch);
   char_to_room(ch, location);
 
   release_buffer(arg);
   return;
}

/* lets the mobile do a command at another location. Very useful */
 
ACMD(do_mpat)
{
   char *arg;
   room_rnum location;
   room_rnum original;
/*    struct char_data       *wch; */

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }
 
   arg=get_buffer(MAX_STRING_LENGTH);
   argument = one_argument(argument, arg);

   if (arg[0] == '\0' || argument[0] == '\0')
      {
      bug("Mpat - Bad argument: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   if ((location = find_target_room(ch, arg)) < 0)
      {
      bug("Mpat - No such location: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   original = IN_ROOM(ch);
   char_from_room(ch);
   char_to_room(ch, location);
   command_interpreter(ch, argument);

  /*
   * See if 'ch' still exists before continuing!
   * Handles 'at XXXX quit' case.
   */
   if(IN_ROOM(ch) == location) 
      {
      char_from_room(ch);
      char_to_room(ch, original);
      }

   release_buffer(arg);
   return;
}
 
/* lets the mobile transfer people.  the all argument transfers
   everyone in the current room to the specified location */

ACMD(do_mptransfer)
{
   char *arg1;
   char *arg2;
   room_rnum location;
   struct descriptor_data *d;
   struct char_data       *victim;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   arg2=get_buffer(MAX_STRING_LENGTH);
   arg1=get_buffer(MAX_STRING_LENGTH);
   argument = one_argument(argument, arg1);
   argument = one_argument(argument, arg2);
 
   if (arg1[0] == '\0')
      {
      bug("Mptransfer - Bad syntax: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg1);
      release_buffer(arg2);
      return;
      }
 
   if (!str_cmp(arg1, "all"))
      {
      for (d = descriptor_list; d != NULL; d = d->next)
	 {
	 if (STATE(d) == CON_PLAYING
	     &&   d->character != ch
	     &&   IN_ROOM(d->character) != NOWHERE
	     &&   CAN_SEE(ch, d->character))
 	    {
	    char buf[MAX_STRING_LENGTH];
	    sprintf(buf, "%s %s", d->character->player.name, arg2);
	    do_trans(ch, buf, cmd, 0);
 	    }
	 }
      release_buffer(arg1);
      release_buffer(arg2);
      return;
      }
 
  /*
   * Thanks to Grodyn for the optional location parameter.
   */
   if (arg2[0] == '\0')
      {
      location = IN_ROOM(ch);
      }
   else
      {
      if ((location = find_target_room(ch, arg2)) < 0)
	 {
	 bug("Mptransfer - No such location: vnum %ld.",
	     mob_index[ch->nr].vnum);
	 release_buffer(arg1);
	 release_buffer(arg2);
	 return;
	 }
 
      if (IS_SET(world[location].room_flags, ROOM_PRIVATE))
	 {
	 bug("Mptransfer - Private room: vnum %ld.",
	     mob_index[ch->nr].vnum);
	 release_buffer(arg1);
	 release_buffer(arg2);
	 return;
	 }
      }
 
   if ((victim = get_char_vis(ch, arg1,FIND_CHAR_WORLD)) == NULL)
      {
      bug("Mptransfer - No such person: vnum %ld.",
	  mob_index[ch->nr].vnum);
      release_buffer(arg1);
      release_buffer(arg2);
      return;
      }
 
   if (IN_ROOM(victim) == 0)
      {
      bug("Mptransfer - Victim in Limbo: vnum %ld.",
	  mob_index[ch->nr].vnum);
      release_buffer(arg1);
      release_buffer(arg2);
      return;
      }
 
   if (FIGHTING(victim) != NULL)
      stop_fighting(victim);
 
   char_from_room(victim);
   char_to_room(victim, location);
 
   release_buffer(arg1);
   release_buffer(arg2);
   return;
}
 
/* lets the mobile force someone to do something.  must be mortal level
   and the all argument only affects those in the room with the mobile */

ACMD(do_mpforce)
{
   char *arg;

   if (!IS_NPC(ch) || AFF_FLAGGED(ch, AFF_CHARM) || ch->desc)
      {
      send_to_char(ch,"Huh?\r\n");
      return;
      }

   arg=get_buffer(MAX_STRING_LENGTH);
   argument = one_argument(argument, arg);

   if (arg[0] == '\0' || argument[0] == '\0')
      {
      bug("Mpforce - Bad syntax: vnum %ld.", mob_index[ch->nr].vnum);
      release_buffer(arg);
      return;
      }

   if (!str_cmp(arg, "all")) 
      {
      struct descriptor_data *i;
      struct char_data *vch;

      for (i = descriptor_list; i ; i = i->next) 
	 {
	 if(i->character != ch && STATE(i)==CON_PLAYING &&
	    IN_ROOM(i->character) == IN_ROOM(ch)) 
	    {
	    vch = i->character;
	    if((GET_LEVEL(vch) >= LVL_IMMORT)&&PRF_FLAGGED(vch,PRF_NOHASSLE)) 
	       {
	       send_to_char(ch,"You can't force an immortal.\r\n");
	       return;
	       }
	    if(GET_LEVEL(vch) < GET_LEVEL(ch) && CAN_SEE(ch, vch)) 
	       {
	       command_interpreter(vch, argument);
	       }
	    }
         }
      }
   else 
      {
      struct char_data *victim;
 
      if ((victim = get_char_room_vis(ch, arg)) == NULL) 
	 {
	 bug("Mpforce - No such victim: vnum %ld.",
	     mob_index[ch->nr].vnum);
	 release_buffer(arg);
	 return;
	 }
 
      if (victim == ch) 
	 {
	 bug("Mpforce - Forcing oneself: vnum %ld.",
	     mob_index[ch->nr].vnum);
	 release_buffer(arg);
	 return;
	 }
 
      command_interpreter(victim, argument);
      }
 
   release_buffer(arg);
   return;
}
