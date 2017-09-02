/*
 * =====================================================================================
 *
 *       Filename:  prefix.h
 *
 *    Description:  This file defines the data structure for routes and prefixes to be installed in RIB
 *
 *        Version:  1.0
 *        Created:  Wednesday 30 August 2017 02:09:28  IST
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

#ifndef __PREFIX__
#define __PREFIX__

#include "graphconst.h"

#define DEFAULT_PREFIX_METRIC   0
#define PREFIX_DOWNBIT_FLAG     1 
         
/* Key structure for a prefix*/
typedef struct common_pfx_{

    char prefix[PREFIX_LEN + 1];
    unsigned char mask;
} common_pfx_key_t;

typedef struct prefix_{

    char prefix[PREFIX_LEN + 1];
    unsigned char mask;/*Numeric value [0-32]*/
    unsigned int metric;/*Prefix metric, zero for local prefix, non-zeroi for leaked or external prefixes*/
    unsigned int prefix_flags;
    unsigned char ref_count; /*For internal use*/
} prefix_t;

prefix_t *
create_new_prefix(const char *prefix, unsigned char mask);

#define STR_PREFIX(prefix_t_ptr)    (prefix_t_ptr ? prefix_t_ptr->prefix : "NIL")
#define PREFIX_MASK(prefix_t_ptr)   (prefix_t_ptr ? prefix_t_ptr->mask : 0)

#define BIND_PREFIX(target_ptr, prefix_ptr) \
    target_ptr = prefix_ptr;                \
    prefix_ptr->ref_count++;

#define UNBIND_PREFIX(target_ptr)                   \
    if(target_ptr){                                 \
        target_ptr->ref_count--;                    \
        if(!target_ptr->ref_count){                 \
            free(target_ptr);                       \
        }                                           \
        target_ptr = NULL;                          \
    }
    
void
set_prefix_property_metric(prefix_t *prefix, 
                           unsigned int metric);

void
set_prefix_flag(unsigned int flag);

typedef int (*comparison_fn)(void *, void *);

comparison_fn
get_prefix_comparison_fn();

/*This fn leak the prefix from L2 to L1*/
void
leak_prefix(char *node_name, char *prefix, char mask);

#endif /* __ROUTES__ */

