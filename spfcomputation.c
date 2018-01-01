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
#include "Queue.h"
#include "advert.h"

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
    self_spf_result_t *self_res = NULL;
    nh_type_t nh = NH_MAX;

    /*Process untill candidate tree is not empty*/
    sprintf(LOG, "Running Dijkastra with root node = %s, Level = %u", 
            (GET_CANDIDATE_TREE_TOP(ctree, level))->node_name, level); TRACE();

    while(!IS_CANDIDATE_TREE_EMPTY(ctree)){

        /*Take the node with miminum spf_metric off the candidate tree*/
    
        candidate_node = GET_CANDIDATE_TREE_TOP(ctree, level);
        REMOVE_CANDIDATE_TREE_TOP(ctree);
        candidate_node->is_node_on_heap = FALSE;
        sprintf(LOG, "Candidate node %s Taken off candidate list", candidate_node->node_name); TRACE();

        /*Add the node just taken off the candidate tree into result list. pls note, we dont want PN in results list
         * however we process it as ususal like other nodes*/

        //if(candidate_node->node_type[level] != PSEUDONODE){

            spf_result_t *res = calloc(1, sizeof(spf_result_t));
            res->node = candidate_node;
            res->spf_metric = candidate_node->spf_metric[level];
            res->lsp_metric = candidate_node->lsp_metric[level];

            ITERATE_NH_TYPE_BEGIN(nh){

                copy_nh_list(&candidate_node->next_hop[level][nh][0], &res->next_hop[nh][0]); 

            } ITERATE_NH_TYPE_END;

            if(candidate_node->node_type[level] != PSEUDONODE)
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
        //}

        /*Iterare over all the nbrs of Candidate node*/

        ITERATE_NODE_LOGICAL_NBRS_BEGIN(candidate_node, nbr_node, edge, level){
            sprintf(LOG, "Processing Nbr : %s", nbr_node->node_name); TRACE();

            /*Two way handshake check. Nbr-ship should be two way with nbr, even if nbr is PN. Do
             * not consider the node for SPF computation if we find 2-way nbrship is broken. */
             
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
                    
                ITERATE_NH_TYPE_BEGIN(nh){
                    
                    /* is_all_nh_list_empty(node, level) is TRUE only for spf_root and directly connected PNs */

                    if(is_all_nh_list_empty(candidate_node, level) &&
                            nbr_node->node_type[level] == PSEUDONODE){

                        sprintf(LOG, "case 1 if My own NH List is empty, and nbr is Pseuodnode , do nothing"); TRACE();
                    }

                    /*case 2 : if My own List is empty, and nbr is Not a PN, then copy nbr's direct nh list to its own NH list*/
                    else if(is_all_nh_list_empty(candidate_node, level) &&
                            nbr_node->node_type[level] == NON_PSEUDONODE){

                        sprintf(LOG, "case 2 if My own List is empty, and nbr is Not a PN, then copy nbr's direct nh list to its own NH list");
                        TRACE();
                        
                        sprintf(LOG, "Copying %s direct_next_hop %s %s to %s next_hop list", nbr_node->node_name, get_str_level(level), 
                                      nh == IPNH ? "IPNH" : "LSPNH", nbr_node->node_name); TRACE();
                        
                        sprintf(LOG, "printing %s direct_next_hop list at %s %s before copy", nbr_node->node_name, get_str_level(level),
                                      nh == IPNH ? "IPNH" : "LSPNH"); TRACE();

                        print_direct_nh_list(&nbr_node->direct_next_hop[level][nh][0]);
                        copy_direct_to_nh_list(&nbr_node->direct_next_hop[level][nh][0], &nbr_node->next_hop[level][nh][0]);
                        sprintf(LOG, "printing %s next_hop list at %s %s after copy", nbr_node->node_name, get_str_level(level),
                                      nh == IPNH ? "IPNH" : "LSPNH"); TRACE();
                        print_nh_list(&nbr_node->next_hop[level][nh][0]);
                    }


                    /*case 3 : if My own List is not empty, then nbr should inherit my next hop list*/
                    else if(!is_all_nh_list_empty(candidate_node, level)){

                        sprintf(LOG, "case 3 if My own List is not empty, then nbr should inherit my next hop list"); TRACE();
                        sprintf(LOG, "Copying %s next_hop list %s %s to %s next_hop list", candidate_node->node_name, get_str_level(level), 
                                      nh == IPNH ? "IPNH" : "LSPNH", nbr_node->node_name); TRACE();
                        copy_nh_list(&candidate_node->next_hop[level][nh][0], &nbr_node->next_hop[level][nh][0]);
                        sprintf(LOG, "printing %s next_hop list at %s %s after copy", nbr_node->node_name, get_str_level(level),
                                      nh == IPNH ? "IPNH" : "LSPNH"); TRACE();
                        print_nh_list(&nbr_node->next_hop[level][nh][0]);
                    }

                } ITERATE_NH_TYPE_END;

                nbr_node->spf_metric[level] =  IS_OVERLOADED(candidate_node, level) ? 
                                               INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]; 
                nbr_node->lsp_metric[level] =  IS_OVERLOADED(candidate_node, level) ? 
                                               INFINITE_METRIC : candidate_node->lsp_metric[level] + edge->metric[level];

                sprintf(LOG, "%s's spf_metric has been updated to %u",  
                        nbr_node->node_name, nbr_node->spf_metric[level]); TRACE();

                if(nbr_node->is_node_on_heap == FALSE){
                    INSERT_NODE_INTO_CANDIDATE_TREE(ctree, nbr_node, level);
                    nbr_node->is_node_on_heap = TRUE;
                    sprintf(LOG, "%s inserted into candidate tree", nbr_node->node_name); TRACE();
                }
                else{
                    /* We should remove the node and then add again into candidate tree
                     * But now i dont have brain cells to do this useless work. It has impact
                     * on performance, but not on output*/

                    sprintf(LOG, "%s is already present in candidate tree", nbr_node->node_name); TRACE();
                }
            }

            else if((unsigned long long)candidate_node->spf_metric[level] + (IS_OVERLOADED(candidate_node, level) 
                ? (unsigned long long)INFINITE_METRIC : (unsigned long long)edge->metric[level]) == (unsigned long long)nbr_node->spf_metric[level]){

                sprintf(LOG, "Old Metric : %u, New Metric : %u, ECMP path",
                        nbr_node->spf_metric[level], IS_OVERLOADED(candidate_node, level) 
                        ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]); TRACE();

                /*We should do two things here :
                 * 1. union of nexthops (IPNH and LSPNH)
                 * 2. if direct NH is present, then merge it into IPNH or LSPNH depeneding on direct NH type
                 * Help : See pseudonode_ecmp_topo() for detail
                 * */
                ITERATE_NH_TYPE_BEGIN(nh){
                    
                    sprintf(LOG, "Union next_hop of %s %s at %s %s", candidate_node->node_name, 
                                  nbr_node->node_name, get_str_level(level), 
                            nh == IPNH ? "IPNH" : "LSPNH"); TRACE();

                    union_nh_list(&candidate_node->next_hop[level][nh][0]  , &nbr_node->next_hop[level][nh][0] );
                    sprintf(LOG, "next_hop of %s at %s %s after Union", nbr_node->node_name,
                                  get_str_level(level), nh == IPNH ? "IPNH" : "LSPNH"); TRACE();
                    print_nh_list(&nbr_node->next_hop[level][nh][0] );
                    
#if 1
                    sprintf(LOG, "Union direct_next_hop of %s with Next hop of %s at %s %s", nbr_node->node_name, 
                                  nbr_node->node_name, get_str_level(level), 
                                  nh == IPNH ? "IPNH" : "LSPNH"); TRACE();
                    union_direct_nh_list(&nbr_node->direct_next_hop[level][nh][0] , &nbr_node->next_hop[level][nh][0] );
                    sprintf(LOG, "next_hop of %s at %s %s after Union", nbr_node->node_name,
                                  get_str_level(level), nh == IPNH ? "IPNH" : "LSPNH"); TRACE();
                    print_nh_list(&nbr_node->next_hop[level][nh][0] );
#endif
                } ITERATE_NH_TYPE_END;

            }
            else{
                sprintf(LOG, "Old Metric : %u, New Metric : %u, Not a Better Next Hop",
                        nbr_node->spf_metric[level], IS_OVERLOADED(candidate_node, level) 
                        ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]);
                TRACE();
            }
        }
        ITERATE_NODE_LOGICAL_NBRS_END;
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
    node_t *pn_nbr = NULL,
           *nbr_node = NULL,
           *curr_node = NULL;

    edge_t *edge = NULL, *pn_edge = NULL;

    nh_type_t nh;

    /*Drain off results list for level */
    spf_clear_result(spf_root, level);

    /* You should intialize the nxthops and direct nxthops only for 
     * reachable routers to spf root in the same level, not the entire
     * graph.*/

    Queue_t *q = initQ();
    init_instance_traversal(instance);

    spf_root->traversing_bit = 1;

    /*step 1 :Initialize spf root*/

    ITERATE_NH_TYPE_BEGIN(nh){

        for(i = 0; i < MAX_NXT_HOPS; i++)
            spf_root->next_hop[level][nh][i] = 0;

        spf_root->direct_next_hop[level][nh][0] = 0;  

    }ITERATE_NH_TYPE_END;

    spf_root->spf_metric[level] = 0;
    spf_root->lsp_metric[level] = 0;

    /*step 2 : Initialize the entire level graph*/
    enqueue(q, spf_root);

    while(!is_queue_empty(q)){

        curr_node = deque(q);

        ITERATE_NODE_LOGICAL_NBRS_BEGIN(curr_node, nbr_node, edge,
                level){

            if(nbr_node->traversing_bit)
                continue;

            ITERATE_NH_TYPE_BEGIN(nh){

                for(i = 0; i < MAX_NXT_HOPS; i++)
                    nbr_node->next_hop[level][nh][i] = 0;

                nbr_node->direct_next_hop[level][nh][0] = 0;   

            } ITERATE_NH_TYPE_END;
            
            nbr_node->spf_metric[level] = INFINITE_METRIC;
            nbr_node->lsp_metric[level] = INFINITE_METRIC;

            nbr_node->traversing_bit = 1;
            enqueue(q, nbr_node);
        }
        ITERATE_NODE_LOGICAL_NBRS_END;
    }
    assert(is_queue_empty(q));
    free(q);
    q = NULL;


    /*step 3 : Initialize direct nexthops.
     * Iterate over real physical nbrs of root (that is skip PNs)
     * and initialize their direct next hop list. Also, pls note that
     * directly PN's nbrs are also direct next hops to root. In Production
     * code, root has a separate list of directly connected physical real
     * nbrs. In our case, we dont have such list, hence, altenative is treat
     * nbrs of directly connected PN as own nbrs, which is infact the concept
     * of pseudonode. Again, do not compute direct next hops of PN*/

    ITERATE_NODE_LOGICAL_NBRS_BEGIN(spf_root, nbr_node, edge, level){

        if(nbr_node->node_type[level] == PSEUDONODE){

            ITERATE_NODE_LOGICAL_NBRS_BEGIN(nbr_node, pn_nbr, pn_edge, level){

                if(pn_nbr == spf_root)
                    continue;

                if(pn_edge->etype == LSP)
                    pn_nbr->direct_next_hop[level][LSPNH][0] = pn_nbr;
                else
                    pn_nbr->direct_next_hop[level][IPNH][0] = pn_nbr;
            }
            ITERATE_NODE_LOGICAL_NBRS_END;
            continue;
        }

        if(edge->etype == LSP)
            nbr_node->direct_next_hop[level][LSPNH][0] = nbr_node;
        else
            nbr_node->direct_next_hop[level][IPNH][0] = nbr_node;
    }
    ITERATE_NODE_LOGICAL_NBRS_END;


    /*Step 4 : Initialize candidate tree with root*/
    INSERT_NODE_INTO_CANDIDATE_TREE(ctree, spf_root, level);
    spf_root->is_node_on_heap = TRUE;

    /*Step 5 : Link Directly Connected PN to the instance root. This
     * will help identifying the route oif when spf_root is connected to PN */

    ITERATE_NODE_LOGICAL_NBRS_BEGIN(spf_root, nbr_node, edge, level){

        if(nbr_node->node_type[level] == PSEUDONODE)
            nbr_node->pn_intf[level] = &edge->from;/*There is exactly one PN per LAN per level*/            
    }
    ITERATE_NODE_LOGICAL_NBRS_END;
}


void
spf_computation(node_t *spf_root, 
                spf_info_t *spf_info, 
                LEVEL level, spf_type_t spf_type){

    if(level != LEVEL1 && level != LEVEL2){
        printf("%s() : Error : invalid level specified\n", __FUNCTION__);
        return;
    }

    if(IS_OVERLOADED(spf_root, level)){
        printf("%s(): INFO : Node %s is overloaded, SPF cannot be run\n", 
            __FUNCTION__, spf_root->node_name);
        return;
    }

    if(level == LEVEL2 && spf_root->spf_info.spf_level_info[LEVEL1].version == 0){
        sprintf(LOG, "Root : %s, Running first LEVEL1 full SPF run before LEVEL2 full SPF run", 
                        spf_root->node_name); TRACE();
        spf_computation(spf_root, &spf_root->spf_info, LEVEL1, FULL_RUN);      
    }

    sprintf(LOG, "Node : %s, Triggered SPF run : %s, %s", 
                spf_root->node_name, spf_type == FULL_RUN ? "FULL_RUN" : "FORWARD_RUN",
                get_str_level(level)); TRACE();
                 
    RE_INIT_CANDIDATE_TREE(&instance->ctree);

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

/*This macro should work as follows :
 * 1. if X and Y both are non-PN, then compute the dist from X to Y from spf result of X
 * 2. if X is a PN, then compute the dist from X to Y from spf result of X, explicit forward SPF computation on X is required in this case
 * 3. if Y is a PN, then get the dist from X to Y from self_spf_result stored in Y list of self spf result with spf_root = X
 * 4. if X and Y both are PNs, then you need to check your basic forward SPF algorithm, this is invalid case, so assert
 */

unsigned int
DIST_X_Y(node_t *X, node_t *Y, LEVEL _level){

    assert(_level == LEVEL1 || _level == LEVEL2);

    if(X->node_type[_level] != PSEUDONODE &&
            Y->node_type[_level] != PSEUDONODE){
        return ((spf_result_t *)(GET_SPF_RESULT((&(X->spf_info)), Y, _level)))->spf_metric;
    }

    if(X->node_type[_level] == PSEUDONODE &&
            Y->node_type[_level] != PSEUDONODE){
        return ((spf_result_t *)(GET_SPF_RESULT((&(X->spf_info)), Y, _level)))->spf_metric;
    }


    if(X->node_type[_level] != PSEUDONODE &&
            Y->node_type[_level] == PSEUDONODE){
        return ((self_spf_result_t *)(singly_ll_search_by_key(Y->self_spf_result[_level], X)))->res->spf_metric;
    }

    assert(0);
}


