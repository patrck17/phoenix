/* tsplayer.c  -  load a TypeScript-platform player record into char_file_u.
 * See tsplayer.h for why this exists and why it is additive.
 *
 * !! Keep every byte of this file ASCII (see the header).
 */

/* The repository's own include style for these two: the copies in src/ are
 * stale K&R headers whose fcntl/fprintf/sscanf declarations conflict with
 * modern glibc. A deployment that copies localHeader over src/ hides the
 * difference; a build straight from the repository does not. */
#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"
#include "structs.h"
#include "utils.h"
#include "db.h"
#include "handler.h"
#include "json.h"
#include "tsplayer.h"

/* ------------------------------------------------------------------ */
/* JSON accessors. Same shape as the helpers capture_snapshot.c already
 * uses, kept local so this unit has no cross-dependency on it.        */
/* ------------------------------------------------------------------ */

static struct json_object_element_s *ts_find(struct json_object_s *o,
                                             const char *key)
{
   struct json_object_element_s *e;
   if (!o) return NULL;
   for (e = o->start; e; e = e->next)
      if (e->name && !strcmp(e->name->string, key))
         return e;
   return NULL;
}

static double ts_num(struct json_object_s *o, const char *key, double dflt)
{
   struct json_object_element_s *e = ts_find(o, key);
   if (!e || e->value->type != json_type_number) return dflt;
   return atof(((struct json_number_s *) e->value->payload)->number);
}

static const char *ts_str(struct json_object_s *o, const char *key)
{
   struct json_object_element_s *e = ts_find(o, key);
   if (!e || e->value->type != json_type_string) return NULL;
   return ((struct json_string_s *) e->value->payload)->string;
}

/* An ISO-8601 UTC timestamp ("2026-08-15T15:12:26.422Z") as a time_t.
 * Returns `dflt` when the key is absent or does not parse.
 *
 * The record writes times two ways: birthUnix is already epoch seconds, but
 * lastLogin and createdAt are strings, so they need this. timegm is used
 * rather than mktime because the value is UTC and mktime would apply the
 * host's timezone. */
static time_t ts_time(struct json_object_s *o, const char *key, time_t dflt)
{
   const char *v = ts_str(o, key);
   struct tm tm;
   int y, mo, d, h, mi, sec;

   if (!v) return dflt;
   if (sscanf(v, "%d-%d-%dT%d:%d:%d", &y, &mo, &d, &h, &mi, &sec) != 6)
      return dflt;

   memset(&tm, 0, sizeof(tm));
   tm.tm_year = y - 1900;
   tm.tm_mon  = mo - 1;
   tm.tm_mday = d;
   tm.tm_hour = h;
   tm.tm_min  = mi;
   tm.tm_sec  = sec;
   return timegm(&tm);
}

/* One element of a JSON number array, by index. Used for the small fixed-width
 * arrays the record carries - conditions[3], legacyPrefBits[3]. */
static double ts_idx(struct json_object_s *o, const char *key, size_t want,
                     double dflt)
{
   struct json_object_element_s *e = ts_find(o, key);
   struct json_array_element_s *it;
   size_t i = 0;

   if (!e || e->value->type != json_type_array) return dflt;
   for (it = ((struct json_array_s *) e->value->payload)->start;
        it; it = it->next, i++)
      if (i == want)
         return it->value->type == json_type_number
            ? atof(((struct json_number_s *) it->value->payload)->number)
            : dflt;
   return dflt;
}

static struct json_object_s *ts_obj(struct json_object_s *o, const char *key)
{
   struct json_object_element_s *e = ts_find(o, key);
   if (!e || e->value->type != json_type_object) return NULL;
   return (struct json_object_s *) e->value->payload;
}

/* Copy at most len-1 bytes and always NUL-terminate. The engine's string
 * fields are fixed arrays and a TS title or description can exceed them. */
static void ts_copy(char *dst, const char *src, size_t len)
{
   if (!dst || len == 0) return;
   if (!src) { dst[0] = '\0'; return; }
   strncpy(dst, src, len - 1);
   dst[len - 1] = '\0';
}


/* ------------------------------------------------------------------ */
/* Skills and spells.                                                  */
/*                                                                     */
/* The engine keeps two flat byte arrays indexed by skill id:          */
/* skills[] (proficiency) and skills_learn[] (learn progress). A       */
/* TypeScript record instead keeps four sparse maps - skillProfs,      */
/* spellProfs, skillsLearn, spellsLearn - keyed by TS id.              */
/*                                                                     */
/* !! TS ids are not engine ids for every entry. This table is a       */
/* verbatim copy of TS_TO_LEGACY_SKILL_ID in tools/importer/           */
/* serialize-plr.ts, so a character loaded natively lands on the same  */
/* ids as one loaded through the conversion path. Consistency between  */
/* the two paths is the point; if the table is ever corrected, correct */
/* it in BOTH places.                                                  */
/*                                                                     */
/* !! KNOWN DRIFT, deliberately reproduced rather than repaired: the   */
/* inverse map on the TypeScript side (LEGACY_TO_TS_SKILL_ID in        */
/* parse-plr.ts) has 61 entries against this table's 49. Eleven are    */
/* not invertible and one disagrees outright. Which id is correct is a */
/* data question - upstream reused spell ids intentionally - so it     */
/* needs an explicit decision, not a mechanical merge.                 */
/* ------------------------------------------------------------------ */

static const int ts_skill_map[][2] = { {209,111}, {205,117}, {10191,123}, {203,190}, {206,200}, {204,201}, {207,203}, {208,204}, {188,7}, {192,11}, {189,12}, {190,13}, {122,20}, {119,24}, {112,25}, {187,31}, {111,40}, {193,45}, {101,49}, {114,56}, {115,57}, {116,58}, {117,59}, {118,60}, {102,61}, {120,71}, {103,72}, {177,74}, {179,84}, {110,87}, {197,90}, {123,91}, {104,101}, {210,102}, {191,103}, {109,104}, {107,145}, {108,146}, {174,171}, {180,187}, {219,188}, {178,189}, {221,191}, {175,192}, {220,193}, {113,197}, {121,198}, {176,199}, {106,202} };

static int ts_to_legacy_skill(int ts_id)
{
   size_t i;
   for (i = 0; i < sizeof(ts_skill_map) / sizeof(ts_skill_map[0]); i++)
      if (ts_skill_map[i][0] == ts_id)
         return ts_skill_map[i][1];
   return ts_id;
}

/* Copy a sparse {id: value} object into a flat byte array. Ids outside the
 * array are skipped rather than clamped: a wrong id silently overwriting a
 * real skill is worse than a missing one. */
static void ts_fill_skill_array(struct json_object_s *src, byte *dst, int max)
{
   struct json_object_element_s *e;
   if (!src || !dst) return;
   for (e = src->start; e; e = e->next) {
      int id, val;
      if (!e->name || e->value->type != json_type_number) continue;
      id = ts_to_legacy_skill(atoi(e->name->string));
      val = atoi(((struct json_number_s *) e->value->payload)->number);
      if (id < 0 || id > max) continue;
      if (val < 0) val = 0;
      if (val > 255) val = 255;
      dst[id] = (byte) val;
   }
}


/* Affects. The engine keeps a fixed array of MAX_AFFECT slots inside
 * char_file_u; a TS record keeps a list. Slots beyond the list are zeroed so a
 * shorter list cannot leave a previous character's affects behind.
 *
 * !! spellId <= 0 entries are TS-INTERNAL markers - equipment-derived affects,
 * racial innates, and the group-membership flag. Legacy pfiles never carry
 * them and the engine re-derives them at load, so writing them here would both
 * duplicate and corrupt the block. The exporter excludes them for the same
 * reason (serialize-plr.ts).
 *
 * The spell id goes through the same TS->engine remap as skills, so an affect
 * survives as the SAME spell whichever loader ran. */
static void ts_fill_affects(struct json_object_s *o, struct char_file_u *ch)
{
   struct json_object_element_s *e = ts_find(o, "affects");
   struct json_array_element_s *it;
   struct json_array_s *arr;
   int slot = 0;

   if (!e || e->value->type != json_type_array) return;
   arr = (struct json_array_s *) e->value->payload;

   for (it = arr->start; it && slot < MAX_AFFECT; it = it->next) {
      struct json_object_s *a;
      int type;
      if (it->value->type != json_type_object) continue;
      a = (struct json_object_s *) it->value->payload;
      type = (int) ts_num(a, "spellId", 0);
      if (type <= 0) continue;                 /* TS-internal marker */
      ch->affected[slot].type        = (sh_int) ts_to_legacy_skill(type);
      ch->affected[slot].duration    = (sh_int) ts_num(a, "duration", 0);
      ch->affected[slot].modifier    = (long)   ts_num(a, "modifier", 0);
      ch->affected[slot].location    = (byte)   ts_num(a, "location", 0);
      ch->affected[slot].bitvector   = (bitvector_t) ts_num(a, "bitvector", 0);
      ch->affected[slot].spell_level = (sh_int) ts_num(a, "castLevel", 0);
      ch->affected[slot].next        = NULL;
      slot++;
   }
   for (; slot < MAX_AFFECT; slot++)
      memset(&ch->affected[slot], 0, sizeof(ch->affected[slot]));
}

/* ------------------------------------------------------------------ */

const char *tsplayer_dir(void)
{
   const char *env = getenv("PHOENIX_TS_PLAYERS");
   return (env && *env) ? env : "world/players";
}

/* TS record filenames are the lowercased name with non-letters removed,
 * matching the key the TypeScript store uses. */
static void ts_path(char *out, size_t outlen, const char *name)
{
   char key[MAX_NAME_LENGTH + 1];
   size_t i, k = 0;
   for (i = 0; name[i] && k < sizeof(key) - 1; i++)
      if (isalpha((unsigned char) name[i]))
         key[k++] = (char) tolower((unsigned char) name[i]);
   key[k] = '\0';
   snprintf(out, outlen, "%s/%s.json", tsplayer_dir(), key);
}

int tsplayer_available(const char *name)
{
   char path[512];
   FILE *fp;
   if (!name || !*name) return 0;
   ts_path(path, sizeof(path), name);
   fp = fopen(path, "r");
   if (!fp) return 0;
   fclose(fp);
   return 1;
}


/* ------------------------------------------------------------------ */
/* Equipment and inventory.                                            */
/*                                                                     */
/* These do NOT live in char_file_u - the engine keeps them in a        */
/* separate rent file and loads them via Crash_load. A TS record        */
/* carries them inline as equipmentObjects (slot -> object) and         */
/* inventoryObjects (a list).                                           */
/*                                                                      */
/* A TS object record is a DELTA against the prototype: it always has a */
/* vnum, and carries a field only where the instance differs. So the    */
/* loader reads the prototype and applies whatever overrides are        */
/* present, which is also why an unmodified item is just {"vnum": N}.   */
/* ------------------------------------------------------------------ */


/* Set one bit per vnum in a packed bitfield, from a JSON array of vnums.
 *
 * This engine keeps explored rooms and identified items as bit-per-vnum
 * vectors, written to sidecar files next to the pfile
 * (`<Name>.explored`, `<Name>.known`) and read by load_char_ascii. A TS record
 * carries the same information as a plain vnum list, so it converts directly.
 *
 * !! Indexed by VNUM, not rnum. handler.c:776 is the setter and uses
 * vnum/8 with 1 << (vnum%8); several READERS name their loop variable `rnum`
 * while iterating what are really vnums, which makes the code look
 * rnum-indexed when it is not.
 *
 * !! The vector is CLEARED first when the record carries the key. The
 * char_file_u handed to this loader is reused between loads and arrives
 * carrying the previous character's bits; load_char_ascii overwrites the whole
 * vector with one fread, so the replacement has to do the same. ORing onto
 * what was there reported Lokathein at 20918 rooms - 101.131% of a 20685-room
 * world - against the record's true 12855. A record with no entry is left
 * alone. */
static void ts_fill_bitfield(struct json_object_s *o, const char *key,
                             char *bits, int top_vnum, size_t nbytes)
{
   struct json_object_element_s *e = ts_find(o, key);
   struct json_array_element_s *it;

   if (!e || e->value->type != json_type_array || !bits) return;
   memset(bits, 0, nbytes);
   for (it = ((struct json_array_s *) e->value->payload)->start;
        it; it = it->next) {
      int vnum;
      if (it->value->type != json_type_number) continue;
      vnum = atoi(((struct json_number_s *) it->value->payload)->number);
      if (vnum >= 0 && vnum < top_vnum)
         bits[vnum / 8] |= 1 << (vnum % 8);
   }
}

/* Apply an object's inline SHELL - the strings and shape it carries instead of
 * inheriting them from a prototype.
 *
 * Two cases need this. A RESTRUNG item has a real vnum but its own name (512
 * of them in the state repository), and would otherwise wear the prototype's
 * name. A PROTOTYPE-LESS item has vnum < 0 and no prototype at all (139 of
 * them - mail letters, minted money) and is nothing BUT its shell.
 *
 * !! The strings on a prototype-read object point at the PROTOTYPE's own
 * storage, shared by every instance. Overwrite the pointer, never free what
 * was there: freeing it would corrupt the prototype for every other item in
 * the game. That is also why this only assigns where the record actually
 * carries a field. */
static void ts_apply_obj_shell(struct obj_data *obj, struct json_object_s *o)
{
   const char *v;

   if ((v = ts_str(o, "aliases")) != NULL)      obj->name = str_dup(v);
   if ((v = ts_str(o, "shortDesc")) != NULL)    obj->short_description = str_dup(v);
   if ((v = ts_str(o, "longDesc")) != NULL)     obj->description = str_dup(v);
   if ((v = ts_str(o, "detailedDesc")) != NULL) obj->action_description = str_dup(v);

   if (ts_find(o, "itemType"))
      GET_OBJ_TYPE(obj)   = (byte) ts_num(o, "itemType", GET_OBJ_TYPE(obj));
   if (ts_find(o, "wearFlags"))
      GET_OBJ_WEAR(obj)   = (bitvector_t)(long) ts_num(o, "wearFlags", 0);
   if (ts_find(o, "weight"))
      GET_OBJ_WEIGHT(obj) = (int) ts_num(o, "weight", GET_OBJ_WEIGHT(obj));
   if (ts_find(o, "cost"))
      GET_OBJ_COST(obj)   = (int) ts_num(o, "cost", GET_OBJ_COST(obj));
}

/* Apply per-instance overrides to a freshly read prototype. */
static void ts_apply_obj_deltas(struct obj_data *obj, struct json_object_s *o)
{
   struct json_object_element_s *e;
   struct json_array_element_s *it;
   int i;

   if (!obj || !o) return;

   /* values[0..7]. !! All EIGHT: the conversion path truncates to 0..3
    * (PdSavedObject persists four), and a native reader should not inherit
    * that limit - slots 4..7 carry real per-instance state. */
   e = ts_find(o, "values");
   if (e && e->value->type == json_type_array) {
      struct json_array_s *arr = (struct json_array_s *) e->value->payload;
      i = 0;
      for (it = arr->start; it && i < 8; it = it->next, i++)
         if (it->value->type == json_type_number)
            GET_OBJ_VAL(obj, i) =
               atoi(((struct json_number_s *) it->value->payload)->number);
   }

   /* Per-instance affects - what enchant weapon and enchant armor write.
    * Same slot discipline as the character block: clear the tail so a
    * shorter list cannot leave the prototype's affects behind. */
   e = ts_find(o, "objAffects");
   if (e && e->value->type == json_type_array) {
      struct json_array_s *arr = (struct json_array_s *) e->value->payload;
      int slot = 0;
      for (it = arr->start; it && slot < MAX_OBJ_AFFECT; it = it->next) {
         struct json_object_s *a;
         if (it->value->type != json_type_object) continue;
         a = (struct json_object_s *) it->value->payload;
         obj->affected[slot].location = (byte) ts_num(a, "location", 0);
         obj->affected[slot].modifier = (int)  ts_num(a, "modifier", 0);
         slot++;
      }
      for (; slot < MAX_OBJ_AFFECT; slot++) {
         obj->affected[slot].location = 0;
         obj->affected[slot].modifier = 0;
      }
   }

   if (ts_find(o, "timer"))
      GET_OBJ_TIMER(obj) = (int) ts_num(o, "timer", GET_OBJ_TIMER(obj));
   if (ts_find(o, "dgTimer"))
      GET_OBJ_DGTIMER(obj) = (int) ts_num(o, "dgTimer", GET_OBJ_DGTIMER(obj));

   /* The durability TRIPLE. All three are per-instance and all three matter:
    * curr is the damage taken, total is the repairable cap (a repair lowers
    * it), and orig is the prototype's original maximum. "not damaged" is
    * curr == total, and the examine "Wear condition:" line reads against orig,
    * so dropping any of them mis-renders every worn item. */
   obj->obj_flags.curr_dam_slots =
      (int) ts_num(o, "durability", obj->obj_flags.curr_dam_slots);
   obj->obj_flags.total_dam_slots =
      (int) ts_num(o, "totalDurability", obj->obj_flags.total_dam_slots);
   obj->obj_flags.orig_dam_slots =
      (int) ts_num(o, "origDurability", obj->obj_flags.orig_dam_slots);

   /* Per-instance flag words. enchant weapon sets ITEM_MAGIC/ITEM_GLOW here,
    * and a saved instance can differ from a retuned prototype either way, so
    * the record is authoritative wherever it carries the field. */
   if (ts_find(o, "extraFlags"))
      GET_OBJ_EXTRA(obj)  = (bitvector_t)(long) ts_num(o, "extraFlags", 0);
   if (ts_find(o, "extraFlags2"))
      obj->obj_flags.extra_flags2 = (bitvector_t)(long) ts_num(o, "extraFlags2", 0);
   if (ts_find(o, "extraFlags3"))
      obj->obj_flags.extra_flags3 = (bitvector_t)(long) ts_num(o, "extraFlags3", 0);
   if (ts_find(o, "antiFlags"))
      obj->obj_flags.anti_flags   = (bitvector_t)(long) ts_num(o, "antiFlags", 0);
   if (ts_find(o, "bitvectorAffect"))
      obj->obj_flags.bitvector    = (bitvector_t)(long) ts_num(o, "bitvectorAffect", 0);
   if (ts_find(o, "shopOrder"))
      obj->obj_flags.shop_order = (int) ts_num(o, "shopOrder", 0);
   if (ts_find(o, "rentPerDay"))
      obj->obj_flags.cost_per_day = (int) ts_num(o, "rentPerDay", 0);

   ts_apply_obj_shell(obj, o);
}

/* Longest single contents list we will honour. A player record is capped at
 * 16MB, so this only bounds the temporary index array below. */
#define TS_MAX_LIST 4096

static void ts_load_contents(struct char_data *ch, struct obj_data *carrier,
                             struct json_array_s *arr);

/* Read one TS object record and give it to `ch`, worn at `pos` when >= 0, or
 * stowed in `carrier` when that is non-NULL. Recurses into nested contents.
 *
 * !! A carrier that is not an ITEM_CONTAINER does NOT stow: its list is dumped
 * loose to the character. That is Crash_load's own rule (objsave.c:2280/2291) -
 * an arrow saved as content of a bow is carried, not put inside it. */
static void ts_load_one_obj(struct char_data *ch, struct json_object_s *o,
                            int pos, struct obj_data *carrier)
{
   struct obj_data *obj;
   struct json_object_element_s *e;
   obj_vnum vnum;
   obj_rnum rnum;

   if (!ch || !o) return;
   vnum = (obj_vnum) ts_num(o, "vnum", -1);
   rnum = (vnum < 0) ? -1 : real_object(vnum);

   if (rnum >= 0) {
      if (!(obj = read_object(vnum, VIRTUAL))) return;
   } else if (ts_str(o, "shortDesc") != NULL) {
      /* No prototype, but the record carries a full shell - a mail letter or
       * a minted pile of coins. Build it from that instead of skipping it,
       * which would quietly take the item away from the player. */
      obj = create_obj();
      GET_OBJ_RNUM(obj) = NOTHING;
   } else {
      return;               /* prototype gone and nothing to rebuild from */
   }

   ts_apply_obj_deltas(obj, o);

   if (carrier && GET_OBJ_TYPE(carrier) == ITEM_CONTAINER)
      obj_to_obj(obj, carrier);
   else if (pos >= 0 && pos < NUM_WEARS && !GET_EQ(ch, pos))
      equip_char(ch, obj, pos);
   else
      obj_to_char(obj, ch);

   e = ts_find(o, "contents");
   if (e && e->value->type == json_type_array)
      ts_load_contents(ch, obj, (struct json_array_s *) e->value->payload);
}

/* Load a nested contents list into `carrier`.
 *
 * !! Iterates BACKWARDS. The list is stored newest-first (live chain order,
 * head first) and both obj_to_obj and obj_to_char PREPEND (handler.c:825), so
 * loading front-to-back would reverse every list on the way in. Creating the
 * oldest first and prepending each rebuilds the original order. */
static void ts_load_contents(struct char_data *ch, struct obj_data *carrier,
                             struct json_array_s *arr)
{
   struct json_array_element_s *it, **idx;
   size_t n, i;

   if (!ch || !arr || arr->length == 0) return;
   n = arr->length > TS_MAX_LIST ? TS_MAX_LIST : arr->length;
   if (!(idx = (struct json_array_element_s **) malloc(n * sizeof(*idx))))
      return;

   for (i = 0, it = arr->start; it && i < n; it = it->next, i++)
      idx[i] = it;
   while (i-- > 0)
      if (idx[i]->value->type == json_type_object)
         ts_load_one_obj(ch, (struct json_object_s *) idx[i]->value->payload,
                         -1, carrier);

   free(idx);
}

/* Give `ch` a plain prototype by vnum, worn at `pos` when >= 0. Used only for
 * the older records that carry bare vnum lists with no per-instance deltas. */
static void ts_load_bare_obj(struct char_data *ch, int vnum, int pos)
{
   struct obj_data *obj;

   if (vnum < 0 || real_object((obj_vnum) vnum) < 0) return;
   if (!(obj = read_object((obj_vnum) vnum, VIRTUAL))) return;

   if (pos >= 0 && pos < NUM_WEARS && !GET_EQ(ch, pos))
      equip_char(ch, obj, pos);
   else
      obj_to_char(obj, ch);
}

/* Bare vnum inventory list. Backwards for the same reason as ts_load_contents:
 * the list is newest-first and obj_to_char prepends. */
static void ts_load_bare_list(struct char_data *ch, struct json_array_s *arr)
{
   struct json_array_element_s *it, **idx;
   size_t n, i;

   if (!ch || !arr || arr->length == 0) return;
   n = arr->length > TS_MAX_LIST ? TS_MAX_LIST : arr->length;
   if (!(idx = (struct json_array_element_s **) malloc(n * sizeof(*idx))))
      return;

   for (i = 0, it = arr->start; it && i < n; it = it->next, i++)
      idx[i] = it;
   while (i-- > 0)
      if (idx[i]->value->type == json_type_number)
         ts_load_bare_obj(ch,
            atoi(((struct json_number_s *) idx[i]->value->payload)->number), -1);

   free(idx);
}

/* Load equipment and inventory for `ch` from its TS record. Returns 1 when a
 * record was read (even if it held no objects), 0 when there is none - the
 * caller then falls back to Crash_load. */
int tsplayer_load_objects(struct char_data *ch, const char *name)
{
   char path[512];
   FILE *fp;
   long size;
   char *buf;
   struct json_value_s *root;
   struct json_object_s *o, *eq;
   struct json_object_element_s *e;

   if (!ch || !name || !*name) return 0;
   ts_path(path, sizeof(path), name);
   if (!(fp = fopen(path, "rb"))) return 0;
   if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
   size = ftell(fp);
   if (size <= 0 || size > (16 * 1024 * 1024)) { fclose(fp); return 0; }
   rewind(fp);
   if (!(buf = (char *) malloc((size_t) size + 1))) { fclose(fp); return 0; }
   if (fread(buf, 1, (size_t) size, fp) != (size_t) size) {
      free(buf); fclose(fp); return 0;
   }
   buf[size] = 0;
   fclose(fp);

   root = json_parse(buf, (size_t) size);
   free(buf);
   if (!root) return 0;
   if (root->type != json_type_object) { free(root); return 0; }
   o = (struct json_object_s *) root->payload;

   /* A record carries objects TWICE: `equipment` / `inventory` hold bare vnums,
    * and `equipmentObjects` / `inventoryObjects` hold the same items as
    * per-instance deltas, paired POSITIONALLY. The delta form is preferred - it
    * is a superset, since every entry carries its own vnum.
    *
    * !! The bare arrays are not always accompanied by the delta form: 3 of 3048
    * records in the state repository carry `inventory` with no
    * `inventoryObjects` (and 3 carry `equipment` with no `equipmentObjects`).
    * Reading only the delta form would silently strip those characters of
    * everything they own, so each falls back to the bare vnum list. */

   eq = ts_obj(o, "equipmentObjects");
   if (eq) {
      for (e = eq->start; e; e = e->next)
         if (e->name && e->value->type == json_type_object)
            ts_load_one_obj(ch, (struct json_object_s *) e->value->payload,
                            atoi(e->name->string), NULL);
   } else if ((eq = ts_obj(o, "equipment")) != NULL) {
      for (e = eq->start; e; e = e->next)
         if (e->name && e->value->type == json_type_number)
            ts_load_bare_obj(ch,
               atoi(((struct json_number_s *) e->value->payload)->number),
               atoi(e->name->string));
   }

   /* Inventory carries the same newest-first order as a contents list, and
    * obj_to_char prepends, so it loads through the same backwards walk.
    * Passing a NULL carrier makes each entry go to the character. */
   e = ts_find(o, "inventoryObjects");
   if (e && e->value->type == json_type_array)
      ts_load_contents(ch, NULL, (struct json_array_s *) e->value->payload);
   else if ((e = ts_find(o, "inventory")) != NULL
            && e->value->type == json_type_array)
      ts_load_bare_list(ch, (struct json_array_s *) e->value->payload);

   free(root);
   return 1;
}

int tsplayer_load(struct char_file_u *ch, const char *name)
{
   char path[512];
   FILE *fp;
   long size;
   char *buf;
   struct json_value_s *root;
   struct json_object_s *o, *stats;
   const char *s;

   if (!ch || !name || !*name) return 0;
   ts_path(path, sizeof(path), name);
   if (!(fp = fopen(path, "rb"))) return 0;

   if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return 0; }
   size = ftell(fp);
   if (size <= 0 || size > (16 * 1024 * 1024)) { fclose(fp); return 0; }
   rewind(fp);
   if (!(buf = (char *) malloc((size_t) size + 1))) { fclose(fp); return 0; }
   if (fread(buf, 1, (size_t) size, fp) != (size_t) size) {
      free(buf); fclose(fp); return 0;
   }
   buf[size] = '\0';
   fclose(fp);

   root = json_parse(buf, (size_t) size);
   free(buf);
   if (!root) return 0;
   if (root->type != json_type_object) { free(root); return 0; }
   o = (struct json_object_s *) root->payload;

   /* !! CLEAR THE STRUCT FIRST. The caller passes an uninitialized automatic
    * `struct char_file_u tmp_store;` and never clears it - load_char_ascii,
    * which this replaces, opens with exactly this memset (db.c:4005). Without
    * it every field the record does not carry is uninitialized STACK MEMORY:
    * a random wimpy for the 2900 records with no `wimpy`, a title that is not
    * NUL-terminated, garbage PLR flags and saving throws. It first showed up
    * as an explored count of 101.131% - bits ORed onto stack garbage.
    *
    * Done after the parse succeeds, so the documented contract holds: a
    * missing or unparseable record leaves `ch` untouched for the fallback. */
   memset(ch, 0, sizeof(struct char_file_u));

   /* char_player_data */
   if ((s = ts_str(o, "name")))        ts_copy(ch->name, s, sizeof(ch->name));
   if ((s = ts_str(o, "title")))       ts_copy(ch->title, s, sizeof(ch->title));
   if ((s = ts_str(o, "description"))) ts_copy(ch->description, s, sizeof(ch->description));
   /* The credential is stored as scrypt; tsauth verifies it at login. */
   if ((s = ts_str(o, "passwordHash"))) ts_copy(ch->pwd, s, sizeof(ch->pwd));

   ch->sex      = (byte)      ts_num(o, "sex", ch->sex);
   ch->race     = (sbyte)     ts_num(o, "race", ch->race);
   ch->class    = (byte)      ts_num(o, "charClass", ch->class);
   ch->level    = (ush_int)   ts_num(o, "level", ch->level);
   ch->hometown = (room_vnum) ts_num(o, "hometown", ch->hometown);
   ch->weight   = (ubyte)     ts_num(o, "weight", ch->weight);
   ch->height   = (ubyte)     ts_num(o, "height", ch->height);
   ch->played   = (long)      ts_num(o, "playedSeconds", ch->played);

   /* char_point_data */
   ch->points.hit       = (int) ts_num(o, "hp", ch->points.hit);
   ch->points.max_hit   = (int) ts_num(o, "maxHp", ch->points.max_hit);
   ch->points.mana      = (int) ts_num(o, "mana", ch->points.mana);
   ch->points.max_mana  = (int) ts_num(o, "maxMana", ch->points.max_mana);
   ch->points.move      = (int) ts_num(o, "move", ch->points.move);
   ch->points.max_move  = (int) ts_num(o, "maxMove", ch->points.max_move);
   /* !! gold and bank_gold are ARRAYS here - gold[5] (five coin types) and
    * bank_gold[32] (32 accounts) - while the TypeScript record carries a single
    * scalar for each. Index 0 is the primary in both: the importer reads
    * gold[0] and bank_gold[0] (parse-plr.ts), and utils.h:383 shows bank_gold[0]
    * as "Gold in Bank" on the score sheet. The other slots are left untouched
    * so a record that already has them keeps them. */
   ch->points.gold[0]      = (long) ts_num(o, "gold", ch->points.gold[0]);
   ch->points.bank_gold[0] = (long) ts_num(o, "goldBank", ch->points.bank_gold[0]);
   ch->points.exp          = (long) ts_num(o, "experience", ch->points.exp);

   /* char_ability_data. TS nests these under `stats`; fall back to the flat
    * spelling so a record written either way loads. */
   stats = ts_obj(o, "stats");
   if (stats) {
      ch->abilities.str      = (sbyte) ts_num(stats, "str", ch->abilities.str);
      ch->abilities.str_add  = (sbyte) ts_num(stats, "strAdd", ch->abilities.str_add);
      ch->abilities.intel    = (sbyte) ts_num(stats, "int", ch->abilities.intel);
      ch->abilities.wis      = (sbyte) ts_num(stats, "wis", ch->abilities.wis);
      ch->abilities.dex      = (sbyte) ts_num(stats, "dex", ch->abilities.dex);
      ch->abilities.con      = (sbyte) ts_num(stats, "con", ch->abilities.con);
      ch->abilities.cha      = (sbyte) ts_num(stats, "cha", ch->abilities.cha);
   } else {
      ch->abilities.str      = (sbyte) ts_num(o, "strength", ch->abilities.str);
      ch->abilities.intel    = (sbyte) ts_num(o, "intelligence", ch->abilities.intel);
      ch->abilities.wis      = (sbyte) ts_num(o, "wisdom", ch->abilities.wis);
      ch->abilities.dex      = (sbyte) ts_num(o, "dexterity", ch->abilities.dex);
      ch->abilities.con      = (sbyte) ts_num(o, "constitution", ch->abilities.con);
      ch->abilities.cha      = (sbyte) ts_num(o, "charisma", ch->abilities.cha);
   }

   /* char_special_data_saved.
    *
    * !! A bitvector goes through (long) on the way to bitvector_t. ts_num
    * returns a double, the record stores these SIGNED (a pref word reads
    * -276797712), and converting a negative double straight to an unsigned
    * type is undefined - via long it wraps to the intended bit pattern. */
   ch->char_specials_saved.idnum     = (long) ts_num(o, "idnum", ch->char_specials_saved.idnum);
   ch->char_specials_saved.alignment = (int)  ts_num(o, "alignment", ch->char_specials_saved.alignment);
   if (ts_find(o, "aff2"))
      ch->char_specials_saved.affected_by2 =
         (bitvector_t)(long) ts_num(o, "aff2", 0);
   if (ts_find(o, "aff3"))
      ch->char_specials_saved.affected_by3 =
         (bitvector_t)(long) ts_num(o, "aff3", 0);

   /* Skills and spells share the engine's flat arrays; proficiency maps to
    * skills[] and learn progress to skills_learn[]. Spells are applied after
    * skills so a colliding id resolves the same way the exporter resolves it. */
   ts_fill_skill_array(ts_obj(o, "skillProfs"),
                       ch->player_specials_saved.skills, MAX_SKILLS);
   ts_fill_skill_array(ts_obj(o, "spellProfs"),
                       ch->player_specials_saved.skills, MAX_SKILLS);
   ts_fill_skill_array(ts_obj(o, "skillsLearn"),
                       ch->player_specials_saved.skills_learn, MAX_SKILLS);
   ts_fill_skill_array(ts_obj(o, "spellsLearn"),
                       ch->player_specials_saved.skills_learn, MAX_SKILLS);
   ch->player_specials_saved.last_learnt =
      (int) ts_num(o, "lastSkillLearned", ch->player_specials_saved.last_learnt);

   ts_fill_affects(o, ch);

   /* player_special_data_saved */
   ch->player_specials_saved.load_room =
      (room_vnum) ts_num(o, "loadroom", ch->player_specials_saved.load_room);

   /* Preference words. The record carries them as legacyPrefBits[3], already
    * in this engine's own bit order - they came out of a pfile originally. */
   if (ts_find(o, "legacyPrefBits")) {
      ch->player_specials_saved.pref  =
         (bitvector_t)(long) ts_idx(o, "legacyPrefBits", 0, 0);
      ch->player_specials_saved.pref2 =
         (bitvector_t)(long) ts_idx(o, "legacyPrefBits", 1, 0);
      ch->player_specials_saved.pref3 =
         (bitvector_t)(long) ts_idx(o, "legacyPrefBits", 2, 0);
   }

   /* Drunk, full, thirsty. */
   if (ts_find(o, "conditions")) {
      int ci;
      for (ci = 0; ci < 3; ci++)
         ch->player_specials_saved.conditions[ci] =
            (sbyte) ts_idx(o, "conditions", (size_t) ci,
                           ch->player_specials_saved.conditions[ci]);
   }

   /* Counters the player sees on the score sheet and in whois. These persist
    * in the pfile too, so a character keeps them across an engine switch. */
   ch->player_specials_saved.wimp_level =
      (int) ts_num(o, "wimpy", ch->player_specials_saved.wimp_level);
   ch->player_specials_saved.old_mobkills =
      (int) ts_num(o, "mobKills", ch->player_specials_saved.old_mobkills);
   ch->player_specials_saved.pkills =
      (int) ts_num(o, "pKills", ch->player_specials_saved.pkills);
   ch->player_specials_saved.deaths =
      (int) ts_num(o, "deaths", ch->player_specials_saved.deaths);
   ch->player_specials_saved.q_points =
      (int) ts_num(o, "questPoints", ch->player_specials_saved.q_points);
   ch->player_specials_saved.learn_tic =
      (ubyte) ts_num(o, "learnTic", ch->player_specials_saved.learn_tic);
   ch->player_specials_saved.screensize =
      (int) ts_num(o, "screensize", ch->player_specials_saved.screensize);
   ch->player_specials_saved.board_number =
      (int) ts_num(o, "boardNumber", ch->player_specials_saved.board_number);
   ch->player_specials_saved.cl_rank =
      (int) ts_num(o, "clanRank", ch->player_specials_saved.cl_rank);

   /* Remort count is written under either name; timesRemorted is the one this
    * engine's field is named for, so it wins when both are present. */
   ch->player_specials_saved.times_remorted =
      (int) ts_num(o, "timesRemorted",
                   ts_num(o, "remortCount",
                          ch->player_specials_saved.times_remorted));

   if ((s = ts_str(o, "clanName")) != NULL)
      ts_copy(ch->player_specials_saved.cl_name, s,
              sizeof(ch->player_specials_saved.cl_name));

   /* Identity strings and timestamps. birthUnix is already epoch seconds;
    * lastLogin is an ISO string. */
   if ((s = ts_str(o, "title")) != NULL)
      ts_copy(ch->title, s, sizeof(ch->title));
   if ((s = ts_str(o, "description")) != NULL)
      ts_copy(ch->description, s, sizeof(ch->description));
   ch->birth      = (time_t) ts_num(o, "birthUnix", ch->birth);
   ch->last_logon = ts_time(o, "lastLogin", ch->last_logon);

   /* Explored rooms and identified items. tsplayer_load REPLACES
    * load_char_ascii, and load_char_ascii is what normally reads the two
    * sidecar bit-vector files, so without this a TS-backed character loses
    * their map ("Explored" on the score sheet) and every persistent identify.
    * store_to_char recomputes explored_total from these bits (db.c:4408). */
   ts_fill_bitfield(o, "exploredRooms",   ch->explored_vnums,
                    EXPLORED_TOP_VNUM, sizeof(ch->explored_vnums));
   ts_fill_bitfield(o, "identifiedItems", ch->known_vnums,
                    KNOWN_TOP_VNUM,    sizeof(ch->known_vnums));

   free(root);
   return 1;
}
