/*
 * =====================================================================================
 *
 *       Filename:  libtrace.h
 *
 *    Description:  Defines the libtrace data structures
 *
 *        Version:  1.0
 *        Created:  Friday 16 February 2018 01:05:18  IST
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

#ifndef __LIBTRACE__
#define __LIBTRACE__

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#define TR_IS_BIT_SET(n, pos)  ((n & (1 << (pos))) != 0)
#define TR_TOGGLE_BIT(n, pos)  (n = n ^ (1 << (pos)))
#define TR_COMPLEMENT(num)     (num = num ^ 0xFFFFFFFFFFFFFFFF)
#define TR_UNSET_BIT(n, pos)   (n = n & ((1 << pos) ^ 0xFFFFFFFFFFFFFFFF))
#define TR_SET_BIT(n, pos)     (n = n | 1 << pos)


typedef enum{
    TR_FALSE,
    TR_TRUE
} tr_boolean;

typedef enum{
    
    CONSOLE,
    LOG_FILE
} log_storage_t;

typedef struct {
    
    char b[256];
    unsigned long long bit_mask;
    tr_boolean enable;
    void (*display_trace_options)(unsigned long long);
    log_storage_t logstorage;
    FILE *logf_fd;
} traceoptions;

void
trace_enable(traceoptions *traceopts, tr_boolean enable);

void 
init_trace(traceoptions *traceopts);

void
set_trace_storage(traceoptions *traceopts, log_storage_t logstorage);

extern char fn_line_buff[32];

#define trace(traceopts_ptr, bit)                                                       \
    if((traceopts_ptr)->enable == TR_TRUE){                                             \
        if(TR_IS_BIT_SET((traceopts_ptr)->bit_mask, bit)){                              \
            if((traceopts_ptr)->logstorage == CONSOLE)                                  \
                printf("%s(%d) : %s\n", __FUNCTION__, __LINE__, (traceopts_ptr)->b);    \
            else  {                                                                     \
                memset(fn_line_buff, 0, 32);                                            \
                sprintf(fn_line_buff, "%s(%u) : ", __FUNCTION__, __LINE__);             \
                fwrite(fn_line_buff, sizeof(char), strlen(fn_line_buff),                \
                    (traceopts_ptr)->logf_fd);                                          \
                fwrite((traceopts_ptr)->b,  sizeof(char),                               \
                    strlen((traceopts_ptr)->b), (traceopts_ptr)->logf_fd);              \
                fwrite("\n", 1, 1, (traceopts_ptr)->logf_fd);                           \
            }                                                                           \
            memset((traceopts_ptr)->b, 0, strlen((traceopts_ptr)->b));                  \
        }                                                                               \
}

void
trace_set_log_medium(traceoptions *traceopts, log_storage_t logstorage);

void
register_display_trace_options(traceoptions *traceopts, 
    void (*display_trace_options)(unsigned long long));

void
enable_trace_event(traceoptions *traceopts, unsigned long long bit);

void
disable_trace_event(traceoptions *traceopts, unsigned long long bit);

#endif /* __LIBTRACE__ */
