#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "buffer.h"

extern struct room_data *world;
extern struct descriptor_data *descriptor_list;
extern struct zone_data *zone_table;

/* same as any_one_arg except that it stops at punctuation */
char *any_one_name(char *argument, char *first_arg)
   {
   char* arg;

   /* Find first non blank */
   while(isspace((int)*argument))
      argument++;

   /* Find length of first word */
   for(arg = first_arg ;
           (*argument&&!isspace((int)*argument) &&
            (!ispunct((int)*argument)||*argument=='#'||*argument == '-')) ;
           arg++, argument++)
      *arg = LOWER(*argument);
   *arg = '\0';

   return argument;
   }


void sub_write_to_char(char_data *ch, char *tokens[],
                       void *otokens[], char type[])
   {
   char *sb=get_buffer(MAX_STRING_LENGTH);
   int i;

   strcpy(sb,"");

   for (i = 0; tokens[i + 1]; i++)
      {
      strcat(sb,tokens[i]);

      switch (type[i])
         {
      case '~':
         if (!otokens[i])
            strcat(sb,"someone");
         else if ((char_data *)otokens[i] == ch)
            strcat(sb,"you");
         else
            strcat(sb,PERS((char_data *)otokens[i], ch));
         break;

      case '@':
         if (!otokens[i])
            strcat(sb,"someone's");
         else if ((char_data *)otokens[i] == ch)
            strcat(sb,"your");
         else
            {
            strcat(sb,PERS((char_data *) otokens[i], ch));
            strcat(sb,"'s");
            }
         break;

      case '^':
         if (!otokens[i] || !CAN_SEE(ch, (char_data *) otokens[i]))
            strcat(sb,"its");
         else if (otokens[i] == ch)
            strcat(sb,"your");
         else
            strcat(sb,HSHR((char_data *) otokens[i]));
         break;

      case '$':
         if (!otokens[i] || !CAN_SEE(ch, (char_data *) otokens[i]))
            strcat(sb,"it");
         else if (otokens[i] == ch)
            strcat(sb,"you");
         else
            strcat(sb,HSSH((char_data *) otokens[i]));
         break;

      case '*':
         if (!otokens[i] || !CAN_SEE(ch, (char_data *) otokens[i]))
            strcat(sb,"it");
         else if (otokens[i] == ch)
            strcat(sb,"you");
         else
            strcat(sb,HMHR((char_data *) otokens[i]));
         break;

      case '`':
         if (!otokens[i])
            strcat(sb,"something");
         else
            strcat(sb,OBJS(((obj_data *) otokens[i]), ch));
         break;
         }
      }

   strcat(sb,tokens[i]);
   strcat(sb,"\r\n");
   sb[0] = toupper(sb[0]);
   send_to_char(ch,"%s",sb);
   release_buffer(sb);
   }


void sub_write(char *arg, char_data *ch, byte find_invis, int targets)
   {
   char *str;
   char *type, *name;
   char *tokens[MAX_INPUT_LENGTH], *s, *p;
   void *otokens[MAX_INPUT_LENGTH];
   char_data *to;
   obj_data *obj;
   int i, tmp;
   int to_sleeping =1;

   if (!arg)
      return;

   str=get_buffer(MAX_INPUT_LENGTH*2);
   type=get_buffer(MAX_INPUT_LENGTH);
   name=get_buffer(MAX_INPUT_LENGTH);

   tokens[0] = str;

   for (i = 0, p = arg, s = str; *p;)
      {
      switch (*p)
         {
      case '~':
      case '@':
      case '^':
      case '$':
      case '*':
         /* get char_data, move to next token */
         type[i] = *p;
         *s = '\0';
         p = any_one_name(++p, name);
         otokens[i] = find_invis ? get_char(name) : get_char_room_vis(ch, name);
         tokens[++i] = ++s;
         break;

      case '`':
         /* get obj_data, move to next token */
         type[i] = *p;
         *s = '\0';
         p = any_one_name(++p, name);
         otokens[i] =
            find_invis ? (obj = get_obj(name)) :
            ((obj = get_obj_in_list_vis(ch, name,
                                        world[IN_ROOM(ch)].contents)) ? obj :
             (obj = get_object_in_equip_vis(ch, name,
                                            ch->equipment, &tmp)) ?
             obj :
             (obj = get_obj_in_list_vis(ch, name, ch->carrying)));
         otokens[i] = obj;
         tokens[++i] = ++s;
         break;

      case '\\':
         p++;
         *s++ = *p++;
         break;

      default:
         *s++ = *p++;
         }
      }

   *s = '\0';
   tokens[++i] = NULL;

   if (IS_SET(targets, TO_CHAR) && SENDOK(ch))
      sub_write_to_char(ch, tokens, otokens, type);

   if (IS_SET(targets, TO_ROOM))
      for (to = world[ch->in_room].people;
              to; to = to->next_in_room)
         if (to != ch && SENDOK(to))
            sub_write_to_char(to, tokens, otokens, type);

   release_buffer(name);
   release_buffer(type);
   release_buffer(str);
   }

void send_to_zone(char *messg, zone_rnum zrn)
   {
   struct descriptor_data *i;

   if (!messg || !*messg)
      return;

   for (i = descriptor_list; i; i = i->next)
      if (!i->connected && i->character && AWAKE(i->character) &&
              (IN_ROOM(i->character) != NOWHERE) &&
              (world[IN_ROOM(i->character)].zone == zrn))
         SEND_TO_Q(i, "%s", messg);
   }





