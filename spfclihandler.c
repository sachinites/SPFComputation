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

#include <stdio.h>
#include "spfclihandler.h"
#include "cmdtlv.h"
#include "routes.h"
#include "bitsop.h"
#include "spfutil.h"
#include "spfcmdcodes.h"
#include "advert.h"
#include "rlfa.h"

extern instance_t * instance;

static void
_run_spf_run_all_nodes(){

    LEVEL level_it;
    singly_ll_node_t *list_node = NULL;
    node_t *node = NULL;;

    /*Ist run LEVEL2 spf run on all nodes, so that L1L2 routers would set multi_area bit appropriately*/

    for(level_it = LEVEL2; level_it >= LEVEL1; level_it--){

        ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
            node = list_node->data;
            if(node->node_type[level_it] == PSEUDONODE)
                continue;
            spf_computation(node, &node->spf_info, level_it, FULL_RUN);
        } ITERATE_LIST_END;
    }
}

int
run_spf_run_all_nodes(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    _run_spf_run_all_nodes();
    return 0;
}


void
spf_node_slot_enable_disable(node_t *node, char *slot_name,
                                op_mode enable_or_disable){


    unsigned int i = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;
    char found = 0;
    LEVEL level_it; 

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        if(!edge_end){
            return;
        }

        if(strncmp(edge_end->intf_name, slot_name, strlen(edge_end->intf_name)) == 0 &&
            strlen(edge_end->intf_name) == strlen(slot_name)){
          
            edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
            edge->status = (enable_or_disable == CONFIG_DISABLE) ? 0 : 1;
            if(edge->status == 0){
                /*remove the edge_end prefixes from node*/
                dettach_edge_end_prefix_on_node(edge->from.node, &edge->from);
            }
            else{
                /*Attach the edge end prefix to node.*/
                attach_edge_end_prefix_on_node(edge->from.node, &edge->from);
            }
            found = 1;
            break;
        }
    }
    if(!found){
        printf("%s() : INFO : Node : %s, slot %s not found\n", __FUNCTION__, node->node_name, slot_name);
        return;
    }
    
    dist_info_hdr_t dist_info_hdr;

    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        if(!IS_LEVEL_SET(edge->level, level_it))
            continue;
        memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
        dist_info_hdr.info_dist_level = level_it;
        dist_info_hdr.advert_id = TLV2;
        generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);
    }

    node_t *nbr_node = edge->to.node;

    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        if(!IS_LEVEL_SET(edge->level, level_it))
            continue;
        memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
        dist_info_hdr.info_dist_level = level_it;
        dist_info_hdr.advert_id = TLV2;
        generate_lsp(instance, nbr_node, lsp_distribution_routine, &dist_info_hdr);
    }
}

void
spf_node_slot_metric_change(node_t *node, char *slot_name,
                            LEVEL level, unsigned int new_metric){

    unsigned int i = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;
    boolean found = FALSE;

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        
        if(!edge_end){
            return;
        }

        if(edge_end->dirn != OUTGOING)
            continue;

        if(!(strncmp(edge_end->intf_name, slot_name, strlen(edge_end->intf_name)) == 0 &&
            strlen(edge_end->intf_name) == strlen(slot_name)))
                continue;

        found = TRUE;
        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);

        if(!IS_LEVEL_SET(edge->level, level))
            continue;

        if(edge->metric[level] == new_metric)
            return;

        edge->metric[level] = new_metric;
        break;
   } 

    if(found == FALSE){
        printf("Error : node %s, Interface %s not found\n", node->node_name, edge_end->intf_name);
        return;
    }

    dist_info_hdr_t dist_info_hdr;
    memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
    dist_info_hdr.info_dist_level = level;
    dist_info_hdr.advert_id = TLV2;
    generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);
}




void
display_instance_nodes(param_t *param, ser_buff_t *tlv_buf){

    singly_ll_node_t *list_node = NULL;
    node_t *node = NULL;

    ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){

        node = (node_t *)list_node->data;
        printf("    %s\n", node->node_name);
    }ITERATE_LIST_END;
}

void
display_instance_node_interfaces(param_t *param, ser_buff_t *tlv_buf){

    tlv_struct_t *tlv = NULL;
    unsigned int i = 0;
    char *node_name = NULL;
    node_t *node = NULL;
    edge_end_t *edge_end = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
    } TLV_LOOP_END;

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

static void
dump_route_info(routes_t *route){

    singly_ll_node_t *list_node = NULL;
    node_t *node = NULL;
    prefix_t *prefix = NULL;
    nh_type_t nh;
    prefix_pref_data_t prefix_pref = {ROUTE_UNKNOWN_PREFERENCE, 
                                      "ROUTE_UNKNOWN_PREFERENCE"};

    printf("Route : %s/%u, %s\n", route->rt_key.prefix, route->rt_key.mask, get_str_level(route->level));
    printf("Version : %d, spf_metric = %u, lsp_metric = %u, ext_metric = %u\n", 
                    route->version, route->spf_metric, route->lsp_metric, route->ext_metric);

    printf("flags : up/down : %s , Route : %s, Metric type : %s\n", 
            IS_BIT_SET(route->flags, PREFIX_DOWNBIT_FLAG)    ? "SET" : "NOT SET",
            IS_BIT_SET(route->flags, PREFIX_EXTERNABIT_FLAG) ? "EXTERNAL" : "INTERNAL",
            IS_BIT_SET(route->flags, PREFIX_METRIC_TYPE_EXT) ? "EXTERNAL" : "INTERNAL");

    printf("hosting_node = %s\n", route->hosting_node->node_name);
    printf("Like prefix list count : %u\n", GET_NODE_COUNT_SINGLY_LL(route->like_prefix_list));

    ITERATE_LIST_BEGIN(route->like_prefix_list, list_node){

        prefix = list_node->data;
        prefix_pref = route_preference(prefix->prefix_flags, prefix->level);

        printf("%s/%u, hosting_node : %s, prefix->metric : %u, prefix->level = %s, pfx preference = %s(%u)\n", 
        prefix->prefix, prefix->mask, prefix->hosting_node->node_name, prefix->metric,
        get_str_level(prefix->level), prefix_pref.pref_str, prefix_pref.pref);

    }ITERATE_LIST_END;
    
    printf("Install state : %s\n", route_intall_status_str(route->install_state));

    ITERATE_NH_TYPE_BEGIN(nh){
        
        printf("%s Primary Nxt Hops count : %u\n",
                nh == IPNH ? "IPNH" : "LSPNH", GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[nh]));

        ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node){

            node = (node_t *)list_node->data;
            printf(" %s ", node->node_name);
        }ITERATE_LIST_END;

        printf("\n");

    } ITERATE_NH_TYPE_END;

}

int
show_route_tree_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    tlv_struct_t *tlv = NULL;
    char *node_name = NULL;
    node_t *node = NULL;
    routes_t *route = NULL;
    char *prefix = NULL;
    char mask = 0;
    singly_ll_node_t *list_node = NULL;
    int cmd_code = -1;
    char masked_prefix[PREFIX_LEN + 1];

    cmd_code = EXTRACT_CMD_CODE(tlv_buf);
      
    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "prefix", strlen("prefix")) ==0)
            prefix = tlv->value;
        else if(strncmp(tlv->leaf_id, "mask", strlen("mask")) ==0)
            mask = atoi(tlv->value);
        else
            assert(0);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    
    switch(cmd_code){
        case CMDCODE_DEBUG_INSTANCE_NODE_ALL_ROUTES:
            ITERATE_LIST_BEGIN(node->spf_info.routes_list, list_node){

                route = (routes_t *)list_node->data;
                dump_route_info(route);
            }ITERATE_LIST_END;
        break;
        case CMDCODE_DEBUG_INSTANCE_NODE_ROUTE:
            apply_mask(prefix, mask, masked_prefix);
            masked_prefix[PREFIX_LEN] = '\0';
            ITERATE_LIST_BEGIN(node->spf_info.routes_list, list_node){
                
                route = (routes_t *)list_node->data;
                if(strncmp(route->rt_key.prefix, masked_prefix, PREFIX_LEN) != 0)
                    continue;
                dump_route_info(route);
                break;
            }ITERATE_LIST_END;

        break;
        default:
            assert(0);
    }
    return 0;
}

/* level could LEVEL12*/
void
inset_lsp_as_forward_adjacency(node_t *ingress_lsr_node, 
                           char *lsp_name, 
                           unsigned int metric, 
                           char *tail_end_ip, 
                           LEVEL level){

    node_t *tail_end_node = NULL;
    edge_t *lsp = create_new_lsp_adj(lsp_name, metric, level);

    switch(level){
        case LEVEL1:
        case LEVEL2:
            tail_end_node = get_system_id_from_router_id(ingress_lsr_node, tail_end_ip, level);    
            break;
        case LEVEL12:
            tail_end_node = get_system_id_from_router_id(ingress_lsr_node, tail_end_ip, LEVEL1);
            if(!tail_end_node)
                tail_end_node = get_system_id_from_router_id(ingress_lsr_node, tail_end_ip, LEVEL2);
            break;
        default:
            assert(0);
    }

    if(!tail_end_node){
        printf("Error : Could not find Node for Router id %s in LEVEL%u", tail_end_ip, level);   
        free(lsp);
        lsp = NULL;
        return;
    }

    insert_edge_between_2_nodes(lsp, ingress_lsr_node, tail_end_node, UNIDIRECTIONAL);
}

int
lfa_rlfa_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    node_t *node = NULL,
           *dest_node = NULL;
    char *node_name = NULL,
         *dest_name = NULL;
    char *intf_name = NULL; 
    int cmd_code = -1, i = 0;
    tlv_struct_t *tlv = NULL;
    edge_t *edge = NULL;
    edge_end_t *edge_end = NULL;
    LEVEL level = LEVEL_UNKNOWN;

    cmd_code = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
            intf_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "level", strlen("level")) ==0)
            level = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "destination", strlen("destination")) ==0)
            dest_name = tlv->value;

    } TLV_LOOP_END;

    if(node_name == NULL)
        node = instance->instance_root;
    else
        node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    dest_node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, dest_name);

    for(i = 0; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        if(!edge_end){
            printf("%s() : Error : Interface %s do not exist\n", __FUNCTION__, intf_name);
            return 0;
        }

        if(edge_end->dirn != OUTGOING)
            continue;

        if(strncmp(intf_name, edge_end->intf_name, strlen(edge_end->intf_name)))
            continue;

        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        break;
    }

    switch(cmd_code){
        case CMDCODE_CONFIG_INTF_LINK_PROTECTION:
        {
            lfa_t *lfa_l1 = NULL, *lfa_l2 = NULL;

            if(IS_LEVEL_SET(edge->level, LEVEL1))
                lfa_l1 = compute_lfa(node, edge, LEVEL1, TRUE);
            
            if(IS_LEVEL_SET(edge->level, LEVEL2))
                lfa_l2 = compute_lfa(node, edge, LEVEL2, TRUE);

            print_lfa_info(lfa_l1);
            print_lfa_info(lfa_l2); 
            free_lfa(lfa_l1);
            free_lfa(lfa_l2);
        }
        break;
        case CMDCODE_DEBUG_SHOW_DESTINATION_SPEC_PQ_NODES:
            p2p_compute_rlfa_for_given_dest(node, level, edge, dest_node);
            break;
    } 
    return 0;
}
