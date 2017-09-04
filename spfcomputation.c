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

#include "instance.h"
#include "heap_interface.h"
#include "spfutil.h"
#include "spfcomputation.h"
#include <stdio.h>
#include "logging.h"
#include "routes.h"


extern instance_t *instance;

static void
run_dijkastra(node_t *spf_root, LEVEL level, candidate_tree_t *ctree){

    node_t *candidate_node = NULL,
           *nbr_node = NULL;
    edge_t *edge = NULL;

    /*Process untill candidate tree is not empty*/
    sprintf(LOG, "Running Dijkastra with root node = %s, Level = %u", 
            (GET_CANDIDATE_TREE_TOP(ctree, level))->node_name, level); TRACE();

    while(!IS_CANDIDATE_TREE_EMPTY(ctree)){

        /*Take the node with miminum spf_metric off the candidate tree*/
    
        candidate_node = GET_CANDIDATE_TREE_TOP(ctree, level);
        REMOVE_CANDIDATE_TREE_TOP(ctree);
        sprintf(LOG, "Candidate node %s Taken off candidate list", candidate_node->node_name); TRACE();

        /*Add the node just taken off the candidate tree into result list. pls note, we dont want PN in results list
         * however we process it as ususal like other nodes*/

        if(candidate_node->node_type[level] != PSEUDONODE){

            spf_result_t *res = calloc(1, sizeof(spf_result_t));
            res->node = candidate_node;
            candidate_node->spf_result = res; /*back pointer from node to result node*/
            res->spf_metric = candidate_node->spf_metric[level];
            singly_ll_add_node_by_val(spf_root->spf_run_result[level], (void *)res);
        }


        /*Iterare over all the nbrs of Candidate node*/


        ITERATE_NODE_NBRS_BEGIN(candidate_node, nbr_node, edge, level){
            sprintf(LOG, "Processing Nbr : %s", nbr_node->node_name); TRACE();

            /*To way handshake check. Nbr-ship should be two way with nbr, even if nbr is PN. Do
             * not consider the node for SPF computation if we find 2-way nbrship is broken*/

            if(!is_two_way_nbrship(candidate_node, nbr_node, level)){

                sprintf(LOG, "Two Way nbrship broken with nbr %s", nbr_node->node_name); TRACE();
                continue;
            }
            
            sprintf(LOG, "Two Way nbrship verified with nbr %s",nbr_node->node_name); TRACE();

            if(candidate_node->spf_metric[level] + edge->metric[level] < nbr_node->spf_metric[level]){
                
                sprintf(LOG, "Old Metric : %u, New Metric : %u, Better Next Hop", 
                        nbr_node->spf_metric[level], candidate_node->spf_metric[level] + edge->metric[level]);
                TRACE();

                
                /*case 1 : if My own List is empty, and nbr is Pseuodnode , do nothing*/
                if(is_nh_list_empty(&candidate_node->next_hop[level][0]) &&
                        nbr_node->node_type[level] == PSEUDONODE){

                    sprintf(LOG, "case 1 if My own List is empty, and nbr is Pseuodnode , do nothing"); TRACE();
                }

                
                
                /*case 2 : if My own List is empty, and nbr is Not a PN, then copy nbr's direct nh list to its own NH list*/
                else if(is_nh_list_empty(&candidate_node->next_hop[level][0]) &&
                        nbr_node->node_type[level] == NON_PSEUDONODE){
    
                     sprintf(LOG, "case 2 if My own List is empty, and nbr is Not a PN, then copy nbr's direct nh list to its own NH list");
                     TRACE();

                    copy_nh_list(&nbr_node->direct_next_hop[level][0], &nbr_node->next_hop[level][0]);
                }

                
                
                /*case 3 : if My own List is not empty, then nbr should inherit my next hop list*/
                else if(!is_nh_list_empty(&candidate_node->next_hop[level][0])){

                    sprintf(LOG, "case 3 if My own List is not empty, then nbr should inherit my next hop list"); TRACE();
                    copy_nh_list(&candidate_node->next_hop[level][0], &nbr_node->next_hop[level][0]);
                }

                nbr_node->spf_metric[level] =  candidate_node->spf_metric[level] + edge->metric[level]; 
                INSERT_NODE_INTO_CANDIDATE_TREE(ctree, nbr_node, level);
                sprintf(LOG, "%s's spf_metric has been updated to %u, and inserted into candidate list", 
                        nbr_node->node_name, nbr_node->spf_metric[level]); 
                TRACE();
            }

            else{
                sprintf(LOG, "Old Metric : %u, New Metric : %u, Not a Better Next Hop",
                        nbr_node->spf_metric[level], candidate_node->spf_metric[level] + edge->metric[level]);
                TRACE();
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
spf_init(candidate_tree_t *ctree, 
         node_t *spf_root, 
         LEVEL level){

    /*step 1 : Purge NH list of all nodes in the topo*/

    unsigned int i = 0;
    node_t *node = NULL, *pn_nbr = NULL;
    edge_t *edge = NULL, *pn_edge = NULL;
    singly_ll_node_t *list_node = NULL;

    ITERATE_LIST(instance->instance_node_list, list_node){    
        node = (node_t *)list_node->data;
        for(i = 0; i < MAX_NXT_HOPS; i++){
            node->next_hop[level][i] = 0;
            node->direct_next_hop[level][i] = 0;   
        }
    }

    /*step 2 : Metric intialization*/
    ITERATE_LIST(instance->instance_node_list, list_node){
        node = (node_t *)list_node->data;
        node->spf_metric[level] = INFINITE_METRIC;
    }

    spf_root->spf_metric[level] = 0;

    /*step 3 : Initialize direct nexthops.
     * Iterate over real physical nbrs of root (that is skip PNs)
     * and initialize their direct next hop list. Also, pls note that
     * directly PN's nbrs are also direct next hops to root. In Production
     * code, root has a separate list of directly connected physical real
     * nbrs. In our case, we dont have such list, hence, altenative is treat
     * nbrs of directly connected PN as own nbrs, which is infact the concept
     * of pseudonode. Again, do not compute direct next hops of PN*/

    ITERATE_NODE_NBRS_BEGIN(spf_root, node, edge, level){
        
        if(node->node_type[level] == PSEUDONODE){

            ITERATE_NODE_NBRS_BEGIN(node, pn_nbr, pn_edge, level){
            
                if(pn_nbr == spf_root)
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
   if(spf_root->node_type[level] == PSEUDONODE) 
       assert(0); /*SPF computation never starts with PN*/

   INSERT_NODE_INTO_CANDIDATE_TREE(ctree, spf_root, level);
   
   /*Step 5 : Link Directly Conneccted PN to the instance root
    * I dont know why it is done, but lets do */

    ITERATE_NODE_NBRS_BEGIN(spf_root, node, edge, level){

        if(node->node_type[level] == PSEUDONODE)
            node->pn_intf[level] = &edge->from;/*There is exactly one PN per LAN per level*/            
    }
    ITERATE_NODE_NBRS_END;
}

void
spf_computation(node_t *spf_root, 
                spf_info_t *spf_info, 
                LEVEL level){

    RE_INIT_CANDIDATE_TREE(&spf_info->ctree);

    /*Drain off results list for level */
    if(level != LEVEL1 && level != LEVEL2){
        printf("%s() : Error : invalid level specified\n", __FUNCTION__);
        return;
    }

    delete_singly_ll(spf_root->spf_run_result[level]); 

    spf_init(&spf_info->ctree, spf_root, level);
    spf_info->spf_level_info[level].version++;
    run_dijkastra(spf_root, level, &spf_info->ctree);

    //FREE_CANDIDATE_TREE_INTERNALS(&spf_info->ctree);

    /* Route Building After SPF computation*/

    sprintf(LOG, "Route building starts After SPF skeleton run"); TRACE();

    spf_postprocessing(spf_info, spf_root, level);
}


