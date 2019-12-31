/*
 * =====================================================================================
 *
 *       Filename:  spring_adjsid.c
 *
 *    Description:  This file implements the APIs required for Adjacency SIDs functionality
 *
 *        Version:  1.0
 *        Created:  Wednesday 19 September 2018 11:01:29  IST
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
#include "spring_adjsid.h"
#include "spfcmdcodes.h"
#include "instance.h"
#include "spfutil.h"

void
print_lan_adj_sid_info(edge_end_t *interface, 
                       lan_intf_adj_sid_t *lan_intf_adj_sid){

    printf("Interface %s : LAN Adj sid : %u, type = %s, Nbr Sys id : %s\n",
        interface->intf_name,
        lan_intf_adj_sid->sid.sid, 
        lan_intf_adj_sid->adj_sid_type == ADJ_SID_TYPE_INDEX ? "INDEX" : "LABEL",
        lan_intf_adj_sid->nbr_system_id);
    printf("\t");
    PRINT_ADJ_SID_FLAGS(lan_intf_adj_sid->flags);
    printf("\n");
}

void
print_p2p_adj_sid_info(edge_end_t *interface, 
                       p2p_intf_adj_sid_t *p2p_intf_adj_sid){

    printf("Interface %s : PTP Adj sid : %u, type = %s\n",
        interface->intf_name,
        p2p_intf_adj_sid->sid.sid, 
        p2p_intf_adj_sid->adj_sid_type == ADJ_SID_TYPE_INDEX ? "INDEX" : "LABEL");
    printf("\t");
    PRINT_ADJ_SID_FLAGS(p2p_intf_adj_sid->flags);
    printf("\n");
}

void
unset_adj_sid(node_t *node, char *intf_name, LEVEL level, unsigned int label){

}

boolean
is_lan_static_adj_sid_exist(glthread_t *lan_adj_sid_list, unsigned int label){

    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(lan_adj_sid_list, curr){

       lan_intf_adj_sid_t *lan_intf_adj_sid = glthread_to_cfg_lan_adj_sid(curr);
       if(lan_intf_adj_sid->sid.sid == label)  
           return TRUE;
    } ITERATE_GLTHREAD_END(lan_adj_sid_list, curr);

    return FALSE;
}

boolean
is_static_adj_sid_exist_on_interface(edge_end_t *interface, mpls_label_t label){

    /*Check on P2P config first*/
    ADJ_SID_PROTECTION_TYPE asf;
    LEVEL level_it;
    for(asf = PROTECTED_ADJ_SID; asf < ADJ_SID_PROTECTION_MAX; asf++){
       for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
            if(interface->cfg_p2p_adj_sid_db[level_it][asf].sid.sid == label)
                return TRUE;   
       }
    }
    /*Check for LAN config next*/
    for(asf = PROTECTED_ADJ_SID; asf < ADJ_SID_PROTECTION_MAX; asf++){
        for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
            if(is_lan_static_adj_sid_exist(&interface->cfg_lan_adj_sid_db[level_it][asf], label))
                return TRUE;
        }
    }
    return FALSE;
} 

boolean
is_static_adj_sid_in_use(node_t *node, mpls_label_t label){

    unsigned int i = 0;
    edge_end_t *interface = NULL;
    for( ; i < MAX_NODE_INTF_SLOTS; i++){
        interface = node->edges[i];
        if(!interface) return FALSE;
        if(is_static_adj_sid_exist_on_interface(interface, label))
            return TRUE;
    }
    return FALSE;
}

void
set_adj_sid(node_t *node, char *intf_name, LEVEL level, 
               mpls_label_t label, char *nbr_sys_id, int cmdcode){

    glthread_t *head = NULL;
    p2p_intf_adj_sid_t *adj_sid_entry = NULL;

    if(is_static_adj_sid_in_use(node, label)){
        printf("Adj sid already in Use, configuration checkout failed\n");
        return;
    }

    edge_end_t *interface = get_interface_from_intf_name(node, intf_name);
    
    if(!interface){
        printf("Interface %s on node %s do not exist, config checkout failed\n", intf_name, node->node_name);
        return;
    }

    switch(cmdcode){
        case CMDCODE_CONFIG_NODE_INTF_LAN_ADJ_SID_PROTECTED:
            head = &interface->cfg_lan_adj_sid_db[level][PROTECTED_ADJ_SID];
            break;
        case CMDCODE_CONFIG_NODE_INTF_LAN_ADJ_SID_UNPROTECTED:
            head = &interface->cfg_lan_adj_sid_db[level][UNPROTECTED_ADJ_SID];
            break;
        case CMDCODE_CONFIG_NODE_INTF_P2P_ADJ_SID_PROTECTED:
            adj_sid_entry = &interface->cfg_p2p_adj_sid_db[level][PROTECTED_ADJ_SID];
            break;
        case CMDCODE_CONFIG_NODE_INTF_P2P_ADJ_SID_UNPROTECTED:
            adj_sid_entry = &interface->cfg_p2p_adj_sid_db[level][UNPROTECTED_ADJ_SID];
            break;
        default:
            assert(0);
    }

    if(head){
        lan_intf_adj_sid_t *lan_intf_adj_sid = calloc(1, sizeof(lan_intf_adj_sid_t));
        lan_intf_adj_sid->adj_sid_type = ADJ_SID_TYPE_LABEL; /*We support only labels*/
        lan_intf_adj_sid->sid.sid = label;
        strncpy(lan_intf_adj_sid->nbr_system_id, nbr_sys_id, PREFIX_LEN);
        lan_intf_adj_sid->nbr_system_id[PREFIX_LEN] = '\0';
        SET_STATIC_ADJ_SID_DEFAULT_FLAG(lan_intf_adj_sid->flags);
        if(cmdcode == CMDCODE_CONFIG_NODE_INTF_LAN_ADJ_SID_PROTECTED)
            SET_BIT(lan_intf_adj_sid->flags, ADJ_SID_BACKUP_B_FLAG);
        init_glthread(&lan_intf_adj_sid->cfg_glthread);
        glthread_add_next(head, &lan_intf_adj_sid->cfg_glthread);
    }else{
        adj_sid_entry->adj_sid_type = ADJ_SID_TYPE_LABEL;
        adj_sid_entry->sid.sid = label;
        SET_STATIC_ADJ_SID_DEFAULT_FLAG(adj_sid_entry->flags);
        if(cmdcode == CMDCODE_CONFIG_NODE_INTF_P2P_ADJ_SID_PROTECTED)
            SET_BIT(adj_sid_entry->flags, ADJ_SID_BACKUP_B_FLAG);
    }
}

void
show_node_adj_sids(node_t *node){

    LEVEL level_it;
    unsigned int i = 0;
    edge_end_t *interface = NULL;
    ADJ_SID_PROTECTION_TYPE prot_type;
    p2p_intf_adj_sid_t *p2p_intf_adj_sid = NULL;
    glthread_t *lan_adj_sid_list = NULL, *curr = NULL;
    lan_intf_adj_sid_t *lan_intf_adj_sid = NULL;

    for (; i < MAX_NODE_INTF_SLOTS; i++){
        interface = node->edges[i];    
        if(!interface) break;
        for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
            for(prot_type = PROTECTED_ADJ_SID; prot_type < ADJ_SID_PROTECTION_MAX; prot_type++){
                p2p_intf_adj_sid = &interface->cfg_p2p_adj_sid_db[level_it][prot_type];               
                if(!p2p_intf_adj_sid->sid.sid)
                    continue;
                print_p2p_adj_sid_info(interface, p2p_intf_adj_sid); 
            }
        }

        for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
            for(prot_type = PROTECTED_ADJ_SID; prot_type < ADJ_SID_PROTECTION_MAX; prot_type++){
                lan_adj_sid_list = &interface->cfg_lan_adj_sid_db[level_it][prot_type];
                ITERATE_GLTHREAD_BEGIN(lan_adj_sid_list, curr){
                    lan_intf_adj_sid = glthread_to_cfg_lan_adj_sid(curr);                
                    print_lan_adj_sid_info(interface, lan_intf_adj_sid); 
                }ITERATE_GLTHREAD_END(lan_adj_sid_list, curr);
            }
        }
    }
}

mpls_label_t
get_adj_sid_minimum(node_t *node1, node_t *node2, LEVEL level){

    /*Let us first search a P2P adj sid*/
    int i = 0;
    edge_end_t *interface;
    edge_t *edge = NULL;
    unsigned int metric = 0xFFFFFFFF;
    p2p_intf_adj_sid_t *p2p_intf_adj_sid = NULL;

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        interface = node1->edges[i];
        if(!interface) break;
        edge = GET_EGDE_PTR_FROM_FROM_EDGE_END(interface);
        if(get_edge_direction(node1, edge) != OUTGOING) continue;
        if(is_broadcast_link(edge, level)) continue;
        if(!IS_LEVEL_SET(GET_EDGE_END_LEVEL(interface), level)) continue;
        if(!edge->status) continue;
        if(edge->to.node != node2) continue;
        if(metric > edge->metric[level]){
            if(!&interface->cfg_p2p_adj_sid_db[level][UNPROTECTED_ADJ_SID]) continue;
            p2p_intf_adj_sid = &interface->cfg_p2p_adj_sid_db[level][UNPROTECTED_ADJ_SID];
            metric = edge->metric[level];
        }
    }
    return p2p_intf_adj_sid ? p2p_intf_adj_sid->sid.sid : 0;
}
