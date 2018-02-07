/* 
 * File: rr.c
 * Author: Stephen Sampson
 * Date: June 2016
 * Purpose: This file contains the round robin module 
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

/*
 * linked list implementation adapted from: http://tutorialspoint.com/
 * data_structures_algorithms/linked_list_program_in_c.html
 */

/* LINKED LIST STRUCTS */
struct rrnode{
	int sequence;						// sequence number of the job coming in
	int socket;							// socket number to send the file on
	int fd;								// file descriptor of the file requested
	float bytes_left;					// remaining bytes to transfer to client
	int quantum;						// how many bytes will be sent at a time
	struct rrnode *rrnext;			// pointer to the next node in the linked list
};
struct rrnode *rrhead = NULL;		// pointer to the head of the linked list
struct rrnode *rrcurrent = NULL;	// last entry in the linked list points to NULL

/* LINKED LIST FUNCTION PROTOTYPES */
bool rrisEmpty(void);			// determined whether linked list has any entried
void rrprintList(void);			// debugging only - prints all jobs in queue
/* adds new node at beginning of linked list if empty. Adds to end otherwise */
void rrpush(int sequence, int socket, int fd, float bytes_left, int quantum);
struct rrnode *rrpop_front(void);	// removes job at front of linked list

/* RR SCHEDULING ALGORITHM EXECUTION */
void rr(int port_number, int debug) {
	int client_socket = -1;		// socket the client connected on
	int sequence = 0;				// global sequence counter - how many jobs
	int quantum = 8192;			// maximum # of bytes to be sent at once
	/* array to store request elements in (method, file, protocol) */
	char **request_elements = calloc(3, sizeof(char**));
	char *to_parse = calloc(256, sizeof(char));	// request string to be parsed
	char *location = calloc(256, sizeof(char));	// location of requested file
	FILE *requested_file;		// the requested file
	struct rrnode *processing;	// job currently being processed
	int seqip;						// sequence number of job in process
	int sockip;						// socket number of job in process
	int fdip;						// file descriptor of job in process
	float blip;						// bytes left of job in process
	int quantip;					// quantum for job in process
	network_init(port_number);	// initialize network on specified port
	while(1){
		if (rrisEmpty())			// check if queue is empty
			network_wait();		// if queue is empty put program to sleep
		/* The code within the following while loop is used to interpret and queue 
		 * any valid waiting requests */
		while((client_socket = network_open()) != -1){
			/* if network_open() returns a positive integer, parse the request
			 * on this socket. */
			if(client_socket>0){
				read(client_socket, to_parse, 256);	// read request into buffer
				parse(request_elements, to_parse);	// parse request from buffer
				/* if any of the three requred HTTP request elements are missing, 
				 * send error 400 malformed request on client socket */
				if((request_elements[0]==NULL)|(request_elements[1]==NULL)|
				(request_elements[2]==NULL)){
					msg400(client_socket);		// malformed request. Error 400
					close(client_socket);		// close client socket
				} else {
					/* if the request is valid, look for the requested file */
					if((strcmp(request_elements[0],"GET")==0)&&
					(strcmp(request_elements[2],"HTTP/1.1")==0)){
						location = request_elements[1];	// store location
						location++;								// strip leading slash
						/* if requested file is where it should be, open it and get
						 * required information on the file then add the job to the
						 * queue */
						if(fopen( location, "r" ) != NULL){
							requested_file = fopen( location, "r");
							int fd = -1;					// var to store file descriptor
							fd = fileno(requested_file);	// store file descriptor
							struct stat buf;				// create a buffer
							fstat(fd, &buf);				// retrieve file stats
							float filesize = buf.st_size;	// find file size
							/* if filesize does not exceed sendfile capabilities add
							 * it to the queue. Note: we could simply remove this code
							 * and it would work fine, however Dr. Brodsky told me
							 * that it is ok to limit the server to handle files 
							 * under 2GB in size */
							sequence++;			// increment global sequence counter
							/* push job to queue with all required info */
							rrpush(sequence, client_socket, fd, filesize, quantum);
							msg200(client_socket);		// request ok!
						} 	else {
				 			msg404(client_socket);	// fopen() returned NULL. Error 404
							close(client_socket);	// close client connections
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
						memset(request_elements[i],'\0',strlen(request_elements[i]));
				}
			}
		}
		/* if request queue is not empty, process first request in queue for 
		 * the smaller of 'bytes_left' or 'quantum'. If there are still
		 * bytes remaining to transfer after the quantum is used up, 
		 * push the request to the back of the queue with an updated value for
		 * bytes_left */
 		if(!rrisEmpty()){							// check if queue is empty
			processing = rrpop_front();		// pop first entry
			seqip = processing->sequence;		// store sequence of popped job
			sockip = processing->socket;		// store socket of popped job
			fdip = processing->fd;				// store fd of popped job 
			blip = processing->bytes_left;	// store bytes left of popped job
			quantip = processing->quantum;	// store quantum of popped job
			/* if the remaining number of bytes to transfer is less than the 
			 * quantum then send the remainder of the file */
			if(blip<=quantum){				
				lseek(fdip, (off_t)-blip, SEEK_END);		// set location in file
				sendfile(sockip, fdip, NULL, blip);			// send entire file
				fflush(stdout);									// flush stdout buffer
				printf("Request %d Completed\n", seqip);	// print to console
				close(fdip);						// close file descriptor in progress
				close(sockip);						// close socket in progress
			/* otherwise, send the next 'quantum' bytes of the file and re-queue */
			} else {
				sendfile(sockip, fdip, NULL ,quantum );
				blip = blip-quantum;									// update bytes left
				rrpush(seqip, sockip, fdip, blip, quantip);	// push to queue
			}	
		} 
	}
}


/******************************************************************************
 *									LINKED LIST FUNCTIONS
 *****************************************************************************/

/* IS LINKED LIST EMPTY */
bool rrisEmpty(void){
	return rrhead==NULL;
}

/* ADD  ENTRY TO  LINKED LIST
 * Note: If list is entry, this will push to the front. Otherwise, it will 
 * push to the back */
void rrpush(int sequence, int socket, int fd, float bytes_left, int quantum){
	int empty = 0;
	if(rrisEmpty())
		empty=1;
	struct rrnode *link = (struct rrnode*) malloc(sizeof(struct rrnode));
	struct rrnode*ptr = rrhead;
	link->sequence = sequence;
	link->socket = socket;
	link->fd = fd;
	link->bytes_left = bytes_left;
	link->quantum = quantum;
	if(empty==1){						// push front
		link->rrnext = rrhead;
		rrhead = link;
	}else{								// push back
		while(ptr->rrnext!=NULL)
			ptr=ptr->rrnext;
		ptr->rrnext = link;
	}
}

/* REMOVE ENTRY FROM FRONT OF LINKED LIST */
struct rrnode *rrpop_front(void){
	struct rrnode *tempLink = rrhead;
	rrhead = rrhead->rrnext;
	return tempLink;
}

/******************************************************************************
 * 											END OF FILE
 *****************************************************************************/


