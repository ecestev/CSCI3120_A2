/* 
 * File: mlfb.c
 * Author: Stephen Sampson
 * Date: June 2016
 * Purpose: This file contains the multi-level queue with feedback module
 * 			to serve HTTP requests.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include "network.h"
#include "universal_functions.h"

/* linked list implementation adapted from: http://tutorialspoint.com/
 * data_structures_algorithms/linked_lists_program_in_c.html
 */

/* LINKED LIST STRUCTS */
struct hpnode{							// high priority queue
	int sequence;						// sequence number of the job coming in
	int socket;							// socket number to send the file on
	int fd;								// file descriptor of file requested
	float bytes_left;					// remaining bytes to transfer to client
	int quantum;						// how many bytes will be sent at a time
	struct hpnode *hpnext;			// pointer to the next node in the linked list
};
struct hpnode *hphead = NULL;		// pointer to the head of the linked list
struct hpnode *hpcurrent = NULL;	// last entry in the linked list points to NULL

struct mpnode{							// medium priority queue
	int sequence;						// sequence number fo the job coming in
	int socket;							// socket number to send the file on
	int fd;								// file descriptor of file requested
	float bytes_left;					// remaining bytes to transfer to client
	int quantum;						// how many bytes will be sent at a time
	struct mpnode *mpnext;			// pointer to the next node in the linked list
};		
struct mpnode *mphead = NULL;		// pointer to the head of the linked list
struct mpnode *mpcurrent = NULL;	// last entry in the linked list points to NULL

struct fcfsnode{						// lowest priority queue
	int sequence;						// sequence number of the job coming in
	int socket;							// socket number to send the file on
	int fd;								// file descriptor of the file requested
	float bytes_left;					// remaining bytes to transfer to client
	int quantum;						// how many bytes will be sent at a time
	struct fcfsnode *fcfsnext;		// pointer to the nest node in the linked list
};
struct fcfsnode *fcfshead = NULL;	// pointer to the head of the linked list
struct fcfsnode *fcfscurrent = NULL;// last entry points to NULL

/* LINKED LIST FUNCTION PROTOTYPES */
bool hpisEmpty(void);				// determine whether HP queue has any entries
/* adds new node at beginning of HP queue if empty or at end otherwise */
void hppush(int sequence, int socket, int fd, float bytes_left, int quantum);
struct hpnode *hppop_front(void);	// remove job from front of HP queue
bool mpisEmpty(void);				// determine whether MP queue has any entries
/* adds new node at beginning of MP queue if empty or at end otherwise */
void mppush(int sequence, int socket, int fd, float bytes_left, int quantum);
struct mpnode *mppop_front(void);	// remove job from front of MP queue
bool fcfsisEmpty(void);				// determine whether LP queue has any entries
/* adds new node at beginning of LP queue if empty or at end otherwise */
void fcfspush(int sequence, int socket, int fd, float bytes_left, int quantum);
struct fcfsnode *fcfspop_front(void);	// remove job from front of LP queue

/* MLFB SCHEDULING ALGORITHM EXECUTION */
void mlfb(int port_number, int debug) {
	int client_socket = -1;				// socket the client connected on
	int sequence = 0;						// global sequence counter - how many jobs
	int hpquant = 8192;					// quantum of high priority queue
	int mpquant = 65535;					// quantum of medium priority queue
	int fcfsquant = 65535;				// quantum of low priority queue
	/* array to store request elements in (method, file, protocol) */
	char **request_elements = calloc(3, sizeof(char**));
	char *to_parse = calloc(256, sizeof(char));	// request string to be parsed
	char *location = calloc(256, sizeof(char));	// location of requested file
	FILE *requested_file;				// the requested file
	struct hpnode *hpprocessing;		// HP job currently being processed
	struct mpnode *mpprocessing;		// MP job currently being processed
	struct fcfsnode *fcfsprocessing;	// LP job currently being processed
	int seqip;								// sequence number of job in progress
	int sockip;								// socket number of job in progress
	int fdip;								// file descriptor of job in progress
	float blip;								// bytes left of job in progress
	int quantip;							// quantum for job in progress
	network_init(port_number);			// initialize network on specified port
	while(1){
		/* check if all queues are empty */
		if((hpisEmpty())&&(mpisEmpty())&&(fcfsisEmpty()))	
			network_wait();				// if queues are empty, put program to sleep
		/* The code within the following while loop is used to interpret and queue 
		 * any valid waiting requests */
		while((client_socket = network_open()) != -1){
			/* if network_open() returns a positive integer, parse the request 
			 * on this socket */
			if(client_socket>0){
				read(client_socket, to_parse, 256);	// read request into buffer
				parse(request_elements, to_parse);	// parse request from buffer
				/* if any of the three required HTTP request elements are missing,
				 * send error 400 malformed request on client socket */
				if((request_elements[0]==NULL)|(request_elements[1]==NULL)|
				(request_elements[2]==NULL)){
					msg400(client_socket);				// malformed request. Error 400
					close(client_socket);				// close client socket
				} else {
					/* if the request is valid, look for the requested file */
					if((strcmp(request_elements[0],"GET")==0)&&
					(strcmp(request_elements[2],"HTTP/1.1")==0)){
						location = request_elements[1];	// store location
						location++;								// stip leading slash
						/* if requested file is where it should be, open it and get
						 * required information on the file then add the job to the
						 * queue */
						if(fopen( location, "r" ) != NULL){
							requested_file = fopen( location, "r");
							int fd = -1;					// var to store file descriptor
							fd = fileno(requested_file);	// store file descriptor
							struct stat buf;					// create a buffer
							fstat(fd, &buf);					// retrieve file stats
							float filesize = buf.st_size;	// store filesize
							sequence++;			// increment global sequence counter
							/* push job to queue with all required info */
							hppush(sequence, client_socket, fd, filesize, hpquant);
							msg200(client_socket);	// request ok!
						} 	else {
				 			msg404(client_socket);	// fopen() returned NULL. Error 404
							close(client_socket);	// close client connection
						}
					} 	else {
						msg400(client_socket);		// malformed request. Error 400
						close(client_socket);		// close client connection
					}
				}
				/* ensure request array is cleaned up */
				int i=0;
				for(i=0;i<3;i++){
					if(request_elements[i]!=NULL)
						memset(request_elements[i], '\0',strlen(request_elements[i]));
				}
			}
		}
 		
		/* BEGIN MLFB PROCESSING ALGORITHM */
		/* If the high priority queue is not empty when the program code reaches
		 * this line then process next item in high priority queue. */
		if(!hpisEmpty()){							// check if queue is empty
			hpprocessing = hppop_front();		// pop first entry
			seqip = hpprocessing->sequence;	// store sequence of popped job
			sockip = hpprocessing->socket;	// store socket of popped job
			fdip = hpprocessing->fd;			// store file descriptor of popped job
			blip = hpprocessing->bytes_left;	// store bytes left of popped job
			quantip = hpprocessing->quantum;	// store quantum of popped job
			/* if the remaining number of bytes to transfer is less than the 
			 * quantum for the HP queue (8K) then send the remainder of the file */
			if(blip<=hpquant){
				lseek(fdip, (off_t)-blip, SEEK_END);		// set location in file
				sendfile(sockip, fdip, NULL, blip);			// send remainder of file
				fflush(stdout);									// flush stdout buffer
				printf("Request %d Completed\n", seqip);	// print to console
				close(fdip);						// close file descriptor in progress
				close(sockip);						// close socket in progress
			/* otherwise send the next 'quantum' bytes of the file and re-queue */
			} else {
				sendfile(sockip, fdip, NULL ,quantip);			// send next quantum
				blip = blip-hpquant;									// update bytes left
				/* push remainder of job to the back of the MP queue */
				mppush(seqip, sockip, fdip, blip, mpquant);	
			}	
		/* If the high priority queue is empty when the program code reaches this
		 * point in the code then take a look at the next queue. If the medium
		 * priority queue has entries then process the next item in that queue */
		} else {
			if(!mpisEmpty()){							// check if queue is empty
				fflush(stdout);						// pop first entry
				mpprocessing = mppop_front();		// pop forst entry
				seqip = mpprocessing->sequence;	// store sequence of popped job
				sockip = mpprocessing->socket;	// store socket of popped job
				fdip = mpprocessing->fd;			// store fd left of popped job
				blip = mpprocessing->bytes_left;	// store bytes left of popped job
				quantip = mpprocessing->quantum;	// store quantum of popped job
				/* if the remaining number of bytes to transfer is less than the
				 * quantum for the MP queue (64K) then send the remainder of
				 * the file */
				if(blip<=mpquant){				
					lseek(fdip, (off_t)-blip, SEEK_END);	// set location in file
					sendfile(sockip, fdip, NULL, blip);		// send remainder of file
					fflush(stdout);								// flush stdout buffer
					printf("Request %d Completed\n", seqip);	// print to console
					close(fdip);					// close file descriptor in progress
					close(sockip);					// close socket in progress
				} else {
					/* otherwise send the next 'quantum' bytes of the file and 
					 * re-queue */
					sendfile(sockip, fdip, NULL ,quantip);	// send next quantum
					blip = blip-mpquant;							// update bytes left
					/* push remainder of job to the back of the RR queue */
					fcfspush(seqip, sockip, fdip, blip, fcfsquant);
				}
			/* If the high and medium priority queues are empty when the program
			 * reaches this point in the code then take a look at the next (RR) 
			 * queue. If the queue has entries then process the next item in the 
			 * queue */
			} else {
				if(!fcfsisEmpty()){					// check if queue is empty
					fcfsprocessing = fcfspop_front();	// pop first entry
					seqip = fcfsprocessing->sequence;	// store sequence of job
					sockip = fcfsprocessing->socket;		// store socket of job
					fdip = fcfsprocessing->fd;				// store fd of job
					blip = fcfsprocessing->bytes_left;	// store bytes left for job
					quantip = fcfsprocessing->quantum;	// store quantum for job
					/* if the remaining number of bytes to transfer is less than
					 * the quantum for the RR queue (64K) then send the remainder
					 * of the file */
					if(blip<=fcfsquant){
						lseek(fdip, (off_t)-blip, SEEK_END);	// set location in file
						sendfile(sockip, fdip, NULL, blip);		// send rest of file
						fflush(stdout);								// flush stdout buffer
						printf("Request %d Completed\n", seqip);	// print to console
						close(fdip);							// close fd in progress
						close(sockip);							// close socket in progress
					/* otherwise sent ehe nest 'quantum' bytes of the file and
					 * re-queue */
					} else {
						sendfile(sockip, fdip, NULL ,quantip);	// send next quantum
						blip = blip-fcfsquant;						// update bytes left
						/* push remainder of job to the back of the RR queue */
						fcfspush(seqip, sockip, fdip, blip, fcfsquant);
					}
				}
			}
			/* END MLFB PROCESSING ALGORITHM */
		}
	}
}

/******************************************************************************
 *									LINKED LIST FUNCTIONS
 *****************************************************************************/

/* IS LINKED LIST EMPTY */
bool hpisEmpty(void){
	return hphead==NULL;
}

/* ADD  ENTRY TO  LINKED LIST */
void hppush(int sequence, int socket, int fd, float bytes_left, int quantum){
	int empty = 0;
	if(hpisEmpty())
		empty=1;
	struct hpnode *link = (struct hpnode*) malloc(sizeof(struct hpnode));
	struct hpnode*ptr = hphead;
	link->sequence = sequence;
	link->socket = socket;
	link->fd = fd;
	link->bytes_left = bytes_left;
	link->quantum = quantum;
	if(empty==1){						// push front
		link->hpnext = hphead;
		hphead = link;
	}else{								// push back
		while(ptr->hpnext!=NULL)
			ptr=ptr->hpnext;
		ptr->hpnext = link;
	}
}

/* REMOVE ENTRY FROM FRONT OF LINKED LIST */
struct hpnode *hppop_front(void){
	struct hpnode *tempLink = hphead;
	hphead = hphead->hpnext;
	return tempLink;
}

/* IS LINKED LIST EMPTY */
bool mpisEmpty(void){
	return mphead==NULL;
}

/* ADD  ENTRY TO  LINKED LIST */
void mppush(int sequence, int socket, int fd, float bytes_left, int quantum){
	int empty = 0;
	if(mpisEmpty())
		empty=1;
	struct mpnode *link = (struct mpnode*) malloc(sizeof(struct mpnode));
	struct mpnode*ptr = mphead;
	link->sequence = sequence;
	link->socket = socket;
	link->fd = fd;
	link->bytes_left = bytes_left;
	link->quantum = quantum;
	if(empty==1){						// push front
		link->mpnext = mphead;
		mphead = link;
	}else{								// push back
		while(ptr->mpnext!=NULL)
			ptr=ptr->mpnext;
		ptr->mpnext = link;
	}
}

/* REMOVE ENTRY FROM FRONT OF LINKED LIST */
struct mpnode *mppop_front(void){
	struct mpnode *tempLink = mphead;
	mphead = mphead->mpnext;
	return tempLink;
}

/* IS LINKED LIST EMPTY */
bool fcfsisEmpty(void){
	return fcfshead==NULL;
}

/* ADD  ENTRY TO  LINKED LIST */
void fcfspush(int sequence, int socket, int fd, float bytes_left, int quantum){
	int empty = 0;
	if(fcfsisEmpty())
		empty=1;
	struct fcfsnode *link = (struct fcfsnode*) malloc(sizeof(struct fcfsnode));
	struct fcfsnode*ptr = fcfshead;
	link->sequence = sequence;
	link->socket = socket;
	link->fd = fd;
	link->bytes_left = bytes_left;
	link->quantum = quantum;
	if(empty==1){						// push front
		link->fcfsnext = fcfshead;
		fcfshead = link;
	}else{								// push back
		while(ptr->fcfsnext!=NULL)
			ptr=ptr->fcfsnext;
		ptr->fcfsnext = link;
	}
}

/* REMOVE ENTRY FROM FRONT OF LINKED LIST */
struct fcfsnode *fcfspop_front(void){
	struct fcfsnode *tempLink = fcfshead;
	fcfshead = fcfshead->fcfsnext;
	return tempLink;
}

/******************************************************************************
 *											END OF FILE
 *****************************************************************************/


