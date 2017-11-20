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

#define DEFAULT_LOCAL_PREFIX_METRIC         0
#define DEFAULT_PHYSICAL_INTF_PFX_METRIC    LINK_DEFAULT_METRIC

/* Prefix flags*/
#define PREFIX_DOWNBIT_FLAG         0/*0th bit*/
#define PREFIX_EXTERNABIT_FLAG      1/*Ist bit*/
#define PREFIX_METRIC_TYPE_EXT      2/*2nd bit*/

/* Preferences - RFC 5302 For TLV128/130. We also incorporate RFC 7775 for routes advertised by TLV
 * 135,235,236,237. Pls note that, RFC 5302 explicitely written for IPv4 takes complete care of RFC 
 * 7775 as well which is written for IPV4|IPV6. However, for the sake of clarity we have intoduced 
 * RFC 7775 same level enums*/

typedef enum{

    L1_INT_INT = 0,    /* 5302 : L1 Internal routes with interbal metric, 7775 3.3 : L1 Intra area routes*/
    L1_EXT_INT = 0,    /* 5302 : L1 External routes with internal metric, 7775 3.3 : L1 External routes*/
    L2_INT_INT = 1,    /* 5302 : L2 Internal routes with internal metric, 7775 3.3 : L2 Intra area routes*/
    L2_EXT_INT = 1,    /* 5302 : L2 External routes with internal metric, 7775 3.3 : L2 external routes*/
    L1_L2_INT_INT = 1, /* 5302 : L1->L2 Internal routes with internal metric, 7775 3.3 : L1->L2 inter-area routes*/
    L1_L2_EXT_INT = 1, /* 5302 : L1->L2 External routes with internal metric, 7775 3.4 : L1-L2 external routes*/
    L2_L2_EXT_INT = 1, /* 7775 3.3 : L2-L2 inter-area routes */
    L2_L1_INT_INT = 2, /* 5302 : L2->L1 Internal routes with internal metric,  7775 3.3 : L2->L1 inter-area routes*/
    L2_L1_EXT_INT = 2, /* 5302 : L2->L1 External routes with internal metric, 7775 3.1 Dont know why RFC 7775 3.1 do not list this case*/
    L1_L1_EXT_INT = 2, /* 7775 3.3 : L1->L1 inter-area routes */
    L1_EXT_EXT = 3,    /* 5302 : L1 External routes with  External metric*/
    L2_EXT_EXT = 4,    /* 5302 : L2 External routes with  External metric*/
    L1_L2_EXT_EXT = 4, /* 5302 : L1->L2 External routes with  External metric*/
    L2_L1_EXT_EXT = 5, /* 5302 : L2->L1 External routes with  External metric*/
    ROUTE_UNKNOWN_PREFERENCE = 6
} prefix_preference_t;

typedef struct prefix_pref_data_t_{

    prefix_preference_t pref;
    char *pref_str;
} prefix_pref_data_t;
   
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
    FLAG prefix_flags;
    node_t *hosting_node;   /*back pointer to hosting node*/
    /*Extras*/
    unsigned char ref_count; /*For internal use*/
} prefix_t;

FLAG
is_prefix_byte_equal(prefix_t *prefix1, prefix_t *prefix2, unsigned int prefix2_hosting_node_metric);

prefix_t *
create_new_prefix(const char *prefix, unsigned char mask);

#define STR_PREFIX(prefix_t_ptr)    (prefix_t_ptr ? prefix_t_ptr->prefix : "NIL")
#define PREFIX_MASK(prefix_t_ptr)   (prefix_t_ptr ? prefix_t_ptr->mask : 0)

#define BIND_PREFIX(target_ptr, prefix_ptr) \
    target_ptr = prefix_ptr;                \
    target_ptr->metric = DEFAULT_PHYSICAL_INTF_PFX_METRIC; \
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
prefix_t *
leak_prefix(char *node_name, char *prefix, char mask, 
            LEVEL from_level, LEVEL to_level);

/*We need prefix list management routines for route_t->like_prefix_list
 * and node_t->local_prefix_list*/

typedef struct routes_ routes_t;

FLAG
add_prefix_to_prefix_list(ll_t *prefix_list, prefix_t *prefix, unsigned int hosting_node_metric);

void
delete_prefix_from_prefix_list(ll_t *prefix_list, char *prefix, char mask);

prefix_pref_data_t
route_preference(FLAG route_flags, LEVEL level);


#endif /* __ROUTES__ */

