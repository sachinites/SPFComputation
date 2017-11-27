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
create_new_prefix(const char *prefix, unsigned char mask, LEVEL level){

    prefix_t *_prefix = calloc(1, sizeof(prefix_t));
    if(prefix)
        strncpy(_prefix->prefix, prefix, strlen(prefix));
    _prefix->prefix[PREFIX_LEN] = '\0';
    _prefix->mask = mask;
     _prefix->level = level;
    set_prefix_property_metric(_prefix, DEFAULT_LOCAL_PREFIX_METRIC);
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

/*Currently it decides based only on metric, in future we will
 * enhance this fn with other parameters such as external/internal routes
 * inter area/intra area routes etc*/
static int
prefix_order_comparison_fn(void *_prefix1, void *_prefix2){

    prefix_t *prefix1 = _prefix1;
    prefix_t *prefix2 = _prefix2;

    if(prefix1->metric < prefix2->metric)
        return -1;
    if(prefix1->metric == prefix2->metric)
        return 0;
    if(prefix1->metric > prefix2->metric)
        return 1;
    return 0;
}

prefix_pref_data_t
route_preference(FLAG route_flags, LEVEL level){

/* Same preference 0*/
    prefix_pref_data_t pref;

    if(level == LEVEL1 &&
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) && 
       !IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = L1_INT_INT;
            pref.pref_str = "L1_INT_INT";
            return pref;
    }

    if(level == LEVEL1 &&
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) && 
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = L1_EXT_INT;
            pref.pref_str = "L1_EXT_INT";
            return pref;
    }

/* Same preference 1*/
    if(level == LEVEL2 &&
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) && 
       !IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = L2_INT_INT;
            pref.pref_str = "L2_INT_INT";
            return pref;
    }

    if(level == LEVEL2 &&
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) &&  
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
             pref.pref =  L2_EXT_INT;
             pref.pref_str = "L2_EXT_INT";
            return pref;
    }

    if(level == LEVEL2 && 
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref =  L1_L2_INT_INT;
            pref.pref_str = "L1_L2_INT_INT";
            return pref;
    }

    if(level == LEVEL2 && 
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref =  L1_L2_EXT_INT;
            pref.pref_str = "L1_L2_EXT_INT";
            return pref;
    }


    if(level == LEVEL2 &&
       (IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) || 1) && /* Up/Down bit is set but ignored as if it is unset*/
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref =  L2_L2_EXT_INT;
            pref.pref_str = "L2_L2_EXT_INT";
            return pref;
    }

/* Same preference 2*/
    if(level == LEVEL1 && 
        IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref =  L2_L1_INT_INT;
            pref.pref_str = "L2_L1_INT_INT";
            return pref;
    }

    if(level == LEVEL1 && 
        IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = L2_L1_EXT_INT;
            pref.pref_str = "L2_L1_EXT_INT";
            return pref;
    }
    
    if(level == LEVEL1 &&
        IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
       !IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = L1_L1_EXT_INT;
            pref.pref_str = "L1_L1_EXT_INT";
            return pref;
    }

/* Same preference 3*/
    if(level == LEVEL1 && 
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = L1_EXT_EXT;
            pref.pref_str = "L1_EXT_EXT";
            return pref;
    }
    
/* Same preference 4*/
    if(level == LEVEL2 &&
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) && 
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = L2_EXT_EXT;
            pref.pref_str = "L2_EXT_EXT";
            return pref;
    }

    if(level == LEVEL2 && 
       !IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
             pref.pref = L1_L2_EXT_EXT;
             pref.pref_str = "L1_L2_EXT_EXT";
            return pref;
    }

/* Same preference 5*/
    if(level == LEVEL1 &&
        IS_BIT_SET(route_flags, PREFIX_DOWNBIT_FLAG) && 
        IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = L2_L1_EXT_EXT;
            pref.pref_str = "L2_L1_EXT_EXT";
            return pref;
    }

/* Such a prefix is meant to be ignored*/
    if(!IS_BIT_SET(route_flags, PREFIX_EXTERNABIT_FLAG) &&
        IS_BIT_SET(route_flags, PREFIX_METRIC_TYPE_EXT)){
            pref.pref = ROUTE_UNKNOWN_PREFERENCE;
            pref.pref_str = "ROUTE_UNKNOWN_PREFERENCE";
            return pref;
    }

    assert(0);
    pref.pref = ROUTE_UNKNOWN_PREFERENCE;
    pref.pref_str = "ROUTE_UNKNOWN_PREFERENCE";
    return pref; /* Make compiler happy*/
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


order_comparison_fn
get_prefix_order_comparison_fn(){
    return prefix_order_comparison_fn;
}

#if 0
THREAD_NODE_TO_STRUCT(prefix_t,     
                      like_prefix_thread, 
                      get_prefix_from_like_prefix_thread);
#endif
/* Returns the prefix being leaked from L2 to L1 (Or otherwise). If the prefix is already
 * leaked, return NULL. This fn simply add the new prefix to new prefix list.*/

prefix_t *
leak_prefix(char *node_name, char *_prefix, char mask, 
                LEVEL from_level, LEVEL to_level){

    node_t *node = NULL;
    prefix_t *prefix = NULL, *leaked_prefix = NULL,
              prefix_key;
    routes_t *route_to_be_leaked = NULL;

    if(!instance){
        printf("%s() : Network Graph is NULL\n", __FUNCTION__);
        return NULL;
    }

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    prefix = node_local_prefix_search(node, from_level, _prefix, mask);
       
    /* Case 1 : leaking prefix on a local hosting node */ 
    if(prefix){
   
        /*Now add this prefix to to_level prefix list of node*/
        if(node_local_prefix_search(node, to_level, _prefix, mask)){
            printf("%s () : Error : Node : %s, prefix : %s already leaked/present in %s\n", 
            __FUNCTION__, node->node_name, STR_PREFIX(prefix), get_str_level(to_level));
            return NULL;
        }

        leaked_prefix = attach_prefix_on_node (node, prefix->prefix, prefix->mask, 
                        to_level, prefix->metric, prefix->prefix_flags);
        if(!leaked_prefix){
            sprintf("Node : %s, equal best prefix : %s already leaked/present in %s\n",
                node->node_name, STR_PREFIX(prefix), get_str_level(to_level)); TRACE();
            return NULL;
        }
        leaked_prefix->ref_count = 0;

        if(from_level == LEVEL2 && to_level == LEVEL1)
            SET_BIT(leaked_prefix->prefix_flags, PREFIX_DOWNBIT_FLAG);

        sprintf(LOG, "Node : %s : prefix %s/%u leaked from %s to %s", 
                node->node_name, STR_PREFIX(prefix), PREFIX_MASK(prefix), get_str_level(from_level), get_str_level(to_level));
                TRACE();

        return leaked_prefix;
    }

    /* case 2 : Leaking prefix on a remote node*/
    if(!prefix){
        /*if prefix do not exist, means, we are leaking remote prefix on a node*/

        init_prefix_key(&prefix_key, _prefix, mask);
        route_to_be_leaked = search_route_in_spf_route_list(&node->spf_info, &prefix_key, from_level);
        
        /*Route must exist on remote node*/
        if(!route_to_be_leaked){
            printf("%s() : Node : %s : Error : route %s/%u do not exist in %s\n", 
                    __FUNCTION__, node->node_name, 
                    _prefix, mask, 
                    get_str_level(from_level));
            return NULL;
        }

        /* Even though the route for a prefix being leaked is already present in leaking level(destination level),
         * We will go ahead and leak the prefix. This is because, leaked prefix may have better
         * route than the route already present in leaking level for the same prefix. For example, in build_multi_area_topo()
         * if 192.168.0.1 is a local prefix on node R1 in LEVEL1, and node R0 leaks this prefix in LEVEL2,
         * then node R5 will have LEVEL2 cost = 40 for route 192.168.0.1. Now, if node R2 also leaks the LEVEL1
         * prefix 192.168.0.1 to LEVEL2, then node R5 will have LEVEL2 cost = 20 for route 192.168.0.1 which
         * is better than previous cost(40). Hence, we should not prevent node R2 from leaking L1 prefix
         * in L2 domain even if the prefix is already present in L2 domain. By leaking the prefix, node
         * actually conveys to connected nodes in the domain its own cost to reach the leaked prefix which
         * could be better than rest of the nodes currently have */
#if 0
         if(search_route_in_spf_route_list(&node->spf_info, &prefix_key, to_level)){
            printf("%s() : Node : %s : INFO : route %s/%u already present in %s\n", 
                    __FUNCTION__, node->node_name,  
                    _prefix, mask, 
                    get_str_level(to_level));
            return -1;
        }
#endif
        /*We need to add this remote route which is leaked from L2 to L1 in native L1 prefix list
         * so that L1 router can compute route to this leaked prefix by running full spf run */

        leaked_prefix = attach_prefix_on_node (node, _prefix, mask, 
                        to_level, IS_BIT_SET(route_to_be_leaked->flags, PREFIX_EXTERNABIT_FLAG) ? 
                        route_to_be_leaked->ext_metric : route_to_be_leaked->spf_metric, 0);

        if(!leaked_prefix){
            sprintf("Node : %s, equal best prefix : %s already leaked/present in %s\n",
                node->node_name, STR_PREFIX(prefix), get_str_level(to_level)); TRACE();
            return NULL;
        }

        leaked_prefix->prefix_flags = route_to_be_leaked->flags;
        leaked_prefix->ref_count = 0;

        if(from_level == LEVEL2 && to_level == LEVEL1)
            SET_BIT(leaked_prefix->prefix_flags, PREFIX_DOWNBIT_FLAG);

        sprintf(LOG, "Node : %s : prefix %s/%u leaked from %s to %s", 
                node->node_name, route_to_be_leaked->rt_key.prefix, 
                route_to_be_leaked->rt_key.mask, get_str_level(from_level), 
                get_str_level(to_level));  TRACE();

        return leaked_prefix;
    }
    return NULL;
}

void
add_new_prefix_in_list(ll_t *prefix_list , prefix_t *prefix, 
                unsigned int prefix_hosting_node_metric){

    singly_ll_node_t *list_node_prev = NULL, 
                     *list_node_next = NULL;

    prefix_t *list_prefix = NULL;

    /* empty list*/
    if(is_singly_ll_empty(prefix_list)){
        singly_ll_add_node_by_val(prefix_list, prefix);
        return;
    }

    /* Only one node*/
    if(GET_NODE_COUNT_SINGLY_LL(prefix_list) == 1){
        if(prefix->metric + prefix_hosting_node_metric < ((prefix_t *)(prefix_list->head->data))->metric){
            singly_ll_add_node_by_val(prefix_list, prefix);
        }
        else{
            singly_ll_node_t *new_node = singly_ll_init_node(prefix);
            prefix_list->head->next = new_node;
            INC_NODE_COUNT_SINGLY_LL(prefix_list);
        }
        return;
    }

    /* If prefix need to be added as first node in a list*/
    if(prefix->metric + prefix_hosting_node_metric < 
                ((prefix_t *)(GET_HEAD_SINGLY_LL(prefix_list)->data))->metric){

        singly_ll_node_t *new_node = singly_ll_init_node(prefix);
        new_node->next = GET_HEAD_SINGLY_LL(prefix_list);
        prefix_list->head = new_node;
        INC_NODE_COUNT_SINGLY_LL(prefix_list);
        return;
    }


    ITERATE_LIST_BEGIN(prefix_list, list_node_next){

        list_prefix = list_node_next->data;
        if(!(prefix->metric + prefix_hosting_node_metric < list_prefix->metric)){ 

            list_node_prev = list_node_next;
            continue;
        }

        singly_ll_node_t *new_node = singly_ll_init_node(prefix);
        new_node->next = list_node_next;
        list_node_prev->next = new_node;
        INC_NODE_COUNT_SINGLY_LL(prefix_list);
        return;

    }ITERATE_LIST_END;

    /*Add in the end*/
    singly_ll_node_t *new_node = singly_ll_init_node(prefix);
    list_node_prev->next = new_node;
}

FLAG
is_prefix_byte_equal(prefix_t *prefix1, prefix_t *prefix2, 
                    unsigned int prefix2_hosting_node_metric){

    if(strncmp(prefix1->prefix, prefix2->prefix, PREFIX_LEN) == 0   &&
        prefix1->mask == prefix2->mask                              &&
        prefix1->metric == prefix2->metric + prefix2_hosting_node_metric                         &&
        prefix1->hosting_node == prefix2->hosting_node)
            return 1;
    return 0;
}


/* Let us delegate all add logic to this fn*/
/* Returns 1 if prefix added, 0 if rejected*/
FLAG
add_prefix_to_prefix_list(ll_t *prefix_list, 
                          prefix_t *prefix, 
                          unsigned int hosting_node_metric){

    common_pfx_key_t key;
    strncpy(key.prefix, prefix->prefix, PREFIX_LEN);
    key.prefix[PREFIX_LEN] = '\0'; 
    key.mask = prefix->mask;

#if 0
    prefix_t *old_prefix = NULL;

    old_prefix = singly_ll_search_by_key(prefix_list, &key);

    if(old_prefix){        
       if(is_prefix_byte_equal(old_prefix, prefix, hosting_node_metric))
           return 0; 
    }
#endif
    assert(!singly_ll_search_by_key(prefix_list, &key));

    add_new_prefix_in_list(prefix_list, prefix, hosting_node_metric);
    return 1;
}

void
delete_prefix_from_prefix_list(ll_t *prefix_list, char *prefix, char mask){

    prefix_t *old_prefix = NULL;
    common_pfx_key_t key;
    strncpy(key.prefix, prefix, PREFIX_LEN);
    key.prefix[PREFIX_LEN] = '\0';
    key.mask = mask;
    old_prefix = singly_ll_search_by_key(prefix_list, &key);
    assert(old_prefix);
    singly_ll_delete_node_by_data_ptr(prefix_list, old_prefix);
    free(old_prefix);
    old_prefix = NULL;
}


