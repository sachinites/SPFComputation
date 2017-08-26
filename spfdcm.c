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
#include "graph.h"
#include "cmdtlv.h"
#include <stdio.h>
#include "spfutil.h"
#include "spfcomputation.h"

extern
graph_t *graph;

extern 
spf_stats_t spf_stats;

extern void
spf_computation();
/*All Command Handler Functions goes here */

static int
validate_level_no(char *value_passed){

    LEVEL level = atoi(value_passed);
    if(level == LEVEL1 || level == LEVEL2)
        return VALIDATION_SUCCESS;

    printf("Error : Incorrect Level Value.\n");
    return VALIDATION_FAILED;
}
static int
show_spf_run_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    tlv_struct_t *tlv = NULL;
    unsigned int i = 0;
    LEVEL level = LEVEL1 | LEVEL2;;

    TLV_LOOP(tlv_buf, tlv, i){
        level = atoi(tlv->value);
    }
    spf_computation(level);
    return 0;
}

static int
show_spf_stats_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
   
    printf("SPF Statistics:\n");
    printf("# LEVEL1 SPF runs : %u\n", spf_stats.spf_runs_count[LEVEL1]);
    printf("# LEVEL1 SPF runs : %u\n", spf_stats.spf_runs_count[LEVEL2]);
    return 0;
}

static int
show_graph_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    
    unsigned int i = 0;

    singly_ll_node_t *list_node = NULL;

    ITERATE_LIST(graph->graph_node_list, list_node){
         dump_nbrs(list_node->data);
    }
    return 0;
}

static int
show_graph_node_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    tlv_struct_t *tlv = NULL;
    unsigned int i = 0;
    char *node_name = NULL;
    singly_ll_node_t *list_node = NULL;
    node_t *node = NULL;

    /*We get only one TLV*/
    TLV_LOOP(tlv_buf, tlv, i){
        node_name = tlv->value;
    }

    ITERATE_LIST(graph->graph_node_list, list_node){
        
        node = (node_t *)list_node->data;
        if(strncmp(node_name, node->node_name, strlen(node->node_name)) == 0){
            dump_node_info(node); 
            return 0;       
        }
    }

    printf("INFO : %s Node do not exist\n", node_name);
    return 0;
}



void
spf_init_dcm(){
    
    init_libcli();

    param_t *show   = libcli_get_show_hook();
    param_t *debug  = libcli_get_debug_hook();
    param_t *config = libcli_get_config_hook();

/*Show commands*/

    /*show graph | show graph node name */
    static param_t graph;
    init_param(&graph, CMD, "graph", show_graph_handler, 0, INVALID, 0, "Network graph");
    libcli_register_param(show, &graph);

    static param_t graph_node;
    init_param(&graph_node, CMD, "node", 0, 0, INVALID, 0, "node");
    libcli_register_param(&graph, &graph_node);

    static param_t graph_node_name;
    init_param(&graph_node_name, LEAF, 0, show_graph_node_handler, 0, STRING, "node-name", "Node Name");
    libcli_register_param(&graph_node, &graph_node_name);

    /*show spf run*/

    static param_t show_spf;
    init_param(&show_spf, CMD, "spf", 0, 0, INVALID, 0, "Shortest Path Tree");
    libcli_register_param(show, &show_spf);

    static param_t show_spf_run;
    init_param(&show_spf_run, CMD, "run", show_spf_run_handler, 0, INVALID, 0, "run SPT computation");
    libcli_register_param(&show_spf, &show_spf_run);
   
    /*show spf run level */ 
    static param_t show_spf_run_level;
    init_param(&show_spf_run_level, CMD, "level", 0, 0, INVALID, 0, "level");
    libcli_register_param(&show_spf_run, &show_spf_run_level);
    
    /* show spf run level <Level NO>*/
    static param_t show_spf_run_level_N;
    init_param(&show_spf_run_level_N, LEAF, 0, show_spf_run_handler, validate_level_no, INT, "level-no", "level : 1 | 2");
    libcli_register_param(&show_spf_run_level, &show_spf_run_level_N);

    /* show spf statistics */

    static param_t show_spf_statistics;
    init_param(&show_spf_statistics, CMD, "statistics", show_spf_stats_handler, 0, INVALID, 0, "SPF Statistics");
    libcli_register_param(&show_spf, &show_spf_statistics);
/*Debug commands*/
    

    support_cmd_negation(config);
}

static char *
get_str_egde_level(LEVEL level){
    
    switch(level){
        case LEVEL1:
            return "LEVEL1";
        case LEVEL2:
            return "LEVEL2";
        default:
            return "LEVEL_UNKNOWN";
    }
}

static char*
get_str_node_area(AREA area){

    switch(area){
        case AREA1:
            return "AREA1";
        case AREA2:
            return "AREA2";
        case AREA3:
            return "AREA3";
        case AREA4:
            return "AREA4";
        case AREA5:
            return "AREA5";
        case AREA6:
            return "AREA6";
        default:
            return "AREA_UNKNOWN";
    }
}

/*All show/dump functions*/

void
dump_nbrs(node_t *node){

    node_t *nbr_node = NULL;
    edge_t *edge = NULL;
    printf("Node : %s(%s)\n", node->node_name,
                (node->node_type == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE");

    ITERATE_NODE_NBRS_BEGIN(node, nbr_node, edge, LEVEL1 | LEVEL2){
        printf("    Neighborr : %s, Area = %s\n", nbr_node->node_name, get_str_node_area(nbr_node->area));
        printf("    egress intf = %s(%s), peer_intf = %s(%s)\n",
                edge->from.intf_name, edge->from.prefix, edge->to.intf_name, edge->to.prefix);

        printf("    metric = %u, edge level = %s\n\n", edge->metric, get_str_egde_level(edge->level));
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

    printf("node->node_name : %s, PN STATUS = %s, Area = %s\n", node->node_name, 
            (node->node_type == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE", get_str_node_area(node->area));

    printf("Slots :\n");

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        if(!edge_end)
            break;

        printf("    slot%u : %s, %s, %s, local edge-end connected node : %s", i, edge_end->intf_name, edge_end->prefix,
                (edge_end->dirn == OUTGOING) ? "OUTGOING" : "INCOMING", edge_end->node->node_name);

        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        printf(", metric = %u, edge level = %s\n", edge->metric, get_str_egde_level(edge->level));
    }
}

