/*
 * =====================================================================================
 *
 *       Filename:  spfclihandler.c
 *
 *    Description:  This file defines the routines to handle show/debug/config/clear etc commands
 *
 *        Version:  1.0
 *        Created:  Sunday 03 September 2017 10:46:52  IST
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

#include "spfclihandler.h"
#include <stdio.h>
#include "cmdtlv.h"

extern instance_t * instance;

void
spf_node_slot_enable_disable(node_t *node, char *slot_name,
                                op_mode enable_or_disable){


    unsigned int i = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;
    char found = 0;

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        if(!edge_end){
            return;
        }

        if(strncmp(edge_end->intf_name, slot_name, strlen(edge_end->intf_name)) == 0 &&
            strlen(edge_end->intf_name) == strlen(slot_name)){
            
            edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
            edge->status = (enable_or_disable == CONFIG_DISABLE) ? 0 : 1;
            found = 1;
        }
    }
    if(!found)
        printf("%s() : INFO : Node : %s, slot %s not found\n", __FUNCTION__, node->node_name, slot_name);
}

void
display_instance_nodes(param_t *param, ser_buff_t *tlv_buf){

    singly_ll_node_t *list_node = NULL;
    node_t *node = NULL;

    ITERATE_LIST(instance->instance_node_list, list_node){

        node = (node_t *)list_node->data;
        printf("    %s\n", node->node_name);
    }
}

void
display_instance_node_interfaces(param_t *param, ser_buff_t *tlv_buf){

    tlv_struct_t *tlv = NULL;
    unsigned int i = 0;
    char *node_name = NULL;
    node_t *node = NULL;
    edge_end_t *edge_end = NULL;

    TLV_LOOP(tlv_buf, tlv, i){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
    }

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    if(!node)
        return;

    for(i = 0; i < MAX_NODE_INTF_SLOTS; i++){

        edge_end = node->edges[i];
        if(!edge_end)
            break;
        
        printf("    %s %s\n", edge_end->intf_name, (edge_end->dirn == OUTGOING) ? "->" : "<-");
    }
}

int
validate_ipv4_mask(char *mask){

    int _mask = atoi(mask);
    if(_mask >= 0 && _mask <= 32)
        return VALIDATION_SUCCESS;
    return VALIDATION_FAILED;
}

