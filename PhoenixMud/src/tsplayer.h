/* tsplayer.h  -  load a TypeScript-platform player record into char_file_u.
 *
 * The TS platform stores each character as world/players/<name>.json, a flat
 * object of typed fields. This engine stores etc/players_ascii/<L>/<Name>, a
 * POSITIONAL text file. The two carry the same character; only the encoding
 * differs.
 *
 * Reading the JSON directly lets a deployment pull the state repository and
 * run, with no conversion step: the same shape the TypeScript engine boots
 * from.
 *
 * ADDITIVE by design. load_char_ascii stays the default; the JSON path is
 * taken only when a record exists for that name, so an existing lib keeps
 * working unchanged and this engine remains bootable at every step.
 *
 * Parsing uses json.h, already vendored and in use by capture_snapshot.c.
 *
 * !! Keep every byte of this file ASCII. A latin-1 write of a non-ASCII
 * character truncates the file to zero bytes, and the build then fails with a
 * misleading "undefined reference" message.
 */

#ifndef TSPLAYER_H
#define TSPLAYER_H

struct char_file_u;

/* Is there a TypeScript record for `name`? Cheap existence probe: callers use
 * it to choose a loader without paying for a parse. */
int tsplayer_available(const char *name);

/* Fill `ch` from world/players/<name>.json.
 *
 * Returns 1 on success, 0 if the record is missing or unparseable - in which
 * case `ch` is untouched and the caller should fall back to load_char_ascii.
 *
 * Fields with no TypeScript counterpart keep whatever the caller had; fields
 * the engine recomputes at load are not written here. */
int tsplayer_load(struct char_file_u *ch, const char *name);

/* Load equipment and inventory for `ch` from its TS record. Returns 1 when a
 * record was read (even if it held no objects), 0 when there is none - the
 * caller then falls back to Crash_load. */
struct char_data;
int tsplayer_load_objects(struct char_data *ch, const char *name);

/* Directory holding the TypeScript player records. Defaults to
 * "world/players"; override with PHOENIX_TS_PLAYERS for a stage. */
const char *tsplayer_dir(void);

#endif /* TSPLAYER_H */
