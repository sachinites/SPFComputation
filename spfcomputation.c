/*
 * =====================================================================================
 *
 *       Filename:  spfcomputation.c
 *
 *    Description:  This file implements the functionality to run SPF computation on the Network Graph
 *
 *        Version:  1.0
 *        Created:  Thursday 24 August 2017 04:31:44  IST
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
#include "heap_interface.h"

extern graph_t *graph;

static void
run_dijkastra(){


}


static void 
spf_init(){

    /*step 1 : Purge NH list of all nodes in the topo*/

    singly_ll_node_t *list_node = NULL;
    node_t *node = NULL;
    unsigned int i = 0;
    edge_t *edge = NULL;

    ITERATE_LIST(graph->graph_node_list, list_node){    
        node = (node_t *)list_node->data;
        for(i = 0; i < MAX_NXT_HOPS; i++){
            node->next_hop[i] = 0;
            node->direct_next_hop[i] = 0;   
        }
    }

    /*step 2 : Metric intialization*/
    ITERATE_LIST(graph->graph_node_list, list_node){
        node = (node_t *)list_node->data;
        node->spf_metric = INFINITE_METRIC;
    }
    graph->graph_root->spf_metric = 0;

    /*step 3 : Initialize direct nexthops.
     * Iterate over real physical nbrs of root (that is skip PNs)
     * and initialize their direct next hop list*/

    ITERATE_NODE_NBRS_BEGIN(graph->graph_root, node, edge){
        if(node->node_type == PSEUDONODE)/*Do not initialize direct nxt hop of PNs*/
            continue;
        node->direct_next_hop[0] = node;
    }
    ITERATE_NODE_NBRS_END;

    /*Step 4 : Initialize candidate tree with root*/
    

}

void
spf_computation(){

    spf_init();
}
