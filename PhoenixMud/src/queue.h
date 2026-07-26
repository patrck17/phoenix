/*************************************************************************** 
 *  File:  queue.h                                      Part of PhoenixMUD * 
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
**	The queue_event structure defines each event in the queue, of
**	course:
**		queue_id is a long and should be considered unique. When
**		an event is killed or expires, the queue_id is NOT
**		reassigned, and I doubt it will ever overflow. When the
**		MUD reboots, the queue_id restarts at 0. The queue_id
**		does reset when it hits LONG_MAX.
**
**		ch is used for events which are linked to a particular
**		character, either mob or player. For example, QUE_COMMAND
**		would use this since commands must be executed as someone,
**		but QUE_FUNCTION would set it to NULL, since we really can't
**		determine who this command depends on.
**
**		ch_num is used in conjunction with ch. For players it will
**		be the IdNum, and for mobs the nr.
**
**		time is the actual real time this command will execute, in
**		the same form as returned by time(), as the number of seconds
**		from January 1st, 1970. Setting this to "10" for a delay of
**		10 seconds will NOT work. You will have to do time(NULL)+10.
**
**		command is the actual string command for QUE_COMMAND, such
**		as "say Hello!"
**
**		flags are any internal or user defined flags. See flags
**		below.
**
**		function is the pointer to the function to execute, per the
**		add_function_to_queue() function.
**
**		args[] is an array of 10 void pointers to arguments which
**		will be passed to function, above.
**
**		next is, well, take a wild guess.
*/
struct	queue_event
{
	long			queue_id;	/* Queue ID. DO NOT SET! */
	struct char_data	*ch;		/* Acting char if any */
	long			ch_num;		/* IDnum or Rnum of ch */
	time_t 			time;		/* Time of execution */
	char			*command;	/* A regular mud command */
	long			flags;		/* Special flags */
	void			(*function)();	/* Function pointer */
	void			*args[10];	/* Args for functions */
	struct queue_event	*next;
	struct queue_event	*next_in;
};

/*
**	All event flags.
*/

/* 	INTERNAL FLAGS ONLY! */
#define QUE_COMMAND	(1 << 0)
#define QUE_MOBPROG	(1 << 1)	/* Not implemented */
#define QUE_FUNCTION	(1 << 2)
#define QUE_DELAY	(1 << 3)	/* Not implemented */
#define QUE_PLAYER	(1 << 4)	/* Player, not mob for QUE_COMMAND */
/*	USER SETTABLE FLAGS */
/* None right now.. */

/*
**	All functions found in queue.c
*/

extern void	del_event_queue(struct queue_event *);
extern void	add_event_queue(struct queue_event *);
extern void	process_event_queue(void);
extern struct queue_event *add_function_to_queue(time_t, struct char_data *, 
						 long, int num_variables, 
						 void (*)(), ...);
extern void	add_command_to_queue(time_t, long, struct char_data *, char *);
extern int	is_same_and_conn(struct queue_event *);

/* The queue entry process_event_queue() is firing right now, else NULL. */
extern struct queue_event *current_processing_event;
