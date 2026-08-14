/* capture_snapshot.c — record-replay whole-world snapshot dumper (T5).
   Test-observability only (no gameplay effect). Serializes the dynamic world
   state at capture start as a single JSON object matching the schema
   WorldSnapshot (test-bench/replay/schema.ts). The field set is v1 and
   DELIBERATELY incomplete — the M2 legacy self-replay oracle (T7) reveals what
   to add. Written to PHX_CAP_SNAPSHOT (default: <PHX_CAP_FILE dir>/snapshot.json). */

#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "handler.h"
#include "capture.h"

#include <stdlib.h>
#include "json.h"   /* single-header parser, already used by comm.c for GMCP */
#include <string.h>

/* Globals defined in db.c / comm.c / random.c not exported via headers. */
extern struct char_data *character_list;
extern struct obj_data  *object_list;
extern struct obj_data  *obj_proto;
extern struct index_data *mob_index;   /* GET_MOB_VNUM */
extern struct index_data *obj_index;   /* GET_OBJ_VNUM */
extern struct room_data *world;
extern struct zone_data *zone_table;
extern zone_rnum         top_of_zone_table;
/* DG instance-id allocator (db.c:50). Every char and object gets `max_id++` at
   creation (db.c:2945 PC / 3023 mob / 3039+3077 obj), so it is a per-instance
   identity that is STABLE for the instance's lifetime. Captured for O3: the refs
   below are m:<seq>/o:<seq>, indices into the pointer-ordered character_list /
   object_list, so a loader that recreates instances in its own order changes every
   ref and dump->load->dump can never be byte-identical. `id` is order-independent,
   which makes the round trip achievable. `max_id` itself must be restored too, or a
   loaded world re-issues ids that collide with the restored ones. */
extern long              max_id;
extern struct time_info_data time_info;
extern int               pulse;
extern long              dg_global_pulse;
extern unsigned long     circle_get_rng_state(void);
extern unsigned long     circle_rng_calls;
extern int               tics;   /* deadlock-watchdog counter (comm.c). The
   SIGVTALRM handler checkpointing() abort()s when `tics` stays 0 for one 180s
   virtual-CPU interval. The heartbeat bumps it — but the boot snapshot runs
   BEFORE the heartbeat starts, so we bump it here to avoid a false
   "Infinite Loop Suspected" abort on a large/slow dump. */

/* Hard bound on every list walk in the dump. A corrupt/cyclic character_list or
   object_list (a live-world hazard) would otherwise loop forever, burn the 180s
   virtual-CPU watchdog, and abort — this crash-looped the live deploys
   (2026-07-01). Real worlds are ~10-50k; 200k is generous and truncates a cycle
   fast. Override with PHX_CAP_MAX (testing). */
static long cap_list_cap(void)
{
  const char *e = getenv("PHX_CAP_MAX");
  long v = e ? atol(e) : 0;
  return (v > 0) ? v : 200000L;
}

/* ── pointer->seq index (O(1) refs) ─────────────────────────────────────────
   Without it ref_for_char/ref_for_obj do O(n) list scans → the whole dump is
   O(n^2), and on a CYCLIC list that is unbounded CPU (trips the watchdog). We
   index in ONE bounded pass, then look up O(1), so the dump stays O(n) and a
   cycle merely truncates. One table holds both chars and objects (pointers are
   unique): char value = NPC seq (>=0) or -2 (a PC → emit c:idnum); obj value =
   obj seq (>=0); absent (-1) = dangling/non-member → emit nothing. */
#define CAP_IX_SIZE (1 << 20)
#define CAP_IX_MASK (CAP_IX_SIZE - 1)
#define CAP_IX_PC   (-2)
static const void **cap_ix_k = NULL;
static int          *cap_ix_v = NULL;
static int           cap_ix_ready = 0;
static void cap_ix_reset(void)
{
  if (!cap_ix_k) { cap_ix_k = calloc(CAP_IX_SIZE, sizeof(*cap_ix_k)); cap_ix_v = calloc(CAP_IX_SIZE, sizeof(*cap_ix_v)); }
  cap_ix_ready = (cap_ix_k && cap_ix_v);
  if (cap_ix_ready) memset(cap_ix_k, 0, CAP_IX_SIZE * sizeof(*cap_ix_k));
}
static void cap_ix_put(const void *k, int v)
{
  unsigned h; long probes = 0;
  if (!cap_ix_ready || !k) return;
  h = (unsigned) (((unsigned long) k) >> 4) & CAP_IX_MASK;
  while (cap_ix_k[h]) { if (cap_ix_k[h] == k) return; h = (h + 1) & CAP_IX_MASK; if (++probes >= CAP_IX_SIZE) return; }
  cap_ix_k[h] = k; cap_ix_v[h] = v;
}
static int cap_ix_get(const void *k)
{
  unsigned h; long probes = 0;
  if (!cap_ix_ready || !k) return -1;
  h = (unsigned) (((unsigned long) k) >> 4) & CAP_IX_MASK;
  while (cap_ix_k[h]) { if (cap_ix_k[h] == k) return cap_ix_v[h]; h = (h + 1) & CAP_IX_MASK; if (++probes >= CAP_IX_SIZE) return -1; }
  return -1;
}
/* ── CANONICAL ORDER (`O3` step 2) ─────────────────────────────────────────
   The lists are walked in POINTER order, which is an artifact of allocation
   history. That makes both the emission order AND the `m:<seq>`/`o:<seq>` ref
   values arbitrary, so a loader that recreates instances in its own order
   produces a dump that is equivalent but not byte-identical — and byte-identical
   is O3's whole acceptance criterion.

   So order by `id` instead: unique per instance, assigned `max_id++` at creation,
   never reused. Sorting by it is total and deterministic. The sorted arrays are
   built ONCE here and reused by dump_world, so the index (which assigns seq) and
   the emission cannot disagree — if they did, refs would point at the wrong
   entries, which is far worse than an unstable order.

   Ref FORM is unchanged (`m:<seq>`, `o:<seq>`); only the values become canonical.
   Nothing compares refs across bundles — they wire relationships WITHIN one
   snapshot — so this is safe for existing consumers.

   Allocation failure degrades to the old pointer-order walk rather than aborting:
   a dump that is merely non-canonical is worth far more than no dump. */
static struct char_data **cap_sorted_chars = NULL;   /* NPCs and PCs, by id */
static long               cap_sorted_nchars = 0;
static struct obj_data  **cap_sorted_objs = NULL;
static long               cap_sorted_nobjs = 0;
/* Non-zero once the sort succeeded; dump_world falls back to the list walk when 0. */
static int                cap_order_ok = 0;

static int cap_cmp_char(const void *a, const void *b)
{
  long ia = (long) GET_ID(*(struct char_data * const *) a);
  long ib = (long) GET_ID(*(struct char_data * const *) b);
  return (ia > ib) - (ia < ib);
}
static int cap_cmp_obj(const void *a, const void *b)
{
  long ia = (long) GET_ID(*(struct obj_data * const *) a);
  long ib = (long) GET_ID(*(struct obj_data * const *) b);
  return (ia > ib) - (ia < ib);
}

/* Build the index from the current lists (bounded — a cycle stops at the cap). */
static void cap_ix_build(void)
{
  struct char_data *ch; struct obj_data *o;
  long n, cap = cap_list_cap(); int seq;
  cap_ix_reset();

  /* Free any previous run's arrays — cap_ix_build runs once per dump, and the
     segment-rotation path dumps more than once per process. */
  if (cap_sorted_chars) { free(cap_sorted_chars); cap_sorted_chars = NULL; }
  if (cap_sorted_objs)  { free(cap_sorted_objs);  cap_sorted_objs  = NULL; }
  cap_sorted_nchars = cap_sorted_nobjs = 0;
  cap_order_ok = 0;

  /* Count first (bounded), then collect — one pass each, no realloc. */
  for (ch = character_list, n = 0; ch && n < cap; ch = ch->next, n++) ;
  cap_sorted_nchars = n;
  for (o = object_list, n = 0; o && n < cap; o = o->next, n++) ;
  cap_sorted_nobjs = n;

  if (cap_sorted_nchars > 0)
    cap_sorted_chars = (struct char_data **) malloc(sizeof(*cap_sorted_chars) * (size_t) cap_sorted_nchars);
  if (cap_sorted_nobjs > 0)
    cap_sorted_objs = (struct obj_data **) malloc(sizeof(*cap_sorted_objs) * (size_t) cap_sorted_nobjs);

  if ((cap_sorted_nchars == 0 || cap_sorted_chars) && (cap_sorted_nobjs == 0 || cap_sorted_objs)) {
    for (ch = character_list, n = 0; ch && n < cap_sorted_nchars; ch = ch->next, n++) {
      cap_sorted_chars[n] = ch;
      if ((n & 0x3ff) == 0) tics = 1;
    }
    for (o = object_list, n = 0; o && n < cap_sorted_nobjs; o = o->next, n++) {
      cap_sorted_objs[n] = o;
      if ((n & 0x3ff) == 0) tics = 1;
    }
    if (cap_sorted_nchars > 1)
      qsort(cap_sorted_chars, (size_t) cap_sorted_nchars, sizeof(*cap_sorted_chars), cap_cmp_char);
    if (cap_sorted_nobjs > 1)
      qsort(cap_sorted_objs, (size_t) cap_sorted_nobjs, sizeof(*cap_sorted_objs), cap_cmp_obj);
    cap_order_ok = 1;
    tics = 1;
  } else {
    log("SYSERR: cap_dump_snapshot: canonical-order alloc failed — falling back to list order");
    if (cap_sorted_chars) { free(cap_sorted_chars); cap_sorted_chars = NULL; }
    if (cap_sorted_objs)  { free(cap_sorted_objs);  cap_sorted_objs  = NULL; }
    cap_sorted_nchars = cap_sorted_nobjs = 0;
  }

  /* Assign seq in the SAME order the emission will use, so refs match. */
  if (cap_order_ok) {
    for (n = 0, seq = 0; n < cap_sorted_nchars; n++) {
      ch = cap_sorted_chars[n];
      cap_ix_put(ch, IS_NPC(ch) ? seq : CAP_IX_PC);
      if (IS_NPC(ch)) seq++;
    }
    for (n = 0, seq = 0; n < cap_sorted_nobjs; n++) cap_ix_put(cap_sorted_objs[n], seq++);
  } else {
    for (ch = character_list, n = 0, seq = 0; ch && n < cap; ch = ch->next, n++) {
      cap_ix_put(ch, IS_NPC(ch) ? seq : CAP_IX_PC);
      if (IS_NPC(ch)) seq++;
      if ((n & 0x3ff) == 0) tics = 1;
    }
    for (o = object_list, n = 0, seq = 0; o && n < cap; o = o->next, n++) {
      cap_ix_put(o, seq++);
      if ((n & 0x3ff) == 0) tics = 1;
    }
  }
}

/* Test-only: PHX_CAP_TEST_CYCLE splices object_list into a cycle so the bound +
   watchdog handling can be validated red->green. Corrupts the list, so the
   process must be discarded after the dump. */
static void cap_test_inject_cycle(void)
{
  struct obj_data *p, *last = NULL; long i = 0;
  if (!getenv("PHX_CAP_TEST_CYCLE")) return;
  for (p = object_list; p && i < 100000L; p = p->next, i++) last = p;
  if (last && object_list) { last->next = object_list; log("SYSERR: PHX_CAP_TEST_CYCLE: injected object_list cycle (TEST ONLY)"); }
}

/* ── ref ids ───────────────────────────────────────────────────────────────
   c:<idnum> = player, m:<seq> = mob instance (index among NPCs in
   character_list), o:<seq> = object (index in object_list). O(n) per lookup —
   fine for a one-time boot dump. */
/* O(1) via the pre-built index. -1 = dangling/non-member (an object's freed
   owner, or a stale fighting/master link) → emit nothing, never deref garbage.
   CAP_IX_PC = the char is a live PC member → safe to read GET_IDNUM. */
static void ref_for_char(struct char_data *ch, char *buf, size_t n)
{
  int v;
  if (!ch) { buf[0] = '\0'; return; }
  v = cap_ix_get(ch);
  if (v == -1)          { buf[0] = '\0'; return; }
  if (v == CAP_IX_PC)   snprintf(buf, n, "c:%ld", (long) GET_IDNUM(ch));
  else                  snprintf(buf, n, "m:%d", v);
}

static void ref_for_obj(struct obj_data *o, char *buf, size_t n)
{
  int v;
  if (!o) { buf[0] = '\0'; return; }
  v = cap_ix_get(o);                 /* O(1); <0 = not indexed (dangling) */
  if (v < 0) { snprintf(buf, n, "o:?"); return; }
  snprintf(buf, n, "o:%d", v);
}

/* Emit "key":"<escaped>" */
static void emit_str(FILE *f, const char *key, const char *val)
{
  fprintf(f, "\"%s\":\"", key);
  cap_json_escape(f, val);
  fputc('"', f);
}

/* ── affects (shared by chars) ─────────────────────────────────────────────*/
static void dump_char_affects(FILE *f, struct char_data *ch)
{
  struct affected_type *af;
  int first = 1;
  fputs(",\"affects\":[", f);
  for (af = ch->affected; af; af = af->next) {
    fprintf(f, "%s{\"type\":%d,\"duration\":%d,\"modifier\":%ld,\"location\":%d,\"bitvector\":%ld}",
            first ? "" : ",", (int) af->type, (int) af->duration,
            (long) af->modifier, (int) af->location, (long) af->bitvector);
    first = 0;
  }
  fputc(']', f);
}

/* ── one character ─────────────────────────────────────────────────────────*/
static void dump_char(FILE *f, struct char_data *ch)
{
  char ref[32], tmp[32];
  int npc = IS_NPC(ch);

  ref_for_char(ch, ref, sizeof(ref));
  fputc('{', f);
  emit_str(f, "ref", ref);
  /* Order-independent instance identity — see the max_id extern above. */
  fprintf(f, ",\"id\":%ld", (long) GET_ID(ch));
  fprintf(f, ",\"isNpc\":%s", npc ? "true" : "false");
  if (npc) fprintf(f, ",\"vnum\":%d", (int) GET_MOB_VNUM(ch));
  else     fprintf(f, ",\"idnum\":%ld", (long) GET_IDNUM(ch));
  fputc(',', f); emit_str(f, "name", GET_NAME(ch));
  fprintf(f, ",\"inRoom\":%d", (int) (IN_ROOM(ch) == NOWHERE ? -1 : GET_ROOM_VNUM(IN_ROOM(ch))));
  fprintf(f, ",\"position\":%d", (int) GET_POS(ch));
  fprintf(f, ",\"hp\":%d,\"maxHp\":%d,\"mana\":%d,\"maxMana\":%d,\"move\":%d,\"maxMove\":%d",
          (int) GET_HIT(ch), (int) GET_MAX_HIT(ch), (int) GET_MANA(ch),
          (int) GET_MAX_MANA(ch), (int) GET_MOVE(ch), (int) GET_MAX_MOVE(ch));
  fprintf(f, ",\"level\":%d,\"exp\":%d,\"gold\":%d",
          (int) GET_LEVEL(ch), (int) GET_EXP(ch), (int) GET_GOLD(ch));
  /* identity (v3): race/class/sex/alignment drive combat math + skill access, so
     a mortal capture replays as its EXACT class/race, not a fixture. Available on
     both PCs and NPCs (mobs carry proto values). */
  fprintf(f, ",\"race\":%d,\"charClass\":%d,\"sex\":%d,\"alignment\":%d",
          (int) GET_RACE(ch), (int) GET_CLASS(ch), (int) GET_SEX(ch), (int) GET_ALIGNMENT(ch));
  fprintf(f, ",\"stats\":{\"str\":%d,\"strAdd\":%d,\"dex\":%d,\"con\":%d,\"int\":%d,\"wis\":%d,\"cha\":%d}",
          (int) GET_STR(ch), (int) GET_ADD(ch), (int) GET_DEX(ch), (int) GET_CON(ch),
          (int) GET_INT(ch), (int) GET_WIS(ch), (int) GET_CHA(ch));
  /* conditions + skills are PC-only (player_specials); NPCs have neither. */
  if (!npc) {
    int si, first_skill = 1;
    fprintf(f, ",\"conditions\":{\"drunk\":%d,\"full\":%d,\"thirst\":%d}",
            (int) GET_COND(ch, DRUNK), (int) GET_COND(ch, FULL), (int) GET_COND(ch, THIRST));
    /* skill/spell proficiency table — non-zero entries only (id -> prof). Covers
       both skills and spells (skills[] is indexed 1..MAX_SKILLS). */
    for (si = 1; si <= MAX_SKILLS; si++) {
      int prof = (int) GET_SKILL(ch, si);
      if (!prof) continue;
      if (first_skill) { fputs(",\"skills\":{", f); first_skill = 0; }
      else fputc(',', f);
      fprintf(f, "\"%d\":%d", si, prof);
    }
    if (!first_skill) fputc('}', f);
    /* P2 (2026-08-03): raw preference words + wimpy. Numeric on purpose —
       the TS observer emits composePrefBits() so the two engines compare
       bit-for-bit with no name table to drift. Order pinned between skills
       and waitState on BOTH producers (toInterchangeCharOrder). */
    fprintf(f, ",\"prefs\":[%ld,%ld,%ld]",
            (long) PRF_FLAGS(ch), (long) PRF2_FLAGS(ch),
            (long) ch->player_specials->saved.pref3);
    fprintf(f, ",\"wimpy\":%d", (int) GET_WIMP_LEV(ch));
  }
  fprintf(f, ",\"waitState\":%d", (int) GET_WAIT_STATE(ch));
  dump_char_affects(f, ch);
  /* Skip the field when the ref is empty (a dangling link ref_for_char refused
     to dereference) — keeps the JSON well-formed instead of a trailing comma. */
  if (FIGHTING(ch)) { ref_for_char(FIGHTING(ch), tmp, sizeof(tmp)); if (tmp[0]) { fputc(',', f); emit_str(f, "fighting", tmp); } }
  if (ch->master)   { ref_for_char(ch->master,   tmp, sizeof(tmp)); if (tmp[0]) { fputc(',', f); emit_str(f, "master", tmp); } }
  if (AFF_FLAGGED(ch, AFF_CHARM)) fputs(",\"charmed\":true", f);
  fputc('}', f);
}

/* ── one object ────────────────────────────────────────────────────────────*/
static void dump_obj(FILE *f, struct obj_data *o)
{
  char ref[32], tmp[32];
  int i, first;
  obj_rnum rn = GET_OBJ_RNUM(o);
  struct obj_data *proto = (rn >= 0) ? &obj_proto[rn] : NULL;

  ref_for_obj(o, ref, sizeof(ref));
  fputc('{', f);
  emit_str(f, "ref", ref);
  fprintf(f, ",\"id\":%ld", (long) GET_ID(o));
  fprintf(f, ",\"vnum\":%d", (int) GET_OBJ_VNUM(o));

  /* location — exactly one of. A dangling carried_by/worn_by (owner freed but
     not unlinked) yields an empty ref; emit no location rather than SEGV. */
  if (o->carried_by)      { ref_for_char(o->carried_by, tmp, sizeof(tmp)); if (tmp[0]) { fputc(',', f); emit_str(f, "carriedBy", tmp); } }
  else if (o->worn_by)    { ref_for_char(o->worn_by, tmp, sizeof(tmp));    if (tmp[0]) { fputc(',', f); emit_str(f, "wornBy", tmp);
                            fprintf(f, ",\"wornPos\":%d", (int) o->worn_on); } }
  else if (o->in_obj)     { ref_for_obj(o->in_obj, tmp, sizeof(tmp));      fputc(',', f); emit_str(f, "inObj", tmp); }
  else if (o->in_room != NOWHERE) fprintf(f, ",\"inRoom\":%d", (int) GET_ROOM_VNUM(o->in_room));

  fputs(",\"values\":[", f);
  for (i = 0; i < NUM_OBJ_VAL_POSITIONS; i++)
    fprintf(f, "%s%ld", i ? "," : "", (long) GET_OBJ_VAL(o, i));
  fputc(']', f);
  fprintf(f, ",\"timer\":%d", (int) GET_OBJ_TIMER(o));

  /* restring — only fields that differ from the prototype pointer */
  if (proto) {
    int any = 0;
    if (o->name != proto->name || o->short_description != proto->short_description ||
        o->description != proto->description || o->action_description != proto->action_description) {
      fputs(",\"restring\":{", f);
      if (o->name != proto->name)                           { emit_str(f, "name", o->name ? o->name : ""); any = 1; }
      if (o->short_description != proto->short_description) { if (any) fputc(',', f); emit_str(f, "shortDesc", o->short_description ? o->short_description : ""); any = 1; }
      if (o->description != proto->description)             { if (any) fputc(',', f); emit_str(f, "longDesc", o->description ? o->description : ""); any = 1; }
      if (o->action_description != proto->action_description){ if (any) fputc(',', f); emit_str(f, "detailedDesc", o->action_description ? o->action_description : ""); }
      fputc('}', f);
    }
  }

  /* obj affects (location/modifier pairs, non-zero only) */
  first = 1;
  for (i = 0; i < MAX_OBJ_AFFECT; i++) {
    if (o->affected[i].location == APPLY_NONE && o->affected[i].modifier == 0) continue;
    if (first) { fputs(",\"affects\":[", f); first = 0; }
    else fputc(',', f);
    fprintf(f, "{\"location\":%d,\"modifier\":%ld}", (int) o->affected[i].location, (long) o->affected[i].modifier);
  }
  if (!first) fputc(']', f);
  fputc('}', f);
}

/* ── rooms: dynamic deltas only (closed/locked doors, room affects) ─────────*/
static void dump_rooms(FILE *f)
{
  room_rnum rn;
  int first_room = 1;
  fputs("\"rooms\":[", f);
  for (rn = 0; rn <= top_of_world; rn++) {
    int d, has_door = 0, first_door;
    struct room_affected_type *ra;
    /* any closed/locked door? */
    for (d = 0; d < NUM_OF_DIRS; d++)
      if (world[rn].dir_option[d] && (world[rn].dir_option[d]->exit_info & (EX_CLOSED | EX_LOCKED))) { has_door = 1; break; }
    if (!has_door && !world[rn].affected) continue;

    if (!first_room) fputc(',', f);
    first_room = 0;
    fprintf(f, "{\"vnum\":%d", (int) world[rn].number);
    if (has_door) {
      first_door = 1;
      fputs(",\"doors\":[", f);
      for (d = 0; d < NUM_OF_DIRS; d++)
        if (world[rn].dir_option[d] && (world[rn].dir_option[d]->exit_info & (EX_CLOSED | EX_LOCKED))) {
          fprintf(f, "%s{\"dir\":%d,\"flags\":%d}", first_door ? "" : ",", d, (int) world[rn].dir_option[d]->exit_info);
          first_door = 0;
        }
      fputc(']', f);
    }
    if (world[rn].affected) {
      int first_aff = 1;
      fputs(",\"affects\":[", f);
      for (ra = world[rn].affected; ra; ra = ra->next) {
        fprintf(f, "%s{\"type\":%d,\"ticksLeft\":%d}", first_aff ? "" : ",", (int) ra->type, (int) ra->duration);
        first_aff = 0;
      }
      fputc(']', f);
    }
    fputc('}', f);
  }
  fputc(']', f);
}

/* ── derive the snapshot path ──────────────────────────────────────────────*/
static FILE *open_snapshot(void)
{
  const char *snap = getenv("PHX_CAP_SNAPSHOT");
  const char *jrnl;
  char path[1024];
  const char *slash;

  if (snap && *snap) return fopen(snap, "w");
  jrnl = getenv("PHX_CAP_FILE");
  if (!jrnl || !*jrnl) return NULL;
  slash = strrchr(jrnl, '/');
  if (slash) {
    size_t dirlen = (size_t) (slash - jrnl) + 1;
    if (dirlen >= sizeof(path)) return NULL;
    memcpy(path, jrnl, dirlen);
    snprintf(path + dirlen, sizeof(path) - dirlen, "snapshot.json");
  } else {
    snprintf(path, sizeof(path), "snapshot.json");
  }
  return fopen(path, "w");
}

/* ── write the whole-world snapshot to an open stream ──────────────────────*/
static void dump_world(FILE *f)
{
  struct char_data *ch;
  struct obj_data *o;
  zone_rnum z;
  int first;

  fputc('{', f);
  /* Build the O(1) ref index (bounded → cycle-safe) BEFORE dumping anything that
     emits refs. cap_test_inject_cycle is a no-op unless PHX_CAP_TEST_CYCLE. */
  cap_test_inject_cycle();
  cap_ix_build();
  fprintf(f, "\"pulse\":%d,\"dgGlobalPulse\":%ld,\"maxId\":%ld", pulse, dg_global_pulse, max_id);
  fprintf(f, ",\"time\":{\"hours\":%d,\"day\":%d,\"month\":%d,\"year\":%d}",
          time_info.hours, time_info.day, time_info.month, (int) time_info.year);
  fprintf(f, ",\"weather\":{\"sky\":%d,\"change\":%d,\"sunlight\":%d,\"pressure\":%d,\"moon\":%d}",
          weather_info.sky, weather_info.change, weather_info.sunlight,
          weather_info.pressure, weather_info.moon_phase);
  fprintf(f, ",\"rng\":{\"seed\":%lu,\"calls\":%lu}", circle_get_rng_state(), circle_rng_calls);

  /* zones */
  fputs(",\"zones\":[", f);
  for (z = 0; z <= top_of_zone_table; z++)
    fprintf(f, "%s{\"vnum\":%d,\"age\":%d,\"lifespan\":%d}", z ? "," : "",
            (int) zone_table[z].number, zone_table[z].age, zone_table[z].lifespan);
  fputc(']', f);

  /* rooms (dynamic deltas only) */
  fputc(',', f);
  dump_rooms(f);

  /* chars — bounded (cycle-safe) + tics bumped to satisfy the watchdog */
  fputs(",\"chars\":[", f);
  first = 1;
  if (cap_order_ok) {
    long nn;
    for (nn = 0; nn < cap_sorted_nchars; nn++) {
      if (!first) fputc(',', f);
      first = 0;
      dump_char(f, cap_sorted_chars[nn]);
      if ((nn & 0x3ff) == 0) tics = 1;
    }
  } else { long nn = 0, cap = cap_list_cap();
    for (ch = character_list; ch && nn < cap; ch = ch->next, nn++) {
      if (!first) fputc(',', f);
      first = 0;
      dump_char(f, ch);
      if ((nn & 0x3ff) == 0) tics = 1;
    }
    if (ch) log("SYSERR: cap_dump_snapshot: character_list hit cap %ld (cycle?) — truncated", cap);
  }
  fputc(']', f);

  /* objects — bounded (cycle-safe) + tics bumped */
  fputs(",\"objects\":[", f);
  first = 1;
  if (cap_order_ok) {
    long nn;
    for (nn = 0; nn < cap_sorted_nobjs; nn++) {
      if (!first) fputc(',', f);
      first = 0;
      dump_obj(f, cap_sorted_objs[nn]);
      if ((nn & 0x3ff) == 0) tics = 1;
    }
  } else { long nn = 0, cap = cap_list_cap();
    for (o = object_list; o && nn < cap; o = o->next, nn++) {
      if (!first) fputc(',', f);
      first = 0;
      dump_obj(f, o);
      if ((nn & 0x3ff) == 0) tics = 1;
    }
    if (o) log("SYSERR: cap_dump_snapshot: object_list hit cap %ld (cycle?) — truncated", cap);
  }
  fputc(']', f);

  fputs("}\n", f);
}

/* boot snapshot → PHX_CAP_SNAPSHOT (or sibling snapshot.json) */
void cap_dump_snapshot(void)
{
  FILE *f = open_snapshot();
  if (!f) return;
  dump_world(f);
  fclose(f);
}

/* ── segment snapshot path (reboot-resnapshot rotation) ─────────────────────
   snapshot.<seg>.json, placed in the same directory open_snapshot() uses (the
   PHX_CAP_SNAPSHOT override's dir if set, else the PHX_CAP_FILE dir). seg<=0
   defers to open_snapshot() (the boot/segment-0 file). */
static FILE *open_snapshot_seg(int seg)
{
  const char *snap = getenv("PHX_CAP_SNAPSHOT");
  const char *base;
  const char *slash;
  char path[1024];

  if (seg <= 0) return open_snapshot();

  base = (snap && *snap) ? snap : getenv("PHX_CAP_FILE");
  if (!base || !*base) return NULL;
  slash = strrchr(base, '/');
  if (slash) {
    size_t dirlen = (size_t) (slash - base) + 1;
    if (dirlen + 32 >= sizeof(path)) return NULL;
    memcpy(path, base, dirlen);
    snprintf(path + dirlen, sizeof(path) - dirlen, "snapshot.%d.json", seg);
  } else {
    snprintf(path, sizeof(path), "snapshot.%d.json", seg);
  }
  return fopen(path, "w");
}

/* Reboot-resnapshot rotation: dump the reloaded world for `seg`. Reuses the boot
   dump_world() writer verbatim (the dangling-pointer/cyclic-list SEGV fix lives
   there — do NOT duplicate it). Logs + returns on open failure (no crash). */
void cap_dump_snapshot_seg(int seg)
{
  FILE *f = open_snapshot_seg(seg);
  if (!f) { log("SYSERR: cap_dump_snapshot_seg: cannot open snapshot for segment %d", seg); return; }
  dump_world(f);
  fclose(f);
}

/* end-of-run snapshot → PHX_CAP_SNAPSHOT_END (no-op if unset). Used by the
   bench self-replay oracle to compare final world state across runs. */
void cap_snapshot_end(void)
{
  const char *path = getenv("PHX_CAP_SNAPSHOT_END");
  FILE *f;
  if (!path || !*path) return;
  f = fopen(path, "w");
  if (!f) return;
  dump_world(f);
  fclose(f);
}

/* ── login/logout journal entries ──────────────────────────────────────────
   A player entering the game emits a 'connect' carrying the loaded char's full
   state (so replay can inject it at this pulse); logout emits 'disconnect'.
   Written to the live journal stream (cap_stream), pulse-tagged like cmd. */
/* ── P2 (2026-08-03): the connect entry's `objs` payload ────────────────────
   One owned object as a connect-entry record. Twin of the TS
   connectObjStatesOf (snapshot.ts) — the two must emit the same SHAPE and the
   same WALK: worn slots ascending, then carried list order, each item followed
   depth-first by its container contents.

   Refs are minted PER ENTRY (o:1, o:2, …), NOT via ref_for_obj: the connect
   hook has no snapshot-local index (cap_ix is only valid inside a dump walk;
   reusing it here emits "o:?" for every item). A consumer resolves inObj by
   ref-equality WITHIN the entry. Owner refs are the PC's c:<idnum>.
   No "id" field on purpose — the TS twin has no numeric ids to mint, and the
   consumer keys on ref/vnum/values, never on id. */
static void dump_connect_obj(FILE *f, struct char_data *ch, struct obj_data *o,
                             int myref, int wornpos, int parentref)
{
  int i, first;
  obj_rnum rn = GET_OBJ_RNUM(o);
  struct obj_data *proto = (rn >= 0) ? &obj_proto[rn] : NULL;

  fputc('{', f);
  fprintf(f, "\"ref\":\"o:%d\"", myref);
  fprintf(f, ",\"vnum\":%d", (int) GET_OBJ_VNUM(o));
  if (wornpos >= 0)
    fprintf(f, ",\"wornBy\":\"c:%ld\",\"wornPos\":%d", (long) GET_IDNUM(ch), wornpos);
  else if (parentref >= 0)
    fprintf(f, ",\"inObj\":\"o:%d\"", parentref);
  else
    fprintf(f, ",\"carriedBy\":\"c:%ld\"", (long) GET_IDNUM(ch));
  fputs(",\"values\":[", f);
  for (i = 0; i < NUM_OBJ_VAL_POSITIONS; i++)
    fprintf(f, "%s%ld", i ? "," : "", (long) GET_OBJ_VAL(o, i));
  fputc(']', f);
  fprintf(f, ",\"timer\":%d", (int) GET_OBJ_TIMER(o));
  if (proto) {
    int any = 0;
    if (o->name != proto->name || o->short_description != proto->short_description ||
        o->description != proto->description || o->action_description != proto->action_description) {
      fputs(",\"restring\":{", f);
      if (o->name != proto->name)                           { emit_str(f, "name", o->name ? o->name : ""); any = 1; }
      if (o->short_description != proto->short_description) { if (any) fputc(',', f); emit_str(f, "shortDesc", o->short_description ? o->short_description : ""); any = 1; }
      if (o->description != proto->description)             { if (any) fputc(',', f); emit_str(f, "longDesc", o->description ? o->description : ""); any = 1; }
      if (o->action_description != proto->action_description){ if (any) fputc(',', f); emit_str(f, "detailedDesc", o->action_description ? o->action_description : ""); }
      fputc('}', f);
    }
  }
  first = 1;
  for (i = 0; i < MAX_OBJ_AFFECT; i++) {
    if (o->affected[i].location == APPLY_NONE && o->affected[i].modifier == 0) continue;
    if (first) { fputs(",\"affects\":[", f); first = 0; }
    else fputc(',', f);
    fprintf(f, "{\"location\":%d,\"modifier\":%ld}", (int) o->affected[i].location, (long) o->affected[i].modifier);
  }
  if (!first) fputc(']', f);
  fputc('}', f);
}

/* Emit o and its contents depth-first; returns the next free per-entry ref. */
static int dump_connect_obj_tree(FILE *f, struct char_data *ch, struct obj_data *o,
                                 int wornpos, int parentref, int nextref, int *first)
{
  int myref = nextref++;
  struct obj_data *k;
  if (!*first) fputc(',', f);
  *first = 0;
  dump_connect_obj(f, ch, o, myref, wornpos, parentref);
  for (k = o->contains; k; k = k->next_content)
    nextref = dump_connect_obj_tree(f, ch, k, -1, myref, nextref, first);
  return nextref;
}

void cap_connect(struct char_data *ch)
{
  FILE *f;
  cap_maybe_rotate();       /* rotate BEFORE fetching the stream (may change it) */
  f = cap_stream();
  if (!f) return;
  fprintf(f, "{\"pulse\":%d,\"t\":\"connect\",\"actor\":\"c:%ld\",\"name\":\"", pulse, (long) GET_IDNUM(ch));
  cap_json_escape(f, GET_NAME(ch));
  fputs("\",\"char\":", f);
  dump_char(f, ch);   /* PCs only at this hook (dump_char detects isNpc) */
  /* P2: the char's full gear rides in the connect entry — before this the
     record was a naked char, and a replayed TS PC had NO items while legacy's
     replay pfile-loaded a full kit (the measured output-axis residue). */
  fputs(",\"objs\":[", f);
  {
    int i, first = 1, nextref = 1;
    struct obj_data *o;
    for (i = 0; i < NUM_WEARS; i++)
      if (GET_EQ(ch, i))
        nextref = dump_connect_obj_tree(f, ch, GET_EQ(ch, i), i, -1, nextref, &first);
    for (o = ch->carrying; o; o = o->next_content)
      nextref = dump_connect_obj_tree(f, ch, o, -1, -1, nextref, &first);
  }
  fputc(']', f);
  fputs("}\n", f);
  fflush(f);
}

void cap_disconnect(struct char_data *ch)
{
  FILE *f;
  cap_maybe_rotate();       /* rotate BEFORE fetching the stream (may change it) */
  f = cap_stream();
  if (!f) return;
  fprintf(f, "{\"pulse\":%d,\"t\":\"disconnect\",\"actor\":\"c:%ld\"}\n", pulse, (long) GET_IDNUM(ch));
  fflush(f);
}

/* ══ LOADER (`O3` step 4a — scalars + zones) ═══════════════════════════════
 *
 * The inverse of dump_world, kept in the same file deliberately: a serializer
 * and its parser drift apart the moment they live apart, and this pair has to
 * stay byte-reciprocal for the acceptance test to mean anything.
 *
 * WHAT THIS INCREMENT COVERS. Scalars (pulse, time, weather, maxId) and the zone
 * age/lifespan table. NOT chars or objects yet — those are the large part, and
 * landing them together with the scalars would make a failure ambiguous between
 * "the scalar path is wrong" and "the instance path is wrong". Each increment
 * turns one section of the round-trip diff green.
 *
 * `max_id` matters more than it looks: restore the instances without it and the
 * next mob created re-issues an id an existing instance already holds, so refs
 * collide and the world quietly corrupts. It is restored here, ahead of the
 * instance work that will need it.
 *
 * WHY IT HOOKS cap_init(). That runs after boot_db and immediately before the
 * boot snapshot (capture.c:45), so `PHX_CAP_LOAD=<file>` yields exactly the
 * round trip the acceptance test wants: boot → load → dump, compared against the
 * dump that produced the file.
 *
 * FAILURE IS ALWAYS NON-FATAL. A load that cannot parse logs and returns, leaving
 * the freshly booted world untouched. A half-applied world is far worse than an
 * unloaded one — it would look like a successful load and diff as engine drift. */

extern struct weather_data weather_info;

/* Fetch a numeric member, or `dflt` when absent/not a number. */
static double cap_obj_num(struct json_object_s *o, const char *key, double dflt)
{
  struct json_object_element_s *e;
  if (!o) return dflt;
  for (e = o->start; e; e = e->next) {
    if (strcmp(e->name->string, key) != 0) continue;
    if (e->value->type != json_type_number) return dflt;
    return atof(((struct json_number_s *) e->value->payload)->number);
  }
  return dflt;
}

/* Fetch an object member, or NULL. */
static struct json_object_s *cap_obj_obj(struct json_object_s *o, const char *key)
{
  struct json_object_element_s *e;
  if (!o) return NULL;
  for (e = o->start; e; e = e->next) {
    if (strcmp(e->name->string, key) != 0) continue;
    if (e->value->type != json_type_object) return NULL;
    return (struct json_object_s *) e->value->payload;
  }
  return NULL;
}

/* Fetch an array member, or NULL. */
static struct json_array_s *cap_obj_arr(struct json_object_s *o, const char *key)
{
  struct json_object_element_s *e;
  if (!o) return NULL;
  for (e = o->start; e; e = e->next) {
    if (strcmp(e->name->string, key) != 0) continue;
    if (e->value->type != json_type_array) return NULL;
    return (struct json_array_s *) e->value->payload;
  }
  return NULL;
}

void cap_load_snapshot(const char *path)
{
  FILE *fp;
  long len;
  char *buf;
  struct json_value_s *root;
  struct json_object_s *top, *sub;
  struct json_array_s *zones;
  struct json_array_element_s *ze;
  int nzone = 0;
  /* FUNCTION SCOPE on purpose: the OBJECT pass resolves `carriedBy`/`wornBy`
     against these, so the char array must outlive the char block. Freed once at
     the end. Getting this wrong is a use-after-free, not a wrong number. */
  struct char_data **by_seq = NULL;
  int nseq = 0, seq_cap = 0;

  if (!path || !*path) return;
  if (!(fp = fopen(path, "rb"))) {
    log("SYSERR: cap_load_snapshot: cannot open %s", path);
    return;
  }
  fseek(fp, 0, SEEK_END); len = ftell(fp); fseek(fp, 0, SEEK_SET);
  if (len <= 0) { fclose(fp); log("SYSERR: cap_load_snapshot: %s is empty", path); return; }
  if (!(buf = (char *) malloc((size_t) len + 1))) { fclose(fp); log("SYSERR: cap_load_snapshot: OOM"); return; }
  if (fread(buf, 1, (size_t) len, fp) != (size_t) len) {
    fclose(fp); free(buf); log("SYSERR: cap_load_snapshot: short read on %s", path); return;
  }
  buf[len] = '\0';
  fclose(fp);

  root = json_parse(buf, (size_t) len);
  free(buf);
  if (!root || root->type != json_type_object) {
    if (root) free(root);
    log("SYSERR: cap_load_snapshot: %s is not a JSON object", path);
    return;
  }
  top = (struct json_object_s *) root->payload;

  /* ── scalars ── */
  pulse   = (int)  cap_obj_num(top, "pulse", pulse);
  max_id  = (long) cap_obj_num(top, "maxId", (double) max_id);

  if ((sub = cap_obj_obj(top, "time"))) {
    time_info.hours = (int) cap_obj_num(sub, "hours", time_info.hours);
    time_info.day   = (int) cap_obj_num(sub, "day",   time_info.day);
    time_info.month = (int) cap_obj_num(sub, "month", time_info.month);
    time_info.year  = (int) cap_obj_num(sub, "year",  time_info.year);
  }
  if ((sub = cap_obj_obj(top, "weather"))) {
    weather_info.sky      = (int) cap_obj_num(sub, "sky",      weather_info.sky);
    weather_info.change   = (int) cap_obj_num(sub, "change",   weather_info.change);
    weather_info.sunlight = (int) cap_obj_num(sub, "sunlight", weather_info.sunlight);
    weather_info.pressure = (int) cap_obj_num(sub, "pressure", weather_info.pressure);
    /* The dump's "moon" is moon_phase, NOT moonlight (see dump_world) — the two
       are separate fields in weather_data and confusing them would restore a
       phase into a light level. Omitting it entirely was this loader's first
       bug: sky/change/sunlight/pressure all restored, the section still failed
       the round trip, and the diff pointed straight at the one field left out. */
    weather_info.moon_phase = (int) cap_obj_num(sub, "moon", weather_info.moon_phase);
  }

  /* ── zones: match by VNUM, never by index ──
     zone_table's order is a boot-time artifact of the index file. Matching by
     position would silently mis-assign every age the moment a zone is added or
     the index reordered — the same class as the pointer-ordered refs step 2
     removed. */
  if ((zones = cap_obj_arr(top, "zones"))) {
    for (ze = zones->start; ze; ze = ze->next) {
      struct json_object_s *z;
      int vnum, age, life;
      zone_rnum i;
      if (ze->value->type != json_type_object) continue;
      z = (struct json_object_s *) ze->value->payload;
      vnum = (int) cap_obj_num(z, "vnum", -1);
      if (vnum < 0) continue;
      age  = (int) cap_obj_num(z, "age", -1);
      life = (int) cap_obj_num(z, "lifespan", -1);
      for (i = 0; i <= top_of_zone_table; i++) {
        if ((int) zone_table[i].number != vnum) continue;
        if (age  >= 0) zone_table[i].age = age;
        if (life >= 0) zone_table[i].lifespan = life;
        nzone++;
        break;
      }
    }
  }

  /* ── chars (`O3` step 4b — MOBS) ──
     The boot snapshot contains NO PCs: cap_init runs before anyone can log in,
     and a capture of a live world carries its PCs in the journal's connect
     entries instead. Measured on a real dump: 10,884 chars, 0 PCs. So "load the
     chars" means "rebuild the mob population", which is tractable; PCs would
     need pfile composition and are a separate problem.

     PURGE FIRST. boot_db has already run reset_zone for every zone, so the world
     is fully populated with a DIFFERENT mob set than the snapshot's. Loading on
     top would double the population. Extract every NPC, then rebuild.

     `id` is restored from the snapshot rather than left as read_mobile's fresh
     max_id++ — that is the whole point of step 1. Refs are `m:<seq>` by id rank,
     so a mob carrying the wrong id lands at the wrong rank and every ref in the
     re-dump shifts. Note the order: max_id was restored above, and each
     read_mobile call bumps it, so it is re-pinned after the rebuild. */
  {
    struct json_array_s *chars = cap_obj_arr(top, "chars");
    struct char_data *ch, *next_ch;
    long saved_max_id = max_id;
    int nload = 0, nfail = 0;
    /* seq → the mob we created for it, so the second pass can resolve the
       `m:<seq>` refs. Creation order IS seq order: step 2 sorts the dump by id
       and ref_for_char numbers NPCs by that same rank, so the Nth NPC read here
       is `m:N`. Keeping the array rather than re-deriving is what makes the
       resolution O(1) and, more importantly, impossible to get subtly wrong. */

    if (chars) {
      for (ch = character_list; ch; ch = next_ch) {
        next_ch = ch->next;
        if (IS_NPC(ch)) extract_char(ch);
      }
      for (ze = chars->start; ze; ze = ze->next) {
        struct json_object_s *c;
        struct char_data *mob;
        int vnum, room;
        room_rnum rrnum;
        if (ze->value->type != json_type_object) continue;
        c = (struct json_object_s *) ze->value->payload;
        /* PCs carry "idnum" instead of "vnum" — skipped by construction, and the
           count below makes it visible if a capture ever contains one. */
        vnum = (int) cap_obj_num(c, "vnum", -1);
        if (vnum < 0) { nfail++; continue; }
        room = (int) cap_obj_num(c, "inRoom", -1);
        rrnum = (room >= 0) ? real_room(room) : NOWHERE;
        if (rrnum == NOWHERE) { nfail++; continue; }
        if (!(mob = read_mobile(vnum, VIRTUAL))) { nfail++; continue; }
        char_to_room(mob, rrnum);

        /* ID ONLY here. Every other field is restored in the FINAL pass, after
           the objects are placed — see the ordering note there. */
        GET_ID(mob) = (long) cap_obj_num(c, "id", (double) GET_ID(mob));
        if (nseq >= seq_cap) {
          int ncap = seq_cap ? seq_cap * 2 : 4096;
          struct char_data **grown = (struct char_data **) realloc(by_seq, sizeof(*by_seq) * (size_t) ncap);
          /* Out of memory here costs only the relational pass — the mobs
             themselves are already correct — so degrade rather than abort. */
          if (grown) { by_seq = grown; seq_cap = ncap; }
        }
        if (nseq < seq_cap) by_seq[nseq] = mob;
        nseq++;
        nload++;
      }

      /* ── SECOND PASS: relational fields (`O3` step 4c) ──
         `master`, `fighting` and `charmed` are POINTERS between instances, so
         they cannot be set while the instances are still being created — the
         target of a forward reference does not exist yet. Hence a second pass
         over the same array, once every mob is in place.
         Measured on a real dump: 233 followers, 2 fighting pairs, 3 charmed. The
         round trip found this by diffing to `chars[733].master` — the mob
         rebuild was otherwise byte-perfect at 10,883 of 10,884. */
      {
        struct json_array_element_s *e2;
        int idx = 0, nfollow = 0, nfight = 0;
        for (e2 = chars->start; e2 && idx < nseq; e2 = e2->next, idx++) {
          struct json_object_s *c2;
          struct json_object_element_s *f;
          struct char_data *me = (idx < seq_cap) ? by_seq[idx] : NULL;
          if (!me || e2->value->type != json_type_object) continue;
          c2 = (struct json_object_s *) e2->value->payload;
          for (f = c2->start; f; f = f->next) {
            const char *ref;
            int tgt;
            if (f->value->type != json_type_string) continue;
            ref = ((struct json_string_s *) f->value->payload)->string;
            /* Only `m:<seq>` resolves. A `c:<idnum>` ref points at a PC, which a
               boot snapshot never contains — skipping it is correct, not a gap. */
            if (!ref || ref[0] != 'm' || ref[1] != ':') continue;
            tgt = atoi(ref + 2);
            if (tgt < 0 || tgt >= nseq || tgt >= seq_cap || !by_seq[tgt]) continue;
            if (strcmp(f->name->string, "master") == 0) {
              add_follower(me, by_seq[tgt]);
              nfollow++;
            } else if (strcmp(f->name->string, "fighting") == 0) {
              set_fighting(me, by_seq[tgt]);
              nfight++;
            }
          }
        }
        log("cap_load_snapshot: relinked %d follower(s), %d fight(s)", nfollow, nfight);
      }
      /* Re-pin: every read_mobile above consumed a fresh id. */
      max_id = saved_max_id;
      log("cap_load_snapshot: rebuilt %d mob(s), %d skipped", nload, nfail);
    }
  }

  /* ── objects (`O3` step 4d) ──
     Same two-pass shape as the chars, and for a sharper reason: an object's
     placement can point at ANOTHER OBJECT (`inObj`), so containers and contents
     cannot be resolved in one sweep — a forward reference's target does not exist
     yet. Real dump: 4,369 worn · 2,510 in rooms · 2,100 carried · 1,274 nested.
     Nesting is arbitrary depth, so ordering tricks would not save a single pass.

     Pass 1 CREATES every object and restores its own state; pass 2 PLACES each
     one, by which time every possible target exists. */
  {
    struct json_array_s *objs = cap_obj_arr(top, "objects");
    struct obj_data **obj_seq = NULL;
    int nobj = 0, ocap = 0, ofail = 0, nplaced = 0, ghosts = 0;
    long saved_max_id2 = max_id;

    if (objs) {
      /* Purge boot_db's zone-reset population.
         NOT a walk with a saved `next` — that is a USE-AFTER-FREE here, and it
         was this loader's crash. `extract_obj` recursively extracts a container's
         CONTENTS (obj->contains), and those objects are in `object_list` too, so
         the `next_o` saved before the call can already be freed when the loop
         reaches it. The chars loop above is safe from this because extract_char
         does not cascade into other CHARS.
         Extracting the head repeatedly is correct by construction: each call
         removes at least that object, so the list strictly shrinks. */
      while (object_list) extract_obj(object_list);

      for (ze = objs->start; ze; ze = ze->next) {
        struct json_object_s *oj;
        struct obj_data *no;
        int vnum;
        if (ze->value->type != json_type_object) continue;
        oj = (struct json_object_s *) ze->value->payload;
        vnum = (int) cap_obj_num(oj, "vnum", -1);
        /* vnum < 0 is a MINTED object (money, corpses — create_money/make_corpse
           give them no prototype). read_object cannot rebuild one, and a boot
           snapshot should contain none; counted rather than silently dropped. */
        if (vnum < 0 || !(no = read_object(vnum, VIRTUAL))) { ofail++; continue; }

        /* ID ONLY — timer and values are restored AFTER placement, for the same
           reason the char fields are: placement MUTATES them. See below. */
        GET_ID(no) = (long) cap_obj_num(oj, "id", (double) GET_ID(no));
        if (nobj >= ocap) {
          int ncap = ocap ? ocap * 2 : 4096;
          struct obj_data **grown = (struct obj_data **) realloc(obj_seq, sizeof(*obj_seq) * (size_t) ncap);
          if (grown) { obj_seq = grown; ocap = ncap; }
        }
        if (nobj < ocap) obj_seq[nobj] = no;
        nobj++;
      }

      /* PASS 2 — placement. Every target now exists, char or object. */
      {
        struct json_array_element_s *e3;
        int idx = 0;
        for (e3 = objs->start; e3 && idx < nobj; e3 = e3->next, idx++) {
          struct json_object_s *oj;
          struct json_object_element_s *f;
          struct obj_data *me = (idx < ocap) ? obj_seq[idx] : NULL;
          int room = -1, worn = -1;
          const char *carried = NULL, *wornby = NULL, *inobj = NULL;
          if (!me || e3->value->type != json_type_object) continue;
          oj = (struct json_object_s *) e3->value->payload;
          for (f = oj->start; f; f = f->next) {
            if (f->value->type == json_type_number) {
              if (!strcmp(f->name->string, "inRoom"))
                room = (int) atof(((struct json_number_s *) f->value->payload)->number);
              else if (!strcmp(f->name->string, "wornPos"))
                worn = (int) atof(((struct json_number_s *) f->value->payload)->number);
            } else if (f->value->type == json_type_string) {
              const char *v = ((struct json_string_s *) f->value->payload)->string;
              if (!strcmp(f->name->string, "carriedBy")) carried = v;
              else if (!strcmp(f->name->string, "wornBy")) wornby = v;
              else if (!strcmp(f->name->string, "inObj"))  inobj = v;
            }
          }
          /* Exactly one placement, in the dump's own precedence order
             (carriedBy, then wornBy, then inObj, then inRoom — dump_obj). */
          if (carried && carried[0] == 'm') {
            int t = atoi(carried + 2);
            if (t >= 0 && t < nseq && t < seq_cap && by_seq[t])
              { obj_to_char(me, by_seq[t]); nplaced++; }
          } else if (wornby && wornby[0] == 'm' && worn >= 0) {
            int t = atoi(wornby + 2);
            if (t >= 0 && t < nseq && t < seq_cap && by_seq[t])
              { equip_char(by_seq[t], me, worn); nplaced++; }
          } else if (inobj && inobj[0] == 'o') {
            int t = atoi(inobj + 2);
            if (t >= 0 && t < nobj && t < ocap && obj_seq[t])
              { obj_to_obj(me, obj_seq[t]); nplaced++; }
          } else if (room >= 0) {
            room_rnum rr = real_room(room);
            if (rr != NOWHERE) { obj_to_room(me, rr); nplaced++; }
          } else if ((carried && carried[0] == 'c') || (wornby && wornby[0] == 'c')) {
            /* Owned by a PC, and PCs are not loaded (they need pfile
               composition — a separate problem). Without this the object is
               CREATED and never placed: it sits in object_list with no room and
               no owner, so it dumps but exists nowhere in the world. Measured on
               a real mature capture: 65 such ghosts from 3 skipped PCs.
               Extract it — an object whose owner does not exist should not
               either. */
            extract_obj(me);
            ghosts++;
          }
        }
      }
      /* ── object fields, AFTER placement ──
         PLACEMENT MUTATES THE VERY FIELDS WE ARE RESTORING, which the round trip
         caught on 1,810 of 10,253 objects:
           values[5] — obj_to_obj ACCUMULATES each item's weight into its
                       container's contents-weight (handler.c:1473). The snapshot
                       already holds the total, so restoring first and then
                       placing counts every content twice (measured 6 → 12).
           timer     — obj_to_room re-derives it (the IS_CORPSE branch,
                       handler.c:1360-1371), turning a captured -1 into
                       ITEM_DECAY 168.
         The same shape as the char stats double-applying equipment affects, and
         the same fix: place first, then overwrite with the captured truth. */
      {
        struct json_array_element_s *e5;
        int idx2 = 0;
        for (e5 = objs->start; e5 && idx2 < nobj; e5 = e5->next, idx2++) {
          struct json_object_s *oj2;
          struct json_array_s *vals2;
          struct obj_data *me2 = (idx2 < ocap) ? obj_seq[idx2] : NULL;
          if (!me2 || e5->value->type != json_type_object) continue;
          oj2 = (struct json_object_s *) e5->value->payload;
          GET_OBJ_TIMER(me2) = (int) cap_obj_num(oj2, "timer", GET_OBJ_TIMER(me2));
          if ((vals2 = cap_obj_arr(oj2, "values"))) {
            struct json_array_element_s *ve2;
            int vi2;
            for (ve2 = vals2->start, vi2 = 0; ve2 && vi2 < 8; ve2 = ve2->next, vi2++) {
              if (ve2->value->type == json_type_number)
                GET_OBJ_VAL(me2, vi2) = (long) atof(((struct json_number_s *) ve2->value->payload)->number);
            }
          }
        }
      }
      if (obj_seq) free(obj_seq);
      max_id = saved_max_id2;
      log("cap_load_snapshot: rebuilt %d object(s), placed %d, %d skipped, %d PC-owned ghost(s) dropped", nobj, nplaced, ofail, ghosts);
    }
  }

  /* ── FINAL PASS: char fields, AFTER the objects are on ──
     ORDERING IS THE WHOLE POINT, and getting it wrong is subtle. `equip_char`
     APPLIES a worn item's affects — APPLY_STR and friends land in `aff_abils`,
     which is what GET_STR reads and what the dump emits. The snapshot's stats
     therefore ALREADY include every worn item's contribution. Restoring them
     before equipping and then equipping applies each bonus a SECOND time.
     Measured: chars[0].str came back 11 where the source had 12, con 18 vs 20,
     cha 17 vs 13 — a re-roll's worth of drift that looked like the loader
     ignoring stats entirely.
     So: create mobs (id only) → create objects → place objects → restore char
     fields last, overwriting whatever equipping computed with the captured
     truth. Same reason hp/mana/move come last: gear affects those too. */
  {
    struct json_array_s *chars2 = cap_obj_arr(top, "chars");
    struct json_array_element_s *e4;
    int idx = 0;
    if (chars2 && by_seq) {
      for (e4 = chars2->start; e4 && idx < nseq; e4 = e4->next, idx++) {
        struct json_object_s *c;
        struct char_data *mob = (idx < seq_cap) ? by_seq[idx] : NULL;
        if (!mob || e4->value->type != json_type_object) continue;
        c = (struct json_object_s *) e4->value->payload;
        GET_HIT(mob)       = (int)  cap_obj_num(c, "hp",       GET_HIT(mob));
        GET_MAX_HIT(mob)   = (int)  cap_obj_num(c, "maxHp",    GET_MAX_HIT(mob));
        GET_MANA(mob)      = (int)  cap_obj_num(c, "mana",     GET_MANA(mob));
        GET_MAX_MANA(mob)  = (int)  cap_obj_num(c, "maxMana",  GET_MAX_MANA(mob));
        GET_MOVE(mob)      = (int)  cap_obj_num(c, "move",     GET_MOVE(mob));
        GET_MAX_MOVE(mob)  = (int)  cap_obj_num(c, "maxMove",  GET_MAX_MOVE(mob));
        GET_LEVEL(mob)     = (int)  cap_obj_num(c, "level",    GET_LEVEL(mob));
        GET_EXP(mob)       = (int)  cap_obj_num(c, "exp",      GET_EXP(mob));
        GET_GOLD(mob)      = (int)  cap_obj_num(c, "gold",     GET_GOLD(mob));
        GET_ALIGNMENT(mob) = (int)  cap_obj_num(c, "alignment", GET_ALIGNMENT(mob));
        GET_POS(mob)       = (int)  cap_obj_num(c, "position", GET_POS(mob));
        GET_SEX(mob)       = (int)  cap_obj_num(c, "sex",      GET_SEX(mob));
        GET_RACE(mob)      = (int)  cap_obj_num(c, "race",     GET_RACE(mob));
        GET_CLASS(mob)     = (int)  cap_obj_num(c, "charClass", GET_CLASS(mob));
        GET_WAIT_STATE(mob) = (int) cap_obj_num(c, "waitState", GET_WAIT_STATE(mob));
        if ((sub = cap_obj_obj(c, "stats"))) {
          GET_STR(mob) = (int) cap_obj_num(sub, "str", GET_STR(mob));
          GET_ADD(mob) = (int) cap_obj_num(sub, "strAdd", GET_ADD(mob));
          GET_DEX(mob) = (int) cap_obj_num(sub, "dex", GET_DEX(mob));
          GET_CON(mob) = (int) cap_obj_num(sub, "con", GET_CON(mob));
          GET_INT(mob) = (int) cap_obj_num(sub, "int", GET_INT(mob));
          GET_WIS(mob) = (int) cap_obj_num(sub, "wis", GET_WIS(mob));
          GET_CHA(mob) = (int) cap_obj_num(sub, "cha", GET_CHA(mob));
        }
      }
    }
  }

  if (by_seq) free(by_seq);
  free(root);
  log("cap_load_snapshot: %s — pulse=%d maxId=%ld zones=%d", path, pulse, max_id, nzone);
}
