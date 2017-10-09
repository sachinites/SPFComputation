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

#include "spfutil.h"
#include "prefix.h"
#include <stdlib.h>
#include <string.h>
#include "LinkedListApi.h"
#include "instance.h"
#include <stdio.h>
#include "bitsop.h"

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

comparison_fn
get_prefix_comparison_fn(){
    return prefix_comparison_fn;
}

THREAD_NODE_TO_STRUCT(prefix_t,     
                      like_prefix_thread, 
                      get_prefix_from_like_prefix_thread);

void
leak_prefix(char *node_name, char *_prefix, char mask, LEVEL from_level, LEVEL to_level){

    node_t *node = NULL;
    prefix_t *prefix = NULL;

    if(!instance){
        printf("%s() : Network Graph is NULL\n", __FUNCTION__);
        return;
    }

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    common_pfx_key_t pfx_key;
    memset(&pfx_key, 0, sizeof(common_pfx_key_t));
    strncpy((char *)&pfx_key.prefix, _prefix, strlen(_prefix));
    pfx_key.mask = mask;

    prefix = (prefix_t *)singly_ll_search_by_key(GET_NODE_PREFIX_LIST(node, from_level), (void *)&pfx_key);
    if(!prefix){
       printf("%s() : Error : Node : %s, LEVEL : %u, Prefix : %s do not exist\n", 
                        __FUNCTION__, node->node_name, from_level, _prefix); 
       return;
    }

    /*Now add this prefix to L1 prefix list of node*/
    if(singly_ll_search_by_key(GET_NODE_PREFIX_LIST(node, to_level), (void *)&pfx_key)){
        printf("%s () : Error : Node : %s, prefix : %s already leaked\n", __FUNCTION__, node->node_name, STR_PREFIX(prefix));
        return;
    }

    prefix_t *leaked_prefix = calloc(1, sizeof(prefix_t));
    
    strncpy(leaked_prefix->prefix, prefix->prefix, strlen(prefix->prefix));
    leaked_prefix->mask = prefix->mask;
    leaked_prefix->metric = prefix->metric;
    leaked_prefix->prefix_flags = prefix->prefix_flags;/*Abhishek : Not sure if flags needs to be copied as it is, will revisit later if it creates some problem*/
    leaked_prefix->ref_count = 0; /*This is relevant only to interface prefixes*/

    SET_BIT(leaked_prefix->prefix_flags, PREFIX_DOWNBIT_FLAG);

    singly_ll_add_node_by_val(GET_NODE_PREFIX_LIST(node, to_level), leaked_prefix);
    leaked_prefix->hosting_node = node;
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

