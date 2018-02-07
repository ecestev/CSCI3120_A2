/* 
 * File: sjf.h
 * Author: Stephen Sampson
 * Purpose: This file contains the prototypes and describes how to use the 
 *          shortest job first scheduler to serve HTTP requests.
 */

#include <stdio.h>

/* 
 * This module contains the RR scheduling algorithm.
 * rr(int port, int debug) should be called if the RR 
 * scheduler is requested via command line arguments.
 */

 void rr(int port, int debug);
