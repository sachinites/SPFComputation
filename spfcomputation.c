/*
            nbr_node->pred_node[level] = spf_root;
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
#include "spfutil.h"
#include "spfcomputation.h"
#include "routes.h"
#include "bitsop.h"
#include "Queue.h"
#include "advert.h"
#include "spftrace.h"
#include "no_warn.h"
#include "complete_spf_path.h"
#include "spf_candidate_tree.h"
#include "LinuxMemoryManager/uapi_mm.h"

extern instance_t *instance;

extern ll_t *
tilfa_get_spf_result_list(node_t *node, LEVEL level);
extern void compute_tilfa(node_t *spf_root, LEVEL level);
extern boolean tilfa_is_link_pruned(edge_t *edge);
extern boolean tilfa_is_node_pruned(node_t *node);
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

    if(strncmp(_key->u.prefix.prefix, _route->rt_key.u.prefix.prefix, PREFIX_LEN + 1) == 0 &&
        _key->u.prefix.mask == _route->rt_key.u.prefix.mask)
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
run_dijkastra(node_t *spf_root, LEVEL level, candidate_tree_t *ctree,
                    spf_type_t spf_type, ll_t *res_lst){

    node_t *candidate_node = NULL,
           *nbr_node = NULL,
           *pn_node = NULL;

    edge_t *edge = NULL, 
           *pn_edge = NULL;

    spf_result_t *res = NULL;
    nh_type_t nh = NH_MAX;
    self_spf_result_t *self_res = NULL;

    /*Process untill candidate tree is not empty*/
#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Running Dijkastra with root node = %s, Level = %u", 
            (SPF_GET_CANDIDATE_TREE_TOP(ctree))->node_name, level); 
    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
    
    assert(res_lst);
    assert(is_singly_ll_empty(res_lst));

    while(!SPF_IS_CANDIDATE_TREE_EMPTY(ctree)){

        /*Take the node with miminum spf_metric off the candidate tree*/

        candidate_node = SPF_GET_CANDIDATE_TREE_TOP(ctree);
        SPF_REMOVE_CANDIDATE_TREE_TOP(ctree);
        candidate_node->is_node_on_heap = FALSE;
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Candidate node %s Taken off candidate list", candidate_node->node_name); 
        trace(instance->traceopts, DIJKSTRA_BIT);
#endif

        /*Add the node just taken off the candidate tree into result list. pls note, we dont want PN in results list
         * however we process it as ususal like other nodes*/
        if(candidate_node->node_type[level] != PSEUDONODE){
            res = singly_ll_search_by_key(res_lst, candidate_node);
            if(!res) {
                res = XCALLOC(1, spf_result_t);
                singly_ll_add_node_by_val(res_lst, (void *)res);
            }
        }
        res->node = candidate_node;
        res->spf_metric = candidate_node->spf_metric[level];
        res->lsp_metric = candidate_node->lsp_metric[level];

        ITERATE_NH_TYPE_BEGIN(nh){
            
            copy_nh_list2(&candidate_node->next_hop[level][nh][0], &res->next_hop[nh][0]); 
        } ITERATE_NH_TYPE_END;

        if(spf_type != TILFA_RUN){
            self_res = singly_ll_search_by_key(candidate_node->self_spf_result[level], spf_root);

            if(self_res){
#ifdef __ENABLE_TRACE__            
                sprintf(instance->traceopts->b, "Curr node : %s, Overwriting self spf result with spf root %s", 
                        candidate_node->node_name, spf_root->node_name); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                self_res->spf_root = spf_root;
                self_res->res = res;
            }
            else{
#ifdef __ENABLE_TRACE__            
                sprintf(instance->traceopts->b, "Curr node : %s, Creating New self spf result with spf root %s",
                        candidate_node->node_name, spf_root->node_name); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                self_res = XCALLOC(1, self_spf_result_t);
                self_res->spf_root = spf_root;
                self_res->res = res;
                singly_ll_add_node_by_val(candidate_node->self_spf_result[level], self_res);
            }
        }
        /*Iterare over all the nbrs of Candidate node*/

          ITERATE_NODE_LOGICAL_NBRS_BEGIN(candidate_node, nbr_node, edge, level){
#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "Processing Nbr : %s", nbr_node->node_name); 
            trace(instance->traceopts, DIJKSTRA_BIT);
#endif

            /*Two way handshake check. Nbr-ship should be two way with nbr, even if nbr is PN. Do
             * not consider the node for SPF computation if we find 2-way nbrship is broken. */
            if(!is_two_way_nbrship(candidate_node, nbr_node, level) || 
                edge->status == 0){
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Two Way nbrship broken with nbr %s", nbr_node->node_name); 
                trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                continue;
            }

#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "Two Way nbrship verified with nbr %s",nbr_node->node_name); 
            trace(instance->traceopts, DIJKSTRA_BIT);
#endif
            if((unsigned long long)candidate_node->spf_metric[level] + (IS_OVERLOADED(candidate_node, level) 
                        ? (unsigned long long)INFINITE_METRIC : (unsigned long long)edge->metric[level]) < (unsigned long long)nbr_node->spf_metric[level]){

#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Old Metric : %u, New Metric : %u, Better Next Hop", 
                        nbr_node->spf_metric[level], IS_OVERLOADED(candidate_node, level) 
                        ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]);
                trace(instance->traceopts, DIJKSTRA_BIT);
#endif

                /*case 1 : if My own List is empty, and nbr is Pseuodnode , do nothing*/
                if(candidate_node == spf_root && nbr_node->node_type[level] == PSEUDONODE){
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "case 1 if I am root and and nbr is Pseuodnode , do nothing"); 
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                }
                /*case 2 : if My own List is empty, and nbr is Not a PN, then copy nbr's direct nh list to its own NH list*/
                if((candidate_node == spf_root && nbr_node->node_type[level] == NON_PSEUDONODE) || 
                        (candidate_node->node_type[level] == PSEUDONODE && is_all_nh_list_empty2(candidate_node, level))){

                    if(candidate_node == spf_root && nbr_node->node_type[level] == NON_PSEUDONODE)
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "case 2 if i am root, and nbr is Not a PN, then copy nbr's direct nh list to its own NH list");
                    else
                        sprintf(instance->traceopts->b, "case 2 if i am PN and all my nh list are empty");
#endif
                    trace(instance->traceopts, DIJKSTRA_BIT);

                    /*Drain all NH first*/
                    ITERATE_NH_TYPE_BEGIN(nh){
                        empty_nh_list(nbr_node, level, nh);
                    } ITERATE_NH_TYPE_END;

                    /*copy only appropriate direct mexthops to nexthops*/
                    nh = edge->etype == LSP ? LSPNH : IPNH;

#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Copying %s direct_next_hop %s %s to %s next_hop list", nbr_node->node_name, get_str_level(level), 
                            nh == IPNH ? "IPNH" : "LSPNH", nbr_node->node_name); 
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif

#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "printing %s direct_next_hop list at %s %s before copy", nbr_node->node_name, get_str_level(level),
                            nh == IPNH ? "IPNH" : "LSPNH"); 
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif

                    print_nh_list2(&nbr_node->direct_next_hop[level][nh][0]);
                    copy_nh_list2(&nbr_node->direct_next_hop[level][nh][0], &nbr_node->next_hop[level][nh][0]);
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "printing %s next_hop list at %s %s after copy", nbr_node->node_name, get_str_level(level),
                            nh == IPNH ? "IPNH" : "LSPNH"); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    print_nh_list2(&nbr_node->next_hop[level][nh][0]);
                }
                /*case 3 : if My own List is not empty, then nbr should inherit my next hop list*/
                else if(!is_all_nh_list_empty2(candidate_node, level)){

                    ITERATE_NH_TYPE_BEGIN(nh){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "case 3 if My own List is not empty, then nbr should inherit my next hop list"); 
                        trace(instance->traceopts, DIJKSTRA_BIT);
#endif
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Copying %s next_hop list %s %s to %s next_hop list", candidate_node->node_name, get_str_level(level), 
                                nh == IPNH ? "IPNH" : "LSPNH", nbr_node->node_name); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                        copy_nh_list2(&candidate_node->next_hop[level][nh][0], &nbr_node->next_hop[level][nh][0]);
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "printing %s next_hop list at %s %s after copy", nbr_node->node_name, get_str_level(level),
                                nh == IPNH ? "IPNH" : "LSPNH"); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                        print_nh_list2(&nbr_node->next_hop[level][nh][0]);
                        ITERATE_NH_TYPE_END;
                    }
                }

                nbr_node->spf_metric[level] =  IS_OVERLOADED(candidate_node, level) ? 
                    INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]; 
                nbr_node->lsp_metric[level] =  IS_OVERLOADED(candidate_node, level) ? 
                    INFINITE_METRIC : candidate_node->lsp_metric[level] + edge->metric[level];

#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "%s's spf_metric has been updated to %u",  
                        nbr_node->node_name, nbr_node->spf_metric[level]); trace(instance->traceopts, DIJKSTRA_BIT);
#endif

                if(nbr_node->is_node_on_heap == FALSE){
                    SPF_INSERT_NODE_INTO_CANDIDATE_TREE(ctree, nbr_node, level);
                    nbr_node->is_node_on_heap = TRUE;
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "%s inserted into candidate tree", nbr_node->node_name); 
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                }
                else{
                    /* We should remove the node and then add again into candidate tree
                     * But now i dont have brain cells to do this useless work. It has impact
                     * on performance, but not on output*/
                    SPF_CANDIDATE_TREE_NODE_REFRESH(ctree, nbr_node, level);
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "%s is already present in candidate tree", nbr_node->node_name); 
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                }
            }

            else if((unsigned long long)candidate_node->spf_metric[level] + (IS_OVERLOADED(candidate_node, level) 
                        ? (unsigned long long)INFINITE_METRIC : (unsigned long long)edge->metric[level]) == (unsigned long long)nbr_node->spf_metric[level]){

#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Old Metric : %u, New Metric : %u, ECMP path",
                        nbr_node->spf_metric[level], IS_OVERLOADED(candidate_node, level) 
                        ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]); 
                trace(instance->traceopts, DIJKSTRA_BIT);
#endif

                /*We should do two things here :
                 * 1. union of nexthops (IPNH and LSPNH)
                 * 2. if direct NH is present, then merge it into IPNH or LSPNH depeneding on direct NH type
                 * Help : See pseudonode_ecmp_topo() for detail
                 * */
                ITERATE_NH_TYPE_BEGIN(nh){

#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Union next_hop of %s %s at %s %s", candidate_node->node_name, 
                            nbr_node->node_name, get_str_level(level), 
                            nh == IPNH ? "IPNH" : "LSPNH"); trace(instance->traceopts, DIJKSTRA_BIT);
#endif

                    union_nh_list2(&candidate_node->next_hop[level][nh][0]  , &nbr_node->next_hop[level][nh][0]);
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "next_hop of %s at %s %s after Union", nbr_node->node_name,
                            get_str_level(level), nh == IPNH ? "IPNH" : "LSPNH"); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    print_nh_list2(&nbr_node->next_hop[level][nh][0]);
                    
                    if(nbr_node->is_node_on_heap == FALSE){
                        SPF_INSERT_NODE_INTO_CANDIDATE_TREE(ctree, nbr_node, level);
                        nbr_node->is_node_on_heap = TRUE;
#ifdef __ENABLE_TRACE__                    
                        sprintf(instance->traceopts->b, "%s inserted into candidate tree", nbr_node->node_name); 
                        trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    }
                } ITERATE_NH_TYPE_END;
                    
                /* If we reach a node D via PN Or Source S later with same cost, then direct nexthops also
                 * need to be added to nexthop list of D. See topo build_ecmp_topo2 for Detail*/
                nh = edge->etype == LSP ? LSPNH : IPNH;

                if(is_nh_list_empty2(&candidate_node->next_hop[level][nh][0])){
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Union direct_next_hop of %s with Next hop of %s at %s %s", nbr_node->node_name, 
                            nbr_node->node_name, get_str_level(level), 
                            nh == IPNH ? "IPNH" : "LSPNH"); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    union_direct_nh_list2(&nbr_node->direct_next_hop[level][nh][0] , &nbr_node->next_hop[level][nh][0] );
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "next_hop of %s at %s %s after Union", nbr_node->node_name,
                            get_str_level(level), nh == IPNH ? "IPNH" : "LSPNH"); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    print_nh_list2(&nbr_node->next_hop[level][nh][0]);
                }
            }
            else{
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Old Metric : %u, New Metric : %u, Not a Better Next Hop",
                        nbr_node->spf_metric[level], IS_OVERLOADED(candidate_node, level) 
                        ? INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]);
                trace(instance->traceopts, DIJKSTRA_BIT);
#endif
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

   singly_ll_node_t *list_node = NULL,
                    *list_node1 = NULL; 

   spf_result_t *result = NULL;
   self_spf_result_t *self_result = NULL;
   nh_type_t nh;

   ITERATE_LIST_BEGIN(spf_root->spf_run_result[level], list_node){

       result = list_node->data;
        
       self_result = singly_ll_search_by_key(
                    result->node->self_spf_result[level], 
                    spf_root);
        
       if(self_result){
           singly_ll_delete_node_by_data_ptr(
                   result->node->self_spf_result[level],
                   self_result);

           XFREE(self_result);
           self_result = NULL;
       }
        
       XFREE(result);
       result = NULL;    
   }ITERATE_LIST_END;
   delete_singly_ll(spf_root->spf_run_result[level]);
}

void
spf_init(candidate_tree_t *ctree, 
         node_t *spf_root, 
         LEVEL level, spf_type_t spf_type){

    /*step 1 : Purge NH list of all nodes in the topo*/

    unsigned int i = 0;
    node_t *nbr_node = NULL,
           *curr_node = NULL,
           *pn_node = NULL;

    edge_t *edge = NULL, *pn_edge = NULL;
    nh_type_t nh;
    /*Drain off results list for level */

    if(spf_type != TILFA_RUN){
        spf_clear_result(spf_root, level);
    }

    /* You should intialize the nxthops and direct nxthops only for 
     * reachable routers to spf root in the same level, not the entire
     * graph.*/

    Queue_t *q = initQ();
    init_instance_traversal(instance);
    spf_root->traversing_bit = 1;

    /*step 1 :Initialize spf root*/

    ITERATE_NH_TYPE_BEGIN(nh){

        for(i = 0; i < MAX_NXT_HOPS; i++){
            init_internal_nh_t(spf_root->next_hop[level][nh][i]);
            init_internal_nh_t(spf_root->direct_next_hop[level][nh][i]);
        }
    }ITERATE_NH_TYPE_END;

    spf_root->spf_metric[level] = 0;
    spf_root->lsp_metric[level] = 0;

    /*step 2 : Initialize the entire level graph*/
    enqueue(q, spf_root);

    while(!is_queue_empty(q)){

        curr_node = deque(q);
        ITERATE_NODE_LOGICAL_NBRS_BEGIN(curr_node, nbr_node, edge, level){
            
            if(nbr_node->traversing_bit)
                continue;

            ITERATE_NH_TYPE_BEGIN(nh){

                for(i = 0; i < MAX_NXT_HOPS; i++){
                    init_internal_nh_t(nbr_node->next_hop[level][nh][i]);
                    init_internal_nh_t(nbr_node->direct_next_hop[level][nh][i]);
                }
            } ITERATE_NH_TYPE_END;
            
            nbr_node->spf_metric[level] = INFINITE_METRIC;
            nbr_node->lsp_metric[level] = INFINITE_METRIC;

            nbr_node->traversing_bit = 1;
            enqueue(q, nbr_node);
        }
        ITERATE_NODE_LOGICAL_NBRS_END;
    }
    assert(is_queue_empty(q));
    XFREE(q);
    q = NULL;

    /* step 3 : Initialize direct nexthops.
     * Iterate over real physical nbrs of root (that is skip PNs)
     * and initialize their direct next hop list. Also, pls note that
     * directly PN's nbrs are also direct next hops to root. In Production
     * code, root has a separate list of directly connected physical real
     * nbrs. In our case, we dont have such list, hence, altenative is treat
     * nbrs of directly connected PN as own nbrs, which is infact the concept
     * of pseudonode. Again, do not compute direct next hops of PN*/

    unsigned int direct_nh_min_metric = 0,
                 nh_index = 0;

    ITERATE_NODE_PHYSICAL_NBRS_BEGIN(spf_root, nbr_node, pn_node, edge, pn_edge, level){

        if(tilfa_is_link_pruned(edge) || 
           tilfa_is_node_pruned(pn_node)){
            
            ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(spf_root, nbr_node, pn_node, level);
        }

        if(is_nh_list_empty2(&nbr_node->direct_next_hop[level][IPNH][0]) &&
                is_nh_list_empty2(&nbr_node->direct_next_hop[level][LSPNH][0])){
            if(edge->etype == LSP){
                build_mpls_nexthop_from_lsp(&spf_root->spf_info, &nbr_node->direct_next_hop[level][LSPNH][0], (&edge->from)->intf_name, level); 
            }
            else{
                intialize_internal_nh_t(nbr_node->direct_next_hop[level][IPNH][0], level, edge, nbr_node);
                set_next_hop_gw_pfx(nbr_node->direct_next_hop[level][IPNH][0], pn_edge->to.prefix[level]->prefix);
            }
            ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(spf_root, nbr_node, pn_node, level);
        }

        direct_nh_min_metric = !is_nh_list_empty2(&nbr_node->direct_next_hop[level][IPNH][0]) ? 
                               get_direct_next_hop_metric(nbr_node->direct_next_hop[level][IPNH][0], level) : 
                               get_direct_next_hop_metric(nbr_node->direct_next_hop[level][LSPNH][0], level);

        if(edge->metric[level] < direct_nh_min_metric){
            ITERATE_NH_TYPE_BEGIN(nh){
                empty_nh_list(nbr_node, level, nh);
                clear_spf_predecessors(&nbr_node->pred_lst[level][nh]);
            } ITERATE_NH_TYPE_END;
            if(edge->etype == LSP){
                build_mpls_nexthop_from_lsp(&spf_root->spf_info, &nbr_node->direct_next_hop[level][LSPNH][0], (&edge->from)->intf_name, level);
            }
            else{
                intialize_internal_nh_t(nbr_node->direct_next_hop[level][IPNH][0], level, edge, nbr_node);
                set_next_hop_gw_pfx(nbr_node->direct_next_hop[level][IPNH][0], pn_edge->to.prefix[level]->prefix);
            }
            ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(spf_root, nbr_node, pn_node, level);
        }

        if(edge->metric[level] == direct_nh_min_metric){
            nh = edge->etype == UNICAST ? IPNH : LSPNH;
            nh_index = get_nh_count(&nbr_node->direct_next_hop[level][nh][0]);
            
            if(nh_index == MAX_NXT_HOPS){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(spf_root, nbr_node, pn_node, level);
            }
            
            if(edge->etype == LSP){
                build_mpls_nexthop_from_lsp(&spf_root->spf_info, &nbr_node->direct_next_hop[level][LSPNH][nh_index], (&edge->from)->intf_name, level);
            }
            else{
                intialize_internal_nh_t(nbr_node->direct_next_hop[level][IPNH][nh_index], level, edge, nbr_node);
                set_next_hop_gw_pfx(nbr_node->direct_next_hop[level][IPNH][nh_index], pn_edge->to.prefix[level]->prefix);
            }
            ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(spf_root, nbr_node, pn_node, level);
        }
    } ITERATE_NODE_PHYSICAL_NBRS_END(spf_root, nbr_node, pn_node, level);


    /* Step 4 : Initialize candidate tree with root*/
    SPF_INSERT_NODE_INTO_CANDIDATE_TREE(ctree, spf_root, level);
    spf_root->is_node_on_heap = TRUE;

    /* Step 5 : Link Directly Connected PN to the instance root. This
     * will help identifying the right oif when spf_root is connected to PN */

    ITERATE_NODE_LOGICAL_NBRS_BEGIN(spf_root, nbr_node, edge, level){

        if(nbr_node->node_type[level] == PSEUDONODE){
            nbr_node->pn_intf[level] = &edge->from;/*There is exactly one PN per LAN per level*/            
        }
    }
    ITERATE_NODE_LOGICAL_NBRS_END;
}

void
spf_only_intitialization(node_t *spf_root, LEVEL level){

    if(level != LEVEL1 && level != LEVEL2){
        printf("%s() : Error : invalid level specified\n", __FUNCTION__);
        return;
    }

    SPF_RE_INIT_CANDIDATE_TREE(&instance->ctree);
    spf_init(&instance->ctree, spf_root, level, FULL_RUN);
}

static void
compute_backup_routine(node_t *spf_root, LEVEL level){

    unsigned int i = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;
    singly_ll_node_t *list_node = NULL;
    node_t *res_node = NULL;

    if(!IS_BIT_SET(spf_root->backup_spf_options, SPF_BACKUP_OPTIONS_ENABLED))
        return;

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Begin SPF back up calculation"); 
    trace(instance->traceopts, SPF_EVENTS_BIT); 
#endif
    boolean strict_down_stream_lfa = FALSE;
    init_back_up_computation(spf_root, level); 

    /* 1. Run SPF on S to know DIST(S,D) */
    Compute_and_Store_Forward_SPF(spf_root, level);
    /* 2. Run SPF on all nbrs of S to know DIST(N,D) and DIST(N,S)*/
    Compute_PHYSICAL_Neighbor_SPFs(spf_root, level);

    /*Weed out the nodes which do not need any backup support because they
     * are blessed with independant ECMP primary nexthops*/

    ITERATE_LIST_BEGIN(spf_root->spf_run_result[level], list_node){
        res_node = ((spf_result_t *)list_node->data)->node;
        is_independant_primary_next_hop_list_for_nodes(spf_root, res_node, level);
    } ITERATE_LIST_END;

    for(i = 0; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = spf_root->edges[i];
        if(!edge_end) break;
        
        if(IS_BIT_SET(edge_end->edge_config_flags, NO_ELIGIBLE_BACK_UP))
            continue;
        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        if(!edge->status) continue;
        if(edge->etype == LSP)
            continue;
        if(!IS_LINK_NODE_PROTECTION_ENABLED(edge) &&
            !IS_LINK_PROTECTION_ENABLED(edge))
            continue;
       strict_down_stream_lfa = TRUE;

       compute_lfa(spf_root, edge, level, strict_down_stream_lfa);
       
       if(!IS_BIT_SET(spf_root->backup_spf_options, 
            SPF_BACKUP_OPTIONS_REMOTE_BACKUP_CALCULATION))
           continue;
       
       /*clear pq nodes here as pq nodes are computed aftresh for each protected link*/
       clear_pq_nodes(spf_root, level);

       if(is_broadcast_link(edge, level) == FALSE){
           p2p_compute_link_node_protecting_extended_p_space(spf_root, edge, level);
           p2p_filter_select_pq_nodes_from_ex_pspace(spf_root, edge, level);
       }
       else{
           broadcast_compute_link_node_protecting_extended_p_space(spf_root, edge, level);
           broadcast_filter_select_pq_nodes_from_ex_pspace(spf_root, edge, level);
       }
    }
#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "END of SPF back up calculation"); 
    trace(instance->traceopts, SPF_EVENTS_BIT);
#endif
}

void
spf_computation(node_t *spf_root, 
                spf_info_t *spf_info, 
                LEVEL level, spf_type_t spf_type,
                ll_t *res_lst/*output list*/){

    if(level != LEVEL1 && level != LEVEL2){
        printf("%s() : Error : invalid level specified\n", __FUNCTION__);
        return;
    }

    if(IS_OVERLOADED(spf_root, level)){
        printf("%s(): INFO : Node %s is overloaded, SPF cannot be run\n", 
            __FUNCTION__, spf_root->node_name);
        return;
    }

    /*output list provided must be empty by the caller*/
    if(res_lst){
        assert(is_singly_ll_empty(res_lst));
    }

    /*All Tilfa runs must fetch the output in the separate
     * list. This seggregate the TILFA results from rest of 
     * the application code*/
    if(spf_type == TILFA_RUN && !res_lst){
        assert(0);
    }
#if 0
    if(level == LEVEL2 && spf_root->spf_info.spf_level_info[LEVEL1].version == 0){
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Root : %s, Running first LEVEL1 full SPF run before LEVEL2 full SPF run", 
                        spf_root->node_name); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
        spf_computation(spf_root, &spf_root->spf_info, LEVEL1, FULL_RUN);      
    }
#endif
#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s, Triggered SPF run : %s, %s", 
                spf_root->node_name, spf_type == FULL_RUN ? "FULL_RUN" : "FORWARD_RUN",
                get_str_level(level)); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                 
    SPF_RE_INIT_CANDIDATE_TREE(&instance->ctree);

    spf_init(&instance->ctree, spf_root, level, spf_type);

    if(spf_type == FULL_RUN){
        spf_info->spf_level_info[level].version++;
        assert(!res_lst);
        res_lst = spf_root->spf_run_result[level];
        run_dijkastra(spf_root, level, &instance->ctree, spf_type, res_lst);
    }
    else if(spf_type == FORWARD_RUN){
        assert(!res_lst);
        res_lst = spf_root->spf_run_result[level];
        run_dijkastra(spf_root, level, &instance->ctree, spf_type, res_lst);
        return;
    }
    else if(spf_type == TILFA_RUN){
        assert(res_lst);
        run_dijkastra(spf_root, level, &instance->ctree, spf_type, res_lst);
        return;
    }

    /* Flush off backups from all nodes unconditionally 
     * otherwise they will be reflected in routes computed.*/ 
    init_back_up_computation(spf_root, level); 
    compute_backup_routine(spf_root, level);
    compute_tilfa(spf_root, level);
    /* Route Building After SPF computation*/
    /*We dont build routing table for reverse spf run*/
    if(spf_type == FULL_RUN){
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Route building starts After SPF FORWARD run"); 
        trace(instance->traceopts, DIJKSTRA_BIT);
#endif
        spf_postprocessing(spf_info, spf_root, level);
#if 0
        /*backup routine must not impact main spf computation*/
        compute_backup_routine(spf_root, level);
        spf_backup_postprocessing(spf_info, spf_root, level);
#endif
    }
}

static void
init_prc_run(node_t *spf_root, LEVEL level){

    spf_root->spf_info.spf_level_info[level].version++;
    spf_root->spf_info.spf_level_info[level].spf_type = PRC_RUN;
}


void
partial_spf_run(node_t *spf_root, LEVEL level){

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Root : %s, %s", spf_root->node_name, get_str_level(level)); trace(instance->traceopts, DIJKSTRA_BIT);
#endif
    
    if(spf_root->spf_info.spf_level_info[level].version == 0){
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Root : %s, %s. No full SPF run till now. Runnig ...", 
        spf_root->node_name, get_str_level(level)); 
        trace(instance->traceopts, DIJKSTRA_BIT);
#endif
        spf_computation(spf_root, &spf_root->spf_info, level, FULL_RUN, 0); 
        return;
    }

    init_prc_run(spf_root, level);
    compute_backup_routine(spf_root, level);
    spf_postprocessing(&spf_root->spf_info, spf_root, level);
    spf_root->spf_info.spf_level_info[level].spf_type = FULL_RUN;
    if(IS_BIT_SET(spf_root->backup_spf_options, SPF_BACKUP_OPTIONS_ENABLED)){
        /*Clean the result so that other nodes do not export these results into
         * their route calculation*/
        init_back_up_computation(spf_root, level);
    }
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
    
    spf_result_t *res = NULL;
    self_spf_result_t *self_res = NULL;

    if(X->node_type[_level] != PSEUDONODE &&
            Y->node_type[_level] != PSEUDONODE){
        res =  (GET_SPF_RESULT((&(X->spf_info)), Y, _level));
        if(!res)
            return INFINITE_METRIC;
        return res->spf_metric;
    }

    if(X->node_type[_level] == PSEUDONODE &&
            Y->node_type[_level] != PSEUDONODE){
        res =  (GET_SPF_RESULT((&(X->spf_info)), Y, _level));
        if(!res) 
            return INFINITE_METRIC;
        return res->spf_metric;
    }

    if(X->node_type[_level] != PSEUDONODE &&
            Y->node_type[_level] == PSEUDONODE){
        self_res = (self_spf_result_t *)(singly_ll_search_by_key(Y->self_spf_result[_level], X));
        if(!self_res) return INFINITE_METRIC;
        res = ((self_spf_result_t *)(singly_ll_search_by_key(Y->self_spf_result[_level], X)))->res;
        if(!res) return INFINITE_METRIC;
        return res->spf_metric;
    }
    assert(0);
}

/*ToDo : The current implementation is buggy, the RSVP LSP
 * NH must be imported from rib*/
bool_t
build_mpls_nexthop_from_lsp(spf_info_t *spf_info,
                            internal_nh_t *lspnh,
                            char *lsp_name, 
                            LEVEL level){

    node_t *spf_root = GET_SPF_INFO_NODE(spf_info, level);
    rsvp_tunnel_t *rsvp_tunnel = look_up_rsvp_tunnel(
                spf_root, lsp_name); 

    if(!rsvp_tunnel) return FALSE;

    memset(lspnh, 0, sizeof(internal_nh_t));
    lspnh->level = level;
    lspnh->oif = rsvp_tunnel->physical_oif;
    lspnh->protected_link = NULL;
    lspnh->node = rsvp_tunnel->egress_lsr;
    strncpy(lspnh->gw_prefix, rsvp_tunnel->gateway, PREFIX_LEN);
    lspnh->gw_prefix[PREFIX_LEN] = '\0';
    lspnh->nh_type = LSPNH;
    lspnh->lfa_type = NO_LFA;
    lspnh->proxy_nbr = 
        (GET_EGDE_PTR_FROM_FROM_EDGE_END(lspnh->oif))->to.node;
    lspnh->rlfa = NULL;
    lspnh->mpls_label_out[0] = rsvp_tunnel->rsvp_label;
    lspnh->stack_op[0] = PUSH;
    lspnh->root_metric = 0;
    lspnh->dest_metric = 0;
    lspnh->ref_count = 0;
    lspnh->is_eligible = TRUE;
    return TRUE;
}


