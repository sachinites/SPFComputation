/*
 * =====================================================================================
 *
 *       Filename:  sr_tlv_api.c
 *
 *    Description:  implements the API to manipulate SR related TLVs
 *
 *        Version:  1.0
 *        Created:  Monday 26 February 2018 01:49:08  IST
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

#include "sr_tlv_api.h"
#include "instance.h"
#include "prefix.h"
#include "spfutil.h"
#include "bitsop.h"

void
diplay_prefix_sid(prefix_t *prefix){

    char subnet[PREFIX_LEN_WITH_MASK + 1];

    memset(subnet, 0, PREFIX_LEN_WITH_MASK + 1);
    apply_mask2(prefix->prefix, prefix->mask, subnet);
    if(prefix->prefix_sid){
        printf("\t%-18s : %-8u %s\n", subnet, PREFIX_SID_VALUE(prefix), 
            IS_PREFIX_SR_ACTIVE(prefix) ? "ACTIVE" : "INACTIVE");
        printf("\t\tFLAGS : R:%c N:%c P:%c E:%c V:%c L:%c\n", 
        IS_BIT_SET(prefix->prefix_sid->flags, RE_ADVERTISEMENT_R_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix->prefix_sid->flags, NODE_SID_N_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix->prefix_sid->flags, NO_PHP_P_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix->prefix_sid->flags, EXPLICIT_NULL_E_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix->prefix_sid->flags, VALUE_V_FLAG) ? '1' : '0',
        IS_BIT_SET(prefix->prefix_sid->flags, LOCAL_SIGNIFICANCE_L_FLAG) ? '1' : '0');
    }
    else{
        printf("\t%-18s : Not assigned\n", subnet);
    }
}

boolean
set_node_sid(node_t *node, unsigned int node_sid_value){

    LEVEL level_it;
    boolean trigger_conflict_res = FALSE,
            is_prefix_sid_updated = FALSE;
    prefix_t *router_id = NULL;

    if(!node->spring_enabled){
        printf("Error : SPRING not enabled\n");
        return FALSE;
    }

    if(is_mpls_label_in_use(node->srgb, node_sid_value)){
        printf("Warning : Conflict Detected\n");
    }

    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){
        router_id = node_local_prefix_search(node, level_it, node->router_id, 32);
        assert(router_id);
        is_prefix_sid_updated = update_prefix_sid(node, router_id, node_sid_value); 
        if(is_prefix_sid_updated)
            trigger_conflict_res = TRUE;
    }
    return trigger_conflict_res;
}

boolean
unset_node_sid(node_t *node){

    LEVEL level_it;
    boolean trigger_conflict_res = FALSE;
    prefix_t *router_id = NULL;
    mpls_label label = 0 ;

    if(!node->spring_enabled){
        printf("Error : SPRING not enabled\n");
        return FALSE;
    }
    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){
        router_id = node_local_prefix_search(node, level_it, node->router_id, 32);
        assert(router_id);
        if(!router_id->prefix_sid){
            continue;
        }
        label = PREFIX_SID_VALUE(router_id);
        mark_srgb_mpls_label_not_in_use(node->srgb, label); 
        free(router_id->prefix_sid);
        router_id->prefix_sid = NULL;
        trigger_conflict_res = TRUE;
        MARK_PREFIX_SR_INACTIVE(router_id);
    }
    return trigger_conflict_res;
}

boolean
set_interface_address_prefix_sid(node_t *node, char *intf_name, 
                    unsigned int prefix_sid_value){

    LEVEL level_it;
    boolean trigger_conflict_res = FALSE,
            is_prefix_sid_updated = FALSE;
    prefix_t *prefix = NULL,
             *intf_prefix = NULL;

    if(!node->spring_enabled){
        printf("Error : SPRING not enabled\n");
        return FALSE;
    }

    edge_end_t *interface = get_interface_from_intf_name(node, intf_name);\

    if(!interface){
        printf("Error : Node %s Interface %s do not exist\n", node->node_name, intf_name);
        return FALSE;
    }

    if(is_mpls_label_in_use(node->srgb, prefix_sid_value)){
        printf("Warning : Conflict Detected\n");
    }

    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){
        intf_prefix = interface->prefix[level_it];
        if(!intf_prefix) continue;
        prefix = node_local_prefix_search(node, level_it, intf_prefix->prefix, intf_prefix->mask);
        assert(prefix);
        is_prefix_sid_updated = update_prefix_sid(node, prefix, prefix_sid_value);
        //update_prefix_sid(node, intf_prefix, prefix_sid_value);
        if(is_prefix_sid_updated)
            trigger_conflict_res = TRUE;
    }
    return trigger_conflict_res;
}

boolean
unset_interface_address_prefix_sid(node_t *node, char *intf_name){

    LEVEL level_it;
    boolean trigger_conflict_res = FALSE;
    prefix_t *prefix = NULL, *intf_prefix = NULL;
    edge_end_t *interface = NULL;
    mpls_label label = 0;

    if(!node->spring_enabled){
        printf("Error : SPRING not enabled\n");
        return FALSE;
    }

    interface = get_interface_from_intf_name(node, intf_name);

    if(!interface){
        printf("Error : Node %s Interface %s do not exist\n", node->node_name, intf_name);
        return FALSE;
    }

    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){
        intf_prefix = interface->prefix[level_it];
        if(!intf_prefix) continue;
        prefix = node_local_prefix_search(node, level_it, intf_prefix->prefix, intf_prefix->mask);
        assert(prefix);
        if(!prefix->prefix_sid)
            continue;
        label = PREFIX_SID_VALUE(prefix);
        mark_srgb_mpls_label_not_in_use(node->srgb, label);
        free(prefix->prefix_sid);
        prefix->prefix_sid = NULL;
        MARK_PREFIX_SR_INACTIVE(prefix);
        trigger_conflict_res = TRUE;
    }
    
    return trigger_conflict_res;
}

boolean
set_interface_adj_sid(node_t *node, char *interface, unsigned int adj_sid_value){

}

boolean
unset_interface_adj_sid(node_t *node, char *interface){

}

boolean
update_prefix_sid(node_t *node, prefix_t *prefix, 
        unsigned int prefix_sid_value){

    boolean trigger_conflict_res = FALSE;
                    
    if(!prefix->prefix_sid){
        prefix->prefix_sid = calloc(1, sizeof(prefix_sid_subtlv_t));
        prefix->prefix_sid->type = PREFIX_SID_SUBTLV_TYPE;
        prefix->prefix_sid->length = 0; /*never used*/
        prefix->prefix_sid->flags = 0;
        prefix->prefix_sid->algorithm = SHORTEST_PATH_FIRST;
        prefix->prefix_sid->sid.type = SID_SUBTLV_TYPE;
        PREFIX_SID_VALUE(prefix) = prefix_sid_value;
        MARK_PREFIX_SR_ACTIVE(prefix);
        trigger_conflict_res = TRUE;
        mark_srgb_mpls_label_in_use(node->srgb, prefix_sid_value);
        return trigger_conflict_res;
    }

    if(prefix_sid_value != PREFIX_SID_VALUE(prefix)){
        trigger_conflict_res = TRUE;
        mark_srgb_mpls_label_in_use(node->srgb, prefix_sid_value);
        mark_srgb_mpls_label_not_in_use(node->srgb, PREFIX_SID_VALUE(prefix));
    }

    PREFIX_SID_VALUE(prefix) = prefix_sid_value;

    if(trigger_conflict_res)
        MARK_PREFIX_SR_ACTIVE(prefix);

    return trigger_conflict_res;
}
