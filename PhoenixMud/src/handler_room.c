void affect_modify_room(struct room_data *rm, byte loc, long mod, long bitv, 
		   bool add) 
{ 
   int maxabil; 
  
   switch(loc)
      {
       case APPLY_IMMUNE:
	  if(add)
	     SET_BIT(IMMUNE(ch),mod);
	  else
	     REMOVE_BIT(IMMUNE(ch),mod);
	  break;
       case APPLY_RESIST:
	  if(add)
	     SET_BIT(RESIST(ch),mod);
	  else
	     REMOVE_BIT(RESIST(ch),mod);
	  break;
       case APPLY_SUSC:
	  if(add)
	     SET_BIT(SUCCEPT(ch),mod);
	  else
	     REMOVE_BIT(SUCCEPT(ch),mod);
	  break;
       case APPLY_FLY:
	  if(add)
	     {
	     SET_BIT(AFF2_FLAGS(ch),AFF2_FLYING);
	     SET_BIT(AFF_FLAGS(ch), AFF_FLY); 
	     }
	  else
	     {
	     REMOVE_BIT(AFF2_FLAGS(ch),AFF2_FLYING);
	     REMOVE_BIT(AFF_FLAGS(ch), AFF_FLY); 
	     }
	  break;


       default:
	  if (add) 
	     { 
	     SET_BIT(AFF_FLAGS(ch), bitv); 
	     } 
	  else 
	     { 
	     REMOVE_BIT(AFF_FLAGS(ch), bitv); 
	     mod = -mod; 
	     } 
	  
	  
	  maxabil = (IS_NPC(ch) ? 25 : 25); 
	  
	  switch (loc) 
	     { 
	      case APPLY_NONE: 
		 break; 
		 
	      case APPLY_STR: 
		 GET_STR(ch) += mod; 
		 break; 
	      case APPLY_DEX: 
		 GET_DEX(ch) += mod; 
		 break; 
	      case APPLY_INT: 
		 GET_INT(ch) += mod; 
		 break; 
	      case APPLY_WIS: 
		 GET_WIS(ch) += mod; 
		 break; 
	      case APPLY_CON: 
		 GET_CON(ch) += mod; 
		 break; 
	      case APPLY_CHA: 
		 GET_CHA(ch) += mod; 
		 break; 
 
	      case APPLY_CLASS: 
		/* ??? GET_CLASS(ch) += mod; */ 
		 break; 
 
	      case APPLY_LEVEL: 
		/* ??? GET_LEVEL(ch) += mod; */ 
		 break; 

	      case APPLY_SEX:
		 break;

	      case APPLY_AGE: 
		 ch->player.time.birth -= (mod * SECS_PER_MUD_YEAR); 
		 break; 
 
	      case APPLY_CHAR_WEIGHT: 
		 GET_WEIGHT(ch) += mod; 
		 break; 
 
	      case APPLY_CHAR_HEIGHT: 
		 GET_HEIGHT(ch) += mod; 
		 break; 
 
	      case APPLY_MANA: 
		 GET_MAX_MANA(ch) += mod; 
		 break; 
 
	      case APPLY_HIT: 
		 GET_MAX_HIT(ch) += mod; 
		 break; 
 
	      case APPLY_MOVE: 
		 GET_MAX_MOVE(ch) += mod; 
		 break; 
 
	      case APPLY_GOLD: 
		 break; 
 
	      case APPLY_EXP: 
		 break; 
 
	      case APPLY_AC: 
		 GET_AC(ch) += mod; 
		 break; 
 
	      case APPLY_HITROLL: 
		 GET_HITROLL(ch) += mod; 
		 break; 
 
	      case APPLY_DAMROLL: 
		 GET_DAMROLL(ch) += mod; 
		 break; 
 
	      case APPLY_SAVING_PARA: 
		 GET_SAVE(ch, SAVING_PARA) += mod; 
		 break; 
 
	      case APPLY_SAVING_ROD: 
		 GET_SAVE(ch, SAVING_ROD) += mod; 
		 break; 
 
	      case APPLY_SAVING_PETRI: 
		 GET_SAVE(ch, SAVING_PETRI) += mod; 
		 break; 
 
	      case APPLY_SAVING_BREATH: 
		 GET_SAVE(ch, SAVING_BREATH) += mod; 
		 break; 
 
	      case APPLY_SAVING_SPELL: 
		 GET_SAVE(ch, SAVING_SPELL) += mod; 
		 break; 
 
	      case APPLY_LIGHT:
		 world[IN_ROOM(ch)].light += mod;
		 GET_LIGHT(ch) += mod;
		 break;

	      default: 
		 log("SYSERR: Unknown apply adjust attempt (handler.c, affect_modify). (%d)",loc); 
		 break; 
 
	     } 
/* switch */ 
      } 
 
}
 
/* This updates a character by subtracting everything he is affected by */ 
/* restoring original abilities, and then affecting all again           */ 
void affect_total_room(struct room_data *rm) 
{ 
   struct room_affected_type *af; 
 
 
   for (af = ch->affected; af; af = af->next) 
      affect_modify_room(rm, af->location, af->modifier, af->bitvector, FALSE);
  /* 
   * code to set room to default stats should be here if possible
   */
 
   for (af = ch->affected; af; af = af->next) 
      affect_modify_room(rm, af->location, af->modifier, af->bitvector, TRUE); 
   
} 
 
 
 
/* Insert an affect_type in a char_data structure 
   Automatically sets apropriate bits and apply's */ 
void affect_to_room(struct room_data *rm, struct room_affected_type * af) 
{ 
   struct room_affected_type *affected_alloc; 
 
   CREATE(affected_alloc, struct room_affected_type, 1); 
 
   *affected_alloc = *af; 
   affected_alloc->next = rm->affected; 
   rm->affected = affected_alloc; 
 
   affect_modify_room(rm, af->location, af->modifier, af->bitvector, TRUE); 
   affect_total_room(rm); 
} 
 
 
 
/* 
 * Remove an room_affected_type structure from a char (called when duration 
 * reaches zero). Pointer *af must never be NIL!  Frees mem and calls 
 * affect_location_apply 
 */ 
void affect_remove_room(struct room_data *rm, struct room_affected_type * af) 
{ 
   struct room_affected_type *temp; 
 
   if(rm->affected==NULL)
      {
      core_dump();
      return;
      }
   
   affect_modify_room(rm, af->location, af->modifier, af->bitvector, FALSE); 
   REMOVE_FROM_LIST(af, rm->affected, next); 
   free(af); 
   affect_total_room(rm); 
} 
 
 
 
/* Call affect_remove with every spell of spelltype "skill" */ 
void affect_from_room(struct room_data *rm, int type) 
{ 
   struct room_affected_type *hjp, *next; 
 
   for (hjp = rm->affected; hjp; hjp = next) 
      { 
      next = hjp->next; 
      if (hjp->type == type) 
	 affect_remove_room(rm, hjp); 
      } 
} 
 
 
 
/* 
 * Return if a char is affected by a spell (SPELL_XXX), NULL indicates 
 * not affected 
 */ 
bool affected_by_spell_room(struct room_data *rm, int type) 
{ 
   struct room_affected_type *hjp; 
 
   for (hjp = rm->affected; hjp; hjp = hjp->next) 
      if (hjp->type == type) 
	 return TRUE; 
 
   return FALSE; 
} 
 
 
 
void affect_join_room(struct room_data *rm, struct room_affected_type * af, 
		      bool add_dur, bool avg_dur, bool add_mod, bool avg_mod) 
{ 
   struct room_affected_type *hjp; 
   bool found = FALSE; 
 
   for (hjp = rm->affected; !found && hjp; hjp = hjp->next) 
      { 
 
      if ((hjp->type == af->type) && (hjp->location == af->location)) 
	 { 
	 if (add_dur) 
	    af->duration += hjp->duration; 
	 if (avg_dur) 
	    af->duration /= 2; 
 
	 if (add_mod) 
	    af->modifier += hjp->modifier; 
	 if (avg_mod) 
	    af->modifier /= 2; 
 
	 affect_remove_room(rm, hjp); 
	 affect_to_room(rm, af); 
	 found = TRUE; 
	 } 
      } 
   if (!found) 
      affect_to_room(rm, af); 
} 
