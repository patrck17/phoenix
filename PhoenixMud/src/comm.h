/* ************************************************************************
*   File: comm.h                                        Part of CircleMUD *
*  Usage: header file: prototypes of public communication functions       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define NUM_RESERVED_DESCS	8

/* comm.c */
void   send_to_all(const char *messg, ...) __attribute__ ((format (printf, 1, 2)));
void   send_to_char(struct char_data *ch, const char *messg, ...) __attribute__ ((format (printf, 2, 3)));
void   send_to_room(room_rnum room, const char *messg, ...) __attribute__ ((format (printf, 2, 3)));
void   send_to_outdoor(const char *messg, ...) __attribute__ ((format (printf, 1, 2)));
void   perform_to_all(const char *messg, struct char_data *ch, ...) __attribute__ ((format (printf, 1, 3)));
void	close_socket(struct descriptor_data *d);
void   gmcp_comm(struct char_data *ch, const char *channel, const char *talker,
		 const char *text);
void   json_escape_string(const char *in, char *out, size_t out_size);
void   gmcp_char_sidebar(struct char_data *ch);
void   gmcp_comm_channel_list(struct char_data *ch);
void   gmcp_char_roomview(struct char_data *ch);
void   gmcp_char_who(struct char_data *ch);
void   gmcp_who_broadcast(void);
void   gmcp_char_skills(struct char_data *ch);
void   gmcp_combat_event(struct char_data *ch, struct char_data *victim,
                         const char *kind, int amt, const char *cause);

void	perform_act(char *orig, struct char_data *ch, struct obj_data *obj,
		     void *vict_obj, struct char_data *to);

void	act_t(char *str, int hide_invisible, struct char_data *ch,
	    struct obj_data *obj, void *vict_obj,int type,const char *func,int line);
#define act(str,hide_invis,ch,obj,vict,type) act_t((str),(hide_invis),(ch),(obj),(vict),(type),__FUNCTION__,__LINE__)
/*void	act(char *str, int hide_invisible, struct char_data *ch,
	    struct obj_data *obj, void *vict_obj, int type);*/
/** 2/24/97, Anduin - send_auction send_battle **/
void send_auction_mort(const char *messg,...) __attribute__ ((format (printf, 1, 2)));
void send_auction_god(const char *messg, ...) __attribute__ ((format (printf, 1, 2)));
void send_battle(const char *messg, ...) __attribute__ ((format (printf, 1, 2)));
void send_info(const char *messg, ...) __attribute__ ((format (printf, 1, 2)));

#define TO_ROOM		(1<<0)
#define TO_VICT		(1<<1)
#define TO_NOTVICT	(1<<2)
#define TO_CHAR		(1<<3)
#define TO_SLEEP	(1<<4)	/* to char, even if sleeping */
#define FR_FIGHT        (1<<5)
#define FR_WEAR         (1<<6)
#define DG_NO_TRIG      (1<<7)     /* don't check act trigger   */

int	write_to_descriptor(socket_t desc, const char *txt);
void	write_to_q(char *txt, struct txt_q *queue, int aliased); 
void	write_to_q_d(char *txt, struct descriptor_data *d, int aliased);
void    write_to_output(struct descriptor_data *d, const char *format, ...) __attribute__ ((format (printf, 2, 3)));
void	page_string(struct descriptor_data *d, char *str, int keep_internal,
		    char *sHeader);
void   string_add(struct descriptor_data *d, char *str);
void   string_write(struct descriptor_data *d, char **txt, size_t len, 
		    long mailto, void *data);

#define SEND_TO_Q      write_to_output

#define USING_SMALL(d)	((d)->output == (d)->small_outbuf)
#define USING_LARGE(d)  ((d)->output == (d)->large_outbuf)

typedef RETSIGTYPE sigfunc(int);


