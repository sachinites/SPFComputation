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

#include "instanceconst.h"
#include "LinkedListApi.h"

#define DEFAULT_PREFIX_METRIC   0
#define PREFIX_DOWNBIT_FLAG     1
#define STUBBED_PREFIX          2 
         
/* Key structure for a prefix*/
typedef struct common_pfx_{

    char prefix[PREFIX_LEN + 1];
    unsigned char mask;
} common_pfx_key_t;

typedef struct _node_t node_t;

typedef struct prefix_{

    char prefix[PREFIX_LEN + 1];
    unsigned char mask;/*Numeric value [0-32]*/
    unsigned int metric;/*Prefix metric, zero for local prefix, non-zeroi for leaked or external prefixes*/
    unsigned int prefix_flags;
    node_t *hosting_node;   /*back pointer to hosting node*/
    /*Extras*/
    unsigned char ref_count; /*For internal use*/
} prefix_t;


prefix_t *
create_new_prefix(const char *prefix, unsigned char mask);

#define STR_PREFIX(prefix_t_ptr)    (prefix_t_ptr ? prefix_t_ptr->prefix : "NIL")
#define PREFIX_MASK(prefix_t_ptr)   (prefix_t_ptr ? prefix_t_ptr->mask : 0)

#define BIND_PREFIX(target_ptr, prefix_ptr) \
    target_ptr = prefix_ptr;                \
    if(target_ptr) prefix_ptr->ref_count++;

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
init_prefix_key(prefix_t *prefix, char *_prefix, char mask);

void
set_prefix_flag(unsigned int flag);

typedef int (*comparison_fn)(void *, void *);
typedef int (*order_comparison_fn)(void *, void *);

comparison_fn
get_prefix_comparison_fn();

order_comparison_fn
get_prefix_order_comparison_fn();

/*This fn leak the prefix from L2 to L1*/
int
leak_prefix(char *node_name, char *prefix, char mask, 
            LEVEL from_level, LEVEL to_level);

void
fill_prefix(prefix_t *prefix, common_pfx_key_t *common_prefix,
            unsigned int metric, boolean downbit);

/*We need prefix list management routines for route_t->like_prefix_list
 * and node_t->local_prefix_list*/

typedef struct routes_ routes_t;

void
add_prefix_to_prefix_list(ll_t *prefix_list, prefix_t *prefix);

void
delete_prefix_from_prefix_list(ll_t *prefix_list, char *prefix, char mask);

#endif /* __ROUTES__ */

