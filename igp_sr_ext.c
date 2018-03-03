/*
 * =====================================================================================
 *
 *       Filename:  igp_sr_ext.c
 *
 *    Description:  Implementation of SR extnsion for IGPs
 *
 *        Version:  1.0
 *        Created:  Friday 02 March 2018 07:56:04  IST
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

#include "igp_sr_ext.h"
#include "LinkedListApi.h"
#include "instance.h"
#include "advert.h"
#include "Queue.h"
#include "spfutil.h"
#include <arpa/inet.h>

extern instance_t *instance;

boolean
is_identical_mapping_entries(sr_mapping_entry_t *mapping_entry1,
    sr_mapping_entry_t *mapping_entry2){

    if(mapping_entry1->prf == mapping_entry2->prf &&
        mapping_entry1->pi == mapping_entry2->pi &&
        mapping_entry1->pe == mapping_entry2->pe &&
        mapping_entry1->pfx_len == mapping_entry2->pfx_len &&
        mapping_entry1->si == mapping_entry2->si &&
        mapping_entry1->se == mapping_entry2->se &&
        mapping_entry1->range_value == mapping_entry2->range_value &&
        mapping_entry1->algorithm == mapping_entry2->algorithm &&
        mapping_entry1->topology == mapping_entry2->topology)
            return TRUE;
     return FALSE;
}

void
construct_prefix_mapping_entry(prefix_t *prefix, 
            sr_mapping_entry_t *mapping_entry_out){
    
    if(prefix->hosting_node->is_srms)
        mapping_entry_out->prf = IGP_DEFAULT_SID_SRMS_PFX_PREFERENCE_VALUE;
    else
        mapping_entry_out->prf = IGP_DEFAULT_SID_PFX_PREFERENCE_VALUE;

    char subnet[PREFIX_LEN + 1];
    unsigned int binary_prefix = 0;
    
    memset(subnet, 0, PREFIX_LEN + 1);
    apply_mask(prefix->prefix, prefix->mask, subnet);
    inet_pton(AF_INET, subnet, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    mapping_entry_out->pi          = binary_prefix;
    mapping_entry_out->pe          = binary_prefix;
    mapping_entry_out->pfx_len     = prefix->mask;
    mapping_entry_out->si          = PREFIX_SID_VALUE(prefix);
    mapping_entry_out->se          = PREFIX_SID_VALUE(prefix);
    mapping_entry_out->range_value = 1;
    mapping_entry_out->algorithm   = prefix->prefix_sid->algorithm;
    mapping_entry_out->topology    = 0;
}


boolean
is_prefixes_conflicting(sr_mapping_entry_t *pfx_mapping_entry1,
    sr_mapping_entry_t *pfx_mapping_entry2){

    /*topology check*/
    if(pfx_mapping_entry1->topology != pfx_mapping_entry2->topology)
        return FALSE;

    /*algorithm check*/
    if(pfx_mapping_entry1->algorithm != pfx_mapping_entry2->algorithm)
        return FALSE;

    /*address family*/
    if(pfx_mapping_entry1->max_pfx_len != pfx_mapping_entry2->max_pfx_len)
        return FALSE;

    /*prefix length*/
    if(pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len)
        return FALSE;

    /*simplifying by assuming Range is always 1*/
    /*prefix*/
    if(pfx_mapping_entry1->pi != pfx_mapping_entry2->pi)
        return FALSE;

    /*SID*/
    if(pfx_mapping_entry1->si != pfx_mapping_entry2->si)
        return TRUE;

    return FALSE;
}

boolean
is_prefixes_sid_conflicting(sr_mapping_entry_t *pfx_mapping_entry1,
    sr_mapping_entry_t *pfx_mapping_entry2){

    if(is_identical_mapping_entries(pfx_mapping_entry1, pfx_mapping_entry2))
        return FALSE;

    /* If the two prefixes have same SIDs but they different in some way, 
     * then it is SID conflict*/
    if((pfx_mapping_entry1->si == pfx_mapping_entry2->si)                   &&
        (pfx_mapping_entry1->algorithm != pfx_mapping_entry2->algorithm     ||
         pfx_mapping_entry1->pi != pfx_mapping_entry2->pi                   ||
         pfx_mapping_entry1->topology != pfx_mapping_entry2->topology       ||
         pfx_mapping_entry1->max_pfx_len != pfx_mapping_entry2->max_pfx_len ||
         pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len))
        return TRUE;

    return FALSE;
}

static void
free_global_prefix_list(ll_t *global_prefix_list){

    delete_singly_ll(global_prefix_list);
    free(global_prefix_list);
}

static int 
global_prefix_list_comparison_fn(void *data1, void *data2){

    singly_ll_node_t *list_node1 = data1,
                     *list_node2 = data2;

    prefix_t *prefix1 = list_node1->data;
    prefix_t *prefix2 = list_node2->data;

    unsigned int binary_prefix1 = 0, 
                 binary_prefix2 = 0;

    char subnet[PREFIX_LEN + 1];
    memset(subnet, 0, PREFIX_LEN + 1);
    
    apply_mask(prefix1->prefix, prefix1->mask, subnet);
    inet_pton(AF_INET, subnet, &binary_prefix1);
    binary_prefix1 = htonl(binary_prefix1);
    
    memset(subnet, 0, PREFIX_LEN + 1);
    apply_mask(prefix2->prefix, prefix2->mask, subnet);
    inet_pton(AF_INET, subnet, &binary_prefix2);
    binary_prefix2 = htonl(binary_prefix2);

    if(binary_prefix1 < binary_prefix2)
        return -1;
    if(binary_prefix1 > binary_prefix2)
        return 1;
    return 0; 
}

/* Generate a global prefix list. This list contains all prefixes advertised by
 * all nodes in a given level*/

static ll_t *
build_global_prefix_list(node_t *node, LEVEL level){

    node_t *curr_node = NULL,
    *nbr_node = NULL, 
    *pn_node = NULL;

    edge_t *edge1 = NULL,
           *edge2 = NULL;

    singly_ll_node_t *list_node = NULL;
    prefix_t *prefix = NULL;

    init_instance_traversal(instance);
    Queue_t *q = initQ();
    node->traversing_bit = 1;
    
    enqueue(q, node);

    ll_t *global_pfx_lst = init_singly_ll();
    singly_ll_set_comparison_fn(global_pfx_lst, get_prefix_comparison_fn());

    while(!is_queue_empty(q)){
        curr_node = deque(q);
        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(curr_node, nbr_node, pn_node, edge1,
                edge2, level){

            if(nbr_node->traversing_bit){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(curr_node, nbr_node, pn_node, level);
            }

            ITERATE_LIST_BEGIN(nbr_node->local_prefix_list[level], list_node){
                prefix = list_node->data;
                if(prefix->prefix_sid)
                    singly_ll_add_node_by_val(global_pfx_lst, list_node->data);
            } ITERATE_LIST_END;
            
            nbr_node->traversing_bit = 1;
            enqueue(q, nbr_node);
        } ITERATE_NODE_PHYSICAL_NBRS_END(curr_node, nbr_node, pn_node, level);
    }

    /*Add self list*/
    ITERATE_LIST_BEGIN(node->local_prefix_list[level], list_node){
        prefix = list_node->data;
        if(prefix->prefix_sid)
            singly_ll_add_node_by_val(global_pfx_lst, list_node->data);
    } ITERATE_LIST_END;

    return global_pfx_lst;
}


void
resolve_prefix_sid_conflict(prefix_t *prefix1, sr_mapping_entry_t *pfx_mapping_entry1,
        prefix_t *prefix2, sr_mapping_entry_t *pfx_mapping_entry2){

    assert(IS_PREFIX_SR_ACTIVE(prefix1) && 
            IS_PREFIX_SR_ACTIVE(prefix2));

    if(pfx_mapping_entry1->prf != pfx_mapping_entry2->prf){
        
        if(pfx_mapping_entry1->prf > pfx_mapping_entry2->prf)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
         return;
    }
        
    
    if(pfx_mapping_entry1->range_value != pfx_mapping_entry2->range_value){

        if(pfx_mapping_entry1->range_value < pfx_mapping_entry2->range_value)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }


    if(pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len){

        if(pfx_mapping_entry1->pfx_len < pfx_mapping_entry2->pfx_len)
            MARK_PREFIX_SR_INACTIVE(prefix1);
        else
            MARK_PREFIX_SR_INACTIVE(prefix2);
        return;
    }

    if(pfx_mapping_entry1->pi != pfx_mapping_entry2->pi){

        if(pfx_mapping_entry1->pi < pfx_mapping_entry2->pi)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }

    if(pfx_mapping_entry1->algorithm != pfx_mapping_entry2->algorithm){

        if(pfx_mapping_entry1->algorithm < pfx_mapping_entry2->algorithm)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }

#if 0
    if(pfx_mapping_entry1->si != pfx_mapping_entry2->si){

        if(pfx_mapping_entry1->si < pfx_mapping_entry2->si)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }
#endif

    if(pfx_mapping_entry1->topology != pfx_mapping_entry2->topology){
        MARK_PREFIX_SR_INACTIVE(prefix1);
        MARK_PREFIX_SR_INACTIVE(prefix2);
        return;
    }
    else{
        return;
    }
    assert(0);
}

void
resolve_prefix_conflict(prefix_t *prefix1, sr_mapping_entry_t *pfx_mapping_entry1,
        prefix_t *prefix2, sr_mapping_entry_t *pfx_mapping_entry2){

    assert(IS_PREFIX_SR_ACTIVE(prefix1) && 
            IS_PREFIX_SR_ACTIVE(prefix2));

    if(pfx_mapping_entry1->prf != pfx_mapping_entry2->prf){
        
        if(pfx_mapping_entry1->prf > pfx_mapping_entry2->prf)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
         return;
    }
        
    
    if(pfx_mapping_entry1->range_value != pfx_mapping_entry2->range_value){

        if(pfx_mapping_entry1->range_value < pfx_mapping_entry2->range_value)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }


    if(pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len){

        if(pfx_mapping_entry1->pfx_len < pfx_mapping_entry2->pfx_len)
            MARK_PREFIX_SR_INACTIVE(prefix1);
        else
            MARK_PREFIX_SR_INACTIVE(prefix2);
        return;
    }
#if 0
    if(pfx_mapping_entry1->pi != pfx_mapping_entry2->pi){

        if(pfx_mapping_entry1->pi < pfx_mapping_entry2->pi)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }
#endif
    if(pfx_mapping_entry1->si != pfx_mapping_entry2->si){

        if(pfx_mapping_entry1->si < pfx_mapping_entry2->si)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }
    assert(0);
}

ll_t *
prefix_sid_conflict_resolution(ll_t *global_prefix_list){

    singly_ll_node_t *list_node1 = NULL,
                     *list_node2 = NULL;

    sr_mapping_entry_t mapping_entry1, 
                       mapping_entry2;
                     
    prefix_t *prefix1 = NULL,
             *prefix2 = NULL;

    ITERATE_LIST_BEGIN(global_prefix_list, list_node1){

        prefix1 = list_node1->data;
        if(!IS_PREFIX_SR_ACTIVE(prefix1)) continue;

        construct_prefix_mapping_entry(prefix1, &mapping_entry1); 
        for(list_node2 = list_node1->next; list_node2; list_node2 = list_node2->next){
            
            prefix2 = list_node2->data;
            if(!IS_PREFIX_SR_ACTIVE(prefix2)) continue;
            
             construct_prefix_mapping_entry(prefix2, &mapping_entry2); 
             if(is_prefixes_sid_conflicting(&mapping_entry1, &mapping_entry2) == FALSE)
                 continue;

             resolve_prefix_sid_conflict(prefix1, &mapping_entry1, 
                    prefix2, &mapping_entry2);
        } 
    } ITERATE_LIST_END;

    /*Clean up the weed out ones from the global list*/ 
    ITERATE_LIST_BEGIN2(global_prefix_list, list_node1, list_node2){

        prefix1 = list_node1->data;
        if(!IS_PREFIX_SR_ACTIVE(prefix1)){
            ITERATIVE_LIST_NODE_DELETE2(global_prefix_list, list_node1, list_node2);
        }
    } ITERATE_LIST_END2(global_prefix_list, list_node1, list_node2);

    return global_prefix_list;    
}
/*This is the highest level function which takes responsibility 
 * to resolve prefix conflicts */

ll_t *
prefix_conflict_resolution(node_t *node, LEVEL level){

   singly_ll_node_t *list_node1 = NULL,
                    *list_node2 = NULL;
                    
   sr_mapping_entry_t mapping_entry1, 
                      mapping_entry2;
                     
   prefix_t *prefix1 = NULL,
            *prefix2 = NULL;


   ll_t *global_prefix_list = build_global_prefix_list(node, level);
                    
   ITERATE_LIST_BEGIN(global_prefix_list, list_node1){
       
       prefix1 = list_node1->data;
       if(!IS_PREFIX_SR_ACTIVE(prefix1)) 
           continue;

       construct_prefix_mapping_entry(prefix1, &mapping_entry1);

       for(list_node2 = list_node1->next; list_node2; list_node2 = list_node2->next){
        
         prefix2 = list_node2->data;
         if(!IS_PREFIX_SR_ACTIVE(prefix2))
             continue;

         construct_prefix_mapping_entry(prefix2,  &mapping_entry2);

         if(is_prefixes_conflicting(&mapping_entry1, &mapping_entry2) == FALSE)
             continue;
        /*Now resolve the conflict between prefix1 and prefix 2*/
         resolve_prefix_conflict(prefix1, &mapping_entry1, prefix2, &mapping_entry2);
         
         assert((IS_PREFIX_SR_ACTIVE(prefix1) && !IS_PREFIX_SR_ACTIVE(prefix2)) || 
            (!IS_PREFIX_SR_ACTIVE(prefix1) &&  IS_PREFIX_SR_ACTIVE(prefix2)));
       }
   } ITERATE_LIST_END;

   /*Now cleanup the prefixes which are filtered out and will not be considered
    * for SID conflict resolution phase*/

    ITERATE_LIST_BEGIN2(global_prefix_list, list_node1, list_node2){

        prefix1 = list_node1->data;
        if(!IS_PREFIX_SR_ACTIVE(prefix1)){
            ITERATIVE_LIST_NODE_DELETE2(global_prefix_list, list_node1, list_node2);
        }
    } ITERATE_LIST_END2(global_prefix_list, list_node1, list_node2);
   
   return global_prefix_list;
}
