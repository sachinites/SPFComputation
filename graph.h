/*
 * =====================================================================================
 *
 *       Filename:  graph.h
 *
 *    Description:  This is a header file to declare structures to define the Network topology
 *
 *        Version:  1.0
 *        Created:  Wednesday 23 August 2017 01:51:55  IST
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

#ifndef __GRAPH__
#define __GRAPH__

#include "graphconst.h"
#include "LinkedListApi.h"
#include <stdlib.h>
#include <assert.h>

typedef struct edge_end_ edge_end_t;

typedef struct _node_t{
    char node_name[NODE_NAME_SIZE];
    edge_end_t *edges[MAX_NODE_INTF_SLOTS];
    NODE_TYPE node_type;    
    /*SPF Computation members*/
    unsigned int spf_metric;
    struct _node_t *next_hop[MAX_NXT_HOPS]; 
} node_t;

struct edge_end_{
    node_t *node;
    char intf_name[IF_NAME_SIZE];
    char prefix[PREFIX_LEN_WITH_MASK + 1];
    EDGE_END_DIRN dirn;
};

typedef struct _edge_t{
    edge_end_t from;
    unsigned int metric;
    edge_end_t to;
} edge_t;

typedef struct graph_{
    node_t *graph_root;
    ll_t *graph_node_list;
} graph_t;

node_t *
create_new_node(graph_t *graph, char *node_name);


edge_t *
create_new_edge(char *from_ifname, 
                char *to_ifname, 
                unsigned int metric, 
                char *from_prefix, 
                char *to_prefix);

void
insert_edge_between_2_nodes(edge_t *edge, 
                            node_t *from_node, 
                            node_t *to_node, DIRECTION dirn);

void
set_graph_root(graph_t *graph, node_t *root);

graph_t *
get_new_graph();

void
dump_nbrs(node_t *node);

void
dump_node_info(node_t *node);

void 
dump_edge_info(edge_t *edge);

void
mark_node_pseudonode(node_t *node);
/* Macros */

/*Iterate over nbrs of a given node*/

#define GET_EGDE_PTR_FROM_FROM_EDGE_END(edge_end_ptr)   \
    (edge_t *)((char *)edge_end_ptr - (unsigned int)&(((edge_t *)0)->from))

#define GET_EGDE_PTR_FROM_TO_EDGE_END(edge_end_ptr)     \
    (edge_t *)((char *)edge_end_ptr - (unsigned int)&(((edge_t *)0)->to))

static inline edge_t *
GET_EGDE_PTR_FROM_EDGE_END(edge_end_t *edge_end){
    
    edge_t *edge = NULL;
    if(edge_end->dirn == OUTGOING)
        edge = GET_EGDE_PTR_FROM_FROM_EDGE_END(edge_end);
    else if(edge_end->dirn == INCOMING)
        edge = GET_EGDE_PTR_FROM_TO_EDGE_END(edge_end);
    else
        assert(0);

    return edge;
}

/* The function test whether the given edge
 * is outgoing edge or incoming edge wrt to a goven node*/
static inline EDGE_END_DIRN
get_edge_direction(node_t *node, edge_t *edge){

    if(edge->from.node == node && edge->from.dirn == OUTGOING)
        return OUTGOING;
    else if(edge->to.node == node && edge->to.dirn == INCOMING)
        return INCOMING;

    return EDGE_END_DIRN_UNKNOWN;
}


#define ITERATE_NODE_NBRS_BEGIN(node, nbr_node, edge)            \
    nbr_node = NULL;                                             \
    edge = NULL;                                                 \
    do{                                                          \
        unsigned int i = 0;                                      \
        edge_end_t *edge_end = 0;                                \
        for(;i < MAX_NODE_INTF_SLOTS; i++){                      \
            edge_end = node->edges[i];                           \
            if(!edge_end) break;                                 \
            if(edge_end->dirn != OUTGOING)                       \
                continue;                                        \
            edge = GET_EGDE_PTR_FROM_FROM_EDGE_END(edge_end);    \
            nbr_node = edge->to.node;
             
#define ITERATE_NODE_NBRS_END   }}while(0)      
   
#endif /* __GRAPH__ */

