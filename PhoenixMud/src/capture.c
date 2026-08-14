/* capture.c — record-replay journal writer (record-replay M1 / T4).
   Self-contained, test-observability only (no gameplay effect). Appends
   pulse-tagged JSONL JournalEntry lines (matching test-bench/replay/schema.ts)
   to the file named by PHX_CAP_FILE. Disabled (no-op) when that env is unset. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "capture.h"

extern int pulse;                 /* heartbeat counter (comm.c) */
extern void log(const char *format, ...);  /* mud logger (utils.c); capture.c is
   deliberately header-light, so declare rather than pull in utils.h. */

/* The ~10h heartbeat rollover (comm.c:1548: `if (pulse >= 600*60*PASSES_PER_SEC)
   pulse = 0;`) resets `pulse` to ~0 from ~360k for numerical hygiene, WITHOUT
   restarting the process. This is NOT a reboot — the world is CONTINUOUS across
   it — but the pulse-tagged journal timeline WRAPS, which the replay importer
   would otherwise misread as a server reboot and truncate. We RE-ANCHOR instead:
   detect the wrap mechanism-agnostically as `pulse` jumping BACKWARD by more than
   this guard (a normal step is +1; benign jitter is tiny) and rotate to a fresh
   segment (new snapshot.<N>.json of the current world + new journal.<N>.jsonl) so
   each ~10h window is an independently replayable unit — which also bounds any
   replay drift by re-anchoring to ground-truth world state. */
#ifndef REBOOT_PULSE_DROP
#define REBOOT_PULSE_DROP 10000
#endif

static FILE *cap_fp = NULL;
static int   cap_last_pulse = -1; /* last pulse a writer observed (-1 = none yet) */
static int   cap_segment = 0;     /* current segment index (0 = boot) */

int cap_active(void) { return cap_fp != NULL; }

/* The open journal stream (NULL if inactive). Lets capture_snapshot.c write
   connect/disconnect entries into the same journal. */
FILE *cap_stream(void) { return cap_fp; }

void cap_init(void)
{
  const char *path = getenv("PHX_CAP_FILE");
  /* `O3`: restore BEFORE the boot dump, so the snapshot written below describes
     the LOADED world rather than the freshly booted one. That ordering is what
     makes `boot → load → dump` a round trip at all. Independent of PHX_CAP_FILE
     so a load can be exercised without capturing. */
  cap_load_snapshot(getenv("PHX_CAP_LOAD"));
  if (path && *path) {
    cap_fp = fopen(path, "a");
    /* Snapshot the whole world at capture start (after boot), so the journal
       from here on replays against it. */
    if (cap_fp) cap_dump_snapshot();
  }
}

/* Derive a segment-indexed sibling of `base` by inserting ".<seg>" before the
   filename extension: ".../journal.jsonl" + seg 2 -> ".../journal.2.jsonl". No
   extension -> append ".<seg>". Same dir-parsing style as open_snapshot().
   out[0] = '\0' on failure (unusable base / buffer too small). */
static void cap_seg_path(const char *base, int seg, char *out, size_t n)
{
  const char *slash, *fname, *dot;
  size_t headlen;

  if (n) out[0] = '\0';
  if (!base || !*base || !n) return;
  slash = strrchr(base, '/');
  fname = slash ? slash + 1 : base;
  dot   = strrchr(fname, '.');            /* extension within the filename only */
  if (dot) {
    headlen = (size_t) (dot - base);      /* base up to (not incl.) the dot */
    if (headlen + 32 >= n) return;
    snprintf(out, n, "%.*s.%d%s", (int) headlen, base, seg, dot);
  } else {
    snprintf(out, n, "%s.%d", base, seg);
  }
}

/* Detect the pulse reset (world reload / 10h rollover) and rotate to a fresh
   segment so each ~10h window is an independently replayable unit. Called at the
   TOP of every journal writer (cap_cmd/cap_event/cap_connect/cap_disconnect),
   BEFORE it writes — so a rotation's snapshot lands before the segment's first
   journal entry. Owns cap_last_pulse (initialized on first call). Crash-safe: on
   any open failure it logs a SYSERR and leaves capture inactive (cap_fp = NULL)
   rather than crashing or writing to a stale stream. */
void cap_maybe_rotate(void)
{
  int p;

  if (!cap_fp) return;                    /* capture inactive → nothing to do */
  p = pulse;

  if (cap_last_pulse >= 0 && p < cap_last_pulse - REBOOT_PULSE_DROP) {
    const char *base = getenv("PHX_CAP_FILE");
    char path[1024];

    /* (a) seal the current segment's journal with a seam marker, then close it.
       NOTE: no cap_snapshot_end() here — that is only for cap_close() at process
       end; a reboot rotation is mid-life. */
    fprintf(cap_fp, "{\"pulse\":%d,\"t\":\"reboot\",\"segment\":%d}\n", p, cap_segment + 1);
    fflush(cap_fp);
    fclose(cap_fp);
    cap_fp = NULL;

    cap_segment++;

    /* (b) dump the reloaded world to THIS segment's snapshot FIRST, so the
       snapshot precedes the segment's journal entries. Its own file — does not
       touch cap_fp. */
    cap_dump_snapshot_seg(cap_segment);

    /* (c) open the new segment journal (fresh, "w"). */
    cap_seg_path(base, cap_segment, path, sizeof(path));
    if (!path[0]) {
      log("SYSERR: cap rotate: cannot derive segment %d journal path from PHX_CAP_FILE — capture now inactive", cap_segment);
      cap_last_pulse = p;
      return;                             /* cap_fp stays NULL → inactive */
    }
    cap_fp = fopen(path, "w");
    if (!cap_fp) {
      log("SYSERR: cap rotate: cannot open segment journal %s — capture now inactive", path);
      cap_last_pulse = p;
      return;
    }
    log("cap: rotated to segment %d at pulse %d (world reload / rollover) -> %s", cap_segment, p, path);
  }

  cap_last_pulse = p;                     /* remember where we are (init here) */
}

void cap_close(void)
{
  if (cap_fp) {
    cap_snapshot_end();   /* final world state for the self-replay oracle */
    fclose(cap_fp);
    cap_fp = NULL;
  }
}

/* Append a JSON-escaped string body (without the surrounding quotes) to fp. */
void cap_json_escape(FILE *fp, const char *s)
{
  if (!s) return;
  for (; *s; s++) {
    unsigned char c = (unsigned char) *s;
    switch (c) {
      case '"':  fputs("\\\"", fp); break;
      case '\\': fputs("\\\\", fp); break;
      case '\n': fputs("\\n", fp);  break;
      case '\r': fputs("\\r", fp);  break;
      case '\t': fputs("\\t", fp);  break;
      default:
        if (c < 0x20) fprintf(fp, "\\u%04x", c);
        else          fputc(c, fp);
    }
  }
}

/* Convenience wrapper for the journal helpers (always writes to cap_fp). */
static void cap_json_str(const char *s) { cap_json_escape(cap_fp, s); }

void cap_cmd(long idnum, const char *raw)
{
  cap_maybe_rotate();
  if (!cap_fp) return;
  fprintf(cap_fp, "{\"pulse\":%d,\"t\":\"cmd\",\"actor\":\"c:%ld\",\"raw\":\"", pulse, idnum);
  cap_json_str(raw);
  fputs("\"}\n", cap_fp);
  fflush(cap_fp);
}

/* O2b: page-turn input — see capture.h. Hooked at comm.c's show_string
   branch, BEFORE the pager consumes the line. */
void cap_pager(long idnum, const char *raw)
{
  cap_maybe_rotate();
  if (!cap_fp) return;
  fprintf(cap_fp, "{\"pulse\":%d,\"t\":\"pager\",\"actor\":\"c:%ld\",\"raw\":\"", pulse, idnum);
  cap_json_str(raw);
  fputs("\"}\n", cap_fp);
  fflush(cap_fp);
}

void cap_event(const char *type, long idnum, const char *name)
{
  cap_maybe_rotate();
  if (!cap_fp) return;
  fprintf(cap_fp, "{\"pulse\":%d,\"t\":\"%s\",\"actor\":\"c:%ld\"", pulse, type, idnum);
  if (name) { fputs(",\"name\":\"", cap_fp); cap_json_str(name); fputs("\"", cap_fp); }
  fputs("}\n", cap_fp);
  fflush(cap_fp);
}
