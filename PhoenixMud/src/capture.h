/* capture.h — record-replay journal + snapshot writer (T4/T5). Test
   observability only; no gameplay effect. Gated by the PHX_CAP_FILE env var. */
#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdio.h>

void cap_init(void);
void cap_close(void);
int  cap_active(void);
FILE *cap_stream(void);
void cap_cmd(long idnum, const char *raw);
/* O2b (2026-08-03): a PAGER-consumed input line (page turn). While
   d->showstr_count is set, comm.c routes input to show_string and it never
   reaches command_interpreter — so cap_cmd cannot see it, and before this
   kind existed page-turns simply vanished from the journal (a replayed pager
   sat waiting on input that never came). Distinct kind, same shape as cmd. */
void cap_pager(long idnum, const char *raw);
void cap_event(const char *type, long idnum, const char *name);

/* T?/login capture: a player entering the game emits a 'connect' journal entry
   carrying the loaded character's full state; logout emits 'disconnect'. These
   let the bench replay mid-session logins (inject the char at its connect
   pulse). Implemented in capture_snapshot.c (needs the game structs). */
struct char_data;
void cap_connect(struct char_data *ch);
void cap_disconnect(struct char_data *ch);

/* JSON-escape a string body (no surrounding quotes) to fp. Shared by the
   journal (capture.c) and the snapshot dumper (capture_snapshot.c). */
void cap_json_escape(FILE *fp, const char *s);

/* T5: dump the whole-world dynamic snapshot (schema WorldSnapshot) to the file
   named by PHX_CAP_SNAPSHOT (default <PHX_CAP_FILE dir>/snapshot.json). Called
   once at capture start (cap_init), after the world has booted. */
void cap_dump_snapshot(void);

/* `O3` step 4 — restore a world from a snapshot written by cap_dump_snapshot.
   Driven by PHX_CAP_LOAD and applied in cap_init() AFTER boot_db and BEFORE the
   boot dump, so `boot → load → dump` reproduces the source snapshot. Non-fatal
   on any error: a half-applied world is worse than an unloaded one, because it
   looks like success and diffs as engine drift. */
void cap_load_snapshot(const char *path);

/* Reboot-resnapshot rotation (2026-07-07): dump the whole-world snapshot for
   segment `seg`. seg<=0 → the boot path (snapshot.json, == cap_dump_snapshot);
   seg>0 → the segment-indexed sibling snapshot.<seg>.json of PHX_CAP_FILE. Reuses
   the same dump_world() writer as the boot snapshot. */
void cap_dump_snapshot_seg(int seg);

/* Reboot-resnapshot rotation: detect the heartbeat `pulse` jumping backward (a
   world reload / the ~10h rollover) and rotate to a fresh segment. Called at the
   TOP of every journal writer, before it writes. No-op when capture is inactive
   or no reset is seen. Implemented in capture.c. */
void cap_maybe_rotate(void);

/* T7: end-of-run snapshot → PHX_CAP_SNAPSHOT_END (no-op if unset). Called from
   cap_close so the final world state is captured for the self-replay oracle. */
void cap_snapshot_end(void);

#endif
