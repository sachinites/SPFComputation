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

typedef struct edge_end_ edge_end_t;

typedef struct _node_t{
    char node_name[NODE_NAME_SIZE];
    edge_end_t *edges[MAX_NODE_INTF_SLOTS];
    
    /*SPF Computtaion Data*/
    unsigned int spf_metric;
    struct _node_t *next_hop[MAX_NXT_HOPS]; 
} node_t;

struct edge_end_{
    node_t *node;
    char intf_name[IF_NAME_SIZE];
    char prefix[PREFIX_LEN_WITH_MASK + 1];
};

typedef struct _edge_t{
    edge_end_t from;
    unsigned int metric;
    edge_end_t to;
} edge_t;

typedef struct graph_{
    node_t *graph_root;
} graph_t;

node_t *
create_new_node(char *node_name);


edge_t *
create_new_edge(char *from_ifname, 
                char *to_ifname, 
                unsigned int metric, 
                char *from_prefix, 
                char *to_prefix);

void
insert_edge_between_2_nodes(edge_t *edge, 
                            node_t *from_node, 
                            node_t *to_node);

void 
insert_interface_into_node(node_t *node, edge_end_t *edge_end);

void
set_graph_root(graph_t *graph, node_t *root);

graph_t *
get_new_graph();

graph_t *
get_graph();

void
dump_graph();

#endif /* __GRAPH__ */

