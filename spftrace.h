/*
 * =====================================================================================
 *
 *       Filename:  spftrace.h
 *
 *    Description:  This file defines the enums for TRACE categories
 *
 *        Version:  1.0
 *        Created:  Friday 16 February 2018 02:09:45  IST
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

#ifndef __SPF_TRACE__
#define __SPF_TRACE__

typedef enum{

    DIJKSTRA_BIT,
    ROUTE_INSTALLATION_BIT,
    BACKUP_COMPUTATION_BIT,
    ROUTE_CALCULATION_BIT,
    SPF_PREFIX_BIT,
    ROUTING_TABLE_BIT,
    CONFLICT_RESOLUTION_BIT,
    SPF_EVENTS_BIT,
    SPRING_ROUTE_CAL_BIT,
    TRACE_MAX_BIT
} spf_trace_t;

void
enable_spf_tracing();

void
disable_spf_tracing();

char *
get_str_trace(spf_trace_t trace_type);

void
display_spf_traces();

typedef struct instance_ instance_t;

void
enable_spf_trace(instance_t *instance, spf_trace_t spf_trace);

void
disable_spf_trace(instance_t *instance, spf_trace_t spf_trace);

/* 1 if enabled, 0 if disabled*/
unsigned char
is_spf_trace_enabled(instance_t *instance, spf_trace_t spf_trace);

void
spf_display_trace_options();

#define __ENABLE_TRACE__
#endif /* __SPF_TRACE__ */
