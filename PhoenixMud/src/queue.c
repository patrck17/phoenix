/*************************************************************************** 
 *  File: queue.c                                       Part of PhoenixMUD * 
 *  Usage: Event driven code                                               * 
 *                                                                         * 
 *  All rights reserved.  See license.doc for complete information.        * 
 *                                                                         * 
 *  Created by Robert Jon Mudry. I've kept the copyright intact, however,  * 
 *  I do request my name stay here. This code is not guaranteed to work.   * 
 *  December 24, 1995                                                      * 
 *                                                                         * 
 *  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               * 
 *  PhoenixMUD is based on CircleMUD, Copyright (C) 1996-98.               *
 ***************************************************************************/ 
 
/* 
** All functions are heavily commented. READ THEM! For more information 
** on the structures and flags, see the comments in queue.h 
*/ 

#include <stdarg.h> 
#include "../localHeader/conf.h" 
#include "../localHeader/sysdep.h" 
 
#include "structs.h" 
#include "buffer.h" 
#include "utils.h" 
#include "db.h" 
#include "limits.h" 
#include "interpreter.h" 
#include "comm.h" 
#include "spells.h" 
#include "handler.h" 
#include "dg_event.h"
#include "dg_scripts.h"
#include "queue.h" 
 
void MEV_update(void);
void teleport(int room_to, int room_in, struct char_data *ch);
void do_auction_update(void);
void trig_wait_event(void *info);
void stun_by_spell(struct char_data *ch,int iPhase);
void skin_update(struct char_data *pxChar,
		 struct obj_data *pxCarcass,
		 int iState);
void dig_update(struct char_data *pxChar,int iState);
void process_skills(struct char_data *ch, struct char_data *tch, struct obj_data *item,
                    int t_alt, int skill, int state);

/* The command_queue linked list is where all the queue events link to */ 
struct queue_event *command_queue = NULL; 
/* The queue entry process_event_queue() is firing, else NULL.  free_char()
 * checks it so it never deletes the entry process_event_queue() is about to
 * delete on the same trip through the loop. */
struct queue_event *current_processing_event = NULL; 
/* These are for is_same_and_conn() */ 
extern struct descriptor_data *descriptor_list; 
extern struct char_data  *character_list; 
int num_events=0;
long total_event_time=0;
extern struct room_data *world; 
extern struct index_data *mob_index; 
extern struct index_data *obj_index; 
extern struct index_data **trig_index;

/* 
** queue_time_to_string() takes the supplied mybuf pointer and 
** the qtime from the event and copies the time left to live in 
** mybuf. It also returns a pointer to mybuf. qtime is the absolute 
** time in the form returned by the system time() function, as 
** seconds since January 1, 1970. This could be more effecient, but 
** I don't think it really matters here. 
*/ 
char *queue_time_to_string(char *mybuf, time_t qtime) 
{ 
   time_t time_left; 
 
   *mybuf='\0'; 
   time_left = qtime - time(NULL); 
 
   if (time_left / (3600 * 24)) 
      sprintf(mybuf, "%ldd",time_left/(3600*24)); 
   if ((time_left % (3600 * 24))/3600) 
      sprintf(mybuf, "%s%ldh",mybuf,(time_left%(3600*24))/3600); 
   if (((time_left % (3600 * 24))%3600)/60) 
      sprintf(mybuf, "%s%ldm",mybuf,((time_left%(3600*24))%3600)/60); 
   sprintf(mybuf, "%s%lds",mybuf,((time_left%(3600*24))%3600)%60); 
 
   return mybuf; 
} 
 
/* 
** get_queue_event_size() figures out the exact size in bytes a 
** queue event is taking up. Returns size in bytes, given a pointer 
** to the event 
*/ 
long get_queue_event_size(struct queue_event *event) 
{ 
  /* 
   * qsize: size of event in bytes 
   * i:  loop index 
   */ 
   long qsize=0, i; 
 
   qsize = (long)sizeof(struct queue_event); 
   if (event->command) 
      qsize += (long)sizeof(event->command); 
   for (i=0;i<10;i++) 
      if (event->args[i]) 
	 qsize += (long)sizeof(event->args[i]); 
 
   return qsize; 
} 
 
/* 
** do_show_queue() is the workhorse of the MUD "ps" command, much like 
** the Unix command of the same name. 
** Options: -s   quick summary 
**   -t [min][,max]  limit to events executing within 
**   -c [character]  events which depend on <character> 
** N/I  -f <type>  flagged as (command, function, etc) 
** N/I  -l   long listing (detailed) 
**      -q <queueid>  a certain queue id # 
*/ 
ACMD(do_show_queue) 
{ 
  /* 
   * mybuf: used for queue_time_to_string() 
   * sflags: will be used to show flags later 
   * tmpq: used to transverse the linked list of the queue 
   * in_queue: number of commands in queue 
   * in_queue_one...more: commands in queue for exec in 1,5,15,+ mins 
   * queue_in_bytes: size of queue in bytes 
   * min,max_time: time to exec range 
   * ptr:   just a temp pointer 
   * chdepend:  character depend option 
   * is_chdepend:  flag for chdepend option 
   * is_qid:  Queue ID to search for 
   */ 
   struct queue_event *tmpq; 
   char *mybuf, *sflags,*buf; 
   long in_queue=0, in_queue_one=0, in_queue_five=0, in_queue_fifteen=0; 
   long in_queue_more=0, queue_in_bytes=0; 
   int full_listing=1; /* Default full listing */ 
   long min_time=0, max_time=0; /* Time to exec range */ 
   struct char_data *chdepend=NULL; 
   int is_chdepend=0;  
   long is_qid=-1; 
   char *ptr; 
 
 /* local structures */
   struct wait_event_data {
      trig_data *trigger;
      void *go;
      int type;
   };

   if (IS_NPC(ch)) 
      return; 
   
   mybuf=get_buffer(256);
  /* Parse arguments */ 
   if (strstr(argument, "-s")) /* summary mode */ 
      { 
      full_listing=0; 
      } 
   if ((ptr = strstr(argument, "-t"))) /* time range */ 
      { 
      ptr+=2; 
      if (ptr) 
	 { 
	 min_time=atol(ptr); 
	 while (isdigit((int)*(++ptr))); 
	 if (*ptr == ',') 
	    { 
	    ptr++; 
	    max_time = atol(ptr); 
	    } 
	 } 
      } 
   if ((ptr = strstr(argument, "-c"))) /* character depend */ 
      { 
      is_chdepend=1; 
      ptr+=2; 
      while (isspace((int)*(++ptr))); 
      if (*ptr == '-' || !*ptr) /* Really, no char */ 
	 { 
	 chdepend = NULL; 
	 } 
      else 
	 if ( !(chdepend = get_char(ptr)) ) 
	    { 
	    send_to_char(ch, "I can't find that character!\r\n"); 
	    release_buffer(mybuf);
	    return; 
	    } 
      } 
   if ((ptr = strstr(argument, "-q"))) /* queue id */ 
      { 
      ptr+=2; 
      is_qid = atol(ptr); 
      } 
 
   send_to_char(ch, "Commands in queue:\r\n"); 
   if (min_time || max_time) 
      { 
      send_to_char(ch, "\tRange: %s", 
	      queue_time_to_string(mybuf,time(NULL)+min_time)); 
      send_to_char(ch, " %s%s\r\n", 
		   (max_time?"- ":""), 
		   (max_time? queue_time_to_string(mybuf,time(NULL)+max_time):"+")); 
      } 
   if (is_chdepend) 
      { 
      if (!chdepend) 
	 { 
	 send_to_char(ch, "\tChar: *NONE*\r\n"); 
	 } 
      else 
	 { 
	 send_to_char(ch, "\tChar: %s\r\n", GET_NAME(chdepend)); 
	 } 
      } 
   if (is_qid > -1) 
      { 
      send_to_char(ch, "\tQueue ID: %6.6ld\r\n", is_qid); 
      } 
 
   buf=get_buffer(MAX_STRING_LENGTH);
   sflags=get_buffer(256);
   for(tmpq = command_queue;tmpq;tmpq=tmpq->next) 
      { 
      *sflags = '\0'; 
      in_queue++; 
      queue_in_bytes += get_queue_event_size(tmpq); 
      if ((tmpq->time - time(NULL)) > 900) 
	 in_queue_more++; 
      else 
	 if ((tmpq->time - time(NULL)) > 300) 
	    in_queue_fifteen++; 
	 else 
	    if ((tmpq->time - time(NULL)) > 60) 
	       in_queue_five++; 
	    else 
	       in_queue_one++; 
 
      sprintf(buf, "[%6.6ld] ", tmpq->queue_id); 
 
      if (IS_SET(tmpq->flags, QUE_COMMAND)) 
	 { 
	 sprintf(buf+strlen(buf), "[CMND] %s\tCh: %s\tCmd: %s\r\n", 
		 queue_time_to_string(mybuf, tmpq->time), 
		 GET_NAME(tmpq->ch), tmpq->command); 
	 } 
      else 
	 if (IS_SET(tmpq->flags, QUE_MOBPROG)) 
	    { 
	   /* Currently this is not implemented */ 
	    } 
	 else 
	    if (IS_SET(tmpq->flags, QUE_FUNCTION)) 
	       { 
	       if(tmpq->function==MEV_update)
		  sprintf(buf+strlen(buf), "[ TIC] %s%s%s\r\n", 
			  queue_time_to_string(mybuf, tmpq->time), 
			  tmpq->ch ? "\tCh: " : "", 
			  tmpq->ch ? GET_NAME(tmpq->ch) : ""); 
	       else if(tmpq->function==teleport)
		  sprintf(buf+strlen(buf), "[TELE] %s%s%s\r\n", 
			  queue_time_to_string(mybuf, tmpq->time), 
			  tmpq->ch ? "\tCh: " : "", 
			  tmpq->ch ? GET_NAME(tmpq->ch) : ""); 
	       else if(tmpq->function==do_auction_update)
		  sprintf(buf+strlen(buf), "[AUCT] %s%s%s\r\n", 
			  queue_time_to_string(mybuf, tmpq->time), 
			  tmpq->ch ? "\tCh: " : "", 
			  tmpq->ch ? GET_NAME(tmpq->ch) : ""); 
	       else if(tmpq->function==trig_wait_event)
		  {
		  char *buf1=get_buffer(256);
		  struct wait_event_data *trgev=(struct wait_event_data *)tmpq->args[0];
		  switch(trgev->type)
		     {
		      case MOB_TRIGGER:
			 sprintf(buf1,"M-TRIG: %s [%ld] in ",
				 GET_NAME((struct char_data *)trgev->go),
				 GET_MOB_VNUM((struct char_data *)trgev->go));
			 if(IN_ROOM((struct char_data *)trgev->go)>0)
			    sprintf(buf1+strlen(buf1),"%s [%ld] ",
				    world[IN_ROOM((struct char_data *)trgev->go)].name, world[IN_ROOM((struct char_data *)trgev->go)].number);
			 else
			    strcat(buf1,"NOWHERE ");
			 break;
		      case OBJ_TRIGGER:
			 strcpy(buf1,"O-TRIG: ");
			 strcat(buf1,GET_OBJ_NAME((struct obj_data *)trgev->go));
			 break;
		      case WLD_TRIGGER:
			 sprintf(buf1,"W-TRIG: %s [%ld] ",
				((struct room_data *)trgev->go)->name,
				((struct room_data *)trgev->go)->number);
			 break;
		     }
		  sprintf(buf1+strlen(buf1)," Trigger: %s [%ld]",
			  GET_TRIG_NAME(trgev->trigger),
			  GET_TRIG_VNUM(trgev->trigger));
		  sprintf(buf+strlen(buf),"[DGSC] %s\t%s\r\n",
			  queue_time_to_string(mybuf, tmpq->time),buf1);
		  release_buffer(buf1);
		  }
               else if (tmpq->function==skin_update)
                  sprintf(buf+strlen(buf), "[SKIN] %s%s%s\r\n",
                          queue_time_to_string(mybuf, tmpq->time),
                          tmpq->ch ? "\tCh: " : "",
                          tmpq->ch ? GET_NAME(tmpq->ch) : "");
               else if (tmpq->function==dig_update)
                  sprintf(buf+strlen(buf), " [DIG] %s%s%s\r\n",
                          queue_time_to_string(mybuf, tmpq->time),
                          tmpq->ch ? "\tCh: " : "",
                          tmpq->ch ? GET_NAME(tmpq->ch) : "");
               else if (tmpq->function==process_skills)
                  sprintf(buf+strlen(buf), "[SKIL] %s%s%s Tch:%s, Item:%s\r\n",
                          queue_time_to_string(mybuf, tmpq->time),
                          tmpq->ch ? "\tCh: " : "",
                          tmpq->ch ? GET_NAME(tmpq->ch) : "",
                          tmpq->args[1] ? GET_NAME((struct char_data *)tmpq->args[1]) : "NONE",
                          tmpq->args[2] ? GET_OBJ_NAME((struct obj_data *)tmpq->args[2]) : "NONE");
	       else
		  sprintf(buf+strlen(buf), "[FUNC] %s%s%s %p %p %p\r\n", 
			  queue_time_to_string(mybuf, tmpq->time), 
			  tmpq->ch ? "\tCh: " : "", 
			  tmpq->ch ? GET_NAME(tmpq->ch) : "",
			  tmpq->args[0],tmpq->args[1],tmpq->args[2]); 
	       } 
      
      if (full_listing && (tmpq->time>=time(NULL)+min_time)&& 
	  (max_time?(tmpq->time<=time(NULL)+max_time):1)) 
	 if (!is_chdepend ||  
	     (is_chdepend && chdepend == tmpq->ch)) 
	    if ((is_qid==-1) || 
		(is_qid > -1 && is_qid==tmpq->queue_id)) 
	       send_to_char(ch,"%s",buf); 
      } 
 
   if (in_queue == 0) 
      send_to_char(ch,"\t*** Queue empty!\r\n"); 
 
   send_to_char(ch, "\r\nTotal: %ld\t0-60s: %ld\t1m-5m: %ld\t5m-15m: %ld\t15m+: %ld\r\n", 
	   in_queue, in_queue_one, in_queue_five, in_queue_fifteen, in_queue_more); 
   send_to_char(ch, "Bytes: %ld\tAve/Event: %ld\r\n", 
	   queue_in_bytes, in_queue?(long)(queue_in_bytes/in_queue):0); 
   
   send_to_char(ch, "Total Number of Tics: %d,  Ave Time/Tics(sec): %d\r\n",
	   num_events,(int)(total_event_time/num_events));
   release_buffer(sflags);
   release_buffer(buf);
   release_buffer(mybuf);
   return; 
} 
 
/* 
** do_delay_func() is the workhorse behind the MUD "delay" command. 
** Simply, it takes a delay, name, and command and sticks it on the 
** queue. The delay is in seconds from now, and name can be a player 
** or mob. This command should be safe even if the target character 
** disconnects or is killed. 
** 
** Example, which puts the "say Hello there!" command on the queue 
** for 10 seconds, and then executes it as shroom: 
**  delay 10 shroom say Hello there! 
** 
** Take a look at add_command_to_queue() if you are interested in 
** adding a command to the queue using C instead of live on the MUD. 
*/ 

ACMD(do_delay_func) 
{ 
  /* 
   * victim: Who we executing this as? 
   * arg1...3, tmparg...2: For figuring out the args 
   * qtime: Number of seconds from now to execute 
   */ 
   struct char_data *victim; 
   char *arg1=get_buffer(256);
   char *arg2=get_buffer(256);
   char *arg3;
   long qtime; 
 
   if (IS_NPC(ch)) 
      return; 
 
 /* Get arguments... */ 
   arg3= two_arguments(argument, arg1,arg2); 

   
   if (!*arg1 || !*arg2 || !*arg3) 
      { 
      send_to_char(ch,"Usage: delay <time> <target> <command>\r\n"); 
      } 
   /* No victim, no go.. */ 
   else if (!(victim = get_char(arg2))) 
      { 
      send_to_char(ch,"Target not found.\r\n"); 
      } 
   else if ((GET_LEVEL(victim) >= GET_LEVEL(ch)) && (ch != victim))  
      { 
      send_to_char(ch,"Yea right...Like that is gonna happen.\r\n"); 
      } 
   else if ((qtime = atol(arg1)) < 1) 
      { 
      send_to_char(ch,"Time less the 1s. Next time use force.\r\n"); 
      } 
   else
      {
     /* Put it on the queue.. */ 
      add_command_to_queue(qtime, 0, victim, arg3); 
      send_to_char(ch,"Delay added to queue.\r\n"); 
       mudlogf(BRF,GOD_LOG(ch),TRUE,
           "(GC) %s typed 'delay %ld %s%s'",GET_NAME(ch),qtime,GET_NAME(victim),arg3);
      }
   release_buffer(arg2);
   release_buffer(arg1);
} 
 
/* 
** do_kill_event() is the workhorse of the "qkill" MUD command, which 
** is like the Unix "kill" command. It accepts the QueueID of the event 
** to kill. When killed, you are notified and a note is written to the 
** syslog. 
** 
** Example which kills QueueID# 123: 
**  qkill 123 
*/ 
ACMD(do_kill_event) 
{ 
  /* 
   * tmpq: used to go through the linked list 
   * qid:  the QueueID# of the event to kill 
   */ 
   struct queue_event *tmpq; 
   long    qid; 
   if (IS_NPC(ch)) 
      return; 
 
 /* Get the argument.. */ 
   if (!*argument) 
      { 
      send_to_char(ch,"Ok, but WHICH event should I kill?\r\n"); 
      return; 
      } 
 
   qid = atol(argument); 
 
 /* Run through the queue and kill it if you find it */ 
   for (tmpq = command_queue;tmpq;tmpq=tmpq->next) 
      { 
	if (tmpq->queue_id == qid && tmpq->function != MEV_update )
	 { 
	 del_event_queue(tmpq); 
	 send_to_char(ch, "Deleted QueueID # %ld.\r\n", qid); 
	 mudlogf(BRF, GOD_LOG(ch), TRUE,
		 "(GC) %s has deleted QueueID # %ld", 
		 GET_NAME(ch), qid); 
	 return; 
	 } 
      } 
 
   send_to_char(ch, "Unable to find QueueID # %ld on queue.\r\n", qid); 
   return; 
} 
 
/* 
** del_event_queue() accepts a pointer to an event as it's argument 
** and zaps it off the queue, if possible. You should never have to 
** use this, since it is an internal command. 
*/ 
void del_event_queue(struct queue_event *event) 
{ 
  /* 
   * tmpq: used to travel the linked list 
   */ 
   struct queue_event *tmpq; 
 
  /* Is this event the first on the queue? If so, free it and relink */ 
   if (command_queue == event) 
      { 
      command_queue = event->next; 
      if (event->command) 
	 free(event->command); 
      free(event); 
      return; 
      } 
 
  /* Not the first, so let's find it and zap it */ 
   for (tmpq = command_queue;tmpq;tmpq=tmpq->next) 
      { 
      if (tmpq->next == event) 
	 { 
	 tmpq->next = event->next; 
	 if (event->command) 
	    free(event->command); 
	 free(event); 
	 return; 
	 } 
      } 
 
   return; 
} 
 
/* 
** add_event_queue() is an internal command and you should NEVER need 
** to use it, EVER. When given a pointer to an event, it links that 
** event into the event queue. In order to save time later, we find 
** where in the queue it should go (not just "stick on the end") time 
** wise. For example, if the event will execute in 15 seconds, put it 
** after all commands that will execute in 14 seconds or less, but 
** just before anything which will execute 16 seconds or longer from 
** now. This way, when we yank shit off the queue, there will be no 
** need to go over the entire queue, every time. If you want to stick 
** stuff on the queue, see add_command_to_queue() for a MUD command, 
** and add_function_to_queue() for a generic internal function. 
*/ 
void add_event_queue(struct queue_event *event) 
{ 
  /* 
   * next_queue_id: keeps track of the next, unique QueueID # 
   * tmpq, tmplast: used for the linked list 
   */ 
   static long  next_queue_id = 0; 
   struct queue_event *tmpq, *tmplast = NULL; 
 
 /* NULL?! Gotta pass SOMETHING. */ 
   if (!event) 
      return; 
 
 /* Assign the new queue_id and increment the next_queue_id number */ 
   event->queue_id = next_queue_id++; 
 
  /* Make sure we never overflow and just reset the queue_id to 0 if */ 
  /* we hit LONG_MAX */ 
   if (next_queue_id == LONG_MAX) 
      { 
      next_queue_id = 0; 
      } 
 
  /* No commands on queue-- this is the first */ 
   if (!command_queue) 
      { 
      command_queue = event; 
      event->next = NULL; 
      return; 
      } 
 
  /* Go through the queue and find the right spot to link us in */ 
   for(tmpq = command_queue;tmpq;tmpq=tmpq->next) 
      { 
      if (tmpq->time > event->time) /* Finds the right time spot */ 
	 { 
	 event->next = tmpq; 
	 if (tmplast) 
	    tmplast->next = event; 
	 else 
	    command_queue = event; 
	 return; 
	 } 
      else 
	 tmplast = tmpq; 
      } 
 
   tmplast->next = event; 
   event->next = NULL; 
 
   return; 
} 
 
/* 
** process_event_queue() is called in game_loop() in comm.c, right 
** before pulse is incremented. This means it is called once every 
** 1/4 of a second. Should we put this AFTER pulse is incremented 
** and have it call itself every second instead? If there is nothing 
** to execute on the queue, it should take no time at all to execute 
** anyway.. 
** 
** This is a little tricky, so if you are going to modify things, 
** read the comments carefully! 
*/ 
void process_event_queue(void) 
{ 
  /* 
   * event: current event being processed 
   * tmpq: temporary event to help travel the list 
   */ 
   struct queue_event *event, *tmpq; 
 
 
  /* Start with first event on queue. We shouldn't have to go very */ 
  /* far in the queue, since we ordered things when pushing them on */ 
  /* If the time of execution of the event is <= the time now, it */ 
  /* should be executed now. As soon as we hit the queue item which */ 
  /* will not execute now, none of the others on the queue will be */ 
  /* ready to execute yet, either, so just quit */ 
   event = command_queue; 
   while (event && event->time <= time(NULL)) 
      { 
      /* expose the firing entry to free_char() */
      current_processing_event = event; 
      if (IS_SET(event->flags, QUE_FUNCTION)) 
	 { 
	/* is_same_and_conn() is important! Read the */ 
	/* before that function for info! */ 
	 if ((event->ch && is_same_and_conn(event)) 
	     || !event->ch) 
	    (void)event->function( 
	       event->args[0], event->args[1], 
	       event->args[2], event->args[3], 
	       event->args[4], event->args[5], 
	       event->args[6], event->args[7], 
	       event->args[8], event->args[9]); 
	 } 
      else if (IS_SET(event->flags, QUE_COMMAND)) 
	 { 
	/* is_same_and_conn() is important! Read the */ 
	/* before that function for info! */ 
	 if (is_same_and_conn(event)) 
	    command_interpreter(event->ch, event->command); 
	 } 
      current_processing_event = NULL; 
 
     /* This is a little touchy. Remember, we are starting with */ 
     /* the FIRST queue item first, and if it executes, we DELETE */ 
     /* it! That means it no longer exists, of course, and the */ 
     /* list will go loopy because we will skip events that */ 
     /* should execute. Therefore, put the next event point in */ 
     /* tmpq FIRST, then zap the current event, then reassign */ 
     /* tmpq to event. Which SHOULD be the first, but later may */ 
     /* not be. Make sense? */ 
      tmpq = event->next; 
      del_event_queue(event); 
      event = tmpq; 
      } 
 
   return; 
} 
 
/* 
** add_command_to_queue() is one of the two main queue functions you 
** will be using. The other is add_function_to_queue(). This function 
** accepts the time-to-execution (in seconds from now), any flags, 
** the acting character, and the MUD command line to execute, as if 
** the player had typed it. Characters can be players or mobs. 
** 
** Example of the character pointed to by "ch" doing a say 10 seconds 
** from now. No flags are given: 
**  add_command_to_queue(10,0,ch,"say Hello there, stranger!"); 
** 
** If the character is killed or disconnects before the 10 seconds is 
** up, when the queue tries to execute the command it will abort. Please 
** read the comments for is_same_and_conn() for more details on this. 
** Since we check if the character is still around, this is the safest 
** queue command to use. 
** 
** Note: Currently, there are no user settable flags. ALWAYS use 0 
** here. The QUE_FUNCTION, QUE_COMMAND, QUE_MOBPROG, QUE_DELAY, and 
** QUE_PLAYER flags are INTERNAL ONLY and should NOT be specified! 
*/ 
void  add_command_to_queue(time_t qtime, long flags, struct char_data *ch,  
			   char *command) 
{ 
  /* 
   * event: This will be the event to stick on the queue 
   */ 
   struct queue_event *event; 
 
/*    event = (struct queue_event *)malloc(sizeof(struct queue_event));  */
   CREATE(event,struct queue_event,1);
   event->ch = ch; 
   event->flags = flags; 
   SET_BIT(event->flags, QUE_COMMAND); 
   if (!IS_NPC(ch)) 
      { 
/* We're a player, so we need to get the Idnum, not the nr */ 
      SET_BIT(event->flags, QUE_PLAYER); 
      event->ch_num = GET_IDNUM(ch); 
      } 
   else 
      { 
/* Must be a mob, so get the nr, not Idnum */ 
      event->ch_num = ch->nr; 
      } 
   event->next = NULL; 
/*
 * total_event_time+=qtime;
 * num_events++;
 */
   event->time = time(NULL) + (time_t)qtime; 
   CREATE(event->command,char,strlen(command)+1);
/*   event->command = (char *)malloc(sizeof(char) * (strlen(command) + 1));  */
   strcpy(event->command, command); 
 
  /* Pop it on the queue */ 
   add_event_queue(event); 
 
   return; 
} 
 
/* 
** add_function_to_queue() is one of the two main queue functions you 
** will be using. The other is add_command_to_queue() and is preferable 
** to this one, since it is safer. This function will add an actual C 
** function to the queue. The qtime argument is the time from now in 
** seconds to wait, flags should be 0, function is the name of the 
** C function, and the rest of the args will be passed to the function 
** at execution time. There is also a ch argument, which can be used 
** in the occasions when you know who this function depends on. If you 
** do not know or have a ch to put here, simply use NULL. 
** 
** Example of a send_to_char() executing in 10 seconds to character "ch": 
**  add_function_to_queue(10,ch,0,2,send_to_char,"Hello!\r\n",ch); 
** 
** You can pass up to ten additional arguments after the function name. 
** Note: Please see the warnings on "flags" for add_command_to_queue()! 
** 
** Please be VERY careful with this function, since there is NO CHECKING 
** to see if the players being acted on are still connected or alive, 
** unless you give the ch argument. Even then, it can still be dangerous! 
** If they disappear before execution, things may explode, since you may 
** be passing invalid pointers to the function. I would recommend wrapping 
** the function in your OWN function and using is_same_and_conn() or a 
** similar function to double check they are still around. Don't pass 
** pointers that could become invalid.. pass character or object NAMES 
** for get_char(), etc. to parse later! 
** 
** num_variables is for figuring out how many (up to 10) arguments are passed
** in the stdargs section of this function.
*/ 
struct queue_event *add_function_to_queue(time_t qtime, struct char_data *ch,
					   long flags, int num_variables, 
					   void (*function)(), ...) 
{ 
  /* 
   * arg_ptr: for stdarg.h 
   * event: the event this will be 
   * func_arg: for stdarg.h 
   * i:  just a generic loop index 
   */ 
   va_list arg_ptr; 
   struct queue_event *event; 
   void *func_arg; 
   int i; 
 
   if((num_variables <0)||(num_variables>10))
      {
      log("SYSERR: add_function_to_queue: Invalid num_variables: %d",
	  num_variables);
      return NULL;
      }
   va_start(arg_ptr, function); /* for stdarg.h */ 
 
   CREATE(event,struct queue_event,1);
/*    event = (struct queue_event *)malloc(sizeof(struct queue_event));  */
   event->ch = ch;   /* Character MAY be associated! */ 
  /* Call it an 'assertion' */ 
   event->command = NULL;  /* No command! */ 
   event->flags = flags; 
   SET_BIT(event->flags, QUE_FUNCTION); 
   if (event->ch && !IS_NPC(event->ch)) 
      { 
/* We're a player, so we need to get the Idnum, not the nr */ 
      SET_BIT(event->flags, QUE_PLAYER); 
      event->ch_num = GET_IDNUM(ch); 
      } 
   else if (event->ch) 
      { 
/* Must be a mob, so get the nr, not Idnum */ 
      event->ch_num = ch->nr; 
      } 
   for (i=0;i<10;i++)  /* Set all args to NULL */ 
      event->args[i] = NULL; 
   event->next = NULL; 
   event->time = time(NULL) + (time_t)qtime; 
   event->function = function; /* Function pointer */ 

   if(function==MEV_update)
      {
      total_event_time+=qtime;
      num_events++;
      }
 
  /* Assign the args */ 
   for (i=0; (i < num_variables); i++)
      { 
      func_arg = va_arg(arg_ptr, void *);
      event->args[i] = func_arg; 
      } 
 
  /* Push it on the queue */ 
   add_event_queue(event); 
 
   va_end(arg_ptr); /* For stdarg.h */ 
   return event; 
} 
 
/* 
** is_same_and_conn() is used for add_command_to_queue(), and 
** add_function_to_queue() (when we know who the function would 
** be related to or depends on) to determine if the character who is 
** the actor still exists. If the QUE_PLAYER flag is set, we check the 
** descriptor list (disconnected chars are therefore ignored) for speed, 
** and if the flag is not set, we check the character list for any mobs 
** which match the nummber in ch_num. For players, ch_num is the IDnum, 
** and for mobs it is nr. If these numbers match, we then check to see 
** if the pointers are the same to ensure we're talking about the right 
** mob or character. This is the failsafe that makes sure we don't try 
** to execute for an actor whose char_data pointer is no longer valid. 
** The only argument is the event pointer. 
** 
** One thing to keep in mind is that for multiple mobs of the same 
** type (beastly fidos, etc) there is a SLIGHT chance that it will 
** screw up and the wrong fido will execute the command if the original 
** fido is killed. This should be VERY VERY VERY rare, but theoretically 
** possible. 
** 
** Returns: 1 if found and same, 0 if not found or not same. 
*/ 
int is_same_and_conn(struct queue_event *event) 
{ 
  /* 
   * d: temp descriptor_data pointer for travelling the list 
   * c: temp char_data pointer for travelling the list 
   */ 
   struct descriptor_data *d; 
   struct char_data *c; 
 
   if (IS_SET(event->flags, QUE_PLAYER)) 
      { 
     /* We're a player.. go over the descriptor_list */ 
      for (d=descriptor_list;d;d=d->next) 
	 { 
	 if(d->character&& (event->ch_num == GET_IDNUM(d->character)))
	    { 
	   /* ID is same, let's double check the */ 
	   /* pointer. Remember, if a player is killed */ 
	   /* or leaves, and then comes back, the ID's */ 
	   /* may be the same but the pointers will be */ 
	   /* screwed. */ 
	    if (event->ch == d->character) 
	       { 
	      /* Pointer is the same. We're cool. */ 
	       return 1; 
	       } 
	    } 
	 } 
      } 
   else 
      { 
     /* We're a mob.. go over the character_list */ 
      for (c=character_list;c;c=c->next) 
	 { 
	 if (c->nr == event->ch_num) 
	    { 
	   /* Numbers are the same, but we want to */ 
	   /* check for the pointer now so we are sure */ 
	   /* we're talking about the right mob */ 
	    if (c == event->ch) 
	       { 
	      /* Pointers match. We're cool. */ 
	       return 1; 
	       } 
	    } 
	 } 
      } 
 
  /* FUBAR. They're either dead or gone. */ 
   return 0; 
} 
 
