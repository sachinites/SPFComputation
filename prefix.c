/*
 * =====================================================================================
 *
 *       Filename:  prefix.c
 *
 *    Description:  Implementation of prefix.h
 *
 *        Version:  1.0
 *        Created:  Wednesday 30 August 2017 02:14:15  IST
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spfutil.h"
#include "prefix.h"
#include "LinkedListApi.h"
#include "instance.h"
#include "bitsop.h"
#include "logging.h"
#include "routes.h"

extern instance_t *instance;

prefix_t *
create_new_prefix(const char *prefix, unsigned char mask){

    prefix_t *_prefix = calloc(1, sizeof(prefix_t));
    if(prefix)
        strncpy(_prefix->prefix, prefix, strlen(prefix));
    _prefix->prefix[PREFIX_LEN] = '\0';
    _prefix->mask = mask;
    set_prefix_property_metric(_prefix, DEFAULT_PREFIX_METRIC);
    return _prefix;
}

void
set_prefix_property_metric(prefix_t *prefix,
                           unsigned int metric){

    prefix->metric = metric;
}

static int
prefix_comparison_fn(void *_prefix, void *_key){

    prefix_t *prefix = (prefix_t *)_prefix;
    common_pfx_key_t *key = (common_pfx_key_t *)_key;
    if(strncmp(prefix->prefix, key->prefix, strlen(prefix->prefix)) == 0 &&
            strlen(prefix->prefix) == strlen(key->prefix) &&
            prefix->mask == key->mask)
        return TRUE;

    return FALSE;
}

void
init_prefix_key(prefix_t *prefix, char *_prefix, char mask){

    memset(prefix, 0, sizeof(prefix_t));
    apply_mask(_prefix, mask, prefix->prefix);
    prefix->prefix[PREFIX_LEN] = '\0';
    prefix->mask = mask;
}


comparison_fn
get_prefix_comparison_fn(){
    return prefix_comparison_fn;
}

THREAD_NODE_TO_STRUCT(prefix_t,     
                      like_prefix_thread, 
                      get_prefix_from_like_prefix_thread);

/* Returns the metric of the prefix being leaked from L2 to L1. If such a prefix,
 * do not exist, return NULL*/
int
leak_prefix(char *node_name, char *_prefix, char mask, LEVEL from_level, LEVEL to_level){

    node_t *node = NULL;
    prefix_t *prefix = NULL;

    if(!instance){
        printf("%s() : Network Graph is NULL\n", __FUNCTION__);
        return -1;
    }

    if(from_level != LEVEL2){

        printf("%s() : Error : Prefix leaking is allowed only from Level 2 to Level 1\n", __FUNCTION__);
        return -1;
    }
    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    common_pfx_key_t pfx_key;
    memset(&pfx_key, 0, sizeof(common_pfx_key_t));
    strncpy((char *)&pfx_key.prefix, _prefix, strlen(_prefix));
    pfx_key.mask = mask;

    prefix = (prefix_t *)singly_ll_search_by_key(GET_NODE_PREFIX_LIST(node, from_level), (void *)&pfx_key);
    
    if(prefix){
        /*Now add this prefix to L1 prefix list of node*/
        if(singly_ll_search_by_key(GET_NODE_PREFIX_LIST(node, to_level), (void *)&pfx_key)){
            printf("%s () : Error : Node : %s, prefix : %s already leaked\n", __FUNCTION__, node->node_name, STR_PREFIX(prefix));
            return -1;
        }

        prefix_t *leaked_prefix = calloc(1, sizeof(prefix_t));

        strncpy(leaked_prefix->prefix, prefix->prefix, strlen(prefix->prefix));
        leaked_prefix->mask = prefix->mask;
        leaked_prefix->metric = prefix->metric;
        leaked_prefix->prefix_flags = prefix->prefix_flags;/*Abhishek : Not sure if flags needs to be copied as it is, will revisit later if it creates some problem*/
        leaked_prefix->ref_count = 0; /*This is relevant only to interface prefixes*/
        SET_BIT(leaked_prefix->prefix_flags, PREFIX_DOWNBIT_FLAG);
        leaked_prefix->hosting_node = prefix->hosting_node;
        singly_ll_add_node_by_val(GET_NODE_PREFIX_LIST(node, to_level), leaked_prefix);
        sprintf(LOG, "Node : %s : prefix %s/%u leaked from %s to %s", 
                node->node_name, STR_PREFIX(prefix), PREFIX_MASK(prefix), get_str_level(from_level), get_str_level(to_level));
                TRACE();
        return leaked_prefix->metric;
    }

    if(!prefix){
        /*if prefix do not exist, means, we are leaking remote prefix on a node*/
        common_pfx_key_t rt_key;
        apply_mask(_prefix, mask, rt_key.prefix);
        rt_key.prefix[PREFIX_LEN] = '\0';
        rt_key.mask = mask;
        routes_t *route_to_be_leaked = singly_ll_search_by_key(node->spf_info.routes_list, &rt_key);
        if(!route_to_be_leaked){
            printf("%s() : Node : %s : Error : route %s/%u do not exist in %s\n", 
                    __FUNCTION__, node->node_name, 
                    _prefix, mask, 
                    get_str_level(from_level));
            return -1;
        }

         if(IS_LEVEL_SET(to_level, route_to_be_leaked->level)){
            printf("%s() : Node : %s : INFO : route %s/%u already present in %s\n", 
                    __FUNCTION__, node->node_name,  
                    _prefix, mask, 
                    get_str_level(to_level));
            return -1;
        }

        SET_BIT(route_to_be_leaked->flags, PREFIX_DOWNBIT_FLAG);
        route_to_be_leaked->level |= to_level; /*This route is present in both levels now*/
        
        /*We need to add this remote route which is leaked from L2 to L1 in native L1 prefix list
         * so that L1 router can compute route to this leaked prefix by running full spf run */

        prefix_t *leaked_prefix = calloc(1, sizeof(prefix_t));
        strncpy(leaked_prefix->prefix, route_to_be_leaked->rt_key.prefix, PREFIX_LEN);
        leaked_prefix->prefix[PREFIX_LEN] = '\0';
        leaked_prefix->mask = route_to_be_leaked->rt_key.mask;
        leaked_prefix->metric = route_to_be_leaked->spf_metric;
        leaked_prefix->prefix_flags = route_to_be_leaked->flags;/*Abhishek : Not sure if flags needs to be copied as it is, will revisit later if it creates some problem*/
        leaked_prefix->ref_count = 0; /*This is relevant only to interface prefixes*/
        SET_BIT(leaked_prefix->prefix_flags, PREFIX_DOWNBIT_FLAG);
        leaked_prefix->hosting_node = route_to_be_leaked->hosting_node;
        singly_ll_add_node_by_val(GET_NODE_PREFIX_LIST(node, to_level), leaked_prefix);
        sprintf(LOG, "Node : %s : prefix %s/%u leaked from %s to %s", 
                node->node_name, STR_PREFIX(prefix), PREFIX_MASK(prefix), get_str_level(from_level), get_str_level(to_level));
                TRACE();
        sprintf(LOG, "Node : %s : prefix %s/%u leaked from %s to %s", 
                node->node_name, route_to_be_leaked->rt_key.prefix, 
                route_to_be_leaked->rt_key.mask, get_str_level(from_level), 
                get_str_level(to_level));
                TRACE();
        return route_to_be_leaked->spf_metric;
    }
    return -1;
}

/*We assume the caller has zeroes out the memory of the prefix*/
void
fill_prefix(prefix_t *prefix, common_pfx_key_t *common_prefix,
        unsigned int metric, boolean downbit){

    strncpy(prefix->prefix, common_prefix->prefix, PREFIX_LEN);
    prefix->prefix[PREFIX_LEN] = '\0';
    prefix->mask = common_prefix->mask;
    prefix->metric = metric;
    if(downbit)
        SET_BIT(prefix->prefix_flags, PREFIX_DOWNBIT_FLAG);
}

