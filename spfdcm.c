/*
 * =====================================================================================
 *
 *       Filename:  spfdcm.c
 *
 *    Description:  This file initalises the CLI interface for SPFComptation Project
 *
 *        Version:  1.0
 *        Created:  Thursday 24 August 2017 12:55:56  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the SPFComptation distribution (https://github.com/sachinites).
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
#include "libcli.h"
#include "instance.h"
#include "cmdtlv.h"
#include "spfutil.h"
#include "spfcomputation.h"
#include "spftrace.h"
#include "bitsop.h"
#include "spfcmdcodes.h"
#include "spfclihandler.h"
#include "rttable.h"
#include "routes.h"
#include "advert.h"

extern
instance_t *instance;

/*All Command Handler Functions goes here */

static void
show_spf_results(node_t *spf_root, LEVEL level){

    singly_ll_node_t *list_node = NULL;
    spf_result_t *res = NULL;
    unsigned int i = 0, j = 0;
    edge_end_t *oif = NULL;
    nh_type_t nh;
    printf("\nSPF run results for LEVEL%u, ROOT = %s\n", level, spf_root->node_name);

    ITERATE_LIST_BEGIN(spf_root->spf_run_result[level], list_node){
        res = (spf_result_t *)list_node->data;
        printf("DEST : %-10s spf_metric : %-6u", res->node->node_name, res->spf_metric);
        printf(" Nxt Hop : ");

        j = 0;
        ITERATE_NH_TYPE_BEGIN(nh){

            for( i = 0; i < MAX_NXT_HOPS; i++, j++){
                if(is_nh_list_empty2(&res->next_hop[nh][i]))
                    break;

                oif = res->next_hop[nh][i].oif;
                if(j == 0){
                    printf("%s|%-8s       OIF : %-7s    gateway : %s\n", 
                            res->next_hop[nh][i].node->node_name, nh == LSPNH ? "LSPNH" : "IPNH", 
                            oif->intf_name, res->next_hop[nh][i].gw_prefix);
                }
                else{
                    printf("                                              : %s|%-8s       OIF : %-7s    gateway : %s\n", 
                            res->next_hop[nh][i].node->node_name, nh == LSPNH ? "LSPNH" : "IPNH", 
                            oif->intf_name, res->next_hop[nh][i].gw_prefix);
                }
            }
        } ITERATE_NH_TYPE_END;
    }ITERATE_LIST_END;
}

static int
validate_debug_log_enable_disable(char *value_passed){

    if(strncmp(value_passed, "enable", strlen(value_passed)) == 0 ||
        strncmp(value_passed, "disable", strlen(value_passed)) == 0)
        return VALIDATION_SUCCESS;

    printf("Error : Incorrect log status specified\n");
    return VALIDATION_FAILED;
}

static int 
validate_node_extistence(char *node_name){

    if(singly_ll_search_by_key(instance->instance_node_list, node_name))
        return VALIDATION_SUCCESS;

    printf("Error : Node %s do not exist\n", node_name);
    return VALIDATION_FAILED;
}

static int
validate_level_no(char *value_passed){

    LEVEL level = atoi(value_passed);
    if(level == LEVEL1 || level == LEVEL2)
        return VALIDATION_SUCCESS;

    printf("Error : Incorrect Level Value.\n");
    return VALIDATION_FAILED;
}

static int
validate_metric_value(char *value_passed){

    unsigned int metric = atoi(value_passed);
    if(metric >= 0 && metric <= INFINITE_METRIC)
        return VALIDATION_SUCCESS;

    printf("Error : Incorrect metric Value.\n");
    return VALIDATION_FAILED;
}

static int
node_slot_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    tlv_struct_t *tlv = NULL;
    char *slot_name = NULL;
    char *node_name = NULL; 
    node_t *node = NULL;
    int cmd_code = -1;
    LEVEL level = MAX_LEVEL;
    unsigned int metric = 0;
      
    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
            slot_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "level-no", strlen("level-no")) ==0)
            level = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "metric", strlen("metric")) ==0)
            metric = atoi(tlv->value);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    cmd_code = EXTRACT_CMD_CODE(tlv_buf);

    switch(cmd_code){
        case CMDCODE_CONFIG_NODE_SLOT_ENABLE:
            spf_node_slot_enable_disable(node, slot_name, enable_or_disable);
            break;
        case CMFCODE_CONFIG_NODE_SLOT_METRIC_CHANGE:
            if(enable_or_disable == CONFIG_DISABLE)
                spf_node_slot_metric_change(node, slot_name, level, LINK_DEFAULT_METRIC);
            else
                spf_node_slot_metric_change(node, slot_name, level, metric);
            break;
        default:
            printf("%s() : Error : No Handler for command code : %d\n", __FUNCTION__, cmd_code);
            break;
    }
    return 0;
}

static int
show_route_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    char *node_name = NULL;
    tlv_struct_t *tlv = NULL;
    node_t *node = NULL;
    char *prefix = NULL;
    char mask = 0;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
             node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "prefix", strlen("prefix")) ==0)
             prefix = tlv->value;
        else if(strncmp(tlv->leaf_id, "mask", strlen("mask")) ==0)
             mask = atoi(tlv->value);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    show_routing_table(node->spf_info.rttable, prefix, mask);
    return 0;
} 

static int
show_traceroute_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    char *node_name = NULL;
    char *prefix = NULL;
    tlv_struct_t *tlv = NULL;
    
    int cmdcode = EXTRACT_CMD_CODE(tlv_buf);
     
    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
             node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "prefix", strlen("prefix")) ==0)
            prefix = tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;
    switch(cmdcode){
        case CMDCODE_SHOW_NODE_TRACEROUTE_PRIMARY:
            show_traceroute(node_name, prefix);
            break;
        case CMDCODE_SHOW_NODE_TRACEROUTE_BACKUP:
            show_backup_traceroute(node_name, prefix);
            break;
        default:
            assert(0);
    }
    return 0; 
}

extern void
dump_next_hop(internal_nh_t *nh);

static int
show_instance_node_spaces(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    char *node_name = NULL,
         *slot_name = NULL;
    unsigned int i = 0;
    tlv_struct_t *tlv = NULL;
    node_t *node = NULL; 

    edge_end_t *edge_end = NULL;
    int cmdcode = -1;
    LEVEL level_it ;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
             slot_name = tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;

   cmdcode = EXTRACT_CMD_CODE(tlv_buf); 
   node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

   switch(cmdcode){
       case CMDCODE_DEBUG_SHOW_NODE_INTF_EXPSPACE:
           {
               for(i = 0; i < MAX_NODE_INTF_SLOTS; i++ ) {
                   edge_end = node->edges[i];
                   if(edge_end == NULL){
                       printf("Error : slot-no %s do not exist\n", slot_name);
                       return 0;
                   }
                   if(strncmp(slot_name, edge_end->intf_name, strlen(edge_end->intf_name)))
                       continue;

                   if(edge_end->dirn != OUTGOING)
                       continue;

                   edge_t *edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
                   init_back_up_computation(node, edge->level);
                   Compute_and_Store_Forward_SPF(node, edge->level);
                   Compute_PHYSICAL_Neighbor_SPFs(node, edge->level);
                   if(is_broadcast_link(edge, edge->level))
                       broadcast_compute_link_node_protecting_extended_p_space(node, edge, edge->level);
                   else 
                       p2p_compute_link_node_protecting_extended_p_space(node, edge, edge->level);

                   printf("Node %s Extended p-space : \n", node->node_name);

                   unsigned int j = 0, nh_count = 0;
                   internal_nh_t *p_node = NULL;

                   for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
                       printf("\nextended p-space at %s: \n", get_str_level(level_it));
                       nh_count = get_nh_count(node->pq_nodes[level_it]);
                       for(j = 0; j < nh_count; j++){
                            p_node = &node->pq_nodes[level_it][j];
                            dump_next_hop(p_node);
                            printf("\n");
                       }
                   }
                   return 0;
               }
           }
           break;
       case CMDCODE_DEBUG_SHOW_NODE_INTF_PQSPACE:
           {
               /*compute extended p-space first*/
               edge_t *edge = NULL;
               for(i = 0; i < MAX_NODE_INTF_SLOTS; i++ ) {

                   edge_end = node->edges[i];
                   if(edge_end == NULL){
                       printf("Error : slot-no %s do not exist\n", slot_name);
                       return 0;
                   }

                   if(strncmp(slot_name, edge_end->intf_name, strlen(edge_end->intf_name)))
                       continue;

                   if(edge_end->dirn != OUTGOING)
                       continue;

                   edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
                   break;
               }
               init_back_up_computation(node, edge->level);
               Compute_and_Store_Forward_SPF(node, edge->level);
               Compute_PHYSICAL_Neighbor_SPFs(node, edge->level);
               if(is_broadcast_link(edge, edge->level)){
                   broadcast_compute_link_node_protecting_extended_p_space(node, edge, edge->level);
                   broadcast_filter_select_pq_nodes_from_ex_pspace(node, edge, edge->level);
               }
               else{
                   p2p_compute_link_node_protecting_extended_p_space(node, edge, edge->level);
                   p2p_filter_select_pq_nodes_from_ex_pspace(node, edge, edge->level);
               }
               printf("Node %s pq-space computed.\n", node->node_name);
           }
           break;
          default:
            ;
   }
   return 0;
}


static int
instance_node_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int cmd_code = -1;
    int mask = 0;
    node_t *node = NULL;
    tlv_struct_t *tlv = NULL;
    unsigned int i = 0,
                 metric = 0;
    char *node_name = NULL,
         *intf_name = NULL,
         *lsp_name = NULL,
         *tail_end_ip = NULL;

    LEVEL level         = MAX_LEVEL,
          from_level_no = MAX_LEVEL,
          to_level_no   = MAX_LEVEL;
    char *prefix = NULL;
    
    dist_info_hdr_t dist_info_hdr;
    memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
    cmd_code = EXTRACT_CMD_CODE(tlv_buf);
    
    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "prefix", strlen("prefix")) == 0)
            prefix = tlv->value;
        else if(strncmp(tlv->leaf_id, "mask", strlen("mask")) == 0)
            mask = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "level-no", strlen("level-no")) == 0)
            level = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "from-level-no", strlen("from-level-no")) == 0)
            from_level_no = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "to-level-no", strlen("to-level-no")) == 0)
            to_level_no = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
            intf_name =  tlv->value;
        else if(strncmp(tlv->leaf_id, "lsp-name", strlen("lsp-name")) ==0)
            lsp_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "metric-val", strlen("metric-val")) ==0)
            metric = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "tail-end-ip", strlen("tail-end-ip")) ==0)
            tail_end_ip = tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    
    switch(cmd_code){
        case CMDCODE_CONFIG_NODE_SPF_BACKUP_OPTIONS:
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    SET_BIT(node->backup_spf_options, SPF_BACKUP_OPTIONS_ENABLED);
                    break;
                case CONFIG_DISABLE:
                    UNSET_BIT(node->backup_spf_options, SPF_BACKUP_OPTIONS_ENABLED);
                    UNSET_BIT(node->backup_spf_options, SPF_BACKUP_OPTIONS_REMOTE_BACKUP_CALCULATION);
                    UNSET_BIT(node->backup_spf_options, SPF_BACKUP_OPTIONS_NODE_LINK_DEG);
                    break;
                default:
                    assert(0);
            }
            break;
        case CMDCODE_CONFIG_NODE_REMOTE_BACKUP_CALCULATION:
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    SET_BIT(node->backup_spf_options, SPF_BACKUP_OPTIONS_REMOTE_BACKUP_CALCULATION);
                    break;
                case CONFIG_DISABLE:
                    UNSET_BIT(node->backup_spf_options, SPF_BACKUP_OPTIONS_REMOTE_BACKUP_CALCULATION);
                    break;
                default:
                    assert(0);
            }
            break;
        case CMDCODE_CONFIG_NODE_LINK_DEGRADATION:
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    SET_BIT(node->backup_spf_options, SPF_BACKUP_OPTIONS_NODE_LINK_DEG);
                    break;
                case CONFIG_DISABLE:
                    UNSET_BIT(node->backup_spf_options, SPF_BACKUP_OPTIONS_NODE_LINK_DEG);
                    break;
                default:
                    assert(0);
            }
            break;
        case CMDCODE_CONFIG_NODE_RSVPLSP:
            {
                boolean rc = FALSE;
                switch(enable_or_disable){
                    case CONFIG_ENABLE:
                        rc = insert_lsp_as_forward_adjacency(node, lsp_name, metric, tail_end_ip, level);
                        break;
                    case CONFIG_DISABLE:
                        break;
                    default:
                        ;
                }
                if(rc == FALSE)
                    break;
                dist_info_hdr_t dist_info_hdr;
                memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
                dist_info_hdr.info_dist_level = level;
                dist_info_hdr.advert_id = TLV2;
                generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);
            }
            break;
        case CMDCODE_CONFIG_NODE_OVERLOAD_STUBNW:
            {
                edge_end_t *edge_end = NULL;

                switch(enable_or_disable){
                    case CONFIG_ENABLE:
                        if(!IS_OVERLOADED(node, level)){
                            printf("Error : Router not overloaded\n");
                            return 0;
                        }
                        break;
                    case CONFIG_DISABLE:
                        break;
                    default:
                        assert(0);
                }

                for(i = 0; i < MAX_NODE_INTF_SLOTS; i++ ) {
                    edge_end = node->edges[i];

                   if(edge_end == NULL){
                       printf("Error : slot-no %s do not exist\n", intf_name);
                       return 0;
                   }
                    
                    if(strncmp(intf_name, edge_end->intf_name, strlen(edge_end->intf_name)))
                        continue;

                    if(edge_end->dirn != OUTGOING)
                        continue;

                    edge_t *edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);

                    if(!IS_LEVEL_SET(edge->level, level)){
                        printf("Error : slot-no %s is not operating in level %s\n", intf_name, get_str_level(level));
                        return 0;
                    }
                    
                    prefix_t *prefix = edge_end->prefix[level];
                    
                    switch(enable_or_disable){
                        case CONFIG_ENABLE:
                            prefix->metric = INFINITE_METRIC;
                            break;
                        case CONFIG_DISABLE:
                            prefix->metric = DEFAULT_LOCAL_PREFIX_METRIC;
                            break;
                        case MODE_UNKNOWN:
                            return 0;
                        default:
                            ;
                    }

                    prefix_t *list_prefix = node_local_prefix_search(node, level, prefix->prefix, prefix->mask);
                    assert(list_prefix);

                    list_prefix->metric = prefix->metric;

                    /*Update if the prefix is leaked across levels*/
                    LEVEL other_level = (level == LEVEL1) ? LEVEL2 : LEVEL1;

                    prefix_t *leaked_prefix = node_local_prefix_search(node, other_level, prefix->prefix, prefix->mask);

                    if(leaked_prefix)
                        leaked_prefix->metric = list_prefix->metric;
                        
                    tlv128_ip_reach_t ad_msg;
                    memset(&ad_msg, 0, sizeof(tlv128_ip_reach_t));
                    ad_msg.prefix = prefix->prefix,
                    ad_msg.mask = prefix->mask;
                    ad_msg.metric = prefix->metric;
                    ad_msg.prefix_flags = prefix->prefix_flags; /*Interface attached prefixes have up_down_bit = 0*/
                    ad_msg.hosting_node = node;

                    dist_info_hdr.lsp_generator = node;
                    dist_info_hdr.info_dist_level = level;
                    dist_info_hdr.add_or_remove = (enable_or_disable == CONFIG_ENABLE) ? AD_CONFIG_ADDED : AD_CONFIG_REMOVED;
                    dist_info_hdr.advert_id = TLV128;
                    dist_info_hdr.info_data = (char *)&ad_msg;
                    generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);

                    /*Generate other level LSP update*/
                    
                    if(leaked_prefix){
                        
                        ad_msg.prefix_flags = leaked_prefix->prefix_flags;
                        dist_info_hdr.info_dist_level = other_level;
                        generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);
                    }
                    return 0;
                }
            }
             break;

        case CMDCODE_CONFIG_NODE_OVERLOAD:
             {
                 lsp_hdr_t lsp_hdr;

                 switch(enable_or_disable){
                     case CONFIG_ENABLE:
                         SET_BIT(node->attributes[level], OVERLOAD_BIT);
                         lsp_hdr.overload = 1;
                         break;
                     case CONFIG_DISABLE:
                         UNSET_BIT(node->attributes[level], OVERLOAD_BIT);
                         lsp_hdr.overload = 0;
                         break;
                     case MODE_UNKNOWN:
                         return 0;
                     default:
                         ;
                 }

                 dist_info_hdr.lsp_generator = node;
                 dist_info_hdr.info_dist_level = level;
                 dist_info_hdr.add_or_remove = (enable_or_disable == CONFIG_ENABLE) ? AD_CONFIG_ADDED : AD_CONFIG_REMOVED;
                 dist_info_hdr.advert_id = OVERLOAD;
                 dist_info_hdr.info_data = (char *)&lsp_hdr;
                 generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);
             }
             break;

        case CMDCODE_CONFIG_INSTANCE_IGNOREBIT_ENABLE:
            (enable_or_disable == CONFIG_ENABLE) ? SET_BIT(node->instance_flags, IGNOREATTACHED) :
                UNSET_BIT(node->instance_flags, IGNOREATTACHED);
            break;
        case CMDCODE_CONFIG_INSTANCE_ATTACHBIT_ENABLE:
            node->attached = (enable_or_disable == CONFIG_ENABLE) ? 1 : 0;
            break;
        case CMDCODE_CONFIG_NODE_EXPORT_PREFIX:
        {

            prefix_t *pfx = node_local_prefix_search(node, 
                    level, prefix, mask);
            if(pfx){
                printf("Error : Attempt to add duplicate prefix %s/%u in %s",
                        prefix, mask, get_str_level(level));
                return 0;
            }
            tlv128_ip_reach_t ad_msg;
            memset(&ad_msg, 0, sizeof(tlv128_ip_reach_t));
            ad_msg.prefix = prefix,
            ad_msg.mask = mask;
            ad_msg.metric = 0;
            SET_BIT(ad_msg.prefix_flags, PREFIX_EXTERNABIT_FLAG);
            ad_msg.hosting_node = node;

            dist_info_hdr.lsp_generator = node;
            dist_info_hdr.info_dist_level = level;
            dist_info_hdr.add_or_remove = (enable_or_disable == CONFIG_ENABLE) ? AD_CONFIG_ADDED : AD_CONFIG_REMOVED;
            dist_info_hdr.advert_id = TLV128;
            dist_info_hdr.info_data = (char *)&ad_msg;
            /*Adopting to PRC behavior*/
            pfx = attach_prefix_on_node (node, prefix, mask, level, 0, ad_msg.prefix_flags);
            generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);
        }
            break;     
        case CMDCODE_CONFIG_NODE_LEAK_PREFIX:
        {
            prefix_t *leaked_prefix = NULL;
            if((leaked_prefix = leak_prefix(node_name, prefix, mask, from_level_no, to_level_no)) == NULL)
                break;
            tlv128_ip_reach_t ad_msg;
            memset(&ad_msg, 0, sizeof(tlv128_ip_reach_t));
            ad_msg.prefix = prefix,
            ad_msg.mask = mask;
            ad_msg.metric = leaked_prefix->metric;
            ad_msg.prefix_flags = leaked_prefix->prefix_flags;
            ad_msg.hosting_node = node;

            dist_info_hdr.lsp_generator = node;
            dist_info_hdr.info_dist_level = to_level_no;
            dist_info_hdr.add_or_remove = (enable_or_disable == CONFIG_ENABLE) ? AD_CONFIG_ADDED : AD_CONFIG_REMOVED;
            dist_info_hdr.advert_id = TLV128;
            dist_info_hdr.info_data = (char *)&ad_msg;
            generate_lsp(instance, node, lsp_distribution_routine, &dist_info_hdr);
        }
            break;  
        default:
            printf("%s() : Error : No Handler for command code : %d\n", __FUNCTION__, cmd_code);
            break;
    }
    return 0;
}

static int
config_static_route_handler(param_t *param, 
                            ser_buff_t *tlv_buf, 
                            op_mode enable_or_disable){

    node_t *host_node = NULL;
    tlv_struct_t *tlv = NULL;
    char *nh_name = NULL, 
         *host_node_name = NULL,
         *dest_ip = NULL,
         *gw_ip   = NULL,
         *intf_name = NULL;

    int mask = 0;
    rttable_entry_t *rt_entry = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            host_node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "nh-name", strlen("nh-name")) ==0)
            nh_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "destination", strlen("destination")) ==0)
            dest_ip = tlv->value;
        else if(strncmp(tlv->leaf_id, "mask", strlen("mask")) ==0)
            mask = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "gateway", strlen("gateway")) ==0)
            gw_ip = tlv->value;
        else if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
            intf_name =  tlv->value;
        else
            assert(0);
    } TLV_LOOP_END;

    host_node = singly_ll_search_by_key(instance->instance_node_list, host_node_name);

    switch(enable_or_disable){
        case CONFIG_ENABLE:
            
            rt_entry = calloc(1, sizeof(rttable_entry_t));

            strncpy(rt_entry->dest.prefix, dest_ip, 15);
            rt_entry->dest.prefix[15] = '\0';
            rt_entry->dest.mask = mask;
            rt_entry->version = 0;
            rt_entry->cost = 0;
            rt_entry->primary_nh_count[IPNH] = 1;
            rt_entry->primary_nh[IPNH][0].nh_type = IPNH;
            strncpy(rt_entry->primary_nh[IPNH][0].oif, intf_name, IF_NAME_SIZE);
            rt_entry->primary_nh[IPNH][0].oif[IF_NAME_SIZE] = '\0'; 
            strncpy(rt_entry->primary_nh[IPNH][0].nh_name, nh_name, NODE_NAME_SIZE);
            rt_entry->primary_nh[0][IPNH].nh_name[NODE_NAME_SIZE] = '\0';
            strncpy(rt_entry->primary_nh[IPNH][0].gwip, gw_ip, 15);
            rt_entry->primary_nh[IPNH][0].gwip[15] = '\0';

            if(rt_route_install(host_node->spf_info.rttable, rt_entry) > 0)
                return 0;
            free(rt_entry);
            rt_entry = NULL;
            return -1;

        case CONFIG_DISABLE:
            rt_route_delete(host_node->spf_info.rttable, dest_ip, mask);
            break;
        default:
            ;
    }
    return 0;
}

static int
set_unset_traceoptions(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int CMDCODE;
    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);
    switch(CMDCODE){
        case CMDCODE_DEBUG_TRACEOPTIONS_DIJKASTRA:
            enable_or_disable == CONFIG_ENABLE ? enable_spf_trace(instance, DIJKSTRA_BIT):
                disable_spf_trace(instance, DIJKSTRA_BIT);
            break;
        case CMDCODE_DEBUG_TRACEOPTIONS_ROUTE_INSTALLATION:
            enable_or_disable == CONFIG_ENABLE ? enable_spf_trace(instance, ROUTE_INSTALLATION_BIT):
                disable_spf_trace(instance, ROUTE_INSTALLATION_BIT);
            break;
        case CMDCODE_DEBUG_TRACEOPTIONS_ROUTE_CALCULATION:
            enable_or_disable == CONFIG_ENABLE ? enable_spf_trace(instance, ROUTE_CALCULATION_BIT):
                disable_spf_trace(instance, ROUTE_CALCULATION_BIT);
            break;
        case CMDCODE_DEBUG_TRACEOPTIONS_LFA:
            enable_or_disable == CONFIG_ENABLE ? enable_spf_trace(instance, LFA_COMPUTATION_BIT):
                disable_spf_trace(instance, LFA_COMPUTATION_BIT);
            break;
        case CMDCODE_DEBUG_TRACEOPTIONS_RLFA:
            enable_or_disable == CONFIG_ENABLE ? enable_spf_trace(instance, RLFA_COMPUTATION_BIT):
                disable_spf_trace(instance, RLFA_COMPUTATION_BIT);
            break;
        case CMDCODE_DEBUG_TRACEOPTIONS_PREFIXES:
            enable_or_disable == CONFIG_ENABLE ? enable_spf_trace(instance, SPF_PREFIX_BIT):
                disable_spf_trace(instance, SPF_PREFIX_BIT);
            break;
        case CMDCODE_DEBUG_TRACEOPTIONS_ROUTING_TABLE:
            enable_or_disable == CONFIG_ENABLE ? enable_spf_trace(instance, ROUTING_TABLE_BIT):
                disable_spf_trace(instance, ROUTING_TABLE_BIT);
            break;
        case CMDCODE_DEBUG_TRACEOPTIONS_ALL:
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    enable_spf_trace(instance, DIJKSTRA_BIT);
                    enable_spf_trace(instance, ROUTE_CALCULATION_BIT);
                    enable_spf_trace(instance, ROUTE_INSTALLATION_BIT);
                    enable_spf_trace(instance, LFA_COMPUTATION_BIT);
                    enable_spf_trace(instance, RLFA_COMPUTATION_BIT);
                    enable_spf_trace(instance, SPF_PREFIX_BIT);
                    enable_spf_trace(instance, ROUTING_TABLE_BIT);
                    break;
                case CONFIG_DISABLE:
                    disable_spf_trace(instance, DIJKSTRA_BIT);
                    disable_spf_trace(instance, ROUTE_INSTALLATION_BIT);
                    disable_spf_trace(instance, ROUTE_CALCULATION_BIT);
                    disable_spf_trace(instance, LFA_COMPUTATION_BIT);
                    disable_spf_trace(instance, RLFA_COMPUTATION_BIT);
                    disable_spf_trace(instance, SPF_PREFIX_BIT);
                    disable_spf_trace(instance, ROUTING_TABLE_BIT);
                    break;
                default:
                    assert(0);
            }
            break;
        default: 
            assert(0);
    }
    return 0; 
}


static int
debug_log_enable_disable_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    
    tlv_struct_t *tlv = NULL;
    char *value = NULL;
    int CMDCODE = EXTRACT_CMD_CODE(tlv_buf);
     
    TLV_LOOP_BEGIN(tlv_buf, tlv){
        value = tlv->value;        
    } TLV_LOOP_END;

    switch(CMDCODE){
        case CMDCODE_DEBUG_LOG_ENABLE_DISABLE:
            if(strncmp(value, "enable", strlen(value)) ==0){
                enable_spf_tracing();
            }
            else if(strncmp(value, "disable", strlen(value)) ==0){
                disable_spf_tracing();
            }
           break; 
        case CMDCODE_DEBUG_LOG_FILE_ENABLE_DISABLE:
            if(strncmp(value, "enable", strlen(value)) ==0){
                trace_set_log_medium(instance->traceopts, LOG_FILE);
            }
            else if(strncmp(value, "disable", strlen(value)) ==0){
                trace_set_log_medium(instance->traceopts, CONSOLE);
            }
           break; 
        default:
            assert(0);
    } 
    return 0;
}

static void
show_spf_run_stats(node_t *node, LEVEL level){

    printf("SPF Statistics - root : %s, LEVEL%u\n", node->node_name, level);
    printf("# SPF runs : %u\n", node->spf_info.spf_level_info[level].version);
}


static int
show_spf_run_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    tlv_struct_t *tlv = NULL;
    LEVEL level = LEVEL1 | LEVEL2;;
    char *node_name = NULL;
    node_t *spf_root = NULL;
    int CMDCODE = -1;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);
     
    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "level-no", strlen("level-no")) ==0){
            level = atoi(tlv->value);
        }
        else if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0){
            node_name = tlv->value;
        }
    } TLV_LOOP_END;

    if(node_name == NULL)
        spf_root = instance->instance_root;
    else
        spf_root = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
   
   
    switch(CMDCODE){
        case CMDCODE_SHOW_SPF_RUN:
            spf_computation(spf_root, &spf_root->spf_info, level, FULL_RUN);
            show_spf_results(spf_root, level);
            break;
        case CMDCODE_SHOW_SPF_RUN_PRC:
            partial_spf_run(spf_root, level);
            break;
        case CMDCODE_SHOW_SPF_STATS:
            show_spf_run_stats(spf_root, level);
            break;
        case CMDCODE_SHOW_SPF_RUN_INVERSE:
            inverse_topology(instance, level);
            spf_computation(spf_root, &spf_root->spf_info, level, FORWARD_RUN);
            inverse_topology(instance, level);
            show_spf_results(spf_root, level);
            break;
        case CMDCODE_SHOW_SPF_RUN_INIT:
            spf_only_intitialization(spf_root, level);
            show_spf_initialization(spf_root, level);
            break;
        default:
            assert(0);
    }
    return 0;
}

static int
show_instance_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    
    singly_ll_node_t *list_node = NULL;
    tlv_struct_t *tlv = NULL;
    LEVEL level = LEVEL_UNKNOWN;
    char *node_name = NULL;
    int CMDCODE = -1;
    node_t *node = NULL;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf); 

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "level-no", strlen("level-no")) == 0)
            level = atoi(tlv->value);   
        else
            assert(0); 
    } TLV_LOOP_END;
    
    switch(CMDCODE){
        
        case CMDCODE_SHOW_INSTANCE_LEVEL:
            printf("Graph root : %s(0x%x)\n", instance->instance_root->node_name, (unsigned int)instance->instance_root);
            ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
                dump_nbrs(list_node->data, level);
            }ITERATE_LIST_END;
            break;
        case CMDCODE_SHOW_INSTANCE_NODE_LEVEL:
            node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
            dump_nbrs(node, level);
            break;
        default:
            assert(0);
    }
    return 0;
}


static int
show_instance_node_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    tlv_struct_t *tlv = NULL;
    char *node_name = NULL;
    node_t *node = NULL;
    
    TLV_LOOP_BEGIN(tlv_buf, tlv){
        node_name = tlv->value;
    } TLV_LOOP_END;

    node =  (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    
    dump_node_info(node); 
    return 0;
}



void
spf_init_dcm(){
    
    init_libcli();

    param_t *show   = libcli_get_show_hook();
    param_t *debug  = libcli_get_debug_hook();
    param_t *config = libcli_get_config_hook();
    param_t *run    = libcli_get_run_hook();
    param_t *debug_show = libcli_get_debug_show_hook();

    /*run commands*/

    /*run instance sync*/
    
    {
        static param_t instance;
        init_param(&instance, CMD, "instance", 0, 0, INVALID, 0, "Network graph");
        libcli_register_param(run, &instance);

        {
            static param_t sync;
            init_param(&sync, CMD, "sync", run_spf_run_all_nodes, 0, INVALID, 0, "Run L1L2 spf run on entire Network graph");
            libcli_register_param(&instance, &sync);
            set_param_cmd_code(&sync, CMDCODE_RUN_INSTANCE_SYNC);
        }
    }

    /*Show commands*/
    
    /*show instance level <level>*/
    static param_t instance;
    init_param(&instance, CMD, "instance", 0, 0, INVALID, 0, "Network graph");
    libcli_register_param(show, &instance);

    static param_t instance_level;
    init_param(&instance_level, CMD, "level", 0, 0, INVALID, 0, "level");
    libcli_register_param(&instance, &instance_level);
     
    static param_t instance_level_level;
    init_param(&instance_level_level, LEAF, 0, show_instance_handler, validate_level_no, INT, "level-no", "level");
    libcli_register_param(&instance_level, &instance_level_level);
    set_param_cmd_code(&instance_level_level, CMDCODE_SHOW_INSTANCE_LEVEL);

    /*show instance node <node-name>*/
    static param_t instance_node;
    init_param(&instance_node, CMD, "node", 0, 0, INVALID, 0, "node");
    libcli_register_param(&instance, &instance_node);
    libcli_register_display_callback(&instance_node, display_instance_nodes);

    static param_t instance_node_name;
    init_param(&instance_node_name, LEAF, 0, show_instance_node_handler, validate_node_extistence, STRING, "node-name", "Node Name");
    libcli_register_param(&instance_node, &instance_node_name);
    
    {
        /*show instance node <node-name> backup-spf_results */
        static param_t backup_spf_results;
        init_param(&backup_spf_results, CMD, "backup-spf-results", 
                debug_show_node_back_up_spf_results, 0, INVALID, 0, "Back up Results");  
        libcli_register_param(&instance_node_name, &backup_spf_results);
        set_param_cmd_code(&backup_spf_results, CMDCODE_SHOW_BACKUP_SPF_RESULTS);
        {
            static param_t dest;
            init_param(&dest, LEAF, 0, debug_show_node_back_up_spf_results, validate_node_extistence, STRING, "dst-name", "Destination Name");
            libcli_register_param(&backup_spf_results, &dest);
            set_param_cmd_code(&dest, CMDCODE_SHOW_BACKUP_SPF_RESULTS);      
        }
    }

    /*show instance node <node-name> traceroute <prefix>*/

    static param_t traceroute;
    init_param(&traceroute, CMD, "traceroute", 0, 0, INVALID, 0, "trace route");
    libcli_register_param(&instance_node_name, &traceroute);

    static param_t traceroute_prefix;
    init_param(&traceroute_prefix, LEAF, 0, show_traceroute_handler, 0, IPV4, "prefix", "Destination address (ipv4)");
    libcli_register_param(&traceroute, &traceroute_prefix);
    set_param_cmd_code(&traceroute_prefix, CMDCODE_SHOW_NODE_TRACEROUTE_PRIMARY);
        /*show instance node <node-name> traceroute <prefix> backup*/
    {
        static param_t backup;
        init_param(&backup, CMD, "backup", show_traceroute_handler, 0, INVALID, 0, "trace backup route");
        libcli_register_param(&traceroute_prefix, &backup);
        set_param_cmd_code(&backup, CMDCODE_SHOW_NODE_TRACEROUTE_BACKUP);
    }

    /*show instance node <node-name> route*/
    static param_t route;
    init_param(&route, CMD, "route", show_route_handler, 0, INVALID, 0, "routing table");
    libcli_register_param(&instance_node_name, &route);
    {
        static param_t prefix;
        init_param(&prefix, LEAF, 0, 0, 0, IPV4, "prefix", "Ipv4 prefix without mask");
        libcli_register_param(&route, &prefix);
        {
            static param_t mask;
            init_param(&mask, LEAF, 0, show_route_handler, validate_ipv4_mask, INT, "mask", "mask (0-32)");
            libcli_register_param(&prefix, &mask);
        }
    }
    /*show instance node <node-name> level <level-no>*/ 
    static param_t instance_node_name_level;
    init_param(&instance_node_name_level, CMD, "level", 0, 0, INVALID, 0, "level");
    libcli_register_param(&instance_node_name, &instance_node_name_level);

    static param_t instance_node_name_level_level;
    init_param(&instance_node_name_level_level, LEAF, 0, show_instance_handler, validate_level_no, INT, "level-no", "level");
    libcli_register_param(&instance_node_name_level, &instance_node_name_level_level);
    set_param_cmd_code(&instance_node_name_level_level, CMDCODE_SHOW_INSTANCE_NODE_LEVEL);
    
    /*show spf run*/

    static param_t show_spf;
    init_param(&show_spf, CMD, "spf", 0, 0, INVALID, 0, "Shortest Path Tree");
    libcli_register_param(show, &show_spf);

    static param_t show_spf_run;
    init_param(&show_spf_run, CMD, "run", 0, 0, INVALID, 0, "run SPT computation");
    libcli_register_param(&show_spf, &show_spf_run);
   
    /*show spf run level */ 
    static param_t show_spf_run_level;
    init_param(&show_spf_run_level, CMD, "level", 0, 0, INVALID, 0, "level");
    libcli_register_param(&show_spf_run, &show_spf_run_level);
    
    /* show spf run level <Level NO>*/
    static param_t show_spf_run_level_N;
    init_param(&show_spf_run_level_N, LEAF, 0, show_spf_run_handler, validate_level_no, INT, "level-no", "level : 1 | 2");
    libcli_register_param(&show_spf_run_level, &show_spf_run_level_N);
    set_param_cmd_code(&show_spf_run_level_N, CMDCODE_SHOW_SPF_RUN);

    /*show spf run level <level no> init*/
    {
        static param_t init;
        init_param(&init, CMD, "init", show_spf_run_handler, 0, INVALID, 0, "show spf initialization");
        libcli_register_param(&show_spf_run_level_N, &init);
        set_param_cmd_code(&init, CMDCODE_SHOW_SPF_RUN_INIT);
    }
        
    /* show spf run level <Level NO> prc*/
    {
        static param_t prc;
        init_param(&prc, CMD, "prc", show_spf_run_handler, 0, INVALID, 0, "run partial SPT computation");
        libcli_register_param(&show_spf_run_level_N, &prc);
        set_param_cmd_code(&prc, CMDCODE_SHOW_SPF_RUN_PRC);
    }

    static param_t show_spf_run_level_N_inverse;
    init_param(&show_spf_run_level_N_inverse, CMD, "inverse", show_spf_run_handler, 0, INVALID, 0, "Inverse SPF");
    libcli_register_param(&show_spf_run_level_N, &show_spf_run_level_N_inverse);
    set_param_cmd_code(&show_spf_run_level_N_inverse, CMDCODE_SHOW_SPF_RUN_INVERSE);

    static param_t show_spf_run_level_N_root;
    init_param(&show_spf_run_level_N_root, CMD, "root", 0, 0, INVALID, 0, "spf root");
    libcli_register_param(&show_spf_run_level_N, &show_spf_run_level_N_root);
    libcli_register_display_callback(&show_spf_run_level_N_root, display_instance_nodes); 
     
    static param_t show_spf_run_level_N_root_root_name;
    init_param(&show_spf_run_level_N_root_root_name, LEAF, 0, show_spf_run_handler, validate_node_extistence, STRING, "node-name", "node name to be SPF root");
    libcli_register_param(&show_spf_run_level_N_root, &show_spf_run_level_N_root_root_name);
    set_param_cmd_code(&show_spf_run_level_N_root_root_name, CMDCODE_SHOW_SPF_RUN);
    
    /* show spf run level <Level NO> root <node-name> prc*/

    {
        static param_t prc;
        init_param(&prc, CMD, "prc", show_spf_run_handler, 0, INVALID, 0, "run partial SPT computation");
        libcli_register_param(&show_spf_run_level_N_root_root_name, &prc);
        set_param_cmd_code(&prc, CMDCODE_SHOW_SPF_RUN_PRC);
    }
    
    static param_t show_spf_run_level_N_root_root_name_inverse;
    init_param(&show_spf_run_level_N_root_root_name_inverse, CMD, "inverse", show_spf_run_handler, 0, INVALID, 0, "Inverse SPF");
    libcli_register_param(&show_spf_run_level_N_root_root_name, &show_spf_run_level_N_root_root_name_inverse);
    set_param_cmd_code(&show_spf_run_level_N_root_root_name_inverse, CMDCODE_SHOW_SPF_RUN_INVERSE);
    /* show spf statistics */

    static param_t show_spf_statistics;
    init_param(&show_spf_statistics, CMD, "statistics", show_spf_run_handler, 0, INVALID, 0, "SPF Statistics");
    libcli_register_param(&show_spf_run_level_N_root_root_name, &show_spf_statistics);
    set_param_cmd_code(&show_spf_statistics, CMDCODE_SHOW_SPF_STATS);

    /*config commands */
    
        /*config node <node-name> [no] interface <slot-name> enable*/
        static param_t config_node;
        init_param(&config_node, CMD, "node", 0, 0, INVALID, 0, "node");
        libcli_register_param(config, &config_node);
        libcli_register_display_callback(&config_node, display_instance_nodes); 


        /*config debug commands*/

        {
            static param_t debug;
            init_param(&debug, CMD, "debug", 0, 0, INVALID, 0, "debug");
            libcli_register_param(config, &debug);
            {
                static param_t set;
                init_param(&set, CMD, "set", 0, 0, INVALID, 0, "set trace");
                libcli_register_param(&debug, &set);
                {
                    static param_t trace;
                    init_param(&trace, CMD, "trace", 0, 0, INVALID, 0, "set trace");
                    libcli_register_param(&set, &trace);
                    {
                        static param_t trace_all;
                        init_param(&trace_all, CMD, "all", set_unset_traceoptions, 0, INVALID, 0, "Enable|Disable all traces");
                        libcli_register_param(&trace, &trace_all);
                        set_param_cmd_code(&trace_all, CMDCODE_DEBUG_TRACEOPTIONS_ALL);
                    }
                    {
                        static param_t trace_type_dijkastra;
                        init_param(&trace_type_dijkastra, CMD, "dijkastra", set_unset_traceoptions, 0, INVALID, 0, "Enable trace for Dijkastra");
                        libcli_register_param(&trace, &trace_type_dijkastra);
                        set_param_cmd_code(&trace_type_dijkastra, CMDCODE_DEBUG_TRACEOPTIONS_DIJKASTRA);
                    }
                    {
                        static param_t route_installation;
                        init_param(&route_installation, CMD, "route-installation", set_unset_traceoptions, 0, INVALID, 0, "Enable trace for Route Installation");
                        libcli_register_param(&trace, &route_installation);
                        set_param_cmd_code(&route_installation, CMDCODE_DEBUG_TRACEOPTIONS_ROUTE_INSTALLATION);
                    }
                    {
                        static param_t route_calculation;
                        init_param(&route_calculation, CMD, "route-calculation", set_unset_traceoptions, 0, INVALID, 0, "Enable trace for Route Calculation");
                        libcli_register_param(&trace, &route_calculation);
                        set_param_cmd_code(&route_calculation, CMDCODE_DEBUG_TRACEOPTIONS_ROUTE_CALCULATION);
                    }
                    {
                        static param_t lfa;
                        init_param(&lfa, CMD, "lfa", set_unset_traceoptions, 0, INVALID, 0, "Enable trace for LFA");
                        libcli_register_param(&trace, &lfa);
                        set_param_cmd_code(&lfa, CMDCODE_DEBUG_TRACEOPTIONS_LFA);
                    }
                    {
                        static param_t rlfa;
                        init_param(&rlfa, CMD, "rlfa", set_unset_traceoptions, 0, INVALID, 0, "Enable trace for RLFA");
                        libcli_register_param(&trace, &rlfa);
                        set_param_cmd_code(&rlfa, CMDCODE_DEBUG_TRACEOPTIONS_RLFA);
                    }
                    {
                        static param_t prefixes;
                        init_param(&prefixes, CMD, "prefixes", set_unset_traceoptions, 0, INVALID, 0, "Enable trace for prefixes");
                        libcli_register_param(&trace, &prefixes);
                        set_param_cmd_code(&prefixes, CMDCODE_DEBUG_TRACEOPTIONS_PREFIXES);
                    }
                    {
                        static param_t rt;
                        init_param(&rt, CMD, "routing-table", set_unset_traceoptions, 0, INVALID, 0, "Enable trace for routing table");
                        libcli_register_param(&trace, &rt);
                        set_param_cmd_code(&rt, CMDCODE_DEBUG_TRACEOPTIONS_ROUTING_TABLE);
                    }
                }
            }
        }

        static param_t config_node_node_name;
        init_param(&config_node_node_name, LEAF, 0, 0, validate_node_extistence, STRING, "node-name", "Node Name");
        libcli_register_param(&config_node, &config_node_node_name);

        /*config node <node-name> backup-spf-options*/
        
        {
            static param_t backup_spf_options;
            init_param(&backup_spf_options, CMD, "backup-spf-options", instance_node_config_handler, 0, INVALID, 0, "Configure backup spf options");
            libcli_register_param(&config_node_node_name, &backup_spf_options);
            set_param_cmd_code(&backup_spf_options, CMDCODE_CONFIG_NODE_SPF_BACKUP_OPTIONS);
            {
                /*config node <node-name> backup-spf-options remote_backup_calculation*/
                static param_t remote_backup_calculation;
                init_param(&remote_backup_calculation, CMD, "remote-backup-calculation", instance_node_config_handler, 0, INVALID, 0, "Enable|Disable RLFA computation");
                libcli_register_param(&backup_spf_options, &remote_backup_calculation);
                set_param_cmd_code(&remote_backup_calculation, CMDCODE_CONFIG_NODE_REMOTE_BACKUP_CALCULATION);
            }
            {
                /*config node <node-name> backup-spf-options node-link-degradation*/
                static param_t node_link_degradation;
                init_param(&node_link_degradation, CMD, "node-link-degradation", instance_node_config_handler, 0, INVALID, 0, "Enable|Disable Node link Degradation");
                libcli_register_param(&backup_spf_options, &node_link_degradation);
                set_param_cmd_code(&node_link_degradation, CMDCODE_CONFIG_NODE_LINK_DEGRADATION);
            }
        }

        /*SPRING config Commands*/
        /*config node <node-name> source-packet-routing*/
        {
            static param_t spring;
            init_param(&spring, CMD, "source-packet-routing", instance_node_spring_config_handler, 0, INVALID, 0, "Enable Spring");
            libcli_register_param(&config_node_node_name, &spring);
            set_param_cmd_code(&spring, CMDCODE_CONFIG_NODE_SEGMENT_ROUTING_ENABLE);
        }

        static param_t config_node_node_name_slot;
        init_param(&config_node_node_name_slot, CMD, "interface", 0, 0, INVALID, 0, "interface");
        libcli_register_param(&config_node_node_name, &config_node_node_name_slot);
        libcli_register_display_callback(&config_node_node_name_slot, display_instance_node_interfaces);

        static param_t config_node_node_name_slot_slotname;
        init_param(&config_node_node_name_slot_slotname, LEAF, 0, 0, 0, STRING, "slot-no", "interface name ethx/y format");
        libcli_register_param(&config_node_node_name_slot, &config_node_node_name_slot_slotname);

        /*config node <node-name> [no] interface <slot-no> level <level-no> metric <metric_val>*/
        {
            static param_t level;
            init_param(&level, CMD, "level", 0, 0, INVALID, 0, "level");
            libcli_register_param(&config_node_node_name_slot_slotname, &level);

            static param_t level_no;
            init_param(&level_no, LEAF, 0, 0, validate_level_no, INT, "level-no", "level : 1 | 2");
            libcli_register_param(&level, &level_no);

            static param_t metric;
            init_param(&metric, CMD, "metric", 0, 0, INVALID, 0, "metric");
            libcli_register_param(&level_no, &metric);
            
            static param_t metric_val;
            init_param(&metric_val, LEAF, 0, node_slot_config_handler, validate_metric_value, INT, "metric", "metric value [0 - 4294967295]");
            libcli_register_param(&metric, &metric_val);
            set_param_cmd_code(&metric_val, CMFCODE_CONFIG_NODE_SLOT_METRIC_CHANGE);
        }

        /*config node <node-name> [no] interface <slot-no> link-protection*/
        {
            static param_t link_protection;
            init_param(&link_protection, CMD, "link-protection", lfa_rlfa_config_handler, 0, INVALID, 0, "local link protection");
            libcli_register_param(&config_node_node_name_slot_slotname, &link_protection);
            set_param_cmd_code(&link_protection, CMDCODE_CONFIG_INTF_LINK_PROTECTION);
        }

        /*config node <node-name> [no] interface <slot-no> node-link-protection*/
        {
            static param_t node_link_protection;
            init_param(&node_link_protection, CMD, "node-link-protection", lfa_rlfa_config_handler, 0, INVALID, 0, "local node local link protection");
            libcli_register_param(&config_node_node_name_slot_slotname, &node_link_protection);
            set_param_cmd_code(&node_link_protection, CMDCODE_CONFIG_INTF_NODE_LINK_PROTECTION);
        }

        /*config node <node-name> [no] interface <slot-no> no-eligible-backup*/
        {
            static param_t no_eligible_backup;
            init_param(&no_eligible_backup, CMD, "no-eligible-backup", lfa_rlfa_config_handler, 0, INVALID, 0, "exclude interface from selected as Alternate");
            libcli_register_param(&config_node_node_name_slot_slotname, &no_eligible_backup);
            set_param_cmd_code(&no_eligible_backup, CMDCODE_CONFIG_INTF_NO_ELIGIBLE_BACKUP);
        }
        
        static param_t config_node_node_name_slot_slotname_enable;
        init_param(&config_node_node_name_slot_slotname_enable, CMD, "enable", node_slot_config_handler, 0, INVALID, 0, "enable");
        libcli_register_param(&config_node_node_name_slot_slotname, &config_node_node_name_slot_slotname_enable);
        set_param_cmd_code(&config_node_node_name_slot_slotname_enable, CMDCODE_CONFIG_NODE_SLOT_ENABLE);

    
    /*config node <node-name> overload level <level-no>*/
    {
        static param_t overload;
        init_param(&overload, CMD, "overload", 0, 0, INVALID, 0, "overload the router");
        libcli_register_param(&config_node_node_name, &overload);

        static param_t level;
        init_param(&level, CMD, "level", 0, 0, INVALID, 0, "level");
        libcli_register_param(&overload, &level);

        static param_t level_no;
        init_param(&level_no, LEAF, 0, instance_node_config_handler, validate_level_no, INT, "level-no", "level : 1 | 2");
        libcli_register_param(&level, &level_no);
        set_param_cmd_code(&level_no, CMDCODE_CONFIG_NODE_OVERLOAD);

        /* Production code ISIS CLI : set protocol isis stub-network interface <if-name> [ipv4-unicast | ipv6-univast] level <level-no>*/
        /* Overloading the stub network : config node <node-name> overload level <level-no> interface <slot-no>*/
        static param_t interface;
        init_param(&interface, CMD, "interface", 0, 0, INVALID, 0, "interface");
        libcli_register_param(&level_no, &interface);
            
        static param_t slotno;
        init_param(&slotno, LEAF, 0, instance_node_config_handler, 0, STRING, "slot-no", "interface name ethx/y format");
        libcli_register_param(&interface, &slotno);
        set_param_cmd_code(&slotno, CMDCODE_CONFIG_NODE_OVERLOAD_STUBNW);
    }

    /* config node <node-name> [no] ignorebit enable*/
    static param_t config_node_node_name_ignorebit;
    init_param(&config_node_node_name_ignorebit, CMD, "ignorebit", 0, 0, INVALID, 0, "ignore L1 LSPs from added router when set");
    libcli_register_param(&config_node_node_name, &config_node_node_name_ignorebit);

    static param_t config_node_node_name_ignorebit_enable;
    init_param(&config_node_node_name_ignorebit_enable, CMD, "enable", instance_node_config_handler, 0, INVALID, 0, "enable"); 
    libcli_register_param(&config_node_node_name_ignorebit, &config_node_node_name_ignorebit_enable);
    set_param_cmd_code(&config_node_node_name_ignorebit_enable, CMDCODE_CONFIG_INSTANCE_IGNOREBIT_ENABLE);

    /*config node <node name> export prefix <prefix> <mask> level <level no>*/
    static param_t config_node_node_name_add;
    init_param(&config_node_node_name_add, CMD, "export", 0, 0, INVALID, 0, "export the route");
    libcli_register_param(&config_node_node_name, &config_node_node_name_add);

    static param_t config_node_node_name_add_prefix;
    init_param(&config_node_node_name_add_prefix, CMD, "prefix", 0, 0, INVALID, 0, "prefix");
    libcli_register_param(&config_node_node_name_add, &config_node_node_name_add_prefix);

    static param_t config_node_node_name_add_prefix_prefix;
    init_param(&config_node_node_name_add_prefix_prefix, LEAF, 0, 0, 0, IPV4, "prefix", "Ipv4 prefix without mask");
    libcli_register_param(&config_node_node_name_add_prefix, &config_node_node_name_add_prefix_prefix);

    static param_t config_node_node_name_add_prefix_prefix_mask;
    init_param(&config_node_node_name_add_prefix_prefix_mask, LEAF, 0, 0, validate_ipv4_mask, INT, "mask", "mask (0-32)");
    libcli_register_param(&config_node_node_name_add_prefix_prefix, &config_node_node_name_add_prefix_prefix_mask);

    static param_t config_node_node_name_add_prefix_prefix_mask_level;
    init_param(&config_node_node_name_add_prefix_prefix_mask_level, CMD, "level", 0, 0, INVALID, 0, "level");
    libcli_register_param(&config_node_node_name_add_prefix_prefix_mask, &config_node_node_name_add_prefix_prefix_mask_level);

    static param_t config_node_node_name_add_prefix_prefix_mask_level_level;
    init_param(&config_node_node_name_add_prefix_prefix_mask_level_level, LEAF, 0, 
                    instance_node_config_handler, validate_level_no, INT, "level-no", "level : 1 | 2");
    libcli_register_param(&config_node_node_name_add_prefix_prefix_mask_level,
                    &config_node_node_name_add_prefix_prefix_mask_level_level);
    set_param_cmd_code(&config_node_node_name_add_prefix_prefix_mask_level_level, CMDCODE_CONFIG_NODE_EXPORT_PREFIX);
    
    /* config node <node-name> [no] attachbit enable*/    
    static param_t config_node_node_name_attachbit;
    init_param(&config_node_node_name_attachbit, CMD, "attachbit", 0, 0, INVALID, 0, "Set / Unset Attach bit");
    libcli_register_param(&config_node_node_name, &config_node_node_name_attachbit);

    static param_t config_node_node_name_attachbit_enable;
    init_param(&config_node_node_name_attachbit_enable, CMD, "enable", instance_node_config_handler, 0, INVALID, 0, "enable");
    libcli_register_param(&config_node_node_name_attachbit, &config_node_node_name_attachbit_enable);
    set_param_cmd_code(&config_node_node_name_attachbit_enable, CMDCODE_CONFIG_INSTANCE_ATTACHBIT_ENABLE);

    /*config node <node name> static-route 10.1.1.1 24 20.1.1.1 S eth0/1*/

    static param_t config_node_node_name_static_route;
    init_param(&config_node_node_name_static_route, CMD, "static-route", 0, 0, INVALID, 0, "configure static route");
    libcli_register_param(&config_node_node_name, &config_node_node_name_static_route);

    static param_t config_node_node_name_static_route_dest;
    init_param(&config_node_node_name_static_route_dest, LEAF, 0, 0, 0, IPV4, "destination", "Destination subnet (ipv4)");
    libcli_register_param(&config_node_node_name_static_route, &config_node_node_name_static_route_dest);

    static param_t config_node_node_name_static_route_dest_mask;
    init_param(&config_node_node_name_static_route_dest_mask, LEAF, 0, 0, validate_ipv4_mask, INT, "mask", "mask (0-32)");
    libcli_register_param(&config_node_node_name_static_route_dest, &config_node_node_name_static_route_dest_mask);

    static param_t config_node_node_name_static_route_dest_mask_nhip;
    init_param(&config_node_node_name_static_route_dest_mask_nhip, LEAF, 0, 0, 0, IPV4, "gateway", "Gateway Address(ipv4)");
    libcli_register_param(&config_node_node_name_static_route_dest_mask, &config_node_node_name_static_route_dest_mask_nhip);

    static param_t config_node_node_name_static_route_dest_mask_nhip_nhname;
    init_param(&config_node_node_name_static_route_dest_mask_nhip_nhname, LEAF, 0, 0, validate_node_extistence, STRING, "nh-name", "Next Hop Node Name");  
    libcli_register_param(&config_node_node_name_static_route_dest_mask_nhip, &config_node_node_name_static_route_dest_mask_nhip_nhname);

    static param_t config_node_node_name_static_route_dest_mask_nhip_nhname_slotno;
    init_param(&config_node_node_name_static_route_dest_mask_nhip_nhname_slotno, LEAF, 0, config_static_route_handler, 0, STRING, "slot-no", "interface name ethx/y format");
    libcli_register_param(&config_node_node_name_static_route_dest_mask_nhip_nhname, &config_node_node_name_static_route_dest_mask_nhip_nhname_slotno); 

    /*config node <node-name> lsp <lsp-name> metric <metric-value> to <tail-end-ip> level <level-no>*/
    {
        static param_t lsp;
        init_param(&lsp, CMD, "lsp", 0, 0, INVALID, 0, "RSVP Label Switch Path");
        libcli_register_param(&config_node_node_name, &lsp);
        
        static param_t lsp_name;
        init_param(&lsp_name, LEAF, 0, 0, 0, STRING, "lsp-name", "LSP name");
        libcli_register_param(&lsp, &lsp_name);
       
        {
            static param_t backup;
            init_param(&backup, CMD, "backup", 0, 0, INVALID, 0, "backup");
            libcli_register_param(&lsp_name, &backup);

            static param_t to;
            init_param(&to, CMD, "to", 0, 0, INVALID, 0, "LSP tail-end");
            libcli_register_param(&backup, &to);

            static param_t tail_end_ip;
            init_param(&tail_end_ip, LEAF, 0, lfa_rlfa_config_handler, 0, IPV4, "tail-end-ip", "LSP tail end router IP");
            libcli_register_param(&to, &tail_end_ip);
            set_param_cmd_code(&tail_end_ip, CMDCODE_CONFIG_RSVPLSP_AS_BACKUP);
        }
        
        static param_t metric;
        init_param(&metric, CMD, "metric", 0, 0, INVALID, 0, "metric");
        libcli_register_param(&lsp_name, &metric);

        static param_t metric_val;
        init_param(&metric_val, LEAF, 0, 0, 0, INT, "metric-val", "metric value");
        libcli_register_param(&metric, &metric_val);
        
        static param_t to;
        init_param(&to, CMD, "to", 0, 0, INVALID, 0, "LSP tail-end");
        libcli_register_param(&metric_val, &to);

        static param_t tail_end_ip;
        init_param(&tail_end_ip, LEAF, 0, 0, 0, IPV4, "tail-end-ip", "LSP tail end router IP");
        libcli_register_param(&to, &tail_end_ip);

        static param_t level;
        init_param(&level, CMD, "level", 0, 0, INVALID, 0, "level");
        libcli_register_param(&tail_end_ip, &level);

        static param_t level_no;
        init_param(&level_no, LEAF, 0, instance_node_config_handler, validate_level_no, INT, "level-no", "level : 1 | 2");
        libcli_register_param(&level, &level_no);
        
        set_param_cmd_code(&level_no, CMDCODE_CONFIG_NODE_RSVPLSP);
    }

    /*config node <node-name> leak prefix <prefix> mask <mask> from level <level-no> to <level-no>*/
    {
        static param_t leak;
        init_param(&leak, CMD, "leak", 0, 0, INVALID, 0, "'leak' the prefix");
        libcli_register_param(&config_node_node_name, &leak);

        static param_t prefix;
        init_param(&prefix, CMD, "prefix", 0, 0, INVALID, 0, "prefix");
        libcli_register_param(&leak, &prefix);

        static param_t ipv4;
        init_param(&ipv4, LEAF, 0, 0, 0, IPV4, "prefix", "Ipv4 prefix without mask");
        libcli_register_param(&prefix, &ipv4);

        static param_t mask;
        init_param(&mask, LEAF, 0, 0, validate_ipv4_mask, INT, "mask", "mask (0-32)");
        libcli_register_param(&ipv4, &mask);

        static param_t level;
        init_param(&level, CMD, "level", 0, 0, INVALID, 0, "level");
        libcli_register_param(&mask, &level);

        static param_t from_level_no;
        init_param(&from_level_no, LEAF, 0, 0, validate_level_no, INT, "from-level-no", "From level : 1 | 2");
        libcli_register_param(&level, &from_level_no);

        static param_t to_level_no;
        init_param(&to_level_no, LEAF, 0, instance_node_config_handler, validate_level_no, INT, "to-level-no", "To level : 1 | 2");
        libcli_register_param(&from_level_no, &to_level_no);
        set_param_cmd_code(&to_level_no, CMDCODE_CONFIG_NODE_LEAK_PREFIX);
    }

    /*Debug commands*/

    /*debug log*/
    static param_t debug_log;
    init_param(&debug_log, CMD, "log", 0, 0, INVALID, 0, "logging"); 
    libcli_register_param(debug, &debug_log);

    /*debug log enable*/
    static param_t debug_log_enable_disable;
    init_param(&debug_log_enable_disable, LEAF, 0, debug_log_enable_disable_handler, validate_debug_log_enable_disable, STRING, "log-status", "enable | disable"); 
    libcli_register_param(&debug_log, &debug_log_enable_disable);
    set_param_cmd_code(&debug_log_enable_disable, CMDCODE_DEBUG_LOG_ENABLE_DISABLE);

    /*debug log enable file*/
    {
        static param_t debug_file;
        init_param(&debug_file, CMD, "file", debug_log_enable_disable_handler, 0, INVALID, 0, "Enable|Disable file logging");
        libcli_register_param(&debug_log_enable_disable, &debug_file);
        set_param_cmd_code(&debug_file, CMDCODE_DEBUG_LOG_FILE_ENABLE_DISABLE); 
    }

    /* debug instance node <node-name> route-tree*/
    {
        static param_t instance;
        init_param(&instance, CMD, "instance", 0, 0, INVALID, 0, "Network graph");
        libcli_register_param(debug, &instance);

        static param_t instance_node;
        init_param(&instance_node, CMD, "node", 0, 0, INVALID, 0, "node");
        libcli_register_param(&instance, &instance_node);
        libcli_register_display_callback(&instance_node, display_instance_nodes);

        static param_t instance_node_name;
        init_param(&instance_node_name, LEAF, 0, 0, validate_node_extistence, STRING, "node-name", "Node Name");
        libcli_register_param(&instance_node, &instance_node_name);

        static param_t route;
        init_param(&route, CMD, "route", show_route_tree_handler, 0, INVALID, 0,  "route on a node");
        libcli_register_param(&instance_node_name, &route);
        set_param_cmd_code(&route, CMDCODE_DEBUG_INSTANCE_NODE_ALL_ROUTES);

        static param_t prefix;
        init_param(&prefix, LEAF, 0, 0, 0, IPV4, "prefix", "ipv4 prefix");
        libcli_register_param(&route, &prefix);

        static param_t mask;
        init_param(&mask, LEAF, 0, show_route_tree_handler, validate_ipv4_mask, INT, "mask", "mask (0-32)");
        libcli_register_param(&prefix, &mask);
        set_param_cmd_code(&mask, CMDCODE_DEBUG_INSTANCE_NODE_ROUTE);
    }

    /*debug show commands*/

    {
        /*debug show log-status*/
        {
            static param_t log_status;
            init_param(&log_status, CMD, "log-status", display_logging_status, 0, INVALID, 0, "log-status"); 
            libcli_register_param(debug_show, &log_status);
        }

        /*debug show instance*/
        static param_t instance;
        init_param(&instance, CMD, "instance", 0, 0, INVALID, 0, "Network graph");
        libcli_register_param(debug_show, &instance);

        {
            /*debug show instance node*/
            static param_t instance_node;
            init_param(&instance_node, CMD, "node", 0, 0, INVALID, 0, "node");
            libcli_register_param(&instance, &instance_node);
            libcli_register_display_callback(&instance_node, display_instance_nodes);
            {
                /*debug show instance node <node-name>*/
                static param_t instance_node_name;
                init_param(&instance_node_name, LEAF, 0, show_instance_node_handler, 
                    validate_node_extistence, STRING, "node-name", "Node Name");
                libcli_register_param(&instance_node, &instance_node_name);
                {
                    /*debug show instance node <node-name> interface*/
                    static param_t interface;
                    init_param(&interface, CMD, "interface", 0, 0, INVALID, 0, "interface");
                    libcli_register_param(&instance_node_name, &interface);
                    libcli_register_display_callback(&interface, display_instance_node_interfaces);
                    {
                        /*debug show instance node <node-name> interface <slot-no>*/
                        static param_t intf_name;
                        init_param(&intf_name, LEAF, 0, 0, 0, STRING, "slot-no", "interface name ethx/y format");
                        libcli_register_param(&interface, &intf_name);
                        /* debug show instance node <node-name> interface <slot-no> impacted*/
                        {
                            static param_t impacted;
                            init_param(&impacted, CMD, "impacted", 0, 0, INVALID, 0, "impacted");
                            libcli_register_param(&intf_name, &impacted);
                            /* debug show instance node <node-name> interface <slot-no> impacted destinations*/
                            {
                                static param_t destinations;
                                init_param(&destinations, CMD, "destinations", debug_show_node_impacted_destinations, 0, INVALID, 0, "Destinations");
                                libcli_register_param(&impacted, &destinations);
                                set_param_cmd_code(&destinations, CMDCODE_DEBUG_SHOW_IMPACTED_DESTINATIONS);     
                            }
                        }
                        {
                            /*debug show instance node <node-name> interface <slot-no> pqspace*/
                            static param_t pqspace;
                            init_param(&pqspace, CMD, "pqspace", show_instance_node_spaces, 0, INVALID, 0, "pqspace of a protected link");
                            libcli_register_param(&intf_name, &pqspace);
                            set_param_cmd_code(&pqspace, CMDCODE_DEBUG_SHOW_NODE_INTF_PQSPACE);
                        }
                        {
                            /*debug show instance node <node-name> interface <slot-no> extended-pspace*/
                            static param_t expspace;
                            init_param(&expspace, CMD, "extended-pspace", show_instance_node_spaces, 0, INVALID, 0, "extended pspace of a protected link");
                            libcli_register_param(&intf_name, &expspace);
                            set_param_cmd_code(&expspace, CMDCODE_DEBUG_SHOW_NODE_INTF_EXPSPACE);
                        }
                    }
                }
            }           
        }
    }

    /* Added Negation support to appropriate command, post this
     * do not extend any negation supported commands*/

    support_cmd_negation(&config_node_node_name);
    support_cmd_negation(config);
}

/*All show/dump functions*/

void
dump_nbrs(node_t *node, LEVEL level){

    node_t *nbr_node = NULL;
    edge_t *edge = NULL;
    printf("Node : %s(0x%x)(%s) (%s : %s)\n", node->node_name, (unsigned int)node, node->router_id, get_str_level(level), 
                (node->node_type[level] == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE");
    printf("Overloaded ? %s\n", IS_BIT_SET(node->attributes[level], OVERLOAD_BIT) ? "Yes" : "No");

    ITERATE_NODE_LOGICAL_NBRS_BEGIN(node, nbr_node, edge, level){
        printf("    Neighbor : %s, Area = %s\n", nbr_node->node_name, get_str_node_area(nbr_node->area));
        printf("    egress intf = %s(%s/%d), peer_intf  = %s(%s/%d)\n",
                    edge->from.intf_name, STR_PREFIX(edge->from.prefix[level]), 
                    PREFIX_MASK(edge->from.prefix[level]), edge->to.intf_name, 
                    STR_PREFIX(edge->to.prefix[level]), PREFIX_MASK(edge->to.prefix[level]));

        printf("    %s metric = %u, edge level = %s\n\n", get_str_level(level),
            edge->metric[level], get_str_level(edge->level));
    }
    ITERATE_NODE_LOGICAL_NBRS_END;
}

void
dump_edge_info(edge_t *edge){


}

void
dump_node_info(node_t *node){

    unsigned int i = 0, count = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;
    LEVEL level = LEVEL2;
    singly_ll_node_t *list_node = NULL;
    prefix_t *prefix = NULL;

    printf("node->node_name : %s(%s), L1 PN STATUS = %s, L2 PN STATUS = %s, Area = %s\n",
            node->node_name, node->router_id,
            (node->node_type[LEVEL1] == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE", 
            (node->node_type[LEVEL2] == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE",
            get_str_node_area(node->area));
    printf("backup-spf-options : %s\n", IS_BIT_SET(node->backup_spf_options, SPF_BACKUP_OPTIONS_ENABLED) ? \
        "ENABLED" : "DISABLED");
    if(IS_BIT_SET(node->backup_spf_options, SPF_BACKUP_OPTIONS_ENABLED)){
        printf("\tremote-backup-calculation : %s\n", IS_BIT_SET(node->backup_spf_options, SPF_BACKUP_OPTIONS_REMOTE_BACKUP_CALCULATION) ? \
            "ENABLED" : "DISABLED");
        printf("\tnode-link-degradation : %s\n", IS_BIT_SET(node->backup_spf_options, SPF_BACKUP_OPTIONS_NODE_LINK_DEG) ? \
            "ENABLED" : "DISABLED");
    }
    printf("SPRING : %s\n", node->spring_enabled ? "ENABLED" : "DISABLED");    

    printf("Slots :\n");
    for(; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        if(!edge_end)
            break;

        if(edge_end->dirn == INCOMING)
            continue;

        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        printf("    slot%u : %s, L1 prefix : %s/%d, L2 prefix : %s/%d, DIRN: %s, backup protection type : %s", 
            i, edge_end->intf_name, STR_PREFIX(edge_end->prefix[LEVEL1]), PREFIX_MASK(edge_end->prefix[LEVEL1]), 
            STR_PREFIX(edge_end->prefix[LEVEL2]), PREFIX_MASK(edge_end->prefix[LEVEL2]), 
            (edge_end->dirn == OUTGOING) ? "OUTGOING" : "INCOMING",
            IS_BIT_SET(edge_end->edge_config_flags, NO_ELIGIBLE_BACK_UP) ? "NO_ELIGIBLE_BACK_UP" : 
            IS_LINK_NODE_PROTECTION_ENABLED(edge) ? "LINK_NODE_PROTECTION" : IS_LINK_PROTECTION_ENABLED(edge) ? "LINK_PROTECTION" : "None");

        printf("\n          L1 metric = %u, L2 metric = %u, edge level = %s, edge_status = %s\n", 
        edge->metric[LEVEL1], edge->metric[LEVEL2], get_str_level(edge->level), edge->status ? "UP" : "DOWN");
        printf("\n");
    }

    printf("\n");
    for(level = LEVEL2; level >= LEVEL1; level--){

        printf("%s prefixes:\n", get_str_level(level));
        ITERATE_LIST_BEGIN(GET_NODE_PREFIX_LIST(node, level), list_node){
            count++;
            prefix = (prefix_t *)list_node->data;        
            printf("%s/%u%s(%s)     ", prefix->prefix, prefix->mask, IS_BIT_SET(prefix->prefix_flags, PREFIX_DOWNBIT_FLAG) ? "*": "", prefix->hosting_node->node_name);
            if(count % 5 == 0) printf("\n");
        }ITERATE_LIST_END;
        printf("\n"); 
    }

    printf("FLAGS : \n");
    printf("    IGNOREATTACHED : %s\n", IS_BIT_SET(node->instance_flags, IGNOREATTACHED) ? "SET" : "UNSET");
    printf("    ATTACHED       : %s\n", (node->attached) ? "SET" : "UNSET");
    printf("    MULTIAREA      : %s\n", node->spf_info.spff_multi_area ? "SET" : "UNSET");
    printf("    Overload       : L1 : %s, L2 : %s\n", IS_OVERLOADED(node, LEVEL1) ? "yes" : "No", 
                                                      IS_OVERLOADED(node, LEVEL2) ? "yes" : "No");
    printf("    Stub network interfaces : \n");
    for(i = 0; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        
        if(!edge_end) break;
        if(edge_end->dirn != OUTGOING) continue;

        for(level = LEVEL1; level <= LEVEL2; level++){
            if(edge_end->prefix[level] && edge_end->prefix[level]->metric == INFINITE_METRIC)
                printf("%s(%s)\n", edge_end->intf_name, get_str_level(level));
        }
    }
}
