/*
 * =====================================================================================
 *
 *       Filename:  conflct_res.c
 *
 *    Description:  This file implements routines for conflict resolution of Spring Segment IDs.
 *
 *        Version:  1.0
 *        Created:  Thursday 12 April 2018 09:50:17  IST
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

#include "routes.h"
#include "LinkedListApi.h"
#include "sr_tlv_api.h"
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

    if(prefix->hosting_node->am_i_mapping_server)
        mapping_entry_out->prf = IGP_DEFAULT_SID_SRMS_PFX_PREFERENCE_VALUE;
    else
        mapping_entry_out->prf = IGP_DEFAULT_SID_PFX_PREFERENCE_VALUE;

    char subnet[PREFIX_LEN + 1];
    unsigned int binary_prefix = 0;
    prefix_sid_subtlv_t *prefix_sid = get_prefix_sid(prefix);

    memset(subnet, 0, PREFIX_LEN + 1);
    apply_mask(prefix->prefix, prefix->mask, subnet);
    inet_pton(AF_INET, subnet, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    mapping_entry_out->pi          = binary_prefix;
    mapping_entry_out->pe          = binary_prefix;
    mapping_entry_out->pfx_len     = prefix->mask;
    mapping_entry_out->max_pfx_len = 32;
    mapping_entry_out->si          = prefix_sid->sid.sid;
    mapping_entry_out->se          = prefix_sid->sid.sid;
    mapping_entry_out->range_value = 1;
    mapping_entry_out->algorithm   = prefix_sid->algorithm;
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
     *      * then it is SID conflict*/
    if((pfx_mapping_entry1->si == pfx_mapping_entry2->si)                   &&
            (pfx_mapping_entry1->algorithm != pfx_mapping_entry2->algorithm     ||
             pfx_mapping_entry1->pi != pfx_mapping_entry2->pi                   ||
             pfx_mapping_entry1->topology != pfx_mapping_entry2->topology       ||
             pfx_mapping_entry1->max_pfx_len != pfx_mapping_entry2->max_pfx_len ||
             pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len))
        return TRUE;

    return FALSE;
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

            if(!IS_PREFIX_SR_ACTIVE(prefix1))
                break;
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

ll_t *
prefix_conflict_resolution(node_t *node, LEVEL level){

    singly_ll_node_t *list_node1 = NULL,
                     *list_node2 = NULL;

    sr_mapping_entry_t mapping_entry1,
                       mapping_entry2;

    prefix_t *prefix1 = NULL,
             *prefix2 = NULL;


    ll_t *global_prefix_list = NULL; //build_global_prefix_list(node, level);

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

            if(!IS_PREFIX_SR_ACTIVE(prefix1))
                break;

            assert((IS_PREFIX_SR_ACTIVE(prefix1) && !IS_PREFIX_SR_ACTIVE(prefix2)) ||
                    (!IS_PREFIX_SR_ACTIVE(prefix1) &&  IS_PREFIX_SR_ACTIVE(prefix2)));
        }
    } ITERATE_LIST_END;


    /*Now cleanup the prefixes which are filtered out and will not be considered
     *     * for SID conflict resolution phase*/

    ITERATE_LIST_BEGIN2(global_prefix_list, list_node1, list_node2){

        prefix1 = list_node1->data;
        if(!IS_PREFIX_SR_ACTIVE(prefix1)){
            ITERATIVE_LIST_NODE_DELETE2(global_prefix_list, list_node1, list_node2);
        }
    } ITERATE_LIST_END2(global_prefix_list, list_node1, list_node2);

    return global_prefix_list;
}

