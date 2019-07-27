/* ************************************************************************ 
*  File: ident.c                                                          * 
*                                                                         * 
*  Usage: Functions for handling rfc 931/1413 ident lookups               * 
*                                                                         * 
*  Written by Eric Green (egreen@cypronet.com)      * 
************************************************************************ */ 
 
#define __IDENT_C__ 
 
#include "../localHeader/conf.h" 
#include "../localHeader/sysdep.h" 
 
#include <sys/socket.h> 
#include <sys/resource.h> 
#include <netinet/in.h> 
#include <netdb.h> 
 
#include "structs.h" 
#include "buffer.h"
#include "utils.h" 
#include "buffer.h"
#include "comm.h" 
#include "db.h" 
#include "ident.h" 
 
 
/* max time in seconds to make someone wait before entering game */ 
#define IDENT_TIMEOUT 30 
 
#define IDENT_PORT    113 
 
 
extern struct timeval null_time; 
extern int port; 
extern int pulse;

/* extern functions */ 
void nonblock(socket_t s); 
int isbanned(char *hostname); 
 
 
/* start the process of looking up remote username */ 
void ident_start(struct descriptor_data *d, long addr) 
{ 
   socket_t sock; 
   struct sockaddr_in sa; 
 
   if (!ident) 
      { 
      STATE(d) = CON_ASKNAME; 
      d->ident_sock = -1; 
      return; 
      } 
 
   d->idle_tics = 0; 
 
  /* 
   * create a nonblocking socket, and start 
   * the connection to the remote machine 
   */ 
 
   if((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) 
      { 
      perror("socket"); 
      d->ident_sock = -1; 
      STATE(d) = CON_ASKNAME; 
      return; 
      } 
 
   sa.sin_family = AF_INET; 
   sa.sin_port = ntohs(IDENT_PORT); 
   sa.sin_addr.s_addr = addr; 
 
   nonblock(sock); 
   d->ident_sock = sock; 
 
   errno = 0; 
   if (connect(sock, (struct sockaddr*) &sa, sizeof(sa)) != 0) 
      { 
      if (errno == EINPROGRESS) 
	 { 
	/* connection in progress */ 
	 STATE(d) = CON_IDCONING; 
	 return; 
	 } 
 
     /* connection failed */ 
      else if (errno != ECONNREFUSED) 
	 perror("ident connect"); 
 
      STATE(d) = CON_ASKNAME; 
      } 
 
   else    /* connection completed */ 
      STATE(d) = CON_IDCONED; 
} 
 
 
void ident_check(struct descriptor_data *d) 
{ 
   fd_set fd; 
   int rc, rmt_port, our_port, len; 
   char *user=get_buffer(256), *p; 
   char *buf=get_buffer(SMALL_BUFSIZE);
   char *buf2=get_buffer(SMALL_BUFSIZE);
 
  /* 
   * Each pulse, this checks if the ident is ready to proceed to the 
   * next state, by calling select to see if the socket is writeable 
   * (connected) or readable (response waiting).   
   */ 
 
   switch (STATE(d)) 
      { 
       case CON_IDCONING: 
	 /* waiting for connect() to finish */ 
	  if (d->ident_sock != -1) 
	     { 
	     FD_ZERO(&fd); 
	     FD_SET(d->ident_sock, &fd); 
	     } 
 
	  if ((rc = select(d->ident_sock + 1, (fd_set *) 0, &fd, 
			   (fd_set *) 0, &null_time)) == 0) 
	     break; 
 
	  else if (rc < 0) 
	     { 
	     perror("ident check select (conning)"); 
	     STATE(d) = CON_ASKNAME; 
	     break; 
	     } 
 
	  STATE(d) = CON_IDCONED; 
 
       case CON_IDCONED: 
	 /* connected, write request */ 

	  sprintf(buf, "%d, %d\r\n", ntohs(d->peer_port), port); 
  
	  len = strlen(buf); 
/*#ifdef CIRCLE_WINDOWS 
  if (send(d->ident_sock, buf, len, 0) < 0) 
  { 
  #else */
	  if (write(d->ident_sock, buf, len) != len) 
	     { 
/*#endif */
	     if (errno != EPIPE) /* read end closed (no remote identd) */ 
		perror("ident check write (conned)"); 
 
	     STATE(d) = CON_ASKNAME; 
	     break; 
	     } 
 
	  STATE(d) = CON_IDREADING; 
  
       case CON_IDREADING: 
	 /* waiting to read */ 

	  if (d->ident_sock != -1) 
	     { 
	     FD_ZERO(&fd); 
	     FD_SET(d->ident_sock, &fd); 
	     } 
 
	  if ((rc = select(d->ident_sock+1, &fd, (fd_set *) 0, 
			   (fd_set *) 0, &null_time)) == 0) 
	     break; 
 
	  else if (rc < 0) 
	     { 
	     perror("ident check select (reading)"); 
	     STATE(d) = CON_ASKNAME; 
	     break; 
	     } 
 
	  STATE(d) = CON_IDREAD; 
  
       case CON_IDREAD: 
	 /* read ready, get the info */ 

/*#ifdef CIRCLE_WINDOWS 
  if ((len = recv(t->ident_sock, buf, sizeof(buf) - 1, 0)) < 0) 
  #else */
	  if ((len = read(d->ident_sock, buf, sizeof(buf) - 1)) < 0) 
/* #endif  */
	     perror("ident check read (read)"); 
 
	  else 
	     { 
	     buf[len] = '\0'; 
	     buf2[len] = '\0'; 
	     if (sscanf(buf, "%u , %u : USERID :%*[^:]:%255s", 
			&rmt_port, &our_port, user) != 3) 
		{ 
	       /* check if error or malformed */ 
		if (sscanf(buf, "%u , %u : ERROR : %255s", 
			   &rmt_port, &our_port, user) == 3) 
		   { 
		   log("Ident error from %s: \"%s\"", d->host, user);
		   } 
		else 
		   { 
		  /* strip off trailing newline */ 
		   for (p = buf + len - 1; p > buf && ISNEWL(*p); p--); 
		   p[1] = '\0'; 
 
		   log("Malformed ident response from %s: \"%s\"", 
			   d->host, buf); 
		   } 
		} 
	     else 
		{ 
		strncpy(buf2, user, IDENT_LENGTH); 
		strcat(buf2, "@"); 
		strcat(buf2, d->host); 
		strncpy(d->host, buf2, HOST_LENGTH); 
		} 
	     } 
  
	  STATE(d) = CON_ASKNAME; 
  
       case CON_ASKNAME: 
	 /* ident complete, ask for name */ 

	 /* close up the ident socket, if one is opened. */ 
	  if (d->ident_sock != -1) 
	     { 
	     CLOSE_SOCKET(d->ident_sock); 
	     d->ident_sock = -1; 
	     } 
	  d->idle_tics = 0; 
 
	 /* extra ban check */ 
	  if (isbanned(d->host) == BAN_ALL) 
	     { 
	     close_socket(d); 
	     mudlogf(CMP, LVL_DGOD, TRUE,"Connection attempt denied from [%s]",
		     d->host); 
	     release_buffer(buf2);
	     release_buffer(buf);
	     release_buffer(user);
	     return; 
	     } 
 
	  SEND_TO_Q(d,"\x1B[2K\r\nBy what name dost thou wish to be known? ");
	  STATE(d) = CON_GET_NAME; 
	  release_buffer(buf2);
	  release_buffer(buf);
	  release_buffer(user);
	  return; 
 
       default: 
	  release_buffer(buf2);
	  release_buffer(buf);
	  release_buffer(user);
	  return; 
      } 
 
  /* 
   * Print a dot every second so the user knows he hasn't been forgotten. 
   * Allow the user to go on anyways after waiting IDENT_TIMEOUT seconds. 
   */ 
   if ((pulse % PASSES_PER_SEC) == 0) 
      { 
      SEND_TO_Q(d,"."); 
     
      if (d->idle_tics++ >= IDENT_TIMEOUT) 
	 STATE(d) = CON_ASKNAME; 
      } 
   release_buffer(buf2);
   release_buffer(buf);
   release_buffer(user);
} 
 
 
/* returns 1 if waiting for ident to complete, else 0 */ 
int waiting_for_ident(struct descriptor_data *d) 
{ 

   switch (STATE(d)) 
      { 
       case CON_IDCONING: 
       case CON_IDCONED: 
       case CON_IDREADING: 
       case CON_IDREAD: 
       case CON_ASKNAME: 
	  return 1; 
       
       default: 
	  return 0; 
      } 
 
   return 0; 
} 


