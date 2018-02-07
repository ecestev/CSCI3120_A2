#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include "network.h"

/*
 * A simple function to parse the HTTP request. This function
 * will store as many white space separated series of characters
 * as a string until there is no more room in the array of strings.
 */
 
void parse(char **request_elements, char *to_parse){
	char **ap;
 	char *inputstring = to_parse;
	for(ap=request_elements;(*ap=strsep(&inputstring, " \t\n\r")) != NULL;)
		if(**ap != '\0')
			if(++ap >= &request_elements[3])
				break;
}
	
void msg400(int socket) {
	char *msg400	= "HTTP/1.1 400 BAD REQUEST\n\n"; 			  /* response 400 */
	write(socket, msg400, strlen(msg400));
}

void msg404(int socket) {
	char *msg404 = "HTTP/1.1 404 FILE NOT FOUND\n\n"; 			  /* response 404 */
	write(socket, msg404, strlen(msg404));
}

void msg200(int socket) {
	char *msg200 = "HTTP/1.1 200 OK\n\n"; 							  /* response 200 */
	write(socket, msg200, strlen(msg200));
}

void filsizerr(int socket) {
	char *filsizerr = "Requested File Exceeds capabilities of sendfile()\n\n";
	write(socket, filsizerr, strlen(filsizerr));
}
