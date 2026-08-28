/***********************************************************************
*  File: obj_instance.h                                                *
*  Usage: prototype-vs-instance ownership rules for saved objects      *
*                                                                      *
*  A saved object is its prototype plus the facts that happened to     *
*  this copy: charges used, damage taken, an enchant, a restring.      *
*  Everything else re-reads from world data at load, so a builder's    *
*  oedit reaches copies in rent, corpses, houses and shop stock.       *
***********************************************************************/

#ifndef _OBJ_INSTANCE_H_
#define _OBJ_INSTANCE_H_

/* How one value[] slot loads, per item type.  No slot is uniformly
 * instance-owned: value[2] is charges for a wand but max-fuel's partner
 * for a light, value[1] is units for a drink container but the closed/
 * locked bits for a chest.  Hence a table, not a rule. */
#define SLOT_PROTO    0  /* prototype wins; the saved copy is discarded  */
#define SLOT_DELTA    1  /* saved as instance - prototype; re-added      */
#define SLOT_ABS      2  /* saved verbatim                               */
#define SLOT_MASK     3  /* named bits follow the instance, rest proto   */
#define SLOT_DERIVED  4  /* recomputed at load; never stored             */

/* Facts recorded for one instance.  Parsed from the version-3 fact lines
 * of a rent record, or derived by diffing a version-1/2 snapshot against
 * the current prototype (the migration read). */
struct obj_instance_facts {
   int has_type;
   int saved_type;

   byte val_kind[NUM_OBJ_VAL_POSITIONS];      /* SLOT_PROTO unless a fact */
   long val_num[NUM_OBJ_VAL_POSITIONS];       /* delta or absolute value  */
   long val_mask[NUM_OBJ_VAL_POSITIONS];      /* SLOT_MASK only           */

   long extra_set, extra_clear;
   long extra2_set, extra2_clear;
   long extra3_set, extra3_clear;
   long anti_set, anti_clear;
   long bitv_set, bitv_clear;

   int has_cond;
   int cond_wear;                             /* orig - total             */
   int cond_dmg;                              /* total - curr             */

   int num_applies;                           /* enchant grants           */
   struct obj_affected_type applies[MAX_OBJ_AFFECT];
   int applies_dropped;                       /* stale snapshot discarded */

   char *name, *short_desc, *long_desc, *action_desc;   /* owned         */
   int has_exdesc;
   struct extra_descr_data *exdesc;                     /* owned list    */
};

void obj_facts_init(struct obj_instance_facts *f);
void obj_facts_free(struct obj_instance_facts *f);

int  obj_slot_treatment(int item_type, int slot);
long obj_slot_instance_bits(int item_type, int slot);
long obj_instance_extra_bits(void);
int  obj_full_snapshot_vnum(obj_vnum vnum);
int  obj_applies_are_enchant(struct obj_data *obj, struct obj_data *proto);

void obj_derive_instance_facts(struct obj_data *obj, struct obj_data *proto,
                               struct obj_instance_facts *f);
void obj_apply_instance_facts(struct obj_data *obj, struct obj_data *proto,
                              struct obj_instance_facts *f);

#endif /* _OBJ_INSTANCE_H_ */
