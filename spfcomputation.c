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
#include "spfutil.h"
#include "spfcomputation.h"
#include <stdio.h>

extern graph_t *graph;
spf_stats_t spf_stats;

static void
run_dijkastra(LEVEL level, candidate_tree_t *ctree){

    spf_stats.spf_runs_count[level]++;
    node_t *candidate_node = NULL,
           *nbr_node = NULL;
    edge_t *edge = NULL;

    /*Process untill candidate tree is not empty*/
    printf("%s() : Running Dijkastra with root node = %s\n", __FUNCTION__, (GET_CANDIDATE_TREE_TOP(ctree, level))->node_name);
    while(!IS_CANDIDATE_TREE_EMPTY(ctree)){

        /*Take the node with miminum spf_metric off the candidate tree*/
    
        candidate_node = GET_CANDIDATE_TREE_TOP(ctree, level);
        REMOVE_CANDIDATE_TREE_TOP(ctree);
        printf("%s() : Candidate node %s Taken off candidate list\n", __FUNCTION__, candidate_node->node_name);

        /*Add the node just taken off the candidate tree into result list. pls note, we dont want PN in results list
         * however we process it as ususal like other nodes*/

        if(candidate_node->node_type[level] != PSEUDONODE) 
            singly_ll_add_node_by_val(graph->spf_run_result[level], (void *)candidate_node);


        /*Iterare over all the nbrs of Candidate node*/


        ITERATE_NODE_NBRS_BEGIN(candidate_node, nbr_node, edge, level){
            printf("%s() : Processing Nbr : %s\n", __FUNCTION__, nbr_node->node_name);

            /*To way handshake check. Nbr-ship should be two way with nbr, even if nbr is PN.*/

            if(!is_two_way_nbrship(candidate_node, nbr_node, level)){

                printf("%s() : Two Way nbrship broken with nbr %s\n", __FUNCTION__, nbr_node->node_name);
                continue;
            }

            printf("%s() : Two Way nbrship verified with nbr %s\n", __FUNCTION__, nbr_node->node_name);

            if(candidate_node->spf_metric[level] + edge->metric[level] < nbr_node->spf_metric[level]){
                printf("%s() : Old Metric : %u, New Metric : %u, Better Next Hop\n",
                        __FUNCTION__, nbr_node->spf_metric[level], candidate_node->spf_metric[level] + edge->metric[level]);

                /*case 1 : if My own List is empty, and nbr is Pseuodnode , do nothing*/
                if(is_nh_list_empty(&candidate_node->next_hop[level][0]) &&
                        nbr_node->node_type[level] == PSEUDONODE){

                    printf("%s() : case 1 if My own List is empty, and nbr is Pseuodnode , do nothing\n", __FUNCTION__);
                }

                /*case 2 : if My own List is empty, and nbr is Not a PN, then copy nbr's direct nh list to its own NH list*/
                else if(is_nh_list_empty(&candidate_node->next_hop[level][0]) &&
                        nbr_node->node_type[level] == NON_PSEUDONODE){

                    printf("%s() : case 2 if My own List is empty, and nbr is Not a PN, then copy nbr's direct nh list to its own NH list\n",
                            __FUNCTION__);

                    copy_nh_list(&nbr_node->direct_next_hop[level][0], &nbr_node->next_hop[level][0]);
                }

                /*case 3 : if My own List is not empty, then nbr should inherit my next hop list*/
                else if(!is_nh_list_empty(&candidate_node->next_hop[level][0])){

                    printf("%s() : case 3 if My own List is not empty, then nbr should inherit my next hop list\n", __FUNCTION__);
                    copy_nh_list(&candidate_node->next_hop[level][0], &nbr_node->next_hop[level][0]);
                }

                nbr_node->spf_metric[level] =  candidate_node->spf_metric[level] + edge->metric[level]; 
                INSERT_NODE_INTO_CANDIDATE_TREE(ctree, nbr_node, level);
                printf("%s() : %s's spf_metric has been updated to %u, and inserted into candidate list\n",
                        __FUNCTION__, nbr_node->node_name, nbr_node->spf_metric[level]);

            }
            else{
                printf("%s() : Old Metric : %u, New Metric : %u, Not a Better Next Hop\n",
                        __FUNCTION__, nbr_node->spf_metric[level], candidate_node->spf_metric[level] + edge->metric[level]);
            }
        }
        ITERATE_NODE_NBRS_END;
    }
}


/*-----------------------------------------------------------------------------
 *  Fn to initialize the Necessary Data structure prior to run SPF computation.
 *  level can be LEVEL1 Or LEVEL2 Or BOTH
 *-----------------------------------------------------------------------------*/
static void
spf_init(candidate_tree_t *ctree, LEVEL level){

    /*step 1 : Purge NH list of all nodes in the topo*/

    node_t *node = NULL, *pn_nbr = NULL;
    unsigned int i = 0;
    edge_t *edge = NULL, *pn_edge = NULL;
    singly_ll_node_t *list_node = NULL;

    ITERATE_LIST(graph->graph_node_list, list_node){    
        node = (node_t *)list_node->data;
        for(i = 0; i < MAX_NXT_HOPS; i++){
            node->next_hop[level][i] = 0;
            node->direct_next_hop[level][i] = 0;   
        }
    }

    /*step 2 : Metric intialization*/
    ITERATE_LIST(graph->graph_node_list, list_node){
        node = (node_t *)list_node->data;
        node->spf_metric[level] = INFINITE_METRIC;
    }
    graph->graph_root->spf_metric[level] = 0;

    /*step 3 : Initialize direct nexthops.
     * Iterate over real physical nbrs of root (that is skip PNs)
     * and initialize their direct next hop list. Also, pls note that
     * directly PN's nbrs are also direct next hops to root. In Production
     * code, root has a separate list of directly connected physical real
     * nbrs. In our case, we dont have such list, hence, altenative is treat
     * nbrs of directly connected PN as own nbrs, which is infact the concept
     * of pseudonode*/

    ITERATE_NODE_NBRS_BEGIN(graph->graph_root, node, edge, level){
        if(node->node_type[level] == PSEUDONODE){

            ITERATE_NODE_NBRS_BEGIN(node, pn_nbr, pn_edge, level){
            
                if(pn_nbr == graph->graph_root)
                    continue;

                pn_nbr->direct_next_hop[level][0] = pn_nbr;
            }
            ITERATE_NODE_NBRS_END;

            continue;
        }
        node->direct_next_hop[level][0] = node;
    }
    ITERATE_NODE_NBRS_END;

    /*Step 4 : Initialize candidate tree with root*/
   if(graph->graph_root->node_type[level] == PSEUDONODE) 
       assert(0); /*SPF computation never starts with PN*/

   INSERT_NODE_INTO_CANDIDATE_TREE(ctree, graph->graph_root, level);
   
   /*Step 5 : Link Directly Conneccted PN to the graph root
    * I dont know why it is done, but lets do */

    ITERATE_NODE_NBRS_BEGIN(graph->graph_root, node, edge, level){

        if(node->node_type[level] == PSEUDONODE)
            node->pn_intf[level] = &edge->from;/*There is exactly one PN per LAN per level*/            
    }
    ITERATE_NODE_NBRS_END;
}

void
spf_computation(LEVEL level){

    candidate_tree_t ctree;
    CANDIDATE_TREE_INIT(&ctree);

    /*Drain off results list for level */

    delete_singly_ll(graph->spf_run_result[level]); 

    if(IS_LEVEL_SET(level, LEVEL1)){
        spf_init(&ctree, LEVEL1);
        run_dijkastra(LEVEL1, &ctree);
    }

    else if(IS_LEVEL_SET(level, LEVEL2)){
        spf_init(&ctree, LEVEL2);
        run_dijkastra(LEVEL2, &ctree);
    }

    FREE_CANDIDATE_TREE_INTERNALS(&ctree);
}


