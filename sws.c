/******************************************************************************
 ******************************************************************************

							 CSCI3120 - OPERATING SYSTEMS
					  ASSIGNMENT #2  - A SCHEDULING WEB SERVER
				       			
   	 					 	    STEPHEN SAMPSON
					         	    B00568374
						  JUNE 2016 - DALHOUSIE UNIVERSITY

 ******************************************************************************
 ******************************************************************************/

	/* NOTE: I have made an improvement in the implementation of sendfile()
	 * since assignment one removing the ~2GB restricion by using floating 
	 * point values to represent the size of the file and the remaining bytes 
	 * left to transfer. With the quantums implemented here this allows us to 
	 * use sendfile() to transfer files of virtually any size 
	 * (up to 316,912,308,335,304,260,253,906,250,000 GB in size)
	 */

/******************************************************************************
 *                              INTIALIZATION                                 *
 ******************************************************************************/

/* Header files used in this program  */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sjf.h"
#include "rr.h"
#include "mlfb.h"

/* Function Prototypes  */
int request_port(void);
int validate_port(int requested_port);
int request_scheduler(void);
int determine_scheduler(char *desired_scheduler);
	
/******************************************************************************
 *                    				MAIN FUNCTION                                *
 ******************************************************************************/

int main(int argc, char *argv[]) {
	/* SET UP AND INITIALIZE VARIABLES */
	int port_number = 0; 							/* port to initialize network on */
	int scheduler = -1;									 /* 0 = SJF, 1 = RR, 2 = MLFB */
	int debug = 0; 	  /* debugging mode. If enabled, diagnostic info printed */
	char cwd[256];						     /* string for current working directory */
	/* CONFIGURE PROGRAM BEHAVIOR BASED ON COMMAND LINE ARGS  */
	if(argc==1){										 /* no arguments on command line */
		port_number = request_port();								   /* request a port */
		scheduler = request_scheduler();						 /* request a scheduler */
	}	
	if(argc==2){
		port_number = atoi(argv[1]);				/* port declared on command line */
		scheduler = request_scheduler(); 					 /* request a scheduler */
	}
	if(argc==3){
		port_number = atoi(argv[1]);				/* port declared on command line */
		scheduler = determine_scheduler(argv[2]); 		 /* interpret scheduler */
	}
	if(argc==4) {
		port_number = atoi(argv[1]);				/* port declared on command line */
		scheduler = determine_scheduler(argv[2]);			 /* interpret scheduler */
		if (strcmp(argv[3],"-d")==0){;							/* verify debug mode */
			debug = 1;							 /* set debug true if meets conditions */
			printf("\nDebugging Enabled\n");
			printf("Program is running from: %s\n", getcwd(cwd, sizeof(cwd)));
		}
	}
	/* CALL APPROPRIATE SCHEDULER. EACH SCHEDULER IS DEFINED IN ITS OWN */
	/* HEADER FILE AND CODE IS CONTAINED WITHIN ITS OWN C FILE */
	if(debug==1)
		printf("Scheduler Mode: %d. Note: 0 = SJF, 1 = RR, 2 = MLFB.\n", scheduler);
	if(scheduler==0)
		sjf(port_number, debug);
	else if(scheduler==1)
		rr(port_number, debug);
	else if(scheduler==2)
		mlfb(port_number, debug);
	else
		printf("Invalid Scheduler Selected... Closing Program\n");
	return 0;		
}

/******************************************************************************
 *                           EXTERNAL FUNCTIONS                               *
 * ****************************************************************************/

/*
 * For debugging purposes or if no port is specified on the command line. 
 * Simply requests a port number and returns that value
 */
int request_port(void) {
	int requested_port = 0;
	printf("Enter a port number between 1024 and 65535: ");
	/* Scan user input for port and store for later use */
	scanf("%d", &requested_port);
	return requested_port;
}

/*
 *  For debugging purposes - validates that specified port is acceptable. If 
 *  not, continues requesting a new port until an acceptable value has been 
 *  entered at which point this value returned to the main and stored as a 
 *  variable to be used when initializing the network 
 */
int validate_port(int validate_port) {
	while((validate_port<1024) | (validate_port>65535)) {
		printf("\nInvalid Port: %d...\n", validate_port);
		validate_port = request_port();
	}
	printf("Port Validated\n");
	printf("Waiting for Client Connection\n");
	return validate_port;
}

/*
 * Scheduler not declared on command line. Request scheduler from user.
 */
int request_scheduler(void) {
	int scheduler = -1;
	int desired_scheduler = -1;
	while(scheduler < 0) {
		printf("\n**************************************************");
		printf("**************************************************\n");
		printf("The Following Schedulers are supported in this program:\n\n");
		printf("Shortest Job First (SJF) - 0\n");
		printf("Round Robbin (RR) - 1\n");
		printf("Multi Level Queue with Feedback (MLFB) - 2\n\n");
		printf("Please Select Which Scheduler You Would Like To Use: ");
		scanf("%d", &desired_scheduler);
		printf("**************************************************");
		printf("**************************************************\n");
		if(desired_scheduler==0){
			scheduler = 0;
		} else if(desired_scheduler==1){
			scheduler = 1;
		} else if(desired_scheduler==2){
			scheduler = 2;
		}
	}
	return scheduler;
}

/* 
 * Scheduler declared on command line. Interpret command line arguement and 
 * return integer value corresponding to one of the three implemented scheduler. 
 * Request scheduler if unknown scheduler entered on command line
 */
int determine_scheduler(char *desired_scheduler){
	int scheduler = -1;
	if(strcmp(desired_scheduler,"SJF")==0){
		scheduler = 0;
	} else if(strcmp(desired_scheduler,"RR")==0){
		scheduler = 1;
	} else if(strcmp(desired_scheduler,"MLFB")==0){
		scheduler = 2;
	}
	if(scheduler<0){
		scheduler = request_scheduler();
	}
	return scheduler;
}

/******************************************************************************
 *                              END OF FILE                                   *
 * ****************************************************************************/
		

