/**************************************************************************
*  File: wldcmd.c                                                         *
*  Usage: contains the command_interpreter for rooms,                     *
*         room commands.                                                  *
*                                                                         *
*                                                                         *
*  $Author: lucas $
*  $Date: 2006/09/15 02:02:06 $
*  $Revision: 1.1.1.1 $
**************************************************************************/

#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"


#include "structs.h"
#include "screen.h"
#include "dg_scripts.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "buffer.h"

extern struct room_data *world;
extern char *dirs[];
extern struct zone_data *zone_table;
extern int top_of_zone_table;

void die(struct char_data * ch, struct char_data * killer);
void sub_write(char *arg, char_data *ch, byte find_invis, int targets);
void send_to_zone(char *messg, zone_rnum zrn);
long asciiflag_conv(char *flag);
char_data *get_char_by_room(room_data *room, char *name);
room_data *get_room(char *name);
obj_data *get_obj_by_room(room_data *room, char *name);
void script_log(char *msg);
void wld_command_interpreter(room_data *room, char *argument);
void death_cry(struct char_data *ch);

#define WCMD(name)  \
void (name)(room_data *room, char *argument, int cmd, int subcmd)


struct wld_command_info {
   char *command;
   void (*command_pointer)
   (room_data *room, char *argument, int cmd, int subcmd);
   int subcmd;
   };


/* do_wsend */
#define SCMD_WSEND        0
#define SCMD_WECHOAROUND  1



/* attaches room vnum to msg and sends it to script_log */
void wld_log(room_data *room, char *msg)
   {
   char *buf=get_buffer(MAX_INPUT_LENGTH + 100);


   sprintf(buf, "Wld (room %ld): %s", room->number, msg);
   script_log(buf);
   release_buffer(buf);
   }


/* sends str to room */
void act_to_room(char *str, room_data *room)
   {
   /* no one is in the room */
   if (!room->people)
      return;

   /*
    * since you can't use act(..., TO_ROOM) for an room, send it
    * TO_ROOM and TO_CHAR for some char in the room.
    * (just dont use $n or you might get strange results)
    */
   act(str, FALSE, room->people, 0, 0, TO_ROOM);
   act(str, FALSE, room->people, 0, 0, TO_CHAR);
   }



/* World commands */

/* prints the argument to all the rooms aroud the room */
WCMD(do_wasound)
   {
   int  door;

   skip_spaces(&argument);

   if (!*argument)
      {
      wld_log(room, "wasound called with no argument");
      return;
      }

   for (door = 0; door < NUM_OF_DIRS; door++)
      {
      struct room_direction_data *exit_dir;

      if ((exit_dir = room->dir_option[door])&&(exit_dir->to_room != NOWHERE)&&
              room != &world[exit_dir->to_room])
         act_to_room(argument, &world[exit_dir->to_room]);
      }
   }


WCMD(do_wecho)
   {
   skip_spaces(&argument);

   if (!*argument)
      wld_log(room, "wecho called with no args");

   else
      act_to_room(argument, room);
   }


WCMD(do_wsend)
   {
   char *buf=get_buffer(MAX_INPUT_LENGTH);
   char *msg;
   char_data *ch;

   msg = any_one_arg(argument, buf);

   if (!*buf)
      {
      wld_log(room, "wsend called with no args");
      release_buffer(buf);
      return;
      }

   skip_spaces(&msg);

   if (!*msg)
      {
      wld_log(room, "wsend called without a message");
      release_buffer(buf);
      return;
      }

   if ((ch = get_char_by_room(room, buf)))
      {
      if (subcmd == SCMD_WSEND)
         sub_write(msg, ch, TRUE, TO_CHAR);
      else if (subcmd == SCMD_WECHOAROUND)
         sub_write(msg, ch, TRUE, TO_ROOM);
      }

   else
      wld_log(room, "no target found for wsend");
   release_buffer(buf);
   }

static int real_zone(int vnumber)
   {
   int counter;

   for (counter = 0; counter <= top_of_zone_table; counter++)
      if ((vnumber >= (zone_table[counter].number * 100)) &&
              (vnumber <= (zone_table[counter].top)))
         return counter;

   return -1;
   }

WCMD(do_wzoneecho)
   {
   int zone;
   char *zone_name=get_buffer(MAX_INPUT_LENGTH);
   char *buf=get_buffer(MAX_INPUT_LENGTH);
   char *msg;

   msg = any_one_arg(argument, zone_name);
   skip_spaces(&msg);

   if (!*zone_name || !*msg)
      wld_log(room, "wzoneecho called with too few args");

   else if ((zone = real_zone(atoi(zone_name))) < 0)
      wld_log(room, "wzoneecho called for nonexistant zone");

   else
      {
      sprintf(buf, "%s\r\n", msg);
      send_to_zone(buf, zone);
      }
   release_buffer(buf);
   release_buffer(zone_name);
   }


WCMD(do_wdoor)
   {
   char *target=get_buffer(MAX_INPUT_LENGTH);
   char *direction=get_buffer(MAX_INPUT_LENGTH);
   char *field=get_buffer(MAX_INPUT_LENGTH);
   char *value;
   room_data *rm;
   struct room_direction_data *exit_dir;
   int dir=0, fd=0, to_room=0;

   char *door_field[] = {
                           "purge",
                           "description",
                           "flags",
                           "key",
                           "name",
                           "room",
                           "\n"
                        };


   argument = two_arguments(argument, target, direction);
   value = one_argument(argument, field);
   skip_spaces(&value);

   if (!*target || !*direction || !*field)
      {
      wld_log(room, "wdoor called with too few args");
      release_buffer(field);
      release_buffer(direction);
      release_buffer(target);
      return;
      }

   if ((rm = get_room(target)) == NULL)
      {
      wld_log(room, "wdoor: invalid target");
      release_buffer(field);
      release_buffer(direction);
      release_buffer(target);
      return;
      }

   if ((dir = search_block(direction, dirs, FALSE)) == -1)
      {
      wld_log(room, "wdoor: invalid direction");
      release_buffer(field);
      release_buffer(direction);
      release_buffer(target);
      return;
      }

   if ((fd = search_block(field,(char **)door_field, FALSE)) == -1)
      {
      wld_log(room, "wdoor: invalid field");
      release_buffer(field);
      release_buffer(direction);
      release_buffer(target);
      return;
      }

   exit_dir = rm->dir_option[dir];

   /* purge exit */
   if (fd == 0)
      {
      if (exit_dir)
         {
         if (exit_dir->general_description)
            free(exit_dir->general_description);
         if (exit_dir->keyword)
            free(exit_dir->keyword);
         free(exit_dir);
         rm->dir_option[dir] = NULL;
         }
      }

   else
      {
      if (!exit_dir)
         {
         CREATE(exit_dir, struct room_direction_data, 1);
         rm->dir_option[dir] = exit_dir;
         exit_dir->to_room = -1;
         }

      switch (fd)
         {
      case 1:  /* description */
         if (exit_dir->general_description)
            free(exit_dir->general_description);
         CREATE(exit_dir->general_description, char, strlen(value) + 3);
         strcpy(exit_dir->general_description, value);
         strcat(exit_dir->general_description, "\r\n");
         break;
      case 2:  /* flags       */
         exit_dir->exit_info = (sh_int)asciiflag_conv(value);
         break;
      case 3:  /* key         */
         exit_dir->key = atoi(value);
         break;
      case 4:  /* name        */
         if (exit_dir->keyword)
            free(exit_dir->keyword);
         CREATE(exit_dir->keyword, char, strlen(value) + 1);
         strcpy(exit_dir->keyword, value);
         break;
      case 5:  /* room        */
         if ((to_room = real_room(atoi(value))) != NOWHERE)
            exit_dir->to_room = to_room;
         else
            wld_log(room, "wdoor: invalid door target");
         break;
         }
      }
   release_buffer(field);
   release_buffer(direction);
   release_buffer(target);
   }


WCMD(do_wteleport)
   {
   char_data *ch, *next_ch;
   room_rnum target, nr;
   char *arg1=get_buffer(MAX_INPUT_LENGTH);
   char *arg2=get_buffer(MAX_INPUT_LENGTH);

   two_arguments(argument, arg1, arg2);

   if (!*arg1 || !*arg2)
      {
      wld_log(room, "wteleport called with too few args");
      release_buffer(arg2);
      release_buffer(arg1);
      return;
      }

   nr = atoi(arg2);
   target = real_room(nr);

   if (target == NOWHERE)
      wld_log(room, "wteleport target is an invalid room");

   else if (!str_cmp(arg1, "all"))
      {
      if (nr == room->number)
         {
         wld_log(room, "wteleport all target is itself");
         release_buffer(arg2);
         release_buffer(arg1);
         return;
         }

      for (ch = room->people; ch; ch = next_ch)
         {
         next_ch = ch->next_in_room;

         if (IS_NPC(ch) || !PRF_FLAGGED(ch,PRF_NOHASSLE))
            {
            char_from_room(ch);
            char_to_room(ch, target);
            if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_DEATH))
               {
               if (GET_LEVEL(ch) < LVL_IMMORT)
                  {
		    if (handleGetOutOfDeathFree(ch)) {
		      return;
		    }

                  log_death_trap(ch);
                  death_cry(ch);
                  extract_char(ch);
                  }
               }
            }
         }
      }
   else
      {
      if ((ch = get_char_by_room(room, arg1)))
         {
         if (IS_NPC(ch) || !PRF_FLAGGED(ch,PRF_NOHASSLE))
            {
            char_from_room(ch);
            char_to_room(ch, target);
            if (ROOM_FLAGGED(IN_ROOM(ch), ROOM_DEATH))
               {
               if (GET_LEVEL(ch) < LVL_IMMORT)
                  {
		    if (handleGetOutOfDeathFree(ch)) {
		      return 1;
		    }

                  log_death_trap(ch);
                  death_cry(ch);
                  extract_char(ch);
                  }
               }
            }
         }
      else
         {
         wld_log(room, "wteleport: no target found");
         }
      }
   release_buffer(arg2);
   release_buffer(arg1);
   }


WCMD(do_wforce)
   {
   char_data *ch, *next_ch;
   char *arg1=get_buffer(MAX_INPUT_LENGTH);
   char *line;

   line = one_argument(argument, arg1);

   if (!*arg1 || !*line)
      {
      wld_log(room, "wforce called with too few args");
      release_buffer(arg1);
      return;
      }

   if (!str_cmp(arg1, "all"))
      {
      for (ch = room->people; ch; ch = next_ch)
         {
         next_ch = ch->next_in_room;

         if (IS_NPC(ch) || !PRF_FLAGGED(ch,PRF_NOHASSLE))
            {
            command_interpreter(ch, line);
            }
         }
      }

   else
      {
      if ((ch = get_char_by_room(room, arg1)))
         {
         if (IS_NPC(ch) || !PRF_FLAGGED(ch,PRF_NOHASSLE))
            {
            command_interpreter(ch, line);
            }
         }

      else
         wld_log(room, "wforce: no target found");
      }
   release_buffer(arg1);
   }


/* increases the target's exp */
WCMD(do_wexp)
   {
   char_data *ch;
   char *name=get_buffer(MAX_INPUT_LENGTH);
   char *amount=get_buffer(MAX_INPUT_LENGTH);

   two_arguments(argument, name, amount);

   if (!*name || !*amount)
      {
      wld_log(room, "wexp: too few arguments");
      release_buffer(amount);
      release_buffer(name);
      return;
      }

   if ((ch = get_char_by_room(room, name)))
      gain_exp(ch, atoi(amount));
   else
      {
      wld_log(room, "wexp: target not found");
      }
   release_buffer(amount);
   release_buffer(name);
   }


/* purge all objects an npcs in room, or specified object or mob */
WCMD(do_wpurge)
   {
   char *arg=get_buffer(MAX_INPUT_LENGTH);
   char_data *ch, *next_ch;
   obj_data *obj, *next_obj;

   one_argument(argument, arg);

   if (!*arg)
      {
      for (ch = room->people; ch; ch = next_ch )
         {
         next_ch = ch->next_in_room;
         if (IS_NPC(ch))
            extract_char(ch);
         }

      for (obj = room->contents; obj; obj = next_obj )
         {
         next_obj = obj->next_content;
         extract_obj(obj);
         }

      release_buffer(arg);
      return;
      }

   if (!(ch = get_char_by_room(room, arg)))
      {
      if ((obj = get_obj_by_room(room, arg)))
         {
         extract_obj(obj);
         }
      else
         wld_log(room, "wpurge: bad argument");

      release_buffer(arg);
      return;
      }

   if (!IS_NPC(ch) )
      {
      wld_log(room, "wpurge: purging a PC");
      release_buffer(arg);
      return;
      }

   extract_char(ch);
   release_buffer(arg);
   }


/* loads a mobile or object into the room */
WCMD(do_wload)
   {
   char *arg1=get_buffer(MAX_INPUT_LENGTH);
   char *arg2=get_buffer(MAX_INPUT_LENGTH);
   int vnumber = 0;
   char_data *mob;
   obj_data *object;


   two_arguments(argument, arg1, arg2);

   if (!*arg1 || !*arg2 || !is_number(arg2) || ((vnumber = atoi(arg2)) < 0))
      {
      wld_log(room, "wload: bad syntax");
      release_buffer(arg2);
      release_buffer(arg1);
      return;
      }

   if (is_abbrev(arg1, "mob"))
      {
      if ((mob = read_mobile(vnumber, VIRTUAL)) == NULL)
         {
         wld_log(room, "wload: bad mob vnum");
         release_buffer(arg2);
         release_buffer(arg1);
         return;
         }
      GET_MOB_VAL(mob,0)=room->number;
      mob->orig_room=real_room(room->number);
      char_to_room(mob, real_room(room->number));
      load_mtrigger(mob);
      }

   else if (is_abbrev(arg1, "obj"))
      {
      if ((object = read_object(vnumber, VIRTUAL)) == NULL)
         {
         wld_log(room, "wload: bad object vnum");
         release_buffer(arg2);
         release_buffer(arg1);
         return;
         }

      obj_to_room(object, real_room(room->number));
      load_otrigger(object);
      }

   else
      wld_log(room, "wload: bad type");
   release_buffer(arg2);
   release_buffer(arg1);
   }

WCMD(do_wdamage)
   {
   char *name=get_buffer(MAX_INPUT_LENGTH);
   char *amount=get_buffer(MAX_INPUT_LENGTH);
   int dam = 0;
   char_data *ch;

   two_arguments(argument, name, amount);

   if (!*name || !*amount || !isdigit((int)*amount))
      {
      wld_log(room, "wdamage: bad syntax");
      release_buffer(amount);
      release_buffer(name);
      return;
      }

   dam = atoi(amount);

   if ((ch = get_char_by_room(room, name)))
      {
      if (!IS_NPC(ch)&&PRF_FLAGGED(ch,PRF_NOHASSLE))
         {
         send_to_char(ch,"Being a god, you carefully avoid a trap.");
         release_buffer(amount);
         release_buffer(name);
         return;
         }
      GET_HIT(ch) -= dam;
      GET_HIT(ch) = MAX(GET_HIT(ch),-10);
      if (dam < 0)
         {
         send_to_char(ch,"You feel rejuvinated.\r\n");
         release_buffer(amount);
         release_buffer(name);
         return;
         }

      update_pos(ch);
      switch (GET_POS(ch))
         {
      case POS_MORTALLYW:
         act("$n is mortally wounded, and will die soon, if not aided.", TRUE, ch, 0, 0, TO_ROOM);
         send_to_char(ch,"You are mortally wounded, and will die soon, if not aided.\r\n");
         break;
      case POS_INCAP:
         act("$n is incapacitated and will slowly die, if not aided.", TRUE, ch, 0, 0, TO_ROOM);
         send_to_char(ch,"You are incapacitated and will slowly die, if not aided.\r\n");
         break;
      case POS_STUNNED:
         act("$n is stunned, but will probably regain consciousness again.", TRUE, ch, 0, 0, TO_ROOM);
         send_to_char(ch,"You're stunned, but will probably regain consciousness again.\r\n");
         break;
      case POS_DEAD:
         act("$n is dead!  R.I.P.", FALSE, ch, 0, 0, TO_ROOM);
         send_to_char(ch,"You are dead!  Sorry...\r\n");
         break;

      default:   /* >= POSITION SLEEPING */
         if (dam > (GET_MAX_HIT(ch) >> 2))
            act("That really did HURT!", FALSE, ch, 0, 0, TO_CHAR);
         if (GET_HIT(ch) < (GET_MAX_HIT(ch) >> 2))
            {
            send_to_char(ch, "%sYou wish that your wounds would stop BLEEDING so much!%s\r\n",
                         CCRED(ch, C_SPR), CCNRM(ch, C_SPR));
            }
         }
      if (GET_POS(ch) == POS_DEAD)
         {
         if (!IS_NPC(ch))
            {
            mudlogf(BRF, LVL_IMMORT, TRUE, "%s killed by a trap at %s",
                    GET_NAME(ch), world[IN_ROOM(ch)].name);

            }
         die(ch, ch);
         }
      }
   else
      wld_log(room, "wdamage: target not found");
   release_buffer(amount);
   release_buffer(name);
   }

WCMD(do_wat)
   {
   char *location=get_buffer(MAX_INPUT_LENGTH);
   char *arg2=get_buffer(MAX_INPUT_LENGTH);
   room_vnum vnum = 0;
   room_data *r2;
   room_rnum rnum=-1;
   half_chop(argument, location, arg2);

   if (!*location || !*arg2 || !isdigit((int)*location))
      {
      wld_log(room, "wat: bad syntax");
      release_buffer(arg2);
      release_buffer(location);
      return;
      }
   vnum = atol(location);
   if (NOWHERE == (rnum=real_room(vnum)))
      {
      wld_log(room, "wat: location not found");
      release_buffer(arg2);
      release_buffer(location);
      return;
      }

   r2 = &world[rnum];
   wld_command_interpreter(r2, arg2);
   release_buffer(arg2);
   release_buffer(location);
   }



const struct wld_command_info wld_cmd_info[] = {
            { "RESERVED", 0, 0 }
         ,/* this must be first -- for specprocs */

         { "wasound"    , do_wasound   , 0 },
         { "wdoor"      , do_wdoor     , 0 },
         { "wecho"      , do_wecho     , 0 },
         { "wechoaround", do_wsend     , SCMD_WECHOAROUND },
         { "wexp"       , do_wexp      , 0 },
         { "wforce"     , do_wforce    , 0 },
         { "wload"      , do_wload     , 0 },
         { "wpurge"     , do_wpurge    , 0 },
         { "wsend"      , do_wsend     , SCMD_WSEND },
         { "wteleport"  , do_wteleport , 0 },
         { "wzoneecho"  , do_wzoneecho , 0 },
         { "wdamage"    , do_wdamage,    0 },
         { "wat"        , do_wat,        0 },
         { "\n", 0, 0 } /* this must be last */
         };


/*
 *  This is the command interpreter used by rooms, called by script_driver.
 */
void wld_command_interpreter(room_data *room, char *argument)
   {
   int cmd, length;
   char *line;
   char *arg;

   skip_spaces(&argument);

   /* just drop to next line for hitting CR */
   if (!*argument)
      return;

   arg=get_buffer(MAX_INPUT_LENGTH);
   line = any_one_arg(argument, arg);


   /* find the command */
   for (length = strlen(arg), cmd = 0;
           *wld_cmd_info[cmd].command != '\n'; cmd++)
      if (!strncmp(wld_cmd_info[cmd].command, arg, length))
         break;

   if (*wld_cmd_info[cmd].command == '\n')
      {
      char *buf2=get_buffer(256);
      sprintf(buf2, "Unknown world cmd: '%s'", argument);
      wld_log(room, buf2);
      release_buffer(buf2);
      }
   else
      ((*wld_cmd_info[cmd].command_pointer)
              (room, line, cmd, wld_cmd_info[cmd].subcmd));
   release_buffer(arg);
   }
