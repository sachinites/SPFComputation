/*
 * =====================================================================================
 *
 *       Filename:  graph.c
 *
 *    Description:  The topology construction is done in this file
 *
 *        Version:  1.0
 *        Created:  Wednesday 23 August 2017 02:05:28  IST
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

#include "graph.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

node_t *
create_new_node(char *node_name){
    
    assert(node_name);
    node_t * node = calloc(1, sizeof(node_t));
    strncpy(node->node_name, node_name, NODE_NAME_SIZE);
    node->node_name[NODE_NAME_SIZE - 1] = '\0';
    return node;    
}

edge_t *
create_new_edge(char *from_ifname,
        char *to_ifname,
        unsigned int metric,
        char *from_prefix,
        char *to_prefix){

    assert(from_ifname);
    assert(to_ifname);

    edge_t *edge = calloc(1, sizeof(edge_t));

    strncpy(edge->from.intf_name, from_ifname, IF_NAME_SIZE);
    edge->from.intf_name[IF_NAME_SIZE - 1] = '\0';
    
    edge->metric = metric;

    strncpy(edge->to.intf_name, to_ifname, IF_NAME_SIZE);
    edge->to.intf_name[IF_NAME_SIZE - 1] = '\0';

    strncpy(edge->from.prefix, from_prefix, PREFIX_LEN_WITH_MASK + 1);
    edge->from.prefix[PREFIX_LEN_WITH_MASK] = '\0';

    strncpy(edge->to.prefix, to_prefix, PREFIX_LEN_WITH_MASK + 1);
    edge->to.prefix[PREFIX_LEN_WITH_MASK] = '\0';

    return edge;
}

void
insert_interface_into_node(node_t *node, edge_end_t *edge_end){
    
    unsigned int i = 0;
    for(; i < MAX_NODE_INTF_SLOTS; i++){
        if(node->edges[i])
            continue;

        if(i == MAX_NODE_INTF_SLOTS){
            printf("Error : No slots available in node %s\n", node->node_name);
            return;
        }

        node->edges[i] = edge_end;
        edge_end->node = node;
        break;
    }
}

void
insert_edge_between_2_nodes(edge_t *edge,
                            node_t *from_node,
                            node_t *to_node){

    insert_interface_into_node(from_node, &edge->from);
    insert_interface_into_node(to_node, &edge->to);
}

void
set_graph_root(graph_t *graph, node_t *root){
    graph->graph_root = root;
}

graph_t *
get_new_graph(){

    graph_t *graph = calloc(1, sizeof(graph_t));
    return graph;
}

