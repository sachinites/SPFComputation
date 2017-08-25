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

extern
graph_t *graph;

extern void
spf_computation();
/*All Command Handler Functions goes here */

static int
show_spf_run_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    spf_computation();
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
    

/*Debug commands*/
    

    support_cmd_negation(config);
}



/*All show/dump functions*/

void
dump_nbrs(node_t *node){

    node_t *nbr_node = NULL;
    edge_t *edge = NULL;
    printf("printing nbrs of node %s(%s)\n", node->node_name,
                (node->node_type == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE");

    ITERATE_NODE_NBRS_BEGIN(node, nbr_node, edge){
        printf("    nbr : %s\n", nbr_node->node_name);
        printf("    egress intf = %s(%s), peer_intf = %s(%s)\n",
                edge->from.intf_name, edge->from.prefix, edge->to.intf_name, edge->to.prefix);

        printf("    metric = %u\n", edge->metric);
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

    printf("node->node_name : %s, PN STATUS = %s\n", node->node_name, 
            (node->node_type == PSEUDONODE) ? "PSEUDONODE" : "NON_PSEUDONODE");

    printf("Slots :\n");

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        if(!edge_end)
            break;

        printf("    slot%u : %s, %s, %s, local edge-end connected node : %s\n", i, edge_end->intf_name, edge_end->prefix,
                (edge_end->dirn == OUTGOING) ? "OUTGOING" : "INCOMING", edge_end->node->node_name);

    }
}

