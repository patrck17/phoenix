/*
** dg_event.c: This file contains a simplified event system to allow
** DG Script triggers to use the "wait" command, causing a delay in the
** middle of a script.
**
** By: Mark A. Heilpern (Sammy @ eQuoria MUD   equoria.com:4000)
*/
#include "../localHeader/conf.h"
#include "../localHeader/sysdep.h"
#include "structs.h"
#include "utils.h"
#include "dg_event.h"
#include "buffer.h"
#include "queue.h"


/*
** Add an event to the current list
*/
struct queue_event *add_event(int time_delay, EVENT(*func), void *info)
{
   struct queue_event *this;

   this=add_function_to_queue(time_delay,NULL,0,1,func,info);
   return this;
}

void remove_event(struct queue_event *event)
{
   del_event_queue(event);
}

