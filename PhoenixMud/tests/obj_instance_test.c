/***********************************************************************
*  File: obj_instance_test.c                                           *
*  Usage: standalone checks for the prototype-vs-instance rules        *
*                                                                      *
*  Build and run with `make test` from the repository root.  Links     *
*  only obj_instance.o, so everything here works on stack structs.     *
***********************************************************************/

#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"
#include "../src/structs.h"
#include "../src/utils.h"
#include "../src/obj_instance.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond) check((cond), #cond, __LINE__)

static void check(int cond, const char *what, int line)
   {
   checks++;
   if (!cond)
      {
      failures++;
      printf("FAIL %s:%d  %s\n", __FILE__, line, what);
      }
   }

static void reset(struct obj_data *o, int type)
   {
   memset(o, 0, sizeof(*o));
   o->obj_flags.type_flag = type;
   }

/* prototype + a fresh copy of it, the way read_object hands one out */
static void spawn(struct obj_data *proto, struct obj_data *inst)
   {
   *inst = *proto;
   }

static void treatments(void)
   {
   CHECK(obj_slot_treatment(ITEM_WAND, 2) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_WAND, 1) == SLOT_PROTO);
   CHECK(obj_slot_treatment(ITEM_STAFF, 2) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_LIGHT, 2) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_LIGHT, 3) == SLOT_PROTO);
   CHECK(obj_slot_treatment(ITEM_WEAPON, 1) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_WEAPON, 2) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_WEAPON, 6) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_WEAPON, 0) == SLOT_ABS);
   CHECK(obj_slot_treatment(ITEM_WEAPON, 7) == SLOT_ABS);
   CHECK(obj_slot_treatment(ITEM_CONTAINER, 1) == SLOT_MASK);
   CHECK(obj_slot_treatment(ITEM_CONTAINER, 5) == SLOT_DERIVED);
   CHECK(obj_slot_treatment(ITEM_DRINKCON, 1) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_DRINKCON, 2) == SLOT_ABS);
   CHECK(obj_slot_treatment(ITEM_DRINKCON, 3) == SLOT_ABS);
   CHECK(obj_slot_treatment(ITEM_FOUNTAIN, 1) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_FOOD, 0) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_FOOD, 3) == SLOT_ABS);
   CHECK(obj_slot_treatment(ITEM_FUEL, 0) == SLOT_DELTA);
   CHECK(obj_slot_treatment(ITEM_GRENADE, 0) == SLOT_ABS);
   CHECK(obj_slot_treatment(ITEM_PORTAL, 0) == SLOT_ABS);
   CHECK(obj_slot_treatment(ITEM_PORTAL, 1) == SLOT_ABS);
   CHECK(obj_slot_treatment(ITEM_ARMOR, 0) == SLOT_PROTO);
   CHECK(obj_slot_instance_bits(ITEM_CONTAINER, 1) == (CONT_CLOSED | CONT_LOCKED));

   CHECK(obj_full_snapshot_vnum(NOTHING));
   CHECK(obj_full_snapshot_vnum(700));
   CHECK(obj_full_snapshot_vnum(719));
   CHECK(!obj_full_snapshot_vnum(720));
   CHECK(!obj_full_snapshot_vnum(3001));
   }

static void wand_charges(void)
   {
   struct obj_data proto, inst;
   struct obj_instance_facts f;

   reset(&proto, ITEM_WAND);
   GET_OBJ_VAL(&proto, 1) = 20;          /* max charges     */
   GET_OBJ_VAL(&proto, 2) = 20;          /* authored full   */
   spawn(&proto, &inst);
   GET_OBJ_VAL(&inst, 2) = 3;            /* 17 zaps later   */

   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.val_kind[2] == SLOT_DELTA && f.val_num[2] == -17);
   CHECK(f.val_kind[1] == SLOT_PROTO);

   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_VAL(&inst, 2) == 3);

   /* builder raises the wand to 25 charges: the copy keeps its usage */
   GET_OBJ_VAL(&proto, 1) = 25;
   GET_OBJ_VAL(&proto, 2) = 25;
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_VAL(&inst, 2) == 8);

   /* builder cuts it to 2: clamped, never negative */
   GET_OBJ_VAL(&proto, 1) = 2;
   GET_OBJ_VAL(&proto, 2) = 2;
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_VAL(&inst, 2) == 0);
   obj_facts_free(&f);
   }

static void weapon_dice(void)
   {
   struct obj_data proto, inst;
   struct obj_instance_facts f;

   reset(&proto, ITEM_WEAPON);
   GET_OBJ_VAL(&proto, 1) = 4;
   GET_OBJ_VAL(&proto, 2) = 6;

   /* a stale snapshot: dice differ with no forge count and no curse */
   spawn(&proto, &inst);
   GET_OBJ_VAL(&inst, 1) = 6;
   GET_OBJ_VAL(&inst, 2) = 8;
   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.val_kind[1] == SLOT_PROTO && f.val_kind[2] == SLOT_PROTO);
   spawn(&proto, &inst);
   GET_OBJ_VAL(&inst, 1) = 6;
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_VAL(&inst, 1) == 4 && GET_OBJ_VAL(&inst, 2) == 6);
   obj_facts_free(&f);

   /* forged: value[6] moved, so 0/1/2/6/7 are the smith's work */
   spawn(&proto, &inst);
   GET_OBJ_VAL(&inst, 0) = 105;
   GET_OBJ_VAL(&inst, 1) = 5;
   GET_OBJ_VAL(&inst, 2) = 8;
   GET_OBJ_VAL(&inst, 6) = 1;
   GET_OBJ_VAL(&inst, 7) = 4242;
   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.val_kind[0] == SLOT_ABS && f.val_num[0] == 105);
   CHECK(f.val_kind[1] == SLOT_DELTA && f.val_num[1] == 1);
   CHECK(f.val_kind[2] == SLOT_DELTA && f.val_num[2] == 2);
   CHECK(f.val_kind[6] == SLOT_DELTA && f.val_num[6] == 1);
   CHECK(f.val_kind[7] == SLOT_ABS && f.val_num[7] == 4242);

   /* builder retunes the base dice: the forge bonus rides on top */
   GET_OBJ_VAL(&proto, 1) = 10;
   GET_OBJ_VAL(&proto, 2) = 3;
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_VAL(&inst, 1) == 11 && GET_OBJ_VAL(&inst, 2) == 5);
   CHECK(GET_OBJ_VAL(&inst, 0) == 105 && GET_OBJ_VAL(&inst, 7) == 4242);
   obj_facts_free(&f);

   /* cursed: NODROP with a lowered dice size is the curse's doing */
   GET_OBJ_VAL(&proto, 1) = 4;
   GET_OBJ_VAL(&proto, 2) = 6;
   spawn(&proto, &inst);
   SET_BIT(GET_OBJ_EXTRA(&inst), ITEM_NODROP);
   GET_OBJ_VAL(&inst, 2) = 2;
   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.val_kind[2] == SLOT_DELTA && f.val_num[2] == -4);
   CHECK(f.extra_set == ITEM_NODROP);
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_VAL(&inst, 2) == 2);
   CHECK(IS_SET(GET_OBJ_EXTRA(&inst), ITEM_NODROP));
   obj_facts_free(&f);
   }

static void flag_words(void)
   {
   struct obj_data proto, inst;
   struct obj_instance_facts f;

   reset(&proto, ITEM_ARMOR);
   SET_BIT(GET_OBJ_EXTRA(&proto), ITEM_NORENT);
   spawn(&proto, &inst);

   /* a play event set BLESS; something stale cleared NORENT */
   SET_BIT(GET_OBJ_EXTRA(&inst), ITEM_BLESS);
   REMOVE_BIT(GET_OBJ_EXTRA(&inst), ITEM_NORENT);
   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.extra_set == ITEM_BLESS);
   CHECK(f.extra_clear == 0);            /* NORENT is the builder's */
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(IS_SET(GET_OBJ_EXTRA(&inst), ITEM_BLESS));
   CHECK(IS_SET(GET_OBJ_EXTRA(&inst), ITEM_NORENT));
   obj_facts_free(&f);
   }

static void condition(void)
   {
   struct obj_data proto, inst;
   struct obj_instance_facts f;

   reset(&proto, ITEM_ARMOR);
   GET_OBJ_CSLOTS(&proto) = 100;
   GET_OBJ_TSLOTS(&proto) = 100;
   GET_OBJ_OSLOTS(&proto) = 100;
   spawn(&proto, &inst);
   GET_OBJ_CSLOTS(&inst) = 40;           /* beaten on            */
   GET_OBJ_TSLOTS(&inst) = 90;           /* repaired a few times */

   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.has_cond && f.cond_wear == 10 && f.cond_dmg == 50);

   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_CSLOTS(&inst) == 40);
   CHECK(GET_OBJ_TSLOTS(&inst) == 90);
   CHECK(GET_OBJ_OSLOTS(&inst) == 100);

   /* builder gives it 120 slots: damage taken stays, headroom grows */
   GET_OBJ_CSLOTS(&proto) = 120;
   GET_OBJ_TSLOTS(&proto) = 120;
   GET_OBJ_OSLOTS(&proto) = 120;
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_CSLOTS(&inst) == 60);
   CHECK(GET_OBJ_TSLOTS(&inst) == 110);
   obj_facts_free(&f);

   /* broken items stay broken: curr below zero survives the trip */
   GET_OBJ_CSLOTS(&proto) = 100;
   GET_OBJ_TSLOTS(&proto) = 100;
   GET_OBJ_OSLOTS(&proto) = 100;
   spawn(&proto, &inst);
   GET_OBJ_CSLOTS(&inst) = -3;
   obj_derive_instance_facts(&inst, &proto, &f);
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_CSLOTS(&inst) == -3);
   obj_facts_free(&f);
   }

static void enchant_applies(void)
   {
   struct obj_data proto, inst;
   struct obj_instance_facts f;

   reset(&proto, ITEM_WEAPON);
   spawn(&proto, &inst);
   SET_BIT(GET_OBJ_EXTRA(&inst), ITEM_MAGIC | ITEM_UNIQUE_SAVE | ITEM_ANTI_EVIL);
   inst.affected[0].location = APPLY_HITROLL;
   inst.affected[0].modifier = 3;
   inst.affected[1].location = APPLY_DAMROLL;
   inst.affected[1].modifier = 2;

   CHECK(obj_applies_are_enchant(&inst, &proto));
   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.num_applies == 2 && !f.applies_dropped);
   CHECK(f.extra_set == (ITEM_MAGIC | ITEM_UNIQUE_SAVE | ITEM_ANTI_EVIL));

   /* builder later authors an apply on the base item: both survive */
   proto.affected[0].location = APPLY_STR;
   proto.affected[0].modifier = 1;
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(inst.affected[0].location == APPLY_STR);
   CHECK(inst.affected[1].location == APPLY_HITROLL && inst.affected[1].modifier == 3);
   CHECK(inst.affected[2].location == APPLY_DAMROLL && inst.affected[2].modifier == 2);
   CHECK(IS_SET(GET_OBJ_EXTRA(&inst), ITEM_MAGIC));
   obj_facts_free(&f);

   /* a stale snapshot that only LOOKS magical is not an enchant */
   reset(&proto, ITEM_WEAPON);
   spawn(&proto, &inst);
   SET_BIT(GET_OBJ_EXTRA(&inst), ITEM_MAGIC);
   inst.affected[0].location = APPLY_STR;
   inst.affected[0].modifier = 2;
   CHECK(!obj_applies_are_enchant(&inst, &proto));
   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.num_applies == 0 && f.applies_dropped);
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(inst.affected[0].location == APPLY_NONE);
   obj_facts_free(&f);

   /* armor: a single negative AC */
   reset(&proto, ITEM_ARMOR);
   spawn(&proto, &inst);
   SET_BIT(GET_OBJ_EXTRA(&inst), ITEM_MAGIC);
   inst.affected[0].location = APPLY_AC;
   inst.affected[0].modifier = -5;
   CHECK(obj_applies_are_enchant(&inst, &proto));

   /* prototype already magical: applies are the prototype's business */
   SET_BIT(GET_OBJ_EXTRA(&proto), ITEM_MAGIC);
   CHECK(!obj_applies_are_enchant(&inst, &proto));
   }

static void strings(void)
   {
   struct obj_data proto, inst;
   struct obj_instance_facts f;
   static char pname[] = "sword long";
   static char psdesc[] = "a long sword";

   reset(&proto, ITEM_WEAPON);
   proto.name = pname;
   proto.short_description = psdesc;

   /* restrung: unique-save carries the text */
   spawn(&proto, &inst);
   SET_BIT(GET_OBJ_EXTRA(&inst), ITEM_UNIQUE_SAVE);
   inst.name = strdup("stormbringer sword");
   inst.short_description = strdup("Stormbringer");
   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.name && !strcmp(f.name, "stormbringer sword"));
   CHECK(f.short_desc && !strcmp(f.short_desc, "Stormbringer"));
   free(inst.name);
   free(inst.short_description);
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(!strcmp(inst.name, "stormbringer sword"));
   CHECK(!strcmp(inst.short_description, "Stormbringer"));
   CHECK(inst.description == proto.description);
   if (inst.name != proto.name)
      free(inst.name);
   if (inst.short_description != proto.short_description)
      free(inst.short_description);
   obj_facts_free(&f);

   /* the same difference without unique-save is transient */
   spawn(&proto, &inst);
   inst.name = strdup("water sword long");
   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.name == NULL);
   free(inst.name);
   obj_facts_free(&f);
   }

static void drink_container(void)
   {
   struct obj_data proto, inst;
   struct obj_instance_facts f;

   reset(&proto, ITEM_DRINKCON);
   GET_OBJ_VAL(&proto, 0) = 10;          /* capacity     */
   GET_OBJ_VAL(&proto, 1) = 10;          /* authored full */
   GET_OBJ_VAL(&proto, 2) = 0;           /* water        */
   GET_OBJ_WEIGHT(&proto) = 12;
   spawn(&proto, &inst);
   GET_OBJ_VAL(&inst, 1) = 4;            /* six drunk    */
   GET_OBJ_VAL(&inst, 2) = 5;            /* now whisky   */
   GET_OBJ_VAL(&inst, 3) = 1;            /* and poisoned */
   GET_OBJ_WEIGHT(&inst) = 6;

   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.val_kind[1] == SLOT_DELTA && f.val_num[1] == -6);
   CHECK(f.val_kind[2] == SLOT_ABS && f.val_num[2] == 5);
   CHECK(f.val_kind[3] == SLOT_ABS && f.val_num[3] == 1);

   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_VAL(&inst, 1) == 4);
   CHECK(GET_OBJ_VAL(&inst, 2) == 5);
   CHECK(GET_OBJ_VAL(&inst, 3) == 1);
   CHECK(GET_OBJ_WEIGHT(&inst) == 6);    /* weight follows the liquid */
   obj_facts_free(&f);
   }

static void container_state(void)
   {
   struct obj_data proto, inst;
   struct obj_instance_facts f;

   reset(&proto, ITEM_CONTAINER);
   GET_OBJ_VAL(&proto, 0) = 50;
   GET_OBJ_VAL(&proto, 1) = CONT_CLOSEABLE;
   spawn(&proto, &inst);
   SET_BIT(GET_OBJ_VAL(&inst, 1), CONT_CLOSED | CONT_LOCKED);
   GET_OBJ_VAL(&inst, 5) = 33;           /* carried loot weight */

   obj_derive_instance_facts(&inst, &proto, &f);
   CHECK(f.val_kind[1] == SLOT_MASK);
   CHECK(f.val_kind[5] == SLOT_PROTO);

   /* builder makes it pickproof while it sits in rent */
   SET_BIT(GET_OBJ_VAL(&proto, 1), CONT_PICKPROOF);
   spawn(&proto, &inst);
   obj_apply_instance_facts(&inst, &proto, &f);
   CHECK(GET_OBJ_VAL(&inst, 1) ==
         (CONT_CLOSEABLE | CONT_PICKPROOF | CONT_CLOSED | CONT_LOCKED));
   CHECK(GET_OBJ_VAL(&inst, 5) == 0);    /* rebuilt as contents load */
   obj_facts_free(&f);
   }

int main(void)
   {
   treatments();
   wand_charges();
   weapon_dice();
   flag_words();
   condition();
   enchant_applies();
   strings();
   drink_container();
   container_state();

   printf("%d checks, %d failures\n", checks, failures);
   return failures ? 1 : 0;
   }
