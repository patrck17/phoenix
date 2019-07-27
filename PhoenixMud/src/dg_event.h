/*
** how often will heartbeat() call our event function?
*/
#define PULSE_DG_EVENT 1


/*
** macro used to prototype the callback function for an event
*/
#define EVENT(function) void (function)(void *info)


/*
** define event related structures

struct event_info {
  int time_remaining;
  EVENT(*func);
  void *info;
  struct event_info *next;  
};
*/

/*
** prototype event functions
*/
struct queue_event *add_event(int time_delay, EVENT(*func), void *info);
void remove_event(struct queue_event *event);

