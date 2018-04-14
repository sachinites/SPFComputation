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
#include "spftrace.h"
#include "sr_tlv_api.h"
#include "igp_sr_ext.h"
#include "common.h"

#pragma GCC diagnostic ignored "-Wunused-but-set-variable"

extern instance_t * instance;

extern void
mpls_0_display(rt_un_table_t *rib, mpls_label_t in_label);
extern void
show_mpls_ldp_label_local_bindings(node_t *node);
extern void
show_mpls_rsvp_label_local_bindings(node_t *node);
extern void
transient_mpls_pfe_engine(node_t *node, mpls_label_stack_t *mpls_label_stack,
                          node_t **next_node);

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
        if(edge_end->dirn == INCOMING) continue;   

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
    node_t *nbr_node = edge->to.node;
    unsigned int old_nbr_node_spf_version = 0;

    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        if(!IS_LEVEL_SET(edge->level, level_it))
            continue;
        memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
        dist_info_hdr.info_dist_level = level_it;
        dist_info_hdr.advert_id = TLV2;
        old_nbr_node_spf_version = nbr_node->spf_info.spf_level_info[level_it].version;
        generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);
        if(old_nbr_node_spf_version < nbr_node->spf_info.spf_level_info[level_it].version)
            continue;
        memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
        dist_info_hdr.info_dist_level = level_it;
        dist_info_hdr.advert_id = TLV2;
        generate_lsp(instance, nbr_node, lsp_distribution_routine, &dist_info_hdr);
    }
}


static void
show_node_spring_details(node_t *node, LEVEL level){

    if(!node->spring_enabled){
        printf("Spring Disabled on this node\n");
        return;
    }

    prefix_t *prefix = NULL;
    singly_ll_node_t *list_node = NULL;

    srgb_t *srgb = node->srgb;

    printf("Node : %s SRGB Details:\n",  node->node_name);
    printf("\tRange : %u, starting mpls label = %u\n", 
        node->srgb->range, node->srgb->first_sid.sid);

    unsigned int i = 0, in_use_count = 0, 
                        avail_count = 0;
    for(; i < srgb->range; i++){
        if(is_bit_set(SRGB_INDEX_ARRAY(srgb), i))
            in_use_count++;
        else
            avail_count++;
    }
    printf("\t# Mpls labels in Use : %u, Available : %u\n", in_use_count, avail_count);

    printf("\tPrefix SID Database :\n");
    ITERATE_LIST_BEGIN(node->local_prefix_list[level], list_node){
    
        prefix = list_node->data;
        diplay_prefix_sid(prefix);
    } ITERATE_LIST_END;
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
    prefix_t *prefix = NULL;
    nh_type_t nh;
    internal_nh_t *nxthop = NULL;

    prefix_pref_data_t prefix_pref = {ROUTE_UNKNOWN_PREFERENCE, 
                                      "ROUTE_UNKNOWN_PREFERENCE"};

    printf("Route : %s/%u, %s\n", route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, get_str_level(route->level));
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
            nxthop = (internal_nh_t *)list_node->data;
            PRINT_ONE_LINER_NXT_HOP(nxthop);
        }ITERATE_LIST_END;
    } ITERATE_NH_TYPE_END;
    
    ITERATE_NH_TYPE_BEGIN(nh){
        printf("%s Backup Nxt Hops count : %u\n",
                nh == IPNH ? "IPNH" : "LSPNH", GET_NODE_COUNT_SINGLY_LL(route->backup_nh_list[nh]));
        ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node){
            nxthop = (internal_nh_t *)list_node->data;
            PRINT_ONE_LINER_NXT_HOP(nxthop);
        }ITERATE_LIST_END;
    } ITERATE_NH_TYPE_END;
}

static void
dump_spring_route_info(routes_t *route){

    singly_ll_node_t *list_node = NULL;
    prefix_t *prefix = NULL;
    nh_type_t nh;
    internal_nh_t *nxthop = NULL;

    prefix_pref_data_t prefix_pref = {ROUTE_UNKNOWN_PREFERENCE, 
                                      "ROUTE_UNKNOWN_PREFERENCE"};

    printf("Route : %s/%u, Inlabel : %u, %s\n", route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
        route->rt_key.u.label, get_str_level(route->level));
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

        printf("prefix SID : %u, %s/%u, hosting_node : %s, prefix->metric : %u, prefix->level = %s, pfx preference = %s(%u)\n", 
        PREFIX_SID_INDEX(prefix), prefix->prefix, prefix->mask, prefix->hosting_node->node_name, prefix->metric,
        get_str_level(prefix->level), prefix_pref.pref_str, prefix_pref.pref);

    }ITERATE_LIST_END;
    
    printf("Install state : %s\n", route_intall_status_str(route->install_state));

    ITERATE_NH_TYPE_BEGIN(nh){
        printf("%s Primary Nxt Hops count : %u\n",
                nh == IPNH ? "IPNH" : "LSPNH", GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[nh]));
        ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node){
            nxthop = (internal_nh_t *)list_node->data;
            PRINT_ONE_LINER_SPRING_NXT_HOP(nxthop);
        }ITERATE_LIST_END;
    } ITERATE_NH_TYPE_END;
    
    ITERATE_NH_TYPE_BEGIN(nh){
        printf("%s Backup Nxt Hops count : %u\n",
                nh == IPNH ? "IPNH" : "LSPNH", GET_NODE_COUNT_SINGLY_LL(route->backup_nh_list[nh]));
        ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node){
            nxthop = (internal_nh_t *)list_node->data;
            PRINT_ONE_LINER_SPRING_NXT_HOP(nxthop);
        }ITERATE_LIST_END;
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
            ITERATE_LIST_BEGIN(node->spf_info.routes_list[UNICAST_T], list_node){
                route = (routes_t *)list_node->data;
                dump_route_info(route);
                printf("\n");
            }ITERATE_LIST_END;
            break;

        case CMDCODE_DEBUG_INSTANCE_NODE_ROUTE:
            apply_mask(prefix, mask, masked_prefix);
            masked_prefix[PREFIX_LEN] = '\0';
            ITERATE_LIST_BEGIN(node->spf_info.routes_list[UNICAST_T], list_node){
                route = (routes_t *)list_node->data;
                if(strncmp(route->rt_key.u.prefix.prefix, masked_prefix, PREFIX_LEN) != 0)
                    continue;
                dump_route_info(route);
                break;
            }ITERATE_LIST_END;
            break;

        case CMDCODE_DEBUG_INSTANCE_NODE_SPRING_ROUTE:
            ITERATE_LIST_BEGIN(node->spf_info.routes_list[SPRING_T], list_node){
                route = (routes_t *)list_node->data;
                apply_mask(prefix, mask, masked_prefix);
                if(strncmp(route->rt_key.u.prefix.prefix, masked_prefix, PREFIX_LEN) != 0)
                    continue;
                dump_spring_route_info(route);
                break;
            }ITERATE_LIST_END;
            break;

        case CMDCODE_DEBUG_INSTANCE_NODE_ALL_SPRING_ROUTES:
            ITERATE_LIST_BEGIN(node->spf_info.routes_list[SPRING_T], list_node){
                route = (routes_t *)list_node->data;
                dump_spring_route_info(route);
                printf("\n");
            }ITERATE_LIST_END;
            break;
        default:
            assert(0);
    }
    return 0;
}

/* level could LEVEL12*/
boolean
insert_lsp_as_forward_adjacency(node_t *ingress_lsr_node, 
                           char *lsp_name, 
                           unsigned int metric, 
                           char *tail_end_ip, 
                           LEVEL level){

    rsvp_tunnel_t *rsvp_tunnel = look_up_rsvp_tunnel(ingress_lsr_node, lsp_name);
    if(!rsvp_tunnel){
        printf("Error : RSVP tunnel : %s do not exist, LSP export into IGP failed.\n", rsvp_tunnel->lsp_name);
        return FALSE;
    }

    edge_t *lsp = create_new_lsp_adj(lsp_name, metric, level);
    lsp->fa = rsvp_tunnel;

    insert_edge_between_2_nodes(lsp, ingress_lsr_node, lsp->fa->egress_lsr, UNIDIRECTIONAL);
    return TRUE;
}

int
lfa_rlfa_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    node_t *node = NULL;
    char *node_name = NULL;
    char *intf_name = NULL; 
    int cmd_code = -1, i = 0;
    tlv_struct_t *tlv = NULL;
    edge_t *edge = NULL;
    edge_end_t *edge_end = NULL;

    cmd_code = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
            intf_name = tlv->value;
    } TLV_LOOP_END;

    if(node_name == NULL)
        node = instance->instance_root;
    else
        node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

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
        switch(enable_or_disable){
            case CONFIG_ENABLE: 
                SET_LINK_PROTECTION_TYPE(edge, LINK_PROTECTION);
                break;
            case CONFIG_DISABLE:
                UNSET_LINK_PROTECTION_TYPE(edge, LINK_PROTECTION);
                break;
            default:
                assert(0);
        }
        break;
        case CMDCODE_CONFIG_INTF_NODE_LINK_PROTECTION:
        switch(enable_or_disable){
            case CONFIG_ENABLE: 
                SET_LINK_PROTECTION_TYPE(edge, LINK_PROTECTION);
                SET_LINK_PROTECTION_TYPE(edge, LINK_NODE_PROTECTION);
                break;
            case CONFIG_DISABLE:
                UNSET_LINK_PROTECTION_TYPE(edge, LINK_NODE_PROTECTION);
                break;
            default:
                assert(0);
        }
        break;
        case CMDCODE_CONFIG_INTF_LINK_PROTECTION_RLFA:
        {
            if(IS_LEVEL_SET(edge->level, LEVEL1))
                compute_rlfa(node, edge, LEVEL1, TRUE);
            
            if(IS_LEVEL_SET(edge->level, LEVEL2))
                compute_rlfa(node, edge, LEVEL2, TRUE);
        }
        break;
        case CMDCODE_CONFIG_INTF_NO_ELIGIBLE_BACKUP:
        {
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    SET_BIT(edge_end->edge_config_flags, NO_ELIGIBLE_BACK_UP);
                    break;
                case CONFIG_DISABLE:
                    UNSET_BIT(edge_end->edge_config_flags, NO_ELIGIBLE_BACK_UP);
                    break;
                default:
                    ;
            }
        }
        break;
    } 
    return 0;
}

void
show_spf_initialization(node_t *spf_root, LEVEL level){

   node_t *phy_nbr = NULL, 
          *logical_nbr = NULL;
   edge_t *edge = NULL, *pn_edge = NULL;
   
   ITERATE_NODE_PHYSICAL_NBRS_BEGIN(spf_root, phy_nbr, logical_nbr, edge, pn_edge, level){ 
       
        printf("Nbr = %s, IP Direct NH count = %u, LSP Direct NH count = %u, metric = %u\n", 
                phy_nbr->node_name, get_nh_count(&phy_nbr->direct_next_hop[level][IPNH][0]), 
                get_nh_count(&phy_nbr->direct_next_hop[level][LSPNH][0]),
                !is_nh_list_empty2(&phy_nbr->direct_next_hop[level][IPNH][0]) ? 
                    get_direct_next_hop_metric(phy_nbr->direct_next_hop[level][IPNH][0], level) :
                    get_direct_next_hop_metric(phy_nbr->direct_next_hop[level][LSPNH][0], level));
   } ITERATE_NODE_PHYSICAL_NBRS_END(spf_root, phy_nbr, logical_nbr, level);
}

int
debug_show_node_impacted_destinations(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

  node_t *node = NULL;
  char *intf_name = NULL,
       *node_name = NULL;
  int i = 0;
  edge_t *edge = NULL;
  edge_end_t *edge_end = NULL;
  tlv_struct_t *tlv = NULL;
  LEVEL level_it = MAX_LEVEL; 

  TLV_LOOP_BEGIN(tlv_buf, tlv){
      if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
          node_name = tlv->value;
      else if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
          intf_name = tlv->value;
      else
          assert(0);
  } TLV_LOOP_END;
  
  node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
  
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

  spf_result_t *D_res = NULL;
  singly_ll_node_t *list_node = NULL;
  char impact_reason[STRING_REASON_LEN];
  boolean is_impacted = FALSE,
           MANDATORY_NODE_PROTECTION = TRUE;

  for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){

    if(!IS_LEVEL_SET(edge->level, level_it)) continue;

    Compute_and_Store_Forward_SPF(node, level_it);
    Compute_PHYSICAL_Neighbor_SPFs(node, level_it);

    printf("Destinations Impact result for %s, PLR = %s, protected link = %s\n", 
        get_str_level(level_it), node->node_name, edge_end->intf_name);

    printf("Destination                 Is Impacted             Reason\n");
    printf("=============================================================\n");
    ITERATE_LIST_BEGIN(node->spf_run_result[level_it], list_node){
        D_res = list_node->data;
        memset(impact_reason , 0 , 256);
        MANDATORY_NODE_PROTECTION = TRUE;
        is_impacted = is_destination_impacted(node, edge, D_res->node, level_it, impact_reason,
                        &MANDATORY_NODE_PROTECTION);
        printf(" %-20s     %-15s   %s\n", D_res->node->node_name, is_impacted ? "IMPACTED" : "NOT IMPACTED", impact_reason);
    }ITERATE_LIST_END;
  }
  return 0;
}

void 
dump_next_hop(internal_nh_t *nh){

    printf("\toif = %-10s", nh->oif->intf_name);
    printf("\tprotecting link = %-10s", nh->protected_link->intf_name);
    printf("gw_prefix = %-16s", nh->gw_prefix);
    if(nh->node)
        printf(" Node = %-16s\n", nh->node->node_name);
    else
        printf(" Node = %s\n", "NULL");

    printf("\tnh_type = %-8s", nh->nh_type == IPNH ? "IPNH" : "LSPNH");

    //if(nh->rlfa) printf("ref_count = %-5u", *(nh->ref_count));

    printf("lfa_type = %-25s\n", get_str_lfa_type(nh->lfa_type));

    if(nh->proxy_nbr)
        printf("\tproxy_nbr = %-16s", nh->proxy_nbr->node_name);
    else
        printf("\tproxy_nbr = %-16s", "NULL");

    if(nh->rlfa)
        printf("rlfa = %-16s\n", nh->rlfa->node_name);
    else
        printf("rlfa = %-16s\n", "NULL");

    //printf("\tmpls_label_in = %-17u", nh->mpls_label_in);
    printf("root_metric = %-8u", nh->root_metric);
    printf("dest_metric = %-8u", nh->dest_metric);
    printf("is_eligible = %-6s\n", nh->is_eligible ? "TRUE" : "FALSE");
}

int
debug_show_node_back_up_spf_results(param_t *param, 
                ser_buff_t *tlv_buf, 
                op_mode enable_or_disable){

    node_t *node = NULL,
           *D = NULL,
           *dst_filter = NULL;
    tlv_struct_t *tlv = NULL;
    LEVEL level_it = MAX_LEVEL;
    char *node_name = NULL, 
         *dst_name = NULL;
    nh_type_t nh = NH_MAX;
    singly_ll_node_t *list_node = NULL;
    spf_result_t *D_res = NULL;
    unsigned int i = 0,
                 nh_count = 0,
                 j = 0;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "dst-name", strlen("dst-name")) ==0)
            dst_name = tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    if(dst_name)
        dst_filter = (node_t *)singly_ll_search_by_key(instance->instance_node_list, dst_name);
    
    internal_nh_t *nxthop = NULL;

    if(dst_filter){
        D = dst_filter;
        for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
            printf("\n%s backup spf results\n\n", get_str_level(level_it));
            printf("Dest : %s (#IP back-ups = %u, #LSP back-ups = %u)\n", 
                    D->node_name, 
                    get_nh_count(D->backup_next_hop[level_it][IPNH]),
                    get_nh_count(D->backup_next_hop[level_it][LSPNH]));

            D_res = GET_SPF_RESULT((&node->spf_info), D, level_it);
            if(!D_res) return 0;

            ITERATE_NH_TYPE_BEGIN(nh){
                nh_count = get_nh_count(&(D_res->next_hop[nh][0]));
                for( i = 0; i < nh_count; i++){
                    nxthop = &(D_res->next_hop[nh][i]);
                    PRINT_ONE_LINER_NXT_HOP(nxthop);
                }
            } ITERATE_NH_TYPE_END;
            j = 1;
            ITERATE_NH_TYPE_BEGIN(nh){
                nh_count = get_nh_count(D->backup_next_hop[level_it][nh]);
                for( i = 0; i < nh_count; i++){
                    printf("Nh# %u. ", j++);
                    dump_next_hop(&(D->backup_next_hop[level_it][nh][i]));
                    printf("\n");           
                }
            } ITERATE_NH_TYPE_END;
        }
        return 0;
    }

    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        printf("\n%s backup spf results\n\n", get_str_level(level_it));
        ITERATE_LIST_BEGIN(node->spf_run_result[level_it], list_node){
            D_res = list_node->data;
            D = D_res->node;
            printf("Dest : %s (#IP back-ups = %u, #LSP back-ups = %u)\n", 
                    D->node_name, 
                    get_nh_count(D->backup_next_hop[level_it][IPNH]),
                    get_nh_count(D->backup_next_hop[level_it][LSPNH]));
            j = 1;
            ITERATE_NH_TYPE_BEGIN(nh){
                nh_count = get_nh_count(&(D_res->next_hop[nh][0]));
                for( i = 0; i < nh_count; i++){
                    nxthop = &(D_res->next_hop[nh][i]);
                    PRINT_ONE_LINER_NXT_HOP(nxthop);
                }
            } ITERATE_NH_TYPE_END;
            ITERATE_NH_TYPE_BEGIN(nh){
                nh_count = get_nh_count(D->backup_next_hop[level_it][nh]);
                for( i = 0; i < nh_count; i++){
                    printf("Nh# %u. ", j++);
                    dump_next_hop(&(D->backup_next_hop[level_it][nh][i]));
                    printf("\n");           
                }
            } ITERATE_NH_TYPE_END;
        } ITERATE_LIST_END;
    }
    return 0;
}

int
display_logging_status(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    spf_display_trace_options();
    return 0;
}

int
instance_node_spring_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    node_t *node = NULL;
    char *node_name = NULL;
    tlv_struct_t *tlv = NULL;
    int cmd_code = -1;
    char *intf_name = NULL;
    unsigned int prefix_sid = 0,
                 index_range = 0,
                 first_sid = 0;
    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
            intf_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "node-segment", strlen("node-segment")) ==0)
            prefix_sid = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "prefix-sid", strlen("prefix-sid")) ==0)
             prefix_sid = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "index-range",  strlen("index-range")) ==0)
            index_range = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "start-label",  strlen("start-label")) ==0)
            first_sid = atoi(tlv->value);
        else
            assert(0);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    cmd_code = EXTRACT_CMD_CODE(tlv_buf);

    switch(cmd_code){
        case CMDCODE_CONFIG_NODE_SPRING_BACKUPS:
        switch(enable_or_disable){
            case CONFIG_ENABLE:
                if(!node->spring_enabled){
                    printf("Error : source-packet-routing not enabled\n");
                    return 0;
                }
                node->use_spring_backups = TRUE;
                break;
            case CONFIG_DISABLE:
                node->use_spring_backups = FALSE;
                break;
            default:
                assert(0);
        }
        break;
        case CMDCODE_CONFIG_NODE_SEGMENT_ROUTING_ENABLE:
        switch(enable_or_disable){
            case CONFIG_ENABLE:
                if(node->spring_enabled == TRUE){
                    printf("Source Packet Routing Already Enabled\n");
                    return 0;
                }
                node->spring_enabled = TRUE;
                node->srgb = calloc(1, sizeof(srgb_t));
                init_srgb_defaults(node->srgb);
                break;
            case CONFIG_DISABLE:
                node->spring_enabled = FALSE;
                spring_disable_cleanup(node);
                break;
            default:
                ;
        }
        break;
        case CMDCODE_CONFIG_NODE_SR_PREFIX_SID:
        switch(enable_or_disable){
            case CONFIG_ENABLE:
                set_node_sid(node, prefix_sid);
                break;
            case CONFIG_DISABLE:
                unset_node_sid(node);
                break;
             default:
                ;
        }
        break;
        case CMDCODE_CONFIG_NODE_SR_PREFIX_SID_INTF:
        switch(enable_or_disable){
            case CONFIG_ENABLE:
                set_interface_address_prefix_sid(node, intf_name, prefix_sid);
                break;
            case CONFIG_DISABLE:
                unset_interface_address_prefix_sid(node, intf_name);
                break;
             default:
                ;
        }
        break;
        case CMDCODE_CONFIG_NODE_SR_ADJ_SID:
        switch(enable_or_disable){
            case CONFIG_ENABLE:
                set_interface_adj_sid(node, intf_name, prefix_sid);
                break;
            case CONFIG_DISABLE:
                unset_interface_adj_sid(node, intf_name);
                break;
             default:
                ;
        }
        break;
        case CMDCODE_CONFIG_NODE_SR_SRGB_RANGE:
        if(node->spring_enabled == FALSE){
            printf("Source Packet Routing Not Enabled\n");
            return 0;
        }
        node->srgb->range = index_range;
        break;
        case CMDCODE_CONFIG_NODE_SR_SRGB_START_LABEL:
        if(node->spring_enabled == FALSE){
            printf("Source Packet Routing Not Enabled\n");
            return 0;
        }
        node->srgb->first_sid.sid = first_sid;
        break; 
        case CMDCODE_CONFIG_NODE_STATIC_INSTALL_MPLS_ROUTE:
        switch(enable_or_disable){
            case CONFIG_ENABLE:
                //igp_install_mpls_spring_route(node, prefix, mask);
                break;
            case CONFIG_DISABLE:
                //igp_uninstall_mpls_spring_route(node, prefix, mask);
                break;
             default:
                ;
        }
        default:
        ;
    }
    return 0;
}

int
instance_node_spring_show_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    char *node_name = NULL;
    node_t *node = NULL;
    LEVEL level = MAX_LEVEL;
    tlv_struct_t *tlv = NULL;
    int cmd_code = -1;
    char str_prefix_with_mask[PREFIX_LEN_WITH_MASK + 1];

    cmd_code = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "level-no", strlen("level-no")) ==0)
            level = atoi(tlv->value);
        else
            assert(0);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    switch(cmd_code){
        case CMDCODE_SHOW_NODE_MPLS_LDP_BINDINGS:
            show_mpls_ldp_label_local_bindings(node);
            break;
        case CMDCODE_SHOW_NODE_MPLS_RSVP_BINDINGS:
            show_mpls_rsvp_label_local_bindings(node);
            break;
        case CMDCODE_DEBUG_SHOW_PREFIX_CONFLICT_RESULT:
            {
                ll_t *res = prefix_conflict_resolution(node, level);
                singly_ll_node_t *list_node = NULL;
                prefix_t *prefix = NULL;
                printf("prefix-conflict free prefixes :\n");
                printf("\t%s/%-10s %-20s   %s\n", "prefix", "mask", "Hosting node", "mpls-label");
                printf("\t--------------------------------------------------------\n");
                ITERATE_LIST_BEGIN(res, list_node){
                    
                    prefix = list_node->data;
                    assert(IS_PREFIX_SR_ACTIVE(prefix));
                    memset(str_prefix_with_mask, 0, PREFIX_LEN_WITH_MASK + 1);
                    apply_mask2(prefix->prefix, prefix->mask, str_prefix_with_mask);
                    printf("\t%-20s %-20s %u\n", 
                        str_prefix_with_mask, 
                        prefix->hosting_node->node_name, PREFIX_SID_INDEX(prefix));
                } ITERATE_LIST_END;
                delete_singly_ll(res);
                free(res);
            }
            break;
        case CMDCODE_DEBUG_SHOW_PREFIX_SID_CONFLICT_RESULT:
            {
                ll_t *res = prefix_conflict_resolution(node, level);
                res = prefix_sid_conflict_resolution(res);
                
                singly_ll_node_t *list_node = NULL;
                prefix_t *prefix = NULL;
                printf("prefixSID-conflict free prefixes :\n");
                printf("\t%s/%-10s %-20s   %s\n", "prefix", "mask", "Hosting node", "mpls-label");
                printf("\t--------------------------------------------------------\n");
                ITERATE_LIST_BEGIN(res, list_node){
                    
                    prefix = list_node->data;
                    assert(IS_PREFIX_SR_ACTIVE(prefix));
                    memset(str_prefix_with_mask, 0, PREFIX_LEN_WITH_MASK + 1);
                    apply_mask2(prefix->prefix, prefix->mask, str_prefix_with_mask);
                    printf("\t%-20s %-20s %u\n", 
                        str_prefix_with_mask, 
                        prefix->hosting_node->node_name, PREFIX_SID_INDEX(prefix));
                } ITERATE_LIST_END;
                delete_singly_ll(res);
                free(res);
            }
            break;
        case CMDCODE_SHOW_NODE_SPRING:
            show_node_spring_details(node, level);
        break;
        case CMDCODE_SHOW_NODE_MPLS_FORWARDINNG_TABLE:
            mpls_0_display(node->spf_info.rib[MPLS_0], 0);
            break;
        case CMDCODE_SHOW_NODE_MPLS_RSVP_LSP:
            print_all_rsvp_lsp(node);
            break;
        default:
            ;
    } 
    return 0;
}

int
debug_trace_mpls_stack_label(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
   
    char *node_name = NULL;
    node_t *node = NULL, 
           *next_node = NULL;
    tlv_struct_t *tlv = NULL;
    mpls_label_t label[4] = {0,0,0,0};
    int i = 0;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "label", strlen("label")) ==0)
            label[i++] = atoi(tlv->value);
        else
            assert(0);
    } TLV_LOOP_END;

    mpls_label_stack_t *mpls_label_stack = get_new_mpls_label_stack();
    i--;
    
    for(; i >= 0; i-- )
        PUSH_MPLS_LABEL(mpls_label_stack, label[i]);
    
    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    transient_mpls_pfe_engine(node, mpls_label_stack, &next_node);
    free_mpls_label_stack(mpls_label_stack); 
    return 0;
}

int
instance_node_ldp_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int cmd_code = EXTRACT_CMD_CODE(tlv_buf);
    tlv_struct_t *tlv = NULL;
    node_t *node = NULL;
    char *node_name = NULL;
    char *router_id = NULL;
    int rc = 0 ;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "router-id", strlen("router-id")) ==0)
            router_id = tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
     
    switch(cmd_code){
        case CMDCODE_CONFIG_NODE_ENABLE_LDP:
        switch(enable_or_disable){
            case CONFIG_ENABLE:
                enable_ldp(node);            
            break;
            case CONFIG_DISABLE:
                disable_ldp(node);
            break;
            default:
                assert(0);
        }
        break;
        case CMDCODE_CONFIG_NODE_LDP_TUNNEL:
            rc = create_targeted_ldp_tunnel(node, LEVEL1, router_id, 0, 0, 0);
            if(rc == -1) {
                printf("LDP tunnel creation failed\n");   
            }
        break;
    }
    return 0;
}

int
instance_node_rsvp_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int cmd_code = EXTRACT_CMD_CODE(tlv_buf);
    tlv_struct_t *tlv = NULL;
    node_t *node = NULL;
    char *node_name = NULL;
    char *router_id = NULL;
    char *rsvp_lsp_name = NULL;
    int rc = -1;
           
    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "router-id", strlen("router-id")) ==0)
            router_id = tlv->value;
        else if(strncmp(tlv->leaf_id, "rsvp-lsp-name", strlen("rsvp-lsp-name")) ==0)
            rsvp_lsp_name = tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
     
    switch(cmd_code){
        case CMDCODE_CONFIG_NODE_ENABLE_RSVP:
        switch(enable_or_disable){
            case CONFIG_ENABLE:
                enable_rsvp(node);            
            break;
            case CONFIG_DISABLE:
                disable_rsvp(node);
            break;
            default:
                assert(0);
        }
        break;
        case CMDCODE_CONFIG_NODE_RSVP_TUNNEL:
        {
            rsvp_tunnel_t *rsvp_tunnel = look_up_rsvp_tunnel(node, rsvp_lsp_name);
            if(rsvp_tunnel){
                printf("Error : RSVP tunnel %s already exists\n", rsvp_lsp_name);
                return 0;
            }
            rsvp_tunnel_t rsvp_tunnel_data;
            memset(&rsvp_tunnel_data, 0, sizeof(rsvp_tunnel_t));
            rc = create_targeted_rsvp_tunnel(node, LEVEL1, router_id, 0, 0, 0, &rsvp_tunnel_data);
            if(rc == -1) {
                printf("RSVP tunnel creation failed\n");   
            }
            rsvp_tunnel = calloc(1, sizeof(rsvp_tunnel_t));
            memcpy(rsvp_tunnel, &rsvp_tunnel_data, sizeof(rsvp_tunnel_t));
            strncpy(rsvp_tunnel->lsp_name, rsvp_lsp_name, RSVP_LSP_NAME_SIZE);
            rc = add_new_rsvp_tunnel(node, rsvp_tunnel);
            if(rc == -1){
                free(rsvp_tunnel);
            }
        }   
        break;
    }
    return 0;
}

