/*
 * =====================================================================================
 *
 *       Filename:  libtrace.c
 *
 *    Description:  Implementation of Libtrace API
 *
 *        Version:  1.0
 *        Created:  Saturday 17 February 2018 02:43:38  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the Libtrace  distribution (https://github.com/sachinites).
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

#include "libtrace.h"

char fn_line_buff[32];

void
trace_enable(traceoptions *traceopts, tr_boolean enable){
        traceopts->enable = enable;
}

void
init_trace(traceoptions *traceopts){
        memset(traceopts, 0, sizeof(traceoptions));
        traceopts->logstorage = CONSOLE;
}

void
set_trace_storage(traceoptions *traceopts, log_storage_t logstorage);

void
trace_set_log_medium(traceoptions *traceopts, log_storage_t logstorage){

    traceopts->logstorage = logstorage;
    if(logstorage == CONSOLE){
        if(traceopts->logf_fd){
            fclose(traceopts->logf_fd);
            traceopts->logf_fd = NULL;
        }
    }
    else{
        /* File*/
        if(!traceopts->logf_fd)
            traceopts->logf_fd = fopen("log.0", "a");
    }
}

void
register_display_trace_options(traceoptions *traceopts,
        void (*display_trace_options)(unsigned long long)){
    traceopts->display_trace_options = display_trace_options;
}

void
enable_trace_event(traceoptions *traceopts, unsigned long long bit){
    TR_SET_BIT(traceopts->bit_mask, bit);
}


void
disable_trace_event(traceoptions *traceopts, unsigned long long bit){
    TR_UNSET_BIT(traceopts->bit_mask, bit);
}

