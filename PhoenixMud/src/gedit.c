/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *  gedit.c:  Olc written for shoplike guildmasters, code by             *
 *             Jason Goodwin                                             *
 *    Made for Circle3.0 bpl11, its copyright applies                    *
 *                                                                       *
 *  Made for Oasis OLC                                                   *
 *  Copyright 1996 Harvey Gilpin.                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"

#include "structs.h"
#include "comm.h"
#include "buffer.h"
#include "utils.h"
#include "db.h"
#include "spells.h"
#include "guild.h"
#include "olc.h"

/*-------------------------------------------------------------------*/
/* external variables */
extern struct guild_master_data *gm_index;	/*. guild.c      . */
extern int top_guild;		/*. guild.c      . */
extern struct char_data *mob_proto;	/*. db.c        . */
extern struct obj_data *obj_proto;	/*. db.c        . */
extern struct room_data *world;	/*. db.c        . */
extern struct zone_data *zone_table;	/*. db.c        . */
extern struct index_data *mob_index;	/*. db.c        . */
extern struct index_data *obj_index;	/*. db.c        . */
extern char *trade_letters[];	/*. shop.h      . */
extern struct spell_info_type *spells;
extern int spell_sort_info[MAX_SKILLS+1];
/*-------------------------------------------------------------------*/
/*. Handy  macros . */

#define G_NUM(i)	 ((i)->num)
#define G_SK_AND_SP(i,j) ((i)->skills_and_spells[j])
#define G_CHARGE(i)	 ((i)->charge)
#define G_NO_SKILL(i)	 ((i)->no_such_skill)
#define G_NO_GOLD(i)	 ((i)->not_enough_gold)
#define G_TRAINER(i)	 ((i)->gm)
#define G_WITH_WHO(i)	 ((i)->with_who)
#define G_OPEN(i)	 ((i)->open)
#define G_CLOSE(i)	 ((i)->close)
#define G_FUNC(i)	 ((i)->func)
#define G_TYPE(i)	 ((i)->type)
#define G_MINLEV(i)	 ((i)->min_lev)
#define G_MAXLEV(i)	 ((i)->max_lev)

/*-------------------------------------------------------------------*/
/*. Function prototypes . */

int real_gm(int vgm_num);
void gedit_setup_new(struct descriptor_data *d);
void gedit_setup_existing(struct descriptor_data *d, int rgm_num);
void gedit_parse(struct descriptor_data *d, char *arg);
void gedit_disp_menu(struct descriptor_data *d);
void gedit_no_train_menu(struct descriptor_data *d);
void gedit_save_internally(struct descriptor_data *d);
void gedit_save_to_disk(int zone_num);
void copy_gm(struct guild_master_data *tgm, struct guild_master_data *fgm);
void free_gm_strings(struct guild_master_data *pxguild);
void free_gm(struct guild_master_data *pxguild);
void gedit_modify_string(char **str, char *new);

/*. External . */
SPECIAL(guild);

/*-------------------------------------------------------------------*\
  utility functions 
\*-------------------------------------------------------------------*/

void gedit_setup_new(struct descriptor_data *d)
{
   int i;
   struct guild_master_data *pxguild;

  /*. Alloc some gm shaped space . */
   CREATE(pxguild, struct guild_master_data, 1);

  /*. Some default values . */
   G_TRAINER(pxguild) = -1;
   G_OPEN(pxguild) = 28;
   G_CLOSE(pxguild) = 28;
   G_CHARGE(pxguild) = 1.0;
   G_WITH_WHO(pxguild) = 0;
   G_FUNC(pxguild) = NULL;
   G_TYPE(pxguild) = 0;
   G_MINLEV(pxguild)=1;
   G_MAXLEV(pxguild)=LVL_IMMORT-1;
  /*. Some default strings . */
   G_NO_SKILL(pxguild) = str_dup("%s Sorry, but I don't know that one.");
   G_NO_GOLD(pxguild) = str_dup("%s Sorry, but I'm gonna need more gold first.");

  /* init the wasteful skills and spells table */

   for (i = 0; i < MAX_SKILLS + 2; i++)
      G_SK_AND_SP(pxguild, i) = 0;

   OLC_GUILD(d) = pxguild;
   gedit_disp_menu(d);
}

/*-------------------------------------------------------------------*/

void gedit_setup_existing(struct descriptor_data *d, int rgm_num)
{
  /*. Alloc some gm shaped space . */
   CREATE(OLC_GUILD(d), struct guild_master_data, 1);
   copy_gm(OLC_GUILD(d), gm_index + rgm_num);
   gedit_disp_menu(d);
}

/*-------------------------------------------------------------------*/

void copy_gm(struct guild_master_data *tgm, struct guild_master_data *fgm)
{
   int i;

  /*. Copy basic info over . */
   G_NUM(tgm) = G_NUM(fgm);
   G_CHARGE(tgm) = G_CHARGE(fgm);
   G_TRAINER(tgm) = G_TRAINER(fgm);
   G_WITH_WHO(tgm) = G_WITH_WHO(fgm);
   G_OPEN(tgm) = G_OPEN(fgm);
   G_CLOSE(tgm) = G_CLOSE(fgm);
   G_TYPE(tgm) = G_TYPE(fgm);
   G_MINLEV(tgm) = G_MINLEV(fgm);
   G_MAXLEV(tgm) = G_MAXLEV(fgm);

  /*. Copy the strings over . */
   free_gm_strings(tgm);
   G_NO_SKILL(tgm) = str_dup(G_NO_SKILL(fgm));
   G_NO_GOLD(tgm) = str_dup(G_NO_GOLD(fgm));

  /* copy the wasteful skills and spells table over */
   for (i = 0; i < MAX_SKILLS + 2; i++)
      G_SK_AND_SP(tgm, i) = G_SK_AND_SP(fgm, i);

}


/*-------------------------------------------------------------------*/
/*. Free all the character strings in a gm structure . */

void free_gm_strings(struct guild_master_data *pxguild)
{
   if (G_NO_SKILL(pxguild)) {
   free(G_NO_SKILL(pxguild));
   G_NO_SKILL(pxguild) = NULL;
   }
   if (G_NO_GOLD(pxguild)) {
   free(G_NO_GOLD(pxguild));
   G_NO_GOLD(pxguild) = NULL;
   }
}

/*-------------------------------------------------------------------*/
/*. Free up the whole guild structure and its contents . */

void free_gm(struct guild_master_data *pxguild)
{
   free_gm_strings(pxguild);
   free(pxguild);
}

/*-------------------------------------------------------------------*/

int real_gm(int vgm_num)
{
   int rgm_num;

   for (rgm_num = 0; rgm_num < top_guild; rgm_num++)
      if (GM_NUM(rgm_num) == vgm_num)
	 return rgm_num;

   return -1;
}

/*-------------------------------------------------------------------*/
/*. Generic string modifyer for guild master messages . */

void gedit_modify_string(char **str, char *new)
{
   char *pointer;
   char *buf=get_buffer(256);
  /*. Check the '%s' is present, if not, add it . */
   if (*new != '%')
      {
      strcpy(buf, "%s ");
      strcat(buf, new);
      pointer = buf;
      } 
   else
      pointer = new;

   if (*str)
      free(*str);
   *str = str_dup(pointer);
   release_buffer(buf);
}

/*-------------------------------------------------------------------*/

void gedit_save_internally(struct descriptor_data *d)
{
   int rgm, found = 0;
   struct guild_master_data *pxguild;
   struct guild_master_data *new_index;
   char *buf = get_buffer(256);
   rgm = real_gm(OLC_NUM(d));
   pxguild = OLC_GUILD(d);
   G_NUM(pxguild) = OLC_NUM(d);

   if (rgm > -1) 		/*. The GM already exists, just update it. */
      {
      copy_gm((gm_index + rgm), pxguild);
      }
   else 			/*. Doesn't exist - hafta insert it . */
      {
      CREATE(new_index, struct guild_master_data, top_guild + 1);
      for (rgm = 0; rgm < top_guild; rgm++) 
	 {
	 if (!found) 		/*. Is this the place ?. */
	    {
	    if (GM_NUM(rgm) > OLC_NUM(d)) 		/*. Yep, stick it in here . */
	       {
	       found = 1;
	       copy_gm(&(new_index[rgm]), pxguild);
	      /*. Move the entry that used to go here up a place . */
	       new_index[rgm + 1] = gm_index[rgm];
	       } 
	    else 		/*. This isn't the place, copy over info . */
	       {
	       new_index[rgm] = gm_index[rgm];
	       }
	    }
	 else 			/*. GM's already inserted, copy rest over . */
	    {
	    new_index[rgm + 1] = gm_index[rgm];
	    }
	 }
      if (!found)
	 copy_gm(&(new_index[rgm]), pxguild);

     /*. Switch index in . */
      free(gm_index);
      gm_index = new_index;
      top_guild++;
      }
   olc_add_to_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_GM);
   free(zone_table[OLC_ZNUM(d)].nameLastMod);
   sprintf(buf,"%s - guild",GET_NAME(d->character));
   zone_table[OLC_ZNUM(d)].nameLastMod = strdup(buf);
   release_buffer(buf);
   zone_table[OLC_ZNUM(d)].dateLastMod = time(0);
   olc_add_to_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_ZONE);

}


/*-------------------------------------------------------------------*/

void gedit_save_to_disk(int zone_num)
{
   int i, j, rgm, zone, top;
   FILE *gm_file;
   struct guild_master_data *pxguild;
   char *buf=get_buffer(256);
   char *buf1;

   zone = zone_table[zone_num].number;
   top = zone_table[zone_num].top;

   sprintf(buf, "%s/%i.new", GLD_PREFIX, zone);

   if (!(gm_file = fopen(buf, "w"))) 
      {
      mudlogf(BRF, LVL_IMMORT, TRUE, "SYSERR: OLC: Cannot open GM file!");
      release_buffer(buf);
      return;
      }
  
   buf1=get_buffer(MAX_STRING_LENGTH);
   fprintf(gm_file,"@Version: %d~\n",CUR_GUILD_VER);

  /*. Search database for gms in this zone . */
   for (i = zone * 100; i <= top; i++) 
      {
      rgm = real_gm(i);
      if (rgm != -1) 
	 {
	 fprintf(gm_file, "#%d~\n", i);
	 pxguild = gm_index + rgm;

	/* Write which skills and spells the gm knows */
	 for (j = 1; j < MAX_SPELLS ; j++)
	    if (G_SK_AND_SP(pxguild, j))
	       fprintf(gm_file, "%d\n", j);

	 fprintf(gm_file, "-1\n");


	/*. Save charge . */
	 fprintf(gm_file, "%4.1f\n", G_CHARGE(pxguild));

	/*. Save messages . */
	 fprintf(gm_file,
		 "%s~\n%s~\n",
		/*. Added some small'n'silly defaults as sanity checks . */
		 (G_NO_SKILL(pxguild) ? G_NO_SKILL(pxguild) : "%s ERROR"),
		 (G_NO_GOLD(pxguild) ? G_NO_GOLD(pxguild) : "%s ERROR")
	    );
	 
	/* Write what the GM teaches */
	 fprintf(gm_file, "1 %d %d\n",
		 MIN(G_MINLEV(pxguild),G_MAXLEV(pxguild)),
		 MAX(G_MINLEV(pxguild),G_MAXLEV(pxguild)));
	 
	 
	/*. Save the rest . */
	 fprintf(gm_file, "%ld\n%d\n%d\n%d\n",
		 mob_index[G_TRAINER(pxguild)].vnum,
		 G_WITH_WHO(pxguild),
		 G_OPEN(pxguild),
		 G_CLOSE(pxguild));
	 }
      }
   fprintf(gm_file, "$~\n");
   fclose(gm_file);
   sprintf(buf1, "%s/%ld.gld", GLD_PREFIX, zone_table[zone_num].number);
  /*
   * We're fubar'd if we crash between the two lines below.
   */
   remove(buf1);
   if(rename(buf, buf1)!=0)
      perror("rename error");
  
  
   olc_remove_from_save_list(zone, OLC_SAVE_GM);
   release_buffer(buf1);
   release_buffer(buf);
}


/**************************************************************************
  Menu functions 
 **************************************************************************/

/*-------------------------------------------------------------------*/

void gedit_select_skills_menu(struct descriptor_data *d)
{
   int i, j = 0;
   int sortpos;

   get_char_cols(d->character);
   send_to_char(d->character,"[H[J");

   send_to_char(d->character, "Skills known:\r\n");

   for (sortpos = 1; sortpos <= MAX_SPELLS; sortpos++) 
      {
      i=spell_sort_info[sortpos];
      if(spells[i].is_spell==IS_SKILL)
	 {
	 send_to_char(d->character, "[%s%-3.3s%s] %3d %-13.13s  ",
		 cyn, (G_SK_AND_SP(OLC_GUILD(d), i)==1)?"YES":"   ", nrm,
		 i, spells[i].spell_name);
	 if (!(++j % 3))
	    send_to_char(d->character, "\r\n");
	 }
      }
   send_to_char(d->character,"\r\nEnter skill num:  ");
}

/*-------------------------------------------------------------------*/

void gedit_select_spells_menu(struct descriptor_data *d)
{
   int i, j = 0;
   int sortpos;

   get_char_cols(d->character);
   send_to_char(d->character, "[H[J");
   send_to_char(d->character, "Spells known:\r\n");

   for (sortpos = 1; sortpos <= MAX_SPELLS; sortpos++) 
      {
      i=spell_sort_info[sortpos];
      if(spells[i].is_spell==IS_SPELL)
	 {
	 send_to_char(d->character, "[%s%-3.3s%s] %3d %-13.13s  ", 
		 cyn, (G_SK_AND_SP(OLC_GUILD(d), i)==1)?"YES":"   ", nrm,
		 i, spells[i].spell_name);
	 if (!(++j % 3))
	    send_to_char(d->character, "\r\n");
	 }
      }
   send_to_char(d->character, "\r\nEnter spell num:  ");
}

/*-------------------------------------------------------------------*/

void gedit_no_train_menu(struct descriptor_data *d)
{
   int i, count = 0;
   char *buf1=get_buffer(MAX_STRING_LENGTH);
   
   get_char_cols(d->character);
   send_to_char(d->character, "[H[J");
   for (i = 0; i < NUM_TRADERS; i++) 
      {
      send_to_char(d->character, "%s%2d%s) %-20.20s   ",
	      grn, i + 1, nrm, trade_letters[i]
	 );
      if (!(++count % 2))
	 send_to_char(d->character, "\r\n");
      }
   sprintbit(G_WITH_WHO(OLC_GUILD(d)), trade_letters, buf1);
   send_to_char(d->character, "\r\nCurrently won't train: %s%s%s\r\n"
		"Enter choice : ",
		cyn, buf1, nrm
      );
   OLC_MODE(d) = GEDIT_NO_TRAIN;
   release_buffer(buf1);
}

/*-------------------------------------------------------------------*/
/*. Display main menu . */

void gedit_disp_menu(struct descriptor_data *d)
{
   struct guild_master_data *pxguild;
   char *buf1=get_buffer(MAX_STRING_LENGTH);
   
   pxguild = OLC_GUILD(d);
   get_char_cols(d->character);
   
   sprintbit(G_WITH_WHO(pxguild), trade_letters, buf1);
   
   send_to_char(d->character, "[H[J"
	   "-- Guild Number: [%s%d%s]\r\n"
	   "%s0%s) Guild Master	: [%s%ld%s] %s%s\r\n"
	   "%s1%s) Doesn't know skill:\r\n %s%s\r\n"
	   "%s2%s) Player no gold:\r\n %s%s\r\n"
	   "%s3%s) Open:  [%s%d%s]	%s4%s) Close:  [%s%d%s]	%s5%s) Charge:  [%s%3.1f]\r\n"
	   "%s6%s) Don't Train: %s%s\r\n"
	   "%s7%s) Spells Menu\r\n"
	   "%s8%s) Skills Menu\r\n"
	   "%s9%s) Min Level  : %s%d%s %sA%s) Max Level : %s%d\r\n"
	   "%sQ%s) Quit\r\n"
	   "Enter Choice : ",
	     
	   cyn, OLC_NUM(d), nrm,
	   grn, nrm, cyn,
	   (G_TRAINER(pxguild) == -1) ?
	   -1 : mob_index[G_TRAINER(pxguild)].vnum, nrm,
	   yel, (G_TRAINER(pxguild) == -1) ?
	   "none" : mob_proto[G_TRAINER(pxguild)].player.short_descr,
	   grn, nrm, yel, G_NO_SKILL(pxguild),
	   grn, nrm, yel, G_NO_GOLD(pxguild),
	   grn, nrm, cyn, G_OPEN(pxguild), nrm,
	   grn, nrm, cyn, G_CLOSE(pxguild), nrm,
	   grn, nrm, cyn, G_CHARGE(pxguild),
	   grn, nrm, cyn, buf1,
	   grn, nrm, grn, nrm,
	   grn, nrm, cyn, G_MINLEV(pxguild), nrm,
	   grn, nrm, cyn, G_MAXLEV(pxguild),
	   grn, nrm);
   
   OLC_MODE(d) = GEDIT_MAIN_MENU;
   release_buffer(buf1);
}

/**************************************************************************
  The GARGANTUAN event handler
 **************************************************************************/

void gedit_parse(struct descriptor_data *d, char *arg)
{
   int i;

   if (OLC_MODE(d) > GEDIT_NUMERICAL_RESPONSE) 
      {
      if (!isdigit((int)arg[0]) && ((*arg == '-') && (!isdigit((int)arg[1])))) 
	 {
	 send_to_char(d->character, "Field must be numerical, try again : ");
	 return;
	 }
      }
   switch (OLC_MODE(d)) {
/*-------------------------------------------------------------------*/
    case GEDIT_CONFIRM_SAVESTRING:
       switch (*arg) 
	  {
	   case 'y':
	   case 'Y':
	      send_to_char(d->character, "Saving GM to memory.\r\n");
	      gedit_save_internally(d);
	      mudlogf(CMP, LVL_IMMORT, TRUE,
		     "OLC: %s edits GM %d", GET_NAME(d->character),
		      OLC_NUM(d));
	      cleanup_olc(d, CLEANUP_STRUCTS);
	      return;
	   case 'n':
	   case 'N':
	      cleanup_olc(d, CLEANUP_ALL);
	      return;
	   default:
	      send_to_char(d->character, "Invalid choice!\r\n");
	      send_to_char(d->character, "Do you wish to save the GM? : ");
	      return;
	  }
       break;

/*-------------------------------------------------------------------*/
    case GEDIT_MAIN_MENU:
       i = 0;
       switch (*arg) 
	  {
	   case 'q':
	   case 'Q':
	      if (OLC_VAL(d)) 		/*. Anything been changed? . */
		 {
		 send_to_char(d->character, "Do you wish to save the changes to the GM? (y/n) : ");
		 OLC_MODE(d) = GEDIT_CONFIRM_SAVESTRING;
		 }
	      else
		 cleanup_olc(d, CLEANUP_ALL);
	      return;
	   case '0':
	      OLC_MODE(d) = GEDIT_TRAINER;
	      send_to_char(d->character, "Enter virtual number of guild master : ");
	      return;
	   case '1':
	      OLC_MODE(d) = GEDIT_NO_SKILL;
	      i--;
	      break;
	   case '2':
	      OLC_MODE(d) = GEDIT_NO_CASH;
	      i--;
	      break;
	   case '3':
	      OLC_MODE(d) = GEDIT_OPEN;
	      i++;
	      break;
	   case '4':
	      OLC_MODE(d) = GEDIT_CLOSE;
	      i++;
	      break;
	   case '5':
	      OLC_MODE(d) = GEDIT_CHARGE;
	      i++;
	      break;
	   case '6':
	      OLC_MODE(d) = GEDIT_NO_TRAIN;
	      gedit_no_train_menu(d);
	      return;
	   case '7':
	      OLC_MODE(d) = GEDIT_SELECT_SPELLS;
	      gedit_select_spells_menu(d);
	      return;
	   case '8':
	      OLC_MODE(d) = GEDIT_SELECT_SKILLS;
	      gedit_select_skills_menu(d);
	      return;
	   case '9':
	      OLC_MODE(d) = GEDIT_MINLEV;
	      i=1;
	      break;
	   case 'A':
	   case 'a':
	      OLC_MODE(d) = GEDIT_MAXLEV;
	      i=1;
	      break;
	   case 't':
	      G_TYPE(OLC_GUILD(d)) = !(G_TYPE(OLC_GUILD(d)));
	      gedit_disp_menu(d);
	      return;
	   default:
	      gedit_disp_menu(d);
	      return;
	  }

       if (i == 1) 
	  {
	  send_to_char(d->character, "\r\nEnter new value : ");
	  return;
	  }
       if (i == -1) 
	  {
	  send_to_char(d->character, "\r\nEnter new text :\r\n| ");
	  return;
	  }
       break;
/*-------------------------------------------------------------------*/
      /*. String edits . */
    case GEDIT_NO_SKILL:
       gedit_modify_string(&G_NO_SKILL(OLC_GUILD(d)), arg);
       break;
    case GEDIT_NO_CASH:
       gedit_modify_string(&G_NO_GOLD(OLC_GUILD(d)), arg);
       break;

/*-------------------------------------------------------------------*/
      /*. Numerical responses . */

    case GEDIT_TRAINER:
       i = atoi(arg);
       if (i != -1) 
	  {
	  i = real_mobile(i);
	  if (i < 0) 
	     {
	     send_to_char(d->character, "That mobile does not exist, try again : ");
	     return;
	     }
	  }
       G_TRAINER(OLC_GUILD(d)) = i;
       if (i == -1)
	  break;
      /*. Fiddle with special procs . */
       G_FUNC(OLC_GUILD(d)) = mob_index[i].func;
       mob_index[i].func = guild;
       break;
    case GEDIT_OPEN:
       G_OPEN(OLC_GUILD(d)) = MAX(0, MIN(28, atoi(arg)));
       break;
    case GEDIT_CLOSE:
       G_CLOSE(OLC_GUILD(d)) = MAX(0, MIN(28, atoi(arg)));
       break;
    case GEDIT_MINLEV:
       G_MINLEV(OLC_GUILD(d)) = MAX(0, MIN(LVL_IMPL, atoi(arg)));
       break;
    case GEDIT_MAXLEV:
       G_MAXLEV(OLC_GUILD(d)) = MAX(0, MIN(LVL_IMPL, atoi(arg)));
       break;
    case GEDIT_CHARGE:
       sscanf(arg, "%f", &G_CHARGE(OLC_GUILD(d)));
       break;
    case GEDIT_NO_TRAIN:
       i = MAX(0, MIN(NUM_TRADERS, atoi(arg)));
       if (i > 0) 		/*. Toggle bit . */
	  {
	  i = 1 << (i - 1);
	  if (IS_SET(G_WITH_WHO(OLC_GUILD(d)), i))
	     REMOVE_BIT(G_WITH_WHO(OLC_GUILD(d)), i);
	  else
	     SET_BIT(G_WITH_WHO(OLC_GUILD(d)), i);
	  gedit_no_train_menu(d);
	  return;
	  }
       break;

    case GEDIT_SELECT_SPELLS:
       i = atoi(arg);
       if (i == 0)
	  break;
       i = MAX(1, MIN(i, MAX_SPELLS-1));
       if(spells[i].is_spell==IS_SPELL)
	  G_SK_AND_SP(OLC_GUILD(d), i) = !G_SK_AND_SP(OLC_GUILD(d), i);
       gedit_select_spells_menu(d);
       return;

    case GEDIT_SELECT_SKILLS:
       i = atoi(arg);
       if (i == 0)
	  break;
       i = MAX(1, MIN(i, MAX_SPELLS-1));
       if(spells[i].is_spell==IS_SKILL)
	  G_SK_AND_SP(OLC_GUILD(d), i) = !G_SK_AND_SP(OLC_GUILD(d), i);
       gedit_select_skills_menu(d);
       return;

/*-------------------------------------------------------------------*/
    default:
      /*. We should never get here . */
       cleanup_olc(d, CLEANUP_ALL);
       mudlogf(BRF, LVL_IMMORT, TRUE,
	      "SYSERR: OLC: gedit_parse(): Reached default case!");
       break;
   }
/*-------------------------------------------------------------------*/
/*. END OF CASE 
  If we get here, we have probably changed something, and now want to
  return to main menu.  Use OLC_VAL as a 'has changed' flag . */

   OLC_VAL(d) = 1;
   gedit_disp_menu(d);
}

