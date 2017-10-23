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
#include "logging.h"
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
    unsigned int i = 0;

    printf("\nSPF run results for LEVEL%u, ROOT = %s\n", level, spf_root->node_name);

    ITERATE_LIST(spf_root->spf_run_result[level], list_node){
        res = (spf_result_t *)list_node->data;
        printf("DEST : %-10s spf_metric : %-6u", res->node->node_name, res->spf_metric);
        printf(" Nxt Hop : ");
        for( i = 0; i < MAX_NXT_HOPS; i++){
            if(res->next_hop[i] == NULL)
                break;
        
            printf("%-10s       OIF : %-7s\n", res->next_hop[i]->node_name, 
                    (get_min_oif(spf_root, res->next_hop[i], level, NULL))->intf_name);

        }
    }
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
node_slot_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    tlv_struct_t *tlv = NULL;
    unsigned int i = 0;
    char *slot_name = NULL;
    char *node_name = NULL; 
    node_t *node = NULL;
    int cmd_code = -1;
      
    TLV_LOOP(tlv_buf, tlv, i){
        if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
            slot_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
    }

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    cmd_code = EXTRACT_CMD_CODE(tlv_buf);

    switch(cmd_code){
        case CMDCODE_NODE_SLOT_ENABLE:
            spf_node_slot_enable_disable(node, slot_name, enable_or_disable);
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
    unsigned int i = 0;
    tlv_struct_t *tlv = NULL;
    node_t *node = NULL;

    TLV_LOOP(tlv_buf, tlv, i){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
             node_name = tlv->value;
    }

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    show_routing_table(node->spf_info.rttable);
    return 0;
}

static int
show_traceroute_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    char *node_name = NULL;
    char *prefix = NULL;
    unsigned int i = 0;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP(tlv_buf, tlv, i){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
             node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "prefix", strlen("prefix")) ==0)
            prefix = tlv->value;
        else
            assert(0);
    }

    show_traceroute(node_name, prefix);
    return 0; 
}

static int
show_instance_node_spaces(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    char *node_name = NULL,
         *slot_name = NULL;
    LEVEL level;
    unsigned int i = 0;
    tlv_struct_t *tlv = NULL;
    node_t *node = NULL, *p_node = NULL,
            *q_node = NULL, *pq_node = NULL;

    edge_end_t *edge_end = NULL;
    singly_ll_node_t *list_node = NULL;
    int cmdcode = -1;

    TLV_LOOP(tlv_buf, tlv, i){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "level-no", strlen("level-no")) ==0)
            level = atoi(tlv->value);
        else if(strncmp(tlv->leaf_id, "slot-no", strlen("slot-no")) ==0)
             slot_name = tlv->value;
    }

   cmdcode = EXTRACT_CMD_CODE(tlv_buf); 
   node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

   switch(cmdcode){
       case CMDCODE_SHOW_INSTANCE_NODE_PSPACE:
       case CMDCODE_SHOW_INSTANCE_NODE_EXPSPACE:
           {
               p_space_set_t p_space = NULL;
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
                   if(cmdcode == CMDCODE_SHOW_INSTANCE_NODE_PSPACE)
                       p_space = compute_p_space(node, edge, level);
                   else
                       p_space = compute_extended_p_space(node, edge, level);

                   if(cmdcode == CMDCODE_SHOW_INSTANCE_NODE_PSPACE)
                       printf("Node %s p-space : ", node->node_name);
                   else
                        printf("Node %s Extended p-space : ", node->node_name);

                   ITERATE_LIST(p_space, list_node){

                       p_node = (node_t *) list_node->data;
                       printf("%s ", p_node->node_name);   
                   }

                   /*free p-space*/
                   delete_singly_ll(p_space);
                   free(p_space);
                   p_space = NULL;
                   return 0;
               }
           }
           break;
       case CMDCODE_SHOW_INSTANCE_NODE_QSPACE:
           {

               for(i = 0; i < MAX_NODE_INTF_SLOTS; i++ ) {
                   edge_end = node->edges[i];
                   if(edge_end == NULL){
                       printf("Error : slot-no %s do not exist\n", slot_name);
                       return 0;
                   }

                   if(strncmp(slot_name, edge_end->intf_name, strlen(edge_end->intf_name)))
                       continue;

                   if(edge_end->dirn != INCOMING)
                       continue;

                   edge_t *edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
                   q_space_set_t q_space = compute_q_space(node, edge, level);
                   printf("Node %s q-space : ", node->node_name);
                   ITERATE_LIST(q_space, list_node){

                       q_node = (node_t *) list_node->data;
                       printf("%s ", q_node->node_name);   
                   }

                   /*free q-space*/
                   delete_singly_ll(q_space);
                   free(q_space);
                   q_space = NULL;
                   return 0;
               }
           }
           break;
       case CMDCODE_SHOW_INSTANCE_NODE_PQSPACE:
           {
               /*compute extended p-space first*/

               p_space_set_t ex_p_space = NULL;
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
                   ex_p_space = compute_extended_p_space(node, edge, level);
                   break;
               }

               /*Compute Q space now*/
               q_space_set_t q_space = compute_q_space(edge->to.node, edge, level);


               /*now merge extended p-space and q-space*/

               pq_space_set_t pq_space = Intersect_Extended_P_and_Q_Space(ex_p_space, q_space);
               ex_p_space = NULL;
               q_space = NULL;

               printf("Node %s pq-space : ", node->node_name);
               ITERATE_LIST(pq_space, list_node){

                   pq_node = (node_t *) list_node->data;
                   printf("%s ", pq_node->node_name);   
               }

               delete_singly_ll(pq_space);
               free(pq_space);
               pq_space = NULL;
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
    unsigned int i = 0;
    char *node_name = NULL;
    LEVEL level         = MAX_LEVEL,
          from_level_no = MAX_LEVEL,
          to_level_no   = MAX_LEVEL;
    char *prefix = NULL;
    
    dist_info_hdr_t dist_info_hdr;
    memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
    cmd_code = EXTRACT_CMD_CODE(tlv_buf);
    
    TLV_LOOP(tlv_buf, tlv, i){
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
        else
            assert(0);
    }

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
    
    switch(cmd_code){
        case CMDCODE_CONFIG_NODE_OVERLOAD:
            (enable_or_disable == CONFIG_ENABLE) ? SET_BIT(node->attributes[level], OVERLOAD_BIT) :
                UNSET_BIT(node->attributes[level], OVERLOAD_BIT);
                /*Now distribute Overload TLV in the network at this level*/
            break;
        case CMDCODE_INSTANCE_IGNOREBIT_ENABLE:
            (enable_or_disable == CONFIG_ENABLE) ? SET_BIT(node->instance_flags, IGNOREATTACHED) :
                UNSET_BIT(node->instance_flags, IGNOREATTACHED);
            break;
        case CMDCODE_INSTANCE_ATTACHBIT_ENABLE:
            node->attached = (enable_or_disable == CONFIG_ENABLE) ? 1 : 0;
            break;
        case CMDCODE_NODE_ADD_PREFIX:
        {
            tlv128_ip_reach_t ad_msg;
            memset(&ad_msg, 0, sizeof(tlv128_ip_reach_t));
            ad_msg.prefix = prefix,
            ad_msg.mask = mask;
            ad_msg.metric = 0;
            ad_msg.prefix_level = level;
            ad_msg.up_down_bit = 0;
            ad_msg.hosting_node = node;

            dist_info_hdr.lsp_generator = node;
            dist_info_hdr.info_dist_level = level;
            dist_info_hdr.add_or_remove = (enable_or_disable == CONFIG_ENABLE) ? AD_CONFIG_ADDED : AD_CONFIG_REMOVED;
            dist_info_hdr.advert_id = TLV128;
            dist_info_hdr.info_data = (char *)&ad_msg;
            generate_lsp(instance, node, prefix_distribution_routine, &dist_info_hdr);
        }
            break;     
        case CMDCODE_NODE_LEAK_PREFIX:
        {
            int prefix_metric = 0;
            if((prefix_metric = leak_prefix(node_name, prefix, mask, from_level_no, to_level_no)) < 0)
                break;
            tlv128_ip_reach_t ad_msg;
            memset(&ad_msg, 0, sizeof(tlv128_ip_reach_t));
            ad_msg.prefix = prefix,
            ad_msg.mask = mask;
            ad_msg.metric = prefix_metric;
            ad_msg.prefix_level = to_level_no;
            ad_msg.up_down_bit = 1;
            ad_msg.hosting_node = node;

            dist_info_hdr.lsp_generator = node;
            dist_info_hdr.info_dist_level = to_level_no;
            dist_info_hdr.add_or_remove = (enable_or_disable == CONFIG_ENABLE) ? AD_CONFIG_ADDED : AD_CONFIG_REMOVED;
            dist_info_hdr.advert_id = PREFIX_LEAK_ADVERT;
            dist_info_hdr.info_data = (char *)&ad_msg;
            generate_lsp(instance, node, prefix_distribution_routine, &dist_info_hdr);
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
    unsigned int i = 0;
    char *nh_name = NULL, 
         *host_node_name = NULL,
         *dest_ip = NULL,
         *gw_ip   = NULL,
         *intf_name = NULL;

    int mask = 0;
    rttable_entry_t *rt_entry = NULL;

    TLV_LOOP(tlv_buf, tlv, i){

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
    }

    host_node = singly_ll_search_by_key(instance->instance_node_list, host_node_name);

    switch(enable_or_disable){
        case CONFIG_ENABLE:
            
            rt_entry = calloc(1, sizeof(rttable_entry_t));

            strncpy(rt_entry->dest.prefix, dest_ip, 15);
            rt_entry->dest.prefix[15] = '\0';
            rt_entry->dest.mask = mask;
            rt_entry->version = 0;
            rt_entry->cost = 0;
            rt_entry->primary_nh_count = 1;
            rt_entry->primary_nh[0].nh_type = IPNH;
            strncpy(rt_entry->primary_nh[0].oif, intf_name, IF_NAME_SIZE);
            rt_entry->primary_nh[0].oif[IF_NAME_SIZE] = '\0'; 
            strncpy(rt_entry->primary_nh[0].nh_name, nh_name, NODE_NAME_SIZE);
            rt_entry->primary_nh[0].nh_name[NODE_NAME_SIZE] = '\0';
            strncpy(rt_entry->primary_nh[0].gwip, gw_ip, 15);
            rt_entry->primary_nh[0].gwip[15] = '\0';

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
debug_log_enable_disable_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    
    tlv_struct_t *tlv = NULL;
    unsigned int i = 0;
    char *value = NULL;

    TLV_LOOP(tlv_buf, tlv, i){
        value = tlv->value;        
    }

    if(strncmp(value, "enable", strlen(value)) ==0){
        enable_logging();
    }
    else if(strncmp(value, "disable", strlen(value)) ==0){
        disable_logging();
    }
    else
        assert(0);

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
    unsigned int i = 0;
    LEVEL level = LEVEL1 | LEVEL2;;
    char *node_name = NULL;
    node_t *spf_root = NULL;
    int CMDCODE = -1;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);
     
    TLV_LOOP(tlv_buf, tlv, i){
        if(strncmp(tlv->leaf_id, "level-no", strlen("level-no")) ==0){
            level = atoi(tlv->value);
        }
        else if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0){
            node_name = tlv->value;
        }
    }

    if(node_name == NULL)
        spf_root = instance->instance_root;
    else
        spf_root = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
   
   
    switch(CMDCODE){
        case CMDCODE_SHOW_SPF_RUN:
            spf_computation(spf_root, &spf_root->spf_info, level, FULL_RUN);
            //show_spf_results(spf_root, level);
            break;
        case CMDCODE_SHOW_SPF_STATS:
            show_spf_run_stats(spf_root, level);
            break;
        case CMDCODE_SHOW_SPF_RUN_INVERSE:
            inverse_topology(instance, level);
            spf_computation(spf_root, &spf_root->spf_info, level, SKELETON_RUN);
            inverse_topology(instance, level);
            show_spf_results(spf_root, level);
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
    unsigned int i = 0;
    LEVEL level = LEVEL_UNKNOWN;
    char *node_name = NULL;
    int CMDCODE = -1;
    node_t *node = NULL;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf); 

    TLV_LOOP(tlv_buf, tlv, i){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "level-no", strlen("level-no")) == 0)
            level = atoi(tlv->value);   
        else
            assert(0); 
    }
    
    switch(CMDCODE){
        
        case CMDCODE_SHOW_INSTANCE_LEVEL:
            printf("Graph root : %s\n", instance->instance_root->node_name);
            ITERATE_LIST(instance->instance_node_list, list_node){
                dump_nbrs(list_node->data, level);
            }
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
    unsigned int i = 0;
    char *node_name = NULL;
    node_t *node = NULL;

    TLV_LOOP(tlv_buf, tlv, i){
        node_name = tlv->value;
    }

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

    /*show instance node <node-name> traceroute <prefix>*/

    static param_t traceroute;
    init_param(&traceroute, CMD, "traceroute", 0, 0, INVALID, 0, "trace route");
    libcli_register_param(&instance_node_name, &traceroute);

    static param_t traceroute_prefix;
    init_param(&traceroute_prefix, LEAF, 0, show_traceroute_handler, 0, IPV4, "prefix", "Destination address (ipv4)");
    libcli_register_param(&traceroute, &traceroute_prefix);

    /*show instance node <node-name> route*/
    static param_t route;
    init_param(&route, CMD, "route", show_route_handler, 0, INVALID, 0, "routing table");
    libcli_register_param(&instance_node_name, &route);

    /*show instance node <node-name> level <level-no>*/ 
    static param_t instance_node_name_level;
    init_param(&instance_node_name_level, CMD, "level", 0, 0, INVALID, 0, "level");
    libcli_register_param(&instance_node_name, &instance_node_name_level);

    static param_t instance_node_name_level_level;
    init_param(&instance_node_name_level_level, LEAF, 0, show_instance_handler, validate_level_no, INT, "level-no", "level");
    libcli_register_param(&instance_node_name_level, &instance_node_name_level_level);
    set_param_cmd_code(&instance_node_name_level_level, CMDCODE_SHOW_INSTANCE_NODE_LEVEL);
    
    /*show instance node <node-name> level <level-no> pspace*/
    static param_t instance_node_name_level_level_pspace;
    init_param(&instance_node_name_level_level_pspace, CMD, "pspace", 0, 0, INVALID, 0, "pspace of a Node");
    libcli_register_param(&instance_node_name_level_level, &instance_node_name_level_level_pspace);
    libcli_register_display_callback(&instance_node_name_level_level_pspace, display_instance_node_interfaces);

    static param_t instance_node_name_level_level_pspace_intf;
    init_param(&instance_node_name_level_level_pspace_intf, LEAF, 0, show_instance_node_spaces, 0, STRING, "slot-no", "interface name ethx/y format");
    libcli_register_param(&instance_node_name_level_level_pspace, &instance_node_name_level_level_pspace_intf);
    set_param_cmd_code(&instance_node_name_level_level_pspace_intf, CMDCODE_SHOW_INSTANCE_NODE_PSPACE); 
     
    static param_t instance_node_name_level_level_qspace;
    init_param(&instance_node_name_level_level_qspace, CMD, "qspace", 0, 0, INVALID, 0, "qspace of a Node");
    libcli_register_param(&instance_node_name_level_level, &instance_node_name_level_level_qspace);
    libcli_register_display_callback(&instance_node_name_level_level_qspace, display_instance_node_interfaces);

    static param_t instance_node_name_level_level_qspace_intf;
    init_param(&instance_node_name_level_level_qspace_intf, LEAF, 0, show_instance_node_spaces, 0, STRING, "slot-no", "interface name ethx/y format");
    libcli_register_param(&instance_node_name_level_level_qspace, &instance_node_name_level_level_qspace_intf);
    set_param_cmd_code(&instance_node_name_level_level_qspace_intf, CMDCODE_SHOW_INSTANCE_NODE_QSPACE); 
    
    static param_t instance_node_name_level_level_pqspace;
    init_param(&instance_node_name_level_level_pqspace, CMD, "pqspace", 0, 0, INVALID, 0, "pqspace of a Node");
    libcli_register_param(&instance_node_name_level_level, &instance_node_name_level_level_pqspace);
    libcli_register_display_callback(&instance_node_name_level_level_pqspace, display_instance_node_interfaces);

    static param_t instance_node_name_level_level_pqspace_intf;
    init_param(&instance_node_name_level_level_pqspace_intf, LEAF, 0, show_instance_node_spaces, 0, STRING, "slot-no", "interface name ethx/y format");
    libcli_register_param(&instance_node_name_level_level_pqspace, &instance_node_name_level_level_pqspace_intf);
    set_param_cmd_code(&instance_node_name_level_level_pqspace_intf, CMDCODE_SHOW_INSTANCE_NODE_PQSPACE); 


    static param_t instance_node_name_level_level_expspace;
    init_param(&instance_node_name_level_level_expspace, CMD, "expspace", 0, 0, INVALID, 0, "extended pspace of a Node");
    libcli_register_param(&instance_node_name_level_level, &instance_node_name_level_level_expspace);

    static param_t instance_node_name_level_level_expspace_intf;
    init_param(&instance_node_name_level_level_expspace_intf, LEAF, 0, show_instance_node_spaces, 0, STRING, "slot-no", "interface name ethx/y format");
    libcli_register_param(&instance_node_name_level_level_expspace, &instance_node_name_level_level_expspace_intf);
    set_param_cmd_code(&instance_node_name_level_level_expspace_intf, CMDCODE_SHOW_INSTANCE_NODE_EXPSPACE); 

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

    /*config node <node-name> [no] slot <slot-name> enable*/
    static param_t config_node;
    init_param(&config_node, CMD, "node", 0, 0, INVALID, 0, "node");
    libcli_register_param(config, &config_node);
    libcli_register_display_callback(&config_node, display_instance_nodes); 

    static param_t config_node_node_name;
    init_param(&config_node_node_name, LEAF, 0, 0, validate_node_extistence, STRING, "node-name", "Node Name");
    libcli_register_param(&config_node, &config_node_node_name);

    static param_t config_node_node_name_slot;
    init_param(&config_node_node_name_slot, CMD, "slot", 0, 0, INVALID, 0, "slot");
    libcli_register_param(&config_node_node_name, &config_node_node_name_slot);
    libcli_register_display_callback(&config_node_node_name_slot, display_instance_node_interfaces);

    static param_t config_node_node_name_slot_slotname;
    init_param(&config_node_node_name_slot_slotname, LEAF, 0, 0, 0, STRING, "slot-no", "interface name ethx/y format");
    libcli_register_param(&config_node_node_name_slot, &config_node_node_name_slot_slotname);

    static param_t config_node_node_name_slot_slotname_enable;
    init_param(&config_node_node_name_slot_slotname_enable, CMD, "enable", node_slot_config_handler, 0, INVALID, 0, "enable");
    libcli_register_param(&config_node_node_name_slot_slotname, &config_node_node_name_slot_slotname_enable);
    set_param_cmd_code(&config_node_node_name_slot_slotname_enable, CMDCODE_NODE_SLOT_ENABLE);

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
    }

    /* config node <node-name> [no] ignorebit enable*/
    static param_t config_node_node_name_ignorebit;
    init_param(&config_node_node_name_ignorebit, CMD, "ignorebit", 0, 0, INVALID, 0, "ignore L1 LSPs from added router when set");
    libcli_register_param(&config_node_node_name, &config_node_node_name_ignorebit);

    static param_t config_node_node_name_ignorebit_enable;
    init_param(&config_node_node_name_ignorebit_enable, CMD, "enable", instance_node_config_handler, 0, INVALID, 0, "enable"); 
    libcli_register_param(&config_node_node_name_ignorebit, &config_node_node_name_ignorebit_enable);
    set_param_cmd_code(&config_node_node_name_ignorebit_enable, CMDCODE_INSTANCE_IGNOREBIT_ENABLE);

    /*config node <node name> add prefix <prefix> <mask> level <level no>*/
    static param_t config_node_node_name_add;
    init_param(&config_node_node_name_add, CMD, "add", 0, 0, INVALID, 0, "'add' the prefix");
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
    set_param_cmd_code(&config_node_node_name_add_prefix_prefix_mask_level_level, CMDCODE_NODE_ADD_PREFIX);
    
    /* config node <node-name> [no] attachbit enable*/    
    static param_t config_node_node_name_attachbit;
    init_param(&config_node_node_name_attachbit, CMD, "attachbit", 0, 0, INVALID, 0, "Set / Unset Attach bit");
    libcli_register_param(&config_node_node_name, &config_node_node_name_attachbit);

    static param_t config_node_node_name_attachbit_enable;
    init_param(&config_node_node_name_attachbit_enable, CMD, "enable", instance_node_config_handler, 0, INVALID, 0, "enable");
    libcli_register_param(&config_node_node_name_attachbit, &config_node_node_name_attachbit_enable);
    set_param_cmd_code(&config_node_node_name_attachbit_enable, CMDCODE_INSTANCE_ATTACHBIT_ENABLE);

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
        set_param_cmd_code(&to_level_no, CMDCODE_NODE_LEAK_PREFIX);
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
        init_param(&route, CMD, "route", show_route_tree_handler, 0, INVALID, "route", "route on a node");
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
    printf("Node : %s (%s : %s)\n", node->node_name, get_str_level(level), 
                (node->node_type[level] == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE");
    printf("Overloaded ? %s\n", IS_BIT_SET(node->attributes[level], OVERLOAD_BIT) ? "Yes" : "No");

    ITERATE_NODE_NBRS_BEGIN(node, nbr_node, edge, level){
        printf("    Neighbor : %s, Area = %s\n", nbr_node->node_name, get_str_node_area(nbr_node->area));
        printf("    egress intf = %s(%s/%d), peer_intf  = %s(%s/%d)\n",
                    edge->from.intf_name, STR_PREFIX(edge->from.prefix[level]), 
                    PREFIX_MASK(edge->from.prefix[level]), edge->to.intf_name, 
                    STR_PREFIX(edge->to.prefix[level]), PREFIX_MASK(edge->to.prefix[level]));

        printf("    %s metric = %u, edge level = %s\n\n", get_str_level(level),
            edge->metric[level], get_str_level(edge->level));
    }
    ITERATE_NODE_NBRS_END;
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

    printf("node->node_name : %s, L1 PN STATUS = %s, L2 PN STATUS = %s, Area = %s\n", node->node_name, 
            (node->node_type[LEVEL1] == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE", 
            (node->node_type[LEVEL2] == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE",
            get_str_node_area(node->area));

    printf("Slots :\n");

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        if(!edge_end)
            break;

        printf("    slot%u : %s, L1 prefix : %s/%d, L2 prefix : %s/%d, DIRN: %s, local edge-end connected node : %s", 
            i, edge_end->intf_name, STR_PREFIX(edge_end->prefix[LEVEL1]), PREFIX_MASK(edge_end->prefix[LEVEL1]), 
            STR_PREFIX(edge_end->prefix[LEVEL2]), PREFIX_MASK(edge_end->prefix[LEVEL2]), 
            (edge_end->dirn == OUTGOING) ? "OUTGOING" : "INCOMING", edge_end->node->node_name);

        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);

        printf(", L1 metric = %u, L2 metric = %u, edge level = %s, edge_status = %s\n", 
        edge->metric[LEVEL1], edge->metric[LEVEL2], get_str_level(edge->level), edge->status ? "UP" : "DOWN");
    }

    printf("\n");
    for(level = LEVEL2; level >= LEVEL1; level--){

        printf("%s prefixes:\n", get_str_level(level));
        ITERATE_LIST(GET_NODE_PREFIX_LIST(node, level), list_node){
            count++;
            prefix = (prefix_t *)list_node->data;        
            printf("%s/%u%s(%s)     ", prefix->prefix, prefix->mask, IS_BIT_SET(prefix->prefix_flags, PREFIX_DOWNBIT_FLAG) ? "*": "", prefix->hosting_node->node_name);
            if(count % 5 == 0) printf("\n");
        }
        printf("\n"); 
    }

    printf("FLAGS : \n");
    printf("    IGNOREATTACHED : %s\n", IS_BIT_SET(node->instance_flags, IGNOREATTACHED) ? "SET" : "UNSET");
    printf("    ATTACHED       : %s\n", (node->attached) ? "SET" : "UNSET");
    printf("    MULTIAREA      : %s\n", node->spf_info.spff_multi_area ? "SET" : "UNSET");
}
