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
#include "spfutil.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

static void
add_node_to_owning_graph(graph_t *graph, node_t *node){
    singly_ll_add_node_by_val(graph->graph_node_list, (void *)node);
}


node_t *
create_new_node(graph_t *graph, char *node_name, AREA area){
    
    assert(node_name);
    node_t * node = calloc(1, sizeof(node_t));
    strncpy(node->node_name, node_name, NODE_NAME_SIZE);
    node->node_name[NODE_NAME_SIZE - 1] = '\0';
    node->area = area;
    node->node_type[LEVEL1] = NON_PSEUDONODE;
    node->node_type[LEVEL2] = NON_PSEUDONODE;
    node->pn_intf[LEVEL1] = NULL;
    node->pn_intf[LEVEL2] = NULL;
    add_node_to_owning_graph(graph, node);
    return node;    
}

edge_t *
create_new_edge(char *from_ifname,
        char *to_ifname,
        unsigned int metric,
        char *from_prefix,
        char *to_prefix,
        LEVEL level){

    assert(from_ifname);
    assert(to_ifname);

    edge_t *edge = calloc(1, sizeof(edge_t));

    strncpy(edge->from.intf_name, from_ifname, IF_NAME_SIZE);
    edge->from.intf_name[IF_NAME_SIZE - 1] = '\0';
   
    if(IS_LEVEL_SET(level, LEVEL1)) 
        edge->metric[LEVEL1] = metric;

    if(IS_LEVEL_SET(level, LEVEL2)) 
        edge->metric[LEVEL2] = metric;

    strncpy(edge->to.intf_name, to_ifname, IF_NAME_SIZE);
    edge->to.intf_name[IF_NAME_SIZE - 1] = '\0';

    if(IS_LEVEL_SET(level, LEVEL1)){
        strncpy(edge->from.prefix[LEVEL1], from_prefix, PREFIX_LEN_WITH_MASK + 1);
        edge->from.prefix[LEVEL1][PREFIX_LEN_WITH_MASK] = '\0';
        strncpy(edge->to.prefix[LEVEL1], to_prefix, PREFIX_LEN_WITH_MASK + 1);
        edge->to.prefix[LEVEL1][PREFIX_LEN_WITH_MASK] = '\0';
    }


    if(IS_LEVEL_SET(level, LEVEL2)){
        strncpy(edge->from.prefix[LEVEL2], from_prefix, PREFIX_LEN_WITH_MASK + 1);
        edge->from.prefix[LEVEL2][PREFIX_LEN_WITH_MASK] = '\0';
        strncpy(edge->to.prefix[LEVEL2], to_prefix, PREFIX_LEN_WITH_MASK + 1);
        edge->to.prefix[LEVEL2][PREFIX_LEN_WITH_MASK] = '\0';
    }


    edge->level = level;

    edge->from.dirn = EDGE_END_DIRN_UNKNOWN;
    edge->to.dirn   = EDGE_END_DIRN_UNKNOWN;

    return edge;
}

static void
insert_interface_into_node(node_t *node, edge_end_t *edge_end){
    
    unsigned int i = 0;
    for(; i < MAX_NODE_INTF_SLOTS; i++){
        if(node->edges[i])
            continue;

        if(i == MAX_NODE_INTF_SLOTS){
            printf("%s() : Error : No slots available in node %s\n", __FUNCTION__, node->node_name);
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
                            node_t *to_node, DIRECTION dirn){

    insert_interface_into_node(from_node, &edge->from);
    edge->from.dirn = OUTGOING;
    insert_interface_into_node(to_node, &edge->to);
    edge->to.dirn = INCOMING;
    
    edge_t *edge2 = NULL;

    if(dirn == BIDIRECTIONAL){
       
        if(IS_LEVEL_SET(edge->level, LEVEL1)) 
            edge2 = create_new_edge(edge->to.intf_name, edge->from.intf_name, edge->metric[LEVEL1],
                                        edge->to.prefix[LEVEL1], edge->from.prefix[LEVEL1], edge->level);

        else if(IS_LEVEL_SET(edge->level, LEVEL2))
            edge2 = create_new_edge(edge->to.intf_name, edge->from.intf_name, edge->metric[LEVEL2],
                                        edge->to.prefix[LEVEL2], edge->from.prefix[LEVEL2], edge->level);

        insert_edge_between_2_nodes(edge2, to_node, from_node, UNIDIRECTIONAL);
    }
}

void
set_graph_root(graph_t *graph, node_t *root){
    graph->graph_root = root;
}

void
mark_node_pseudonode(node_t *node, LEVEL level){

    unsigned int i = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;

    if(level != LEVEL1 && level != LEVEL2)
        assert(0);

    node->node_type[level] = PSEUDONODE;

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        if(!node->edges[i]) continue;;
        
        edge_end = node->edges[i];
        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        
        if(!IS_LEVEL_SET(edge->level, level))      
            continue;
        
        memset(edge_end->prefix[level], 0, PREFIX_LEN_WITH_MASK + 1);
        strncpy(edge_end->prefix[level], "NIL", PREFIX_LEN_WITH_MASK);
        edge_end->prefix[level][PREFIX_LEN_WITH_MASK] = '\0';

        if(get_edge_direction(node, edge) == OUTGOING)
            edge->metric[level] = 0;
    }
}

graph_t *
get_new_graph(){

    graph_t *graph = calloc(1, sizeof(graph_t));
    graph->graph_node_list = init_singly_ll();
    graph->spf_run_result[LEVEL1] = init_singly_ll();
    graph->spf_run_result[LEVEL2] = init_singly_ll();
    return graph;
}

int
is_two_way_nbrship(node_t *node, node_t *node_nbr, LEVEL level){

    edge_t *edge = NULL;
    node_t *temp_nbr_node = NULL,
           *temp_nbr_node2 = NULL;

    ITERATE_NODE_NBRS_BEGIN(node, temp_nbr_node, edge, level){

        if(temp_nbr_node != node_nbr)
            continue;

        ITERATE_NODE_NBRS_BEGIN(node_nbr, temp_nbr_node2, edge, level){

            if(temp_nbr_node2 != node)
                continue;

            return 1;
        }
        ITERATE_NODE_NBRS_END;
    }
    ITERATE_NODE_NBRS_END;
    return 0;
}

