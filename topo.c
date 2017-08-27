/*
 * =====================================================================================
 *
 *       Filename:  topo.c
 *
 *    Description:  This file builds the network topology
 *
 *        Version:  1.0
 *        Created:  Thursday 24 August 2017 01:07:07  IST
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

graph_t *
build_linear_topo(){

    /*
     *
     * +------+               +------+                +-------+
     * |      |0/0    10.1.1.2|      |0/2     20.1.1.2|       |
     * |  R0  +---------------+  R1  +----------------+  R2   |
     * |      |10.1.1.1    0/1|      |20.1.1.1    0/2 |       |
     * +------+               +------+                +-------+
     *                                                    
     * */
    
    graph_t *graph = get_new_graph();

    node_t *R0 = create_new_node(graph, "R0", AREA1);
    node_t *R1 = create_new_node(graph, "R1", AREA1);
    node_t *R2 = create_new_node(graph, "R2", AREA1);

    edge_t *R0_R1_edge = create_new_edge("eth0/0", "eth0/1", 10,
            "10.1.1.1/24", "10.1.1.2/24", LEVEL12);

    edge_t *R1_R2_edge = create_new_edge("eth0/2", "eth0/2", 10,
            "20.1.1.1/24", "20.1.1.2/24", LEVEL12);

    insert_edge_between_2_nodes(R0_R1_edge, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R2_edge, R1, R2, BIDIRECTIONAL);

    mark_node_pseudonode(R1, LEVEL1);
    set_graph_root(graph, R0);
    return graph;
}
