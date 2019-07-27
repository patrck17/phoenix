#if !defined(_BUFFER_H_)
#define _BUFFER_H_

/*
 * CONFIGURABLES
 * -------------
 */

/*
 * 1 = use buffer system for all memory allocations using CREATE().
 * 0 = use standard calloc/realloc in the CREATE() macro.
 *
 * The advantage to using the buffer system is that it will keep track of
 * all your allocations and warn if one of the malloc buffers is overflowed.
 * You can also view every allocation, what file and line it was allocated
 * from, and how large it was.  This would be useful to detect a memory leak.
 * Using this option, stock CircleMUD bpl12 takes 1.8 seconds to boot on my
 * Pentium 133, and 1.6 seconds to boot without it.
 */
#define BUFFER_MEMORY	0

/*
 * 1 = use a threaded buffer system. (You must have pthreads.)
 * 0 = use the standard heartbeat() method.
 */
#define THREADED	0

/*
 * 1 = Include original CircleMUD buffers too.
 * 0 = Use only new buffer system.
 *
 * This will helpfully point out all your existing global buffer uses if you
 * decide to convert to all buffer system.
 */
#define USE_CIRCLE_BUFFERS 0

/*
 * *** No tweakables below! ***
 */

/*
 * Handle GCC-isms.
 */
#if !defined(__GNUC__)
#define __attribute__(x)
#define __FUNCTION__	__FILE__
#endif

/*
 * Config dependencies.
 */
#if 0 /* BUFFER_SNPRINTF == 1 */
typedef struct buf_data buffer;
#else
typedef char buffer;
#endif

/*
 * Some macros to imitate C++ class styles. release_buffer() automatically
 * NULL's a pointer to prevent further use. CREATE() is preferred over g_m()
 */
#define get_buffer(a)		acquire_buffer((a), BT_STACK, NULL, __FUNCTION__, __LINE__)
#define get_memory(a)		acquire_buffer((a), BT_MALLOC, NULL, __FUNCTION__, __LINE__)
#define release_buffer(a)	do { detach_buffer((a), BT_STACK, __FUNCTION__, __LINE__); (a) = NULL; } while(0)
#define release_memory(a)	do { detach_buffer((a), BT_MALLOC, __FUNCTION__, __LINE__); (a) = NULL; } while(0)
#define release_my_buffers()	detach_my_buffers(__FUNCTION__, __LINE__)

/*
 * Types for the memory to allocate.
 */
#define BT_STACK	0	/* Stack type memory.			*/
#define BT_PERSIST	1	/* A buffer that doesn't time out.	*/
#define BT_MALLOC	2	/* A malloc() memory tracker.		*/

/*
 * How often to scan the buffer list for expirations.
 */
/*#define PULSE_BUFFER	(5 RL_SEC)*/
#if THREADED
/*
 * Assorted lock types.
 */
#define LOCK_NONE		0
#define LOCK_ACQUIRE		1
#define LOCK_WILL_CLEAR		2
#define LOCK_WILL_FREE		4
#define LOCK_WILL_REMOVE	8
#endif

/*
 * Public functions for outside use.
 */
#if 0 /* BUFFER_SNPRINTF */
buffer *str_cpy(buffer *d, buffer*s);
int bprintf(buffer *buf, const char *format, ...);
#endif
#if BUFFER_MEMORY
void *debug_calloc(size_t count, size_t size, const char *var, const char *func, int line);
void *debug_realloc(void *ptr, size_t size, const char *var, const char *func, int line);
void debug_free(void *ptr, const char *func, ush_int line);
char *debug_str_dup(const char *txt, const char *var, const char *func, ush_int line);
#endif
void init_buffers(void);
void exit_buffers(void);
void release_all_buffers(void);
struct buf_data *detach_buffer(buffer *data, byte type, const char *func, const int line_n);
void detach_my_buffers(const char *func, const int line_n);
buffer *acquire_buffer(size_t size, int type, const char *var, const char *who, ush_int line);
void show_buffers(struct char_data *ch, int buffer_type, int display_type);
extern int buffer_cache_stat[];
#define BUFFER_CACHE_HITS	0
#define BUFFER_CACHE_MISSES	1

struct buf_data {
   byte magic;		/* Have we been trashed?		*/
#if THREADED
   byte locked;		/* Don't touch this item.		*/
#endif
   byte type;		/* What type of buffer are we?		*/
   ush_int line;        /* What source code line is using this. */
   ush_int oline;
   size_t req_size;	/* How much did the function request?	*/
   size_t oreq_size;
   union {
      sh_int life;	/* An idle counter to free unused ones.	(B) */
      const char *var;	/* Name of variable allocated to.	(M) */
   } var_life;
   size_t size;          /* How large is this buffer?		*/
   const char *who;      /* Name of the function using this.     */
   const char *old;	 /* name of who was using this */
   char *data;           /* The buffer passed back to functions. */
   struct buf_data *next;	/* Next structure.		*/
};

#endif
