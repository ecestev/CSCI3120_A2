/* 
 * File: sjf.h
 * Author: Stephen Sampson
 * Purpose: This file contains the prototype for the
 * shortest job first scheduler to serve HTTP requests.
 */

#include <stdio.h>

/* 
 * This module contains the SJF scheduling algorithm. 
 * sjf(int port, int debug) should be called if the SJF
 * scheduler is requested via command line arguments
 */
 
 void sjf(int port, int debug);

