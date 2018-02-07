/* 
 * File: sjf.c
 * Author: Stephen Sampson
 * Purpose: This file contains the shortest job first module to serve HTTP 
 * 			requests.
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
struct node{
	int sequence;					// sequence number of the job coming in
	int socket;						// socket number to send the file on
	int fd;							// file descriptor of the file requested
	float bytes_left;				// remaining bytes left to transfer to client
	int quantum;					// how many bytes will be sent at a time
	struct node *next;			// pointer to the next node in linked list
};
struct node *head = NULL;		// pointer to the head of the linked list
struct node *current = NULL; 	// last entry in linked list points to NULL

/* LINKED LIST FUNCTION PROTOTYPES */
bool isEmpty(void);				// determines whether linked list has any entries
int inQueue(void);				// determines how many jobs are in the queue
void printList(void);			// debugging only - prints all jobs in queue
/* adds new node at the beginning of the linked list */
void push_front(int sequence, int socket, int fd, float bytes_left, int quantum);
struct node *pop_front(void);	// removes job at front of linked list
void sort(void);					// sorts linked list by job size

/* SJF SCHEDULING ALGORITHM EXECUTION */
void sjf(int port_number, int debug) {
	int client_socket = -1;		// socket the client connected on
	int sequence = 0;				// global sequence counter - how many jobs
	int quantum = 2147479552;	// maximum size to be sent at once
	char **request_elements = calloc(3, sizeof(char**));	
	char *to_parse = calloc(256, sizeof(char));	// function to parse request
	char *location = calloc(256, sizeof(char));	// location of requested file
	FILE *requested_file;		// the requested file
	struct node *processing;	// job currently being processed
	int seqip;						// sequence number of job in progress
	int sockip;						// socket number of job in progress
	int fdip;						// file descriptor of job in progress
	float blip;						// bytes left of job in progress
	int quantip;					// quantum for job in progress
	network_init(port_number);	// initialize network on specified port
	while(1){
		if (inQueue()==0){		// check if queue is empty
			network_wait();		// if queue is empty, put program to sleep
		}
		/* The code within the following while loop is used to interpret and queue
		 * any valid waiting requests. */
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
					msg400(client_socket);		// malformed request. Error 400
					close(client_socket);		// close socket connection
				}else {
					/* if the request is valid, look for the requested file */
					if((strcmp(request_elements[0],"GET")==0)&&
					(strcmp(request_elements[2],"HTTP/1.1")==0)){
						location = request_elements[1];
						location++;
						/* if requested file is where it should be, open it and get 
						 * required information on the file then add the job to the 
						 * queue */
						if(fopen( location, "r" ) != NULL){
							requested_file = fopen( location, "r");
							int fd = -1;				// var to store file descriptor
							fd = fileno(requested_file);	// store file descriptor
							struct stat buf;			// create a buffer
							fstat(fd, &buf);			// retrieve file statistics
							float filesize = buf.st_size;	// find file size
							sequence++;				// increment glkobal seq counter
							/* push job to queue with all required info */
							push_front(sequence, client_socket, fd, 
							filesize, quantum);
							sort();							// sort queue by job size
							msg200(client_socket);		// request ok!
						} 	else {
				 			msg404(client_socket);	// fopen() returned NULL. Error404
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
						memset(request_elements[i], '\0', 
						strlen(request_elements[i]));
				}
			}
		}
		/* If request queue is not empty, process first request in queue */
		if(!isEmpty()){						// check if queue empty
			processing = pop_front();		// pop first entry store in node
			seqip = processing->sequence;	// store sequence of popped job
			sockip = processing->socket;	//	store socket of popped job
			fdip = processing->fd;			// store fd of popped job
			blip = processing->bytes_left;//	store bytes left of popped job
			quantip = processing->quantum;//	store quantum of popped job
			if(blip<=quantip){				// this will always be true in sjf
				lseek(fdip, (off_t)-blip, SEEK_END);// set location in file
				sendfile(sockip, fdip, NULL, blip);	// send entire file
				fflush(stdout);				// flush stdout buffer
				printf("Request %d Completed\n", seqip);	// print to console
				close(fdip);					// close file descriptor in progress
				close(sockip);					// close socket in progress
			} else {
				/* this section is just to handle files that exceed the filesize
				 * limit of sendfile by sending it in multiple pieces... It is 
				 * not enabled currently as the loop above will not add a job
				 * to the queue if its filesize exceeds sendfile() capabilities */
				sendfile(sockip,fdip,NULL,quantip);
				blip=blip-quantip;
				push_front(seqip, sockip, fdip, blip, quantip);
			}
		}
	}
}

/******************************************************************************
 *									LINKED LIST FUNCTIONS
 *  				ADAPTED FROM TUTORIALSPOINT.COM. FULL LINK ABOVE
 *****************************************************************************/
 
/* CHECK IF LIST IS EMPTY */
bool isEmpty(void){
	return head == NULL;
}

/* DETERMINE NUMBER OF ELEMENTS IN LIST */
int inQueue(void) {
	int length = 0;
	struct node *current;
	for(current = head; current != NULL; current = current->next){
		length++;
	}
	return length;
}

/* PRINT THE LINKED LIST TO CONSOLE */
void printList(void){
	struct node *ptr = head;
	printf("\n[");
	while(ptr != NULL){
		printf("(%d,%d,%d,%f,%d)",ptr->sequence, ptr->socket, ptr->fd, 
		ptr->bytes_left, ptr->quantum);
		ptr = ptr->next;
	}
	printf("]\n\n");
}

/* ADD ENTRY TO FRONT OF LINKED LIST */
void push_front(int sequence, int socket, int fd, float bytes_left, int quantum){
	struct node *link = (struct node*) malloc(sizeof(struct node));
	link->sequence = sequence;
	link->socket = socket;
	link->fd = fd;
	link->bytes_left = bytes_left;
	link->quantum = quantum;
	link->next = head;
	head = link;
}

/* REMOVE ENTRY FROM FRONT OF LINKED LIST */
struct node *pop_front(void){
	struct node *tempLink = head;
	head = head->next;
	return tempLink;
}

/* SORT LINKED LIST BY INCREASING VALUE OF 'BYTES_LEFT' */
void sort(void){
	int i, j, k, tempseq, tempsock, tempfd, tempquant;
	float tempbl;
	struct node *current;
	struct node *next;
	int size = inQueue();
	k = size;
	for(i=0;i<size-1;i++,k--){
		current = head;
		next = head->next;
		for(j=1;j<k;j++){
			/* CHECK IF NEXT ENTRY HAS MORE BYTES LEFT THAN CURRENT ENTRY.
			 * IF SO, SWAP THEM */
			if(current->bytes_left > next->bytes_left) {
				tempseq = current->sequence;
				current->sequence = next->sequence;
				next->sequence = tempseq;
				tempsock=current->socket;
				current->socket = next->socket;
				next->socket = tempsock;
				tempfd = current->fd;
				current->fd = next->fd;
				next->fd = tempfd;
				tempbl = current->bytes_left;
				current->bytes_left = next->bytes_left;
				next->bytes_left = tempbl;
				tempquant = current->quantum;
				current->quantum = next->quantum;
				next->quantum = tempquant;
			/* IF CURRENT ENTRY AND NEXT ENTRY HAVE THE SAME AMOUNT OF BYTES
			 * REMAINING TO TRANSFER, SORT THEM IN ORDER OF SEQUENCE NUMBER */
			} 	else if((current->bytes_left==next->bytes_left)&&
			(current->sequence>next->sequence)){
				tempseq = current->sequence;
				current->sequence = next->sequence;
				next->sequence = tempseq;
				tempsock=current->socket;
				current->socket = next->socket;
				next->socket = tempsock;
				tempfd = current->fd;
				current->fd = next->fd;
				next->fd = tempfd;
				tempbl = current->bytes_left;
				current->bytes_left = next->bytes_left;
				next->bytes_left = tempbl;
				tempquant = current->quantum;
				current->quantum = next->quantum;
				next->quantum = tempquant;
			}
			current = current->next;
			next = next->next;
		}
	}
}

/******************************************************************************
 * 											END OF FILE
 *****************************************************************************/


