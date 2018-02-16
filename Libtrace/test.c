/*
 * =====================================================================================
 *
 *       Filename:  test.c
 *
 *    Description:  Test file
 *
 *        Version:  1.0
 *        Created:  Friday 16 February 2018 01:41:53  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the Libtrace distribution (https://github.com/sachinites).
 *        Copyright (c) 2017 Abhishek Sagar.
 *        This program is free software: you can redistribute it and/or modify
 *        it under the terms of the GNU General Public License as published by  
 *        the Free Software Foundation, version 3.
 *
 *        This program is distributed in the hope that it will be useful, but 
 *        WITHOUT ANY WARRANTY; without even the implied warranty of 
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 *        General Public License for more details.
 *
 *        You should have received a copy of the GNU General Public License 
 *        along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "libtrace.h"

typedef enum{
    LOG1,
    LOG2,
    LOG3
} LOG_T;

int
main(int argc, char **argv){
    
    traceoptions traceopts;
    init_trace(&traceopts);
    enable_trace_event(&traceopts, LOG1);
    enable_trace_event(&traceopts, LOG2);
    trace_enable(&traceopts, TRUE);
    sprintf(traceopts.b, "I am LOG1"); 
    TRACE(&traceopts, LOG1);
    sprintf(traceopts.b, "I am LOG2"); 
    TRACE(&traceopts, LOG2);
    sprintf(traceopts.b, "I am LOG3"); 
    TRACE(&traceopts, LOG3);
    return 0;
}
