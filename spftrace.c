/*
 * =====================================================================================
 *
 *       Filename:  spftrace.c
 *
 *    Description:  This file implements the tracing mechanism for this project
 *
 *        Version:  1.0
 *        Created:  Friday 16 February 2018 02:13:04  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the SPFComputation distribution (https://github.com/sachinites).
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

#include "instance.h"
#include "spftrace.h"
#include "Libtrace/libtrace.h"

extern instance_t *instance;

char 
*get_str_trace(spf_trace_t trace_type){
    switch(trace_type){
        case DIJKSTRA_BIT:
            return "DIJKSTRA_BIT";
        case ROUTE_INSTALLATION_BIT:
            return "ROUTE_INSTALLATION_BIT";
        case ROUTE_CALCULATION_BIT:
            return "ROUTE_CALCULATION_BIT";
        case BACKUP_COMPUTATION_BIT:
            return "BACKUP_COMPUTATION_BIT";
        case SPF_EVENTS_BIT:
            return "SPF_EVENTS_BIT";
        case SPF_PREFIX_BIT:
            return "SPF_PREFIX_BIT";
        case ROUTING_TABLE_BIT:
            return "ROUTING_TABLE_BIT";
        case CONFLICT_RESOLUTION_BIT:
            return "CONFLICT_RESOLUTION_BIT";
        case SPRING_ROUTE_CAL_BIT:
            return "SPRING_ROUTE_CAL_BIT";
        case TRACE_MAX_BIT:
            return "TRACE_MAX_BIT";
        default:
            return "UNKNOWN_TRACE";
    }
    return NULL; 
}

void
enable_spf_tracing(){
    instance->traceopts->enable = TR_TRUE;
}

void
disable_spf_tracing(){
    instance->traceopts->enable = TR_FALSE;
}

void
enable_spf_trace(instance_t *instance, spf_trace_t spf_trace){
    TR_SET_BIT(instance->traceopts->bit_mask, spf_trace);
}

void
disable_spf_trace(instance_t *instance, spf_trace_t spf_trace){
    TR_UNSET_BIT(instance->traceopts->bit_mask, spf_trace);
}

unsigned char
is_spf_trace_enabled(instance_t *instance, spf_trace_t spf_trace){

    return TR_IS_BIT_SET(instance->traceopts->bit_mask, spf_trace);
}

void
_spf_display_trace_options(unsigned long long bit_mask){

    spf_trace_t trace_type = 0;
    printf("Logging : %s\n", instance->traceopts->enable ? "ENABLE" : "DISABLE");
    printf("log storage medium set : %s\n", 
        instance->traceopts->logstorage == CONSOLE ? "CONSOLE" : "FILE");
    printf("traceopts->logf_fd = 0x%x\n", (unsigned int)instance->traceopts->logf_fd);
    for(; trace_type < TRACE_MAX_BIT; trace_type++){
        printf("\t%-30s : %s\n", get_str_trace(trace_type), 
            is_spf_trace_enabled(instance, trace_type) ? "ENABLED" : "DISABLED");
    }
}

void
spf_display_trace_options(){
    instance->traceopts->display_trace_options(instance->traceopts->bit_mask);
}
