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


#include "libcli.h"
#include "instance.h"
#include "cmdtlv.h"
#include <stdio.h>
#include "spfutil.h"
#include "spfcomputation.h"
#include "logging.h"
#include "bitsop.h"
#include "spfcmdcodes.h"
#include "spfclihandler.h"

extern
instance_t *instance;

extern void
spf_computation(node_t *spf_root, spf_info_t *spf_info, LEVEL level);
/*All Command Handler Functions goes here */

static void
show_spf_results(node_t *spf_root, LEVEL level){
    
    singly_ll_node_t *list_node = NULL;
    spf_result_t *res = NULL;
    unsigned int i = 0;

    printf("\nSPF run results for LEVEL : %s, ROOT = %s\n", get_str_level(level), spf_root->node_name);

    ITERATE_LIST(spf_root->spf_run_result[level], list_node){
        res = (spf_result_t *)list_node->data;
        printf("DEST : %-10s spf_metric : %-6u", res->node->node_name, res->node->spf_metric[level]);
        printf(" Nxt Hop : ");
        for( i = 0; i < MAX_NXT_HOPS; i++){
            if(res->node->next_hop[level][i] == NULL)
                break;
        
            printf("%-10s\n", res->node->next_hop[level][i]->node_name);
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
            printf("%s() : Error : No Handler for command code : %d\n", __FUNCTION__, CMDCODE_NODE_SLOT_ENABLE);
            break;
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

static int
show_spf_run_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    tlv_struct_t *tlv = NULL;
    unsigned int i = 0;
    LEVEL level = LEVEL1 | LEVEL2;;
    char *node_name = NULL;
    node_t *spf_root = NULL;

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
   
    if(!spf_root){
        printf("%s() : Node %s not found\n", __FUNCTION__, node_name);   
        return 0; 
    }

    spf_computation(spf_root, &instance->spf_info, level);
    show_spf_results(spf_root, level);    

    return 0;
}

static int
show_spf_stats_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
   
    printf("SPF Statistics:\n");
    printf("# LEVEL1 SPF runs : %u\n", instance->spf_info.spf_level_info[LEVEL1].version);
    printf("# LEVEL2 SPF runs : %u\n", instance->spf_info.spf_level_info[LEVEL2].version);
    return 0;
}

static int
show_instance_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    
    singly_ll_node_t *list_node = NULL;
    tlv_struct_t *tlv = NULL;
    unsigned int i = 0;
    LEVEL level = LEVEL_UNKNOWN;

    TLV_LOOP(tlv_buf, tlv, i){
        level = atoi(tlv->value);    
    }
    
    printf("Graph root : %s\n", instance->instance_root->node_name);
    ITERATE_LIST(instance->instance_node_list, list_node){
         dump_nbrs(list_node->data, level);
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

    /*show instance node <node-name>*/
    static param_t instance_node;
    init_param(&instance_node, CMD, "node", 0, 0, INVALID, 0, "node");
    libcli_register_param(&instance, &instance_node);

    static param_t instance_node_name;
    init_param(&instance_node_name, LEAF, 0, show_instance_node_handler, validate_node_extistence, STRING, "node-name", "Node Name");
    libcli_register_param(&instance_node, &instance_node_name);

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

    static param_t show_spf_run_level_N_root;
    init_param(&show_spf_run_level_N_root, CMD, "root", 0, 0, INVALID, 0, "spf root");
    libcli_register_param(&show_spf_run_level_N, &show_spf_run_level_N_root);
    
    static param_t show_spf_run_level_N_root_root_name;
    init_param(&show_spf_run_level_N_root_root_name, LEAF, 0, show_spf_run_handler, validate_node_extistence, STRING, "node-name", "node name to be SPF root");
    libcli_register_param(&show_spf_run_level_N_root, &show_spf_run_level_N_root_root_name);
    /* show spf statistics */

    static param_t show_spf_statistics;
    init_param(&show_spf_statistics, CMD, "statistics", show_spf_stats_handler, 0, INVALID, 0, "SPF Statistics");
    libcli_register_param(&show_spf, &show_spf_statistics);

    /*config commands */

    /*config node <node-name> [no] slot <slot-name> enable*/
    static param_t config_node;
    init_param(&config_node, CMD, "node", 0, 0, INVALID, 0, "node");
    libcli_register_param(config, &config_node);

    static param_t config_node_node_name;
    init_param(&config_node_node_name, LEAF, 0, 0, validate_node_extistence, STRING, "node-name", "Node Name");
    libcli_register_param(&config_node, &config_node_node_name);

    static param_t config_node_node_name_slot;
    init_param(&config_node_node_name_slot, CMD, "slot", 0, 0, INVALID, 0, "slot");
    libcli_register_param(&config_node_node_name, &config_node_node_name_slot);

    static param_t config_node_node_name_slot_slotname;
    init_param(&config_node_node_name_slot_slotname, LEAF, 0, 0, 0, STRING, "slot-no", "interface name ethx/y format");
    libcli_register_param(&config_node_node_name_slot, &config_node_node_name_slot_slotname);

    static param_t config_node_node_name_slot_slotname_enable;
    init_param(&config_node_node_name_slot_slotname_enable, CMD, "enable", node_slot_config_handler, 0, INVALID, 0, "enable");
    libcli_register_param(&config_node_node_name_slot_slotname, &config_node_node_name_slot_slotname_enable);
    set_param_cmd_code(&config_node_node_name_slot_slotname_enable, CMDCODE_NODE_SLOT_ENABLE);


    /*Debug commands*/

    /*debug log*/
    static param_t debug_log;
    init_param(&debug_log, CMD, "log", 0, 0, INVALID, 0, "logging"); 
    libcli_register_param(debug, &debug_log);

    /*debug log enable*/
    static param_t debug_log_enable_disable;
    init_param(&debug_log_enable_disable, LEAF, 0, debug_log_enable_disable_handler, validate_debug_log_enable_disable, STRING, "log-status", "enable | disable"); 
    libcli_register_param(&debug_log, &debug_log_enable_disable);


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

    unsigned int i = 0;
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

        printf("    slot%u : %s, L1 prefix : %s/%d, L2 prefix : %s/%d, DIRN: %s, local edge-end connected node : %s", i, edge_end->intf_name, STR_PREFIX(edge_end->prefix[LEVEL1]), PREFIX_MASK(edge_end->prefix[LEVEL1]), 
                STR_PREFIX(edge_end->prefix[LEVEL2]), PREFIX_MASK(edge_end->prefix[LEVEL2]), (edge_end->dirn == OUTGOING) ? "OUTGOING" : "INCOMING", edge_end->node->node_name);

        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        printf(", L1 metric = %u, L2 metric = %u, edge level = %s, edge_status = %s\n", edge->metric[LEVEL1], edge->metric[LEVEL2], get_str_level(edge->level), edge->status ? "UP" : "DOWN");
    }

    unsigned int count = 0;
    printf("\n");
    for(level = LEVEL2; level >= LEVEL1; level--){

        printf("LEVEL : %u local prefixes:\n", level);
        ITERATE_LIST(GET_NODE_PREFIX_LIST(node, level), list_node){
            count++;
            prefix = (prefix_t *)list_node->data;        
            printf("%s/%u%s     ", prefix->prefix, prefix->mask, IS_BIT_SET(prefix->prefix_flags, PREFIX_DOWNBIT_FLAG) ? "*": "");
            if(count % 5 == 0) printf("\n");
        }
        printf("\n"); 
    }
}
    

