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

#include <stdio.h>
#include "instance.h"
#include "heap_interface.h"
#include "spfutil.h"
#include "spfcomputation.h"
#include "logging.h"
#include "routes.h"
#include "bitsop.h"

extern instance_t *instance;

int
spf_run_result_comparison_fn(void *spf_result_ptr, void *node_ptr){

    if(((spf_result_t *)spf_result_ptr)->node == (node_t *)node_ptr)
        return 1;
    return 0;
}

int
self_spf_run_result_comparison_fn(void *self_spf_result_ptr, void *node_ptr){

    if(((self_spf_result_t *)self_spf_result_ptr)->spf_root == (node_t *)node_ptr)
        return 1;
    return 0;
}


/*Comparison function for routes searching in spf_info lists*/
/*return 0 or failure, 1 on success*/

int
route_search_comparison_fn(void * route, void *key){

    common_pfx_key_t *_key = (common_pfx_key_t *)key;
    routes_t *_route = (routes_t *)route;

    if(strncmp(_key->prefix, _route->rt_key.prefix, PREFIX_LEN + 1) == 0 &&
        _key->mask == _route->rt_key.mask)
        return 1;

    return 0;
}


/* Inverse the topology wrt to level*/
/* ToDo : To inverse the topo, explore the nbrs of nbr recursively instead
 * of iterating the global list of nodes*/

void
inverse_topology(instance_t *instance, LEVEL level){

    singly_ll_node_t* list_node = NULL;
    node_t *node = NULL;
    unsigned int i = 0, edge_metric = 0;
    edge_end_t *from_edge_end = NULL;
    
    edge_t *edge = NULL;;

    ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
        node = (node_t *)list_node->data;
    
        for(i = 0; i < MAX_NODE_INTF_SLOTS; i++){
            
            from_edge_end = node->edges[i];
            if(!from_edge_end)
                break;

            if(from_edge_end->dirn != OUTGOING)
                continue;

            /*Reverse the edge properties*/
            /*It is enough to swap metric of an edge with reverse edge. There
             * is a strong assumption that all edges are bidrectional */

            edge = GET_EGDE_PTR_FROM_EDGE_END(from_edge_end);

            if(!IS_LEVEL_SET(edge->level, level))
                continue;

            if(!edge->inv_edge)
                continue;

            edge_metric = edge->metric[level];
            edge->metric[level] = edge->inv_edge->metric[level];
            edge->inv_edge->metric[level] = edge_metric;
            edge->inv_edge->inv_edge = NULL;
        }
    }ITERATE_LIST_END;

    /*repair*/
    ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
        
        node = (node_t *)list_node->data;
        for(i = 0; i < MAX_NODE_INTF_SLOTS; i++){

            from_edge_end = node->edges[i];
            if(!from_edge_end)
                break;

            if(from_edge_end->dirn != OUTGOING)
                continue;

            edge = GET_EGDE_PTR_FROM_EDGE_END(from_edge_end);

            if(!edge->inv_edge)
                continue;

            if(!IS_LEVEL_SET(edge->level, level))
                continue;

            edge->inv_edge->inv_edge = edge;
            /* restore the swapped edge metrics*/
            edge_metric = edge->metric[level];
            edge->metric[level] = edge->inv_edge->metric[level];
            edge->inv_edge->metric[level] = edge_metric;
        }
    }ITERATE_LIST_END;
}


static void
run_dijkastra(node_t *spf_root, LEVEL level, candidate_tree_t *ctree){

    node_t *candidate_node = NULL,
           *nbr_node = NULL;
    edge_t *edge = NULL;
    unsigned int i = 0;
    self_spf_result_t *self_res = NULL;

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
            res->spf_metric = candidate_node->spf_metric[level];
            for(i = 0 ; i < MAX_NXT_HOPS; i++){
                if(candidate_node->next_hop[level][i])
                    res->next_hop[i] = candidate_node->next_hop[level][i];
                else
                    break;
            }

            singly_ll_add_node_by_val(spf_root->spf_run_result[level], (void *)res);
            self_res = singly_ll_search_by_key(candidate_node->self_spf_result[level], spf_root);

            if(self_res){
                sprintf(LOG, "Curr node : %s, Overwriting self spf result with spf root %s", 
                            candidate_node->node_name, spf_root->node_name); TRACE();
                self_res->spf_root = spf_root;
                self_res->res = res;
            }
            else{
                 sprintf(LOG, "Curr node : %s, Creating New self spf result with spf root %s",
                            candidate_node->node_name, spf_root->node_name); TRACE();
                 self_res = calloc(1, sizeof(self_spf_result_t));
                 self_res->spf_root = spf_root;
                 self_res->res = res;
                 singly_ll_add_node_by_val(candidate_node->self_spf_result[level], self_res);
            }
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

            if((unsigned long long)candidate_node->spf_metric[level] + (IS_OVERLOADED(candidate_node, level) 
                ? (unsigned long long)INFINITE_METRIC : (unsigned long long)edge->metric[level]) < (unsigned long long)nbr_node->spf_metric[level]){
                
                sprintf(LOG, "Old Metric : %u, New Metric : %u, Better Next Hop", 
                        nbr_node->spf_metric[level], IS_OVERLOADED(candidate_node, level) 
                        ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]);
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

                nbr_node->spf_metric[level] =  IS_OVERLOADED(candidate_node, level) ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]; 
                INSERT_NODE_INTO_CANDIDATE_TREE(ctree, nbr_node, level);
                sprintf(LOG, "%s's spf_metric has been updated to %u, and inserted into candidate list", 
                        nbr_node->node_name, nbr_node->spf_metric[level]); 
                TRACE();
            }

            else if((unsigned long long)candidate_node->spf_metric[level] + (IS_OVERLOADED(candidate_node, level) 
                ? (unsigned long long)INFINITE_METRIC : (unsigned long long)edge->metric[level]) == (unsigned long long)nbr_node->spf_metric[level]){

                sprintf(LOG, "Old Metric : %u, New Metric : %u, ECMP path",
                        nbr_node->spf_metric[level], IS_OVERLOADED(candidate_node, level) 
                        ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]); TRACE();
            }
            else{
                sprintf(LOG, "Old Metric : %u, New Metric : %u, Not a Better Next Hop",
                        nbr_node->spf_metric[level], IS_OVERLOADED(candidate_node, level) 
                        ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]);
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
spf_clear_result(node_t *spf_root, LEVEL level){

   singly_ll_node_t *list_node = NULL;
   spf_result_t *result = NULL;

   ITERATE_LIST_BEGIN(spf_root->spf_run_result[level], list_node){

       result = list_node->data;
       free(result);
       result = NULL;    
   }ITERATE_LIST_END;
   delete_singly_ll(spf_root->spf_run_result[level]);
}

static void
spf_init(candidate_tree_t *ctree, 
         node_t *spf_root, 
         LEVEL level){

    /*step 1 : Purge NH list of all nodes in the topo*/

    unsigned int i = 0;
    node_t *node = NULL, *pn_nbr = NULL;
    edge_t *edge = NULL, *pn_edge = NULL;
    singly_ll_node_t *list_node = NULL;

    /*Drain off results list for level */
    spf_clear_result(spf_root, level);

    /* You should intialize the nxthops and direct nxthops only for 
     * reachable routers to spf root in the same level, not the entire
     * graph.*/

    ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
        
        node = (node_t *)list_node->data;
        for(i = 0; i < MAX_NXT_HOPS; i++){
            node->next_hop[level][i] = 0;
            node->direct_next_hop[level][i] = 0;   
        }
    } ITERATE_LIST_END;
   
    /*step 2 : Metric intialization*/
   ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
        
        node = (node_t *)list_node->data;
        node->spf_metric[level] = INFINITE_METRIC;
   } ITERATE_LIST_END;
   
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
   INSERT_NODE_INTO_CANDIDATE_TREE(ctree, spf_root, level);
   
   /*Step 5 : Link Directly Conneccted PN to the instance root. This
    * will help identifying the route oif when spf_root is connected to PN */

   ITERATE_NODE_NBRS_BEGIN(spf_root, node, edge, level){

       if(node->node_type[level] == PSEUDONODE)
           node->pn_intf[level] = &edge->from;/*There is exactly one PN per LAN per level*/            
   }
   ITERATE_NODE_NBRS_END;
}


void
spf_computation(node_t *spf_root, 
                spf_info_t *spf_info, 
                LEVEL level, spf_type_t spf_type){

    sprintf(LOG, "Node : %s, Triggered SPF run : %s, Level%d", 
                spf_root->node_name, spf_type == FULL_RUN ? "FULL_RUN" : "SKELETON_RUN",
                level); TRACE();
                 
    if(IS_OVERLOADED(spf_root, level)){
        printf("%s(): INFO : Node %s is overloaded, SPF cannot be run\n", 
            __FUNCTION__, spf_root->node_name);
        return;
    }

    RE_INIT_CANDIDATE_TREE(&instance->ctree);

    if(level != LEVEL1 && level != LEVEL2){
        printf("%s() : Error : invalid level specified\n", __FUNCTION__);
        return;
    }

    spf_init(&instance->ctree, spf_root, level);

    if(spf_type == FULL_RUN){
        spf_info->spf_level_info[level].version++;
    }

    run_dijkastra(spf_root, level, &instance->ctree);

    /* Route Building After SPF computation*/
    /*We dont build routing table for reverse spf run*/
    if(spf_type == FULL_RUN){
        sprintf(LOG, "Route building starts After SPF skeleton run"); TRACE();
        spf_postprocessing(spf_info, spf_root, level);
    }
}

static void
init_prc_run(node_t *spf_root, LEVEL level){

    mark_all_routes_stale(&spf_root->spf_info, level);
    spf_root->spf_info.spf_level_info[level].spf_type = PRC_RUN;
}


void
partial_spf_run(node_t *spf_root, LEVEL level){

    sprintf(LOG, "Root : %s, %s", spf_root->node_name, get_str_level(level)); TRACE();
    
    if(spf_root->spf_info.spf_level_info[level].version == 0){
        sprintf(LOG, "Root : %s, %s. No full SPF run till now. Runnig ...", spf_root->node_name, get_str_level(level)); TRACE();
        
        spf_computation(spf_root, &spf_root->spf_info, level, FULL_RUN);      
        return;
    }

    init_prc_run(spf_root, level);
    spf_postprocessing(&spf_root->spf_info, spf_root, level);
    spf_root->spf_info.spf_level_info[level].spf_type = FULL_RUN;
}




