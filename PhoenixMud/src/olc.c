/***************************************************************************
 *  OasisOLC - olc.c                                                       *
 *                                                                         *
 *  Copyright 1996 Harvey Gilpin.                                          *
 ***************************************************************************/
 
#define _OASIS_OLC_ 
 
#include "../localHeader/conf.h" 
#include "../localHeader/sysdep.h" 
#include "structs.h" 
#include "interpreter.h" 
#include "comm.h" 
#include "buffer.h"
#include "utils.h" 
#include "db.h" 
#include "olc.h" 
#include "screen.h" 
#include "dg_olc.h"
 
/*. External data structures .*/ 
extern struct obj_data *obj_proto; 
extern struct char_data *mob_proto; 
extern struct room_data *world; 
extern struct zone_data *zone_table; 
extern struct descriptor_data *descriptor_list; 
extern int top_of_zone_table; 
extern int port; 
/*
 * External functions 
 */ 
int zedit_setup(struct descriptor_data *d, int vroom_num); 
int zedit_save_to_disk(int zone); 
int zedit_new_zone(struct char_data *ch, int new_zone); 
int medit_setup_new(struct descriptor_data *d); 
int medit_setup_existing(struct descriptor_data *d, int rmob_num); 
int medit_save_to_disk(int zone); 
int redit_setup_new(struct descriptor_data *d); 
int redit_setup_existing(struct descriptor_data *d, int rroom_num); 
int redit_save_to_disk(int zone); 
int oedit_setup_new(struct descriptor_data *d); 
int oedit_setup_existing(struct descriptor_data *d, int robj_num); 
int oedit_save_to_disk(int zone); 
int sedit_setup_new(struct descriptor_data *d); 
int sedit_setup_existing(struct descriptor_data *d, int robj_num); 
int sedit_save_to_disk(int zone); 
int real_shop(int vnum); 
int free_shop(struct shop_data *shop); 
int free_room(struct room_data *room); 
void medit_free_mobile(struct char_data *mob); 
void hedit_save_to_disk(void);
void free_help(struct help_index_element *help);
int find_help_rnum(char *keyword);
void hedit_setup_new(struct descriptor_data *d, char *new_key);
void hedit_setup_existing(struct descriptor_data *d, int rnum);
void gedit_save_to_disk(int zone);
void gedit_setup_existing(struct descriptor_data *d, int rgm_num);
void gedit_setup_new(struct descriptor_data *d);
void free_gm(struct guild_master_data *guild);
int  real_gm(int vnum);
void trigedit_setup_new(struct descriptor_data *d);
void trigedit_setup_existing(struct descriptor_data *d, int rtrg_num);
int  real_trigger(int vnum);
void strip_string(char *thingy);

 
/*
 * Internal function prototypes 
 */ 
int real_zone(int vnumber); 
void olc_saveinfo(struct char_data *ch); 
 
/*
 * Global string constants.
 */
const char *save_info_msg[9] = {"Rooms", "Objects", "Zone info",
				"Mobiles", "Shops","Guilds",
				"Help","Assemblies", "Triggers"}; 

/*
 * Internal data 
 */ 
struct olc_scmd_data 
{ 
   char *text; 
   int con_type; 
}
; 
 
struct olc_scmd_data olc_scmd_info[10] = 
{ 
   {"room",    CON_REDIT } , 
   {"object",  CON_OEDIT } , 
   {"room",    CON_ZEDIT } , 
   {"mobile",  CON_MEDIT } , 
   {"shop",    CON_SEDIT } , 
   {"gm",      CON_GEDIT } ,
   {"path",    CON_PEDIT } ,
   {"help",    CON_HEDIT } ,
   {"trigger", CON_TRIGEDIT },
   {"assembly",CON_ASSEDIT }
} ; 
 
/*------------------------------------------------------------*\

 Eported ACMD do_olc function 
 
 This function is the OLC interface.  It deals with all the  
 generic OLC stuff, then passes control to the sub-olc sections. 
\*------------------------------------------------------------*/ 
 
ACMD(do_olc) 
{ 
   long vnumber = -1, save = 0, real_num; 
   struct descriptor_data *d; 
   char *buf1,*buf2;
   zone_rnum rznum;
   struct olc_save_info *entry, *next_entry;
   const char *type = NULL;

  /*
   * No screwing around 
   */ 
   if (IS_NPC(ch)) 
      return; 
 
   if((port==4000)&&(GET_LEVEL(ch)<LVL_SERP))
      {
      send_to_char(ch,"No editing on the player port!!!!\r\n");
      return;
      }
   if (subcmd == SCMD_OLC_SAVEINFO) 
      { 
      olc_saveinfo(ch); 
      return; 
      } 
 
  /*. Parse any arguments .*/ 
   buf1=get_buffer(MAX_INPUT_LENGTH);
   buf2=get_buffer(MAX_INPUT_LENGTH);
   skip_spaces(&argument);
   two_arguments(argument, buf1, buf2); 
   if ((!*buf1) ||(*buf1=='.'))
      { 
     /* No argument given .*/ 
      switch(subcmd) 
	 { 
	  case SCMD_OLC_ZEDIT: 
	  case SCMD_OLC_REDIT: 
	     vnumber = GET_ROOM_VNUM(IN_ROOM(ch)); 
	     break; 
	  case SCMD_OLC_TRIGEDIT:
	  case SCMD_OLC_OEDIT: 
	  case SCMD_OLC_MEDIT: 
	  case SCMD_OLC_SEDIT: 
	  case SCMD_OLC_GEDIT: 
	  case SCMD_OLC_PEDIT: 
	     send_to_char(ch, "Specify a %s VNUM to edit.\r\n", olc_scmd_info[subcmd].text); 
	     release_buffer(buf2);
	     release_buffer(buf1);
	     return; 
	  case SCMD_OLC_HEDIT:
	     send_to_char(ch, "Specify a %s entry to edit.\r\n", 
		     olc_scmd_info[subcmd].text);
	     release_buffer(buf2);
	     release_buffer(buf1);
	     return;

	 } 
      } 
   else if (!isdigit((int)*buf1)) 
      { 
      if (strncmp("save", buf1, 4) == 0) 
	 { 
	 if (subcmd == SCMD_OLC_HEDIT) 
	    {
	    save = 1;
	    vnumber = 0;
	    }
	 else if (!*buf2) 
	    { 
	    send_to_char(ch, "Save which zone?\r\n"); 
	    release_buffer(buf2);
	    release_buffer(buf1);
	    return; 
	    } 
	 else if(!strcmp("all",buf2))
	    {
            mudlogf(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE,
                    "OLC: %s saves info for all zones.", GET_NAME(ch));
	    for (entry = olc_save_list; entry; entry = next_entry) 
	       {
	       next_entry = entry->next;
	       if(entry->type == OLC_SAVE_HELP)
		  {
		  send_to_char(ch, "Saving all help entries.\r\n");
		  mudlogf(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE, 
			  "OLC: %s saves help entries.", GET_NAME(ch));
		  hedit_save_to_disk();
		  }
	       else if ((entry->type < 0) || (entry->type > 5))
		  {
		  log("OLC: Illegal save type %d!", entry->type);
		  olc_remove_from_save_list(entry->zone, entry->type);
		  }
	       else if ((rznum=real_zone(entry->zone*100))==-1)        
		  {
		  log("OLC: Illegal save zone %d!", entry->zone);
		  olc_remove_from_save_list(entry->zone, entry->type);
		  }
	       else if (rznum < 0 || rznum > top_of_zone_table) 
		  {
		  log("OLC: Invalid real zone number %ld!", rznum);
		  olc_remove_from_save_list(entry->zone, entry->type);
		  }
	       else 
		  {
		  log("OLC: All saving %s for zone %ld.",
		      save_info_msg[(int)entry->type], zone_table[rznum].number);
		  switch (entry->type)
		     {
		      case OLC_SAVE_ROOM: 
			 redit_save_to_disk(rznum); 
			 break;
		      case OLC_SAVE_OBJ:
			 oedit_save_to_disk(rznum); 
			 break;
		      case OLC_SAVE_MOB:
			 medit_save_to_disk(rznum); 
			 break;
		      case OLC_SAVE_ZONE:
			 zedit_save_to_disk(rznum);
			 break;
		      case OLC_SAVE_SHOP: 
			 sedit_save_to_disk(rznum);
			 break;
		      case OLC_SAVE_GM: 
			 gedit_save_to_disk(rznum);
			 break;
                      case OLC_SAVE_HELP:
                         hedit_save_to_disk();
                         break;
		      default:
			 log("OLC: Unexpected olc_save_list->type: %d",
			     entry->type); 
			 break;
		     }
		  }
	       }
	    release_buffer(buf2);
	    release_buffer(buf1);
	    send_to_char(ch, "All Set!\r\n");
	    return; 

	    }
	 else  
	    { 
	    save = 1; 
	    vnumber = atoi(buf2) * 100; 
	    } 
	 } 
      else if (subcmd == SCMD_OLC_HEDIT)
	 vnumber = 0;
      else if (subcmd == SCMD_OLC_ZEDIT && GET_LEVEL(ch) >= LVL_SIMP) 
	 { 
	 if ((strncmp("new", buf1, 3) == 0) && *buf2) 
	    zedit_new_zone(ch, atoi(buf2)); 
	 else 
	    send_to_char(ch,"Specify a new zone number.\r\n"); 
	 release_buffer(buf2);
	 release_buffer(buf1);
	 return; 
	 } 
      else 
	 { 
	 send_to_char(ch,"Yikes!  Stop that, someone will get hurt!\r\n"); 
	 release_buffer(buf2);
	 release_buffer(buf1);
	 return; 
	 } 
      } 
 
  /*. If a numeric argument was given, get it .*/ 
   if (vnumber == -1) 
      vnumber = atoi(buf1); 
 
  /*. Check whatever it is isn't already being edited .*/ 
   for (d = descriptor_list; d; d = d->next) 
      if (STATE(d) == olc_scmd_info[subcmd].con_type) 
	 if (d->olc && OLC_NUM(d) == vnumber) 
	    { 
	    if (subcmd == SCMD_OLC_HEDIT)
	       send_to_char(ch,"Help files are already being editted "
			    "by %s.\r\n", GET_NAME(d->character));
	    else
	       send_to_char(ch, "That %s is currently being edited by %s.\r\n",
		       olc_scmd_info[subcmd].text, GET_NAME(d->character)); 
	    release_buffer(buf2);
	    release_buffer(buf1);
	    return; 
	    } 
 
   d = ch->desc;  
 
  /*. Give descriptor an OLC struct .*/ 
   CREATE(d->olc, struct olc_data, 1); 
 
  /*. Find the zone (or help rnum).*/ 
   if (subcmd == SCMD_OLC_HEDIT && !save)
      {
      OLC_ZNUM(d) = find_help_rnum(argument);
      }
   else   if ((OLC_ZNUM(d)=real_zone(vnumber)) == -1)
      { 
      send_to_char(ch,"Sorry, there is no zone for that number!\r\n");
      free(d->olc); 
      release_buffer(buf2);
      release_buffer(buf1);
      return; 
      } 
 
   if(subcmd == SCMD_OLC_HEDIT)
      {
      if ((GET_LEVEL(ch) < LVL_SIMP) && !PRF2_FLAGGED(ch,PRF2_HEDIT))
	 {
	 send_to_char(ch,"You do not have permssion to edit help "
		      "entries.\r\n");
	 free(d->olc);
	 return;
	 }
      } 
  /*. Everyone but IMPLs can only edit zones they have been assigned .*/ 
   else 
      {
      if ((GET_LEVEL(ch) < LVL_SIMP) &&  
	  ((zone_table[OLC_ZNUM(d)].number != GET_OLC_ZONE(ch, 0) &&
	    zone_table[OLC_ZNUM(d)].number != GET_OLC_ZONE(ch, 1) && 
	    zone_table[OLC_ZNUM(d)].number != GET_OLC_ZONE(ch, 2) && 
	    zone_table[OLC_ZNUM(d)].number != GET_OLC_ZONE(ch, 3) && 
	    zone_table[OLC_ZNUM(d)].number != GET_OLC_ZONE(ch, 4))||
	   (zone_table[OLC_ZNUM(d)].number == 0)))
	 { 
	 send_to_char(ch,"You do not have permission to edit this zone.\r\n");
	 free(d->olc); 
	 release_buffer(buf2);
	 release_buffer(buf1);
	 return; 
	 } 
      if((GET_LEVEL(ch)<LVL_ADMIN)&&(zone_table[OLC_ZNUM(d)].status>=3))
	 {
	 send_to_char(ch,"You do not have permission to edit a zone that is marked active or finished.  Please talk to an ADMIN+ to get a zone status change.\r\n");
	 free(d->olc); 
	 release_buffer(buf2);
	 release_buffer(buf1);
	 return; 
	 } 
      }

   if (save) 
      {
      
      switch (subcmd) 
	 {
	  case SCMD_OLC_REDIT:
	     type = "room";
	     break;
	  case SCMD_OLC_ZEDIT:
	     type = "zone"; 
	     break;
	  case SCMD_OLC_SEDIT: 
	     type = "shop";
	     break;
	  case SCMD_OLC_MEDIT:
	     type = "mobile"; 
	     break;
	  case SCMD_OLC_OEDIT:
	     type = "object";
	     break;
	  case SCMD_OLC_GEDIT:
	     type = "guild";
	     break;
	  case SCMD_OLC_HEDIT:
	     type = "help";
	     break;
	  case SCMD_OLC_PEDIT:
	     type = "path";
	     break;
	  case SCMD_OLC_TRIGEDIT:
	     type = "trigger";
	     break;
	 }
      if (!type) 
	 {
	 send_to_char(ch,"Oops, I forgot what you wanted to save.\r\n");
	 release_buffer(buf2);
	 release_buffer(buf1);
	 return;
	 }

     if (subcmd == SCMD_OLC_HEDIT) 
	{
	send_to_char(ch,"Saving all help entries.\r\n");
	mudlogf(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE, 
		"OLC: %s saves help entries.", GET_NAME(ch));
	} 
     else 
	{
	send_to_char(ch, "Saving all %ss in zone %ld.\r\n",
		type, zone_table[OLC_ZNUM(d)].number);
	mudlogf(CMP, MAX(LVL_BUILDER, GET_INVIS_LEV(ch)), TRUE,
		"OLC: %s saves %s info for zone %ld.",GET_NAME(ch),type,
		zone_table[OLC_ZNUM(d)].number);
	}

      switch (subcmd) 
	 {
	  case SCMD_OLC_REDIT:
	     redit_save_to_disk(OLC_ZNUM(d));
	     break;
	  case SCMD_OLC_ZEDIT:
	     zedit_save_to_disk(OLC_ZNUM(d));
	     break;
	  case SCMD_OLC_OEDIT:
	     oedit_save_to_disk(OLC_ZNUM(d));
	     break;
	  case SCMD_OLC_MEDIT:
	     medit_save_to_disk(OLC_ZNUM(d));
	     break;
	  case SCMD_OLC_SEDIT:
	     sedit_save_to_disk(OLC_ZNUM(d));
	     break;
	  case SCMD_OLC_GEDIT:
	     gedit_save_to_disk(OLC_ZNUM(d));
	     break;
	  case SCMD_OLC_HEDIT:
	     hedit_save_to_disk();
	     break;
	  case SCMD_OLC_PEDIT:
/*
 *	     pedit_save_to_disk(OLC_ZNUM(d));
 */
	     break;
	 }
      free(d->olc); 
      release_buffer(buf2);
      release_buffer(buf1);
      return; 
      } 
  
   OLC_NUM(d) = vnumber; 
 
  /*. Steal players descriptor start up subcommands .*/ 
   switch(subcmd) 
      { 
       case SCMD_OLC_REDIT: 
	  real_num = real_room(vnumber); 
	  if (real_num >= 0) 
	     redit_setup_existing(d, real_num); 
	  else 
	     redit_setup_new(d); 
	  STATE(d) = CON_REDIT; 
	  type="room";
	  break; 
       case SCMD_OLC_ZEDIT: 
	  real_num = real_room(vnumber); 
	  if (real_num < 0) 
	     {  
	     send_to_char(ch,"That room does not exist.\r\n");
	     free(d->olc); 
	     release_buffer(buf2);
	     release_buffer(buf1);
	     return; 
	     } 
	  zedit_setup(d, real_num); 
	  STATE(d) = CON_ZEDIT; 
	  type="zone";
	  break; 
       case SCMD_OLC_MEDIT: 
	  real_num = real_mobile(vnumber); 
	  if (real_num < 0) 
	     medit_setup_new(d); 
	  else 
	     medit_setup_existing(d, real_num); 
	  STATE(d) = CON_MEDIT; 
	  type="mobile";
	  break; 
       case SCMD_OLC_OEDIT: 
	  real_num = real_object(vnumber); 
	  if (real_num >= 0) 
	     oedit_setup_existing(d, real_num); 
	  else 
	     oedit_setup_new(d); 
	  STATE(d) = CON_OEDIT; 
	  type="object";
	  break; 
       case SCMD_OLC_SEDIT: 
	  real_num = real_shop(vnumber); 
	  if (real_num >= 0) 
	     sedit_setup_existing(d, real_num); 
	  else 
	     sedit_setup_new(d); 
	  STATE(d) = CON_SEDIT; 
	  type="shop";	  
	  break; 
       case SCMD_OLC_GEDIT:
	  real_num = real_gm(vnumber);
	  if (real_num >= 0)
	     gedit_setup_existing(d, real_num);
	  else
	     gedit_setup_new(d);
	  STATE(d) = CON_GEDIT;
	  type="guild";
	  break;
       case SCMD_OLC_HEDIT:
	  if (OLC_ZNUM(d) < 0)
	     hedit_setup_new(d, buf1);
	  else
	     hedit_setup_existing(d, OLC_ZNUM(d));
	  STATE(d) = CON_HEDIT;
          type="hedit";
	  break;
        case SCMD_OLC_PEDIT:
	 /*
	  real_num = real_path(vnumber);
	  if (real_num >= 0)
	     pedit_setup_existing(d, real_num);
	  else
	     pedit_setup_new(d);
	  STATE(d) = CON_PEDIT;
	  type="path";
	  */
	  break;
       case SCMD_OLC_TRIGEDIT:
	  if ((real_num = real_trigger(vnumber)) >= 0)
	     trigedit_setup_existing(d, real_num);
	  else
	     trigedit_setup_new(d);
	  STATE(d) = CON_TRIGEDIT;
	  type="trigger";
	  break;
      } 
   mudlogf(BRF, MAX(LVL_IMMORT, GET_INVIS_LEV(ch)), FALSE,
	   "OLC: %s starts using OLC, %s %ld", GET_NAME(ch),type,vnumber);
   SET_BIT(PLR_FLAGS (ch), PLR_WRITING); 
   release_buffer(buf2);
   release_buffer(buf1);
} 
/*------------------------------------------------------------*\

 Internal utlities  
\*------------------------------------------------------------*/ 
 
void olc_saveinfo(struct char_data *ch) 
{ 
   struct olc_save_info *entry; 
 
   if (olc_save_list) 
      send_to_char(ch,"The following OLC components need saving:-\r\n"); 
   else 
      send_to_char(ch,"The database is up to date.\r\n"); 
 
   for (entry = olc_save_list; entry; entry = entry->next) 
      { 
      if ((int)entry->type == OLC_SAVE_HELP)
	 send_to_char(ch, " - Help Entries.\r\n");
      else
	 send_to_char(ch, " - %s for zone %d.\r\n",  
		 save_info_msg[(int)entry->type], 
		 entry->zone  
	    ); 
      } 
} 
 
int real_zone(int vnumber) 
{ 
   int counter; 
   for (counter = 0; counter <= top_of_zone_table; counter++) 
      if ((vnumber >= (zone_table[counter].number * 100)) && 
	  (vnumber <= (zone_table[counter].top))) 
	 return counter; 
 
   return -1; 
} 
 
/*------------------------------------------------------------*\

 Exported utlities  
\*------------------------------------------------------------*/ 
 
/*. Add an entry to the 'to be saved' list .*/ 
 
void olc_add_to_save_list(int zone, byte type) 
{ 
   struct olc_save_info *new; 
 
  /*. Return if it's already in the list .*/ 
   for(new = olc_save_list; new; new = new->next) 
      if ((new->zone == zone) && (new->type == type)) 
	 return; 
 
   CREATE(new, struct olc_save_info, 1); 
   new->zone = zone; 
   new->type = type; 
   new->next = olc_save_list; 
   olc_save_list = new; 
} 
 
/*. Remove an entry from the 'to be saved' list .*/ 
 
void olc_remove_from_save_list(int zone, byte type) 
{ 
   struct olc_save_info **entry; 
   struct olc_save_info *temp; 
 
   for(entry = &olc_save_list; *entry; entry = &(*entry)->next) 
      if (((*entry)->zone == zone) && ((*entry)->type == type)) 
	 { 
	 temp = *entry; 
	 *entry = temp->next; 
	 free(temp); 
	 return; 
	 } 
   log("OLC: Hmmm zone:%d type: %s wasn't in the list, odd..",zone,save_info_msg[(int)type]);
} 
 
/*. Set the colour string pointers for that which this char will 
    see at color level NRM.  Changing the entries here will change  
    the colour scheme throught the OLC.*/ 
 
void get_char_cols(struct char_data *ch) 
{ 
   nrm = CCNRM(ch, C_NRM); 
   grn = CCGRN(ch, C_NRM); 
   cyn = CCCYN(ch, C_NRM); 
   yel = CCYEL(ch, C_NRM); 
} 
 
 
/*. This procdure frees up the strings and/or the structures 
    attatched to a descriptor, sets all flags back to how they 
    should be .*/ 
 
void cleanup_olc(struct descriptor_data *d, byte cleanup_type) 
{  
   if (d->olc) 
      { 
     /*. Check for room .*/ 
      if(OLC_ROOM(d)) 
	 { 
	/*. free_room performs no sanity checks, must be carefull here .*/ 
	 switch(cleanup_type) 
	    { 
	     case CLEANUP_ALL: 
		free_room(OLC_ROOM(d)); 
		break; 
	     case CLEANUP_STRUCTS: 
		free(OLC_ROOM(d)); 
		break; 
	     case CLEANUP_NONE:
		break;
	     default: 
	       /*. Caller has screwed up .*/ 
		break; 
	    } 
	 } 
   
     /*. Check for object .*/ 
      if(OLC_OBJ(d)) 
	 { 
	/*. free_obj checks strings arn't part of proto .*/ 
	 free_obj(OLC_OBJ(d)); 
	 } 
 
     /*. Check for mob .*/ 
      if(OLC_MOB(d)) 
	 { 
	/*
	 * medit_free_mobile() makes sure strings are not in the prototype.
	 */
	 medit_free_mobile(OLC_MOB(d)); 
	 } 
   
     /*. Check for zone .*/ 
      if(OLC_ZONE(d)) 
	 { 
	/*. cleanup_type is irrelivent here, free everything .*/ 
	 free(OLC_ZONE(d)->name); 
	 free(OLC_ZONE(d)->cmd); 
	 free(OLC_ZONE(d)); 
	 } 
 
     /*. Check for shop .*/ 
      if(OLC_SHOP(d)) 
	 { 
	/*. free_shop performs no sanity checks, must be carefull here .*/ 
	 switch(cleanup_type) 
	    { 
	     case CLEANUP_ALL: 
		free_shop(OLC_SHOP(d)); 
		break; 
	     case CLEANUP_STRUCTS: 
		free(OLC_SHOP(d)); 
		break; 
	     default: 
	       /*. Caller has screwed up .*/ 
		break; 
	    } 
	 } 
     /*. Check for gm . */
      if (OLC_GUILD(d)) 
	 {              
	 switch (cleanup_type) 
	    {
	     case CLEANUP_ALL:
		free_gm(OLC_GUILD(d));
		break;
	     case CLEANUP_STRUCTS:
		free(OLC_GUILD(d));
		break;
	     default:
		break;
	    }
	 }
      
     /*
      * Check for help.
      */
      if (OLC_HELP(d)) 
	 {
	 switch (cleanup_type) 
	    {
	     case CLEANUP_ALL:
		free_help(OLC_HELP(d));
		break;
	     case CLEANUP_STRUCTS:
		free_help(OLC_HELP(d));
		free(OLC_HELP(d));
		break;
	     default: /* The caller has screwed up. */       
		break;
	    }
	 }
     /*. Restore desciptor playing status .*/ 
      if (d->character) 
	 { 
	 REMOVE_BIT(PLR_FLAGS(d->character), PLR_WRITING); 
	 STATE(d)=CON_PLAYING; 
	 act("$n stops using OLC.", TRUE, d->character, 0, 0, TO_ROOM); 
	 } 
      free(d->olc); 
      } 
} 
 



