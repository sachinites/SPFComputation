/*
 * =====================================================================================
 *
 *       Filename:  rlfa.c
 *
 *    Description:  This file defined the routines to compute remote loop free alternates
 *
 *        Version:  1.0
 *        Created:  Thursday 07 September 2017 03:40:56  IST
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

#include <stdlib.h>
#include <stdio.h>
#include "rlfa.h"
#include "instance.h"
#include "spfutil.h"
#include "logging.h"
#include "bitsop.h"

extern instance_t *instance;

extern int
instance_node_comparison_fn(void *_node, void *input_node_name);

void
init_back_up_computation(node_t *S, LEVEL level){

   singly_ll_node_t* list_node = NULL;
   spf_result_t *res = NULL;
   
   ll_t *spf_result = S->spf_run_result[level];
   
   ITERATE_LIST_BEGIN(spf_result, list_node){
       
       res = (spf_result_t *)list_node->data;
       res->node->p_space_protection_type = 0;
       res->node->q_space_protection_type = 0;
   } ITERATE_LIST_END;
}


void
Compute_and_Store_Forward_SPF(node_t *spf_root,
                              LEVEL level){

    spf_computation(spf_root, &spf_root->spf_info, level, FORWARD_RUN);
}


void
Compute_PHYSICAL_Neighbor_SPFs(node_t *spf_root, LEVEL level){

    
    node_t *nbr_node = NULL;
    edge_t *edge1 = NULL, *edge2 = NULL;

    ITERATE_NODE_PHYSICAL_NBRS_BEGIN2(spf_root, nbr_node, edge1, edge2, level){

        Compute_and_Store_Forward_SPF(nbr_node, level);
    }
    ITERATE_NODE_PHYSICAL_NBRS_END2(spf_root, nbr_node, level);
}

void
Compute_LOGICAL_Neighbor_SPFs(node_t *spf_root, LEVEL level){

    
    node_t *nbr_node = NULL;
    edge_t *edge1 = NULL;

    ITERATE_NODE_LOGICAL_NBRS_BEGIN(spf_root, nbr_node, edge1, level){

        Compute_and_Store_Forward_SPF(nbr_node, level);
    }
    ITERATE_NODE_LOGICAL_NBRS_END;
}


/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the p-space of 'node' wrt to 'edge'.
 *  Note that, 'edge' need not be directly connected edge of 'node'
 *-----------------------------------------------------------------------------*/

p_space_set_t 
p2p_compute_p_space(node_t *S, edge_t *failed_edge, LEVEL level){
    
    spf_result_t *res = NULL;
    singly_ll_node_t* list_node = NULL;
    node_t *E = NULL;

    unsigned int dist_E_to_y = 0,
                 dist_S_to_y = 0,
                 dist_S_to_E = 0;
    
    assert(!is_broadcast_link(failed_edge, level));

    E = failed_edge->to.node;
    Compute_and_Store_Forward_SPF(S, level);
    Compute_and_Store_Forward_SPF(E, level);

    ll_t *spf_result = S->spf_run_result[level];

    p_space_set_t p_space = init_singly_ll();
    singly_ll_set_comparison_fn(p_space, instance_node_comparison_fn);

    /*Iterate over spf_result and filter out all those nodes whose next hop is 
     * failed_edge->to.node */

    dist_S_to_E = DIST_X_Y(S, E , level);

    ITERATE_LIST_BEGIN(spf_result, list_node){
        
        res = (spf_result_t *)list_node->data;

        /*Do not add computing node itself*/
        if(res->node == S)
            continue;
        
        /* RFC 7490 - section 5.4*/
        /* Do not pick up overloaded node as a p-node*/

        if(IS_OVERLOADED(res->node, level))
            continue;

        dist_E_to_y = DIST_X_Y(E, res->node , level);
        dist_S_to_y = DIST_X_Y(S, res->node , level);

        if(dist_S_to_y < dist_S_to_E + dist_E_to_y)
            singly_ll_add_node_by_val(p_space, res->node);

    }ITERATE_LIST_END;
    
    return p_space;
}   


/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the extended p-space of 'node' wrt to
 *  'edge'. Note that, 'edge' need not be directly connected edge of 'node'.
 *-----------------------------------------------------------------------------*/
p_space_set_t
p2p_compute_link_node_protecting_extended_p_space(node_t *S, edge_t *failed_edge, LEVEL level){

    /*Compute p space of all nbrs of node except failed_edge->to.node
     * and union it. Remove duplicates from union*/

    node_t *nbr_node = NULL,
           *E = NULL; 
    edge_t *edge1 = NULL, *edge2 = NULL;
    p_space_set_t ex_p_space = NULL;
    singly_ll_node_t *list_node1 = NULL;

    /* self - computing router*/
    unsigned int d_nbr_to_y = 0,
                 d_nbr_to_S = 0,
                 d_nbr_to_E = 0,
                 d_E_to_y = 0,
                 d_S_to_y = 0,
                 d_S_to_E = 0,
                 d_S_to_nbr = 0,
                 d_E_to_nbr = 0;

    spf_result_t *spf_result_y = NULL;

    if(!IS_LEVEL_SET(failed_edge->level, level))
        return NULL;

    assert(!is_broadcast_link(failed_edge, level));

    ex_p_space = init_singly_ll();
    singly_ll_set_comparison_fn(ex_p_space, instance_node_comparison_fn);

    boolean is_node_protection_enabled = 
        IS_LINK_NODE_PROTECTION_ENABLED(failed_edge);

    boolean is_link_protection_enabled = 
        IS_LINK_PROTECTION_ENABLED(failed_edge);

    /*run spf on self*/
    Compute_and_Store_Forward_SPF(S, level); 

    /*Run SPF on all logical nbrs of S*/
    Compute_PHYSICAL_Neighbor_SPFs(S, level);

    /*iterate over entire network. Note that node->spf_run_result list
     * carries all nodes of the network reachable from source at level l.
     * We deem this list as the "entire network"*/ 

    E = failed_edge->to.node;

    d_S_to_E = DIST_X_Y(S, E, level);

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node1){

        spf_result_y = (spf_result_t *)list_node1->data;

        if(spf_result_y->node == S)
            continue;

        if(IS_OVERLOADED(spf_result_y->node, level))
            continue;

        d_S_to_y = spf_result_y->spf_metric;

        ITERATE_NODE_PHYSICAL_NBRS_BEGIN2(S, nbr_node, edge1, edge2, level){

            /*skip E */
            if(edge1 == failed_edge){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, nbr_node, level);
            }

            /*RFC 7490 section 5.4 : skip neighbors in computation of PQ nodes(extended p space) 
             * which are either overloaded or reachable through infinite metric*/

            if(edge1->metric[level] >= INFINITE_METRIC){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, nbr_node, level);
            }

            if(IS_OVERLOADED(nbr_node, level)){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, nbr_node, level);
            }

            /* RFC : Remote-LFA Node Protection and Manageability
             * draft-ietf-rtgwg-rlfa-node-protection-13 - section 2.2.1*/

            d_S_to_nbr = DIST_X_Y(S, nbr_node, level);
            d_E_to_nbr = DIST_X_Y(E, nbr_node, level);

            /*Testing ECMP equality : Nbr should not be reachable from S through ECMP path
             * which traverses S---E link*/
            sprintf(LOG, "Node : %s : Testing ECMP inequality For nbr %s from Source node %s : d_S_to_nbr(%u) < d_S_to_E(%u) + d_E_to_nbr(%u)",
                            S->node_name, nbr_node->node_name, S->node_name, d_S_to_nbr, d_S_to_E, d_E_to_nbr); TRACE();

            
            if(!(d_S_to_nbr < (d_S_to_E + d_E_to_nbr))){

                sprintf(LOG, "Node : %s : Nbr node %s is skipped because it traverses the failed edge from %s", 
                        S->node_name, nbr_node->node_name, S->node_name); TRACE();
                sprintf(LOG, "Above ECMP inequality failed for nbr %s", nbr_node->node_name); TRACE();
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, nbr_node, level);
            }

            sprintf(LOG, "Above ECMP inequality passed"); TRACE();

            d_nbr_to_y = DIST_X_Y(nbr_node, spf_result_y->node, level);

            if(is_node_protection_enabled == TRUE){

                /*In case of link protection, we skip S---E link but not E, 
                 * in case of node protection we skip E altogether*/

                if(nbr_node == E){
                   ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, nbr_node, level);
                }

                /*If node protection is enabled*/
                d_nbr_to_E = DIST_X_Y(nbr_node, E, level);
                d_E_to_y = DIST_X_Y(E, spf_result_y->node, level);

                /*condition for node protection RLFA - RFC : 
                 * draft-ietf-rtgwg-rlfa-node-protection-13 - section 2.2.6.2*/

                sprintf(LOG, "Node : %s : Testing node protection inequality : S = %s, nbr = %s, Y = %s, E = %s",
                                S->node_name, S->node_name, nbr_node->node_name, spf_result_y->node->node_name, E->node_name); TRACE();

                sprintf(LOG, "Node : %s : d_nbr_to_y(%u) < d_nbr_to_E(%u) + d_E_to_y(%u)", 
                            S->node_name, d_nbr_to_y, d_nbr_to_E, d_E_to_y); TRACE();

                if(d_nbr_to_y < (d_nbr_to_E + d_E_to_y)){

                    sprintf(LOG, "Node : %s : Above inequality passed", S->node_name); TRACE();
                    /*Node has been added to extended p-space, no need to check for link protection
                     * as node-protecting node in extended pspace is automatically link protecting node for P2P links*/
                    
                    if(IS_BIT_SET(spf_result_y->node->p_space_protection_type, LINK_PROTECTION)){
                        sprintf(LOG, "Node : %s : P node %s promoted from LINK_PROTECTION to LINK_NODE_PROTECTION through nbr %s", 
                            S->node_name, spf_result_y->node->node_name, nbr_node->node_name); TRACE();
                    }
                    else{
                        sprintf(LOG, "Node : %s : P node %s status set to LINK_NODE_PROTECTION through nbr  %s",
                            S->node_name, spf_result_y->node->node_name, nbr_node->node_name); TRACE();
                        singly_ll_add_node_by_val(ex_p_space, spf_result_y->node);
                    }
                    
                    SET_BIT(spf_result_y->node->p_space_protection_type, LINK_NODE_PROTECTION);
                    ITERATE_NODE_PHYSICAL_NBRS_BREAK2(S, nbr_node, level); /* We are done with spf_result_y->node, check for next*/
                }else{
                    sprintf(LOG, "Node : %s : Above inequality failed", S->node_name); TRACE();
                }
            }

            /*Apply RFC 5286 Inequality 1 - loop free*/
            if(is_link_protection_enabled == TRUE){
                 
                /*Only link protection is enabled*/
                d_nbr_to_S = DIST_X_Y(nbr_node, S, level);

                sprintf(LOG, "Node : %s : Checking link protection inequality: S = %s, nbr = %s, Y = %s",
                             S->node_name, S->node_name, nbr_node->node_name, spf_result_y->node->node_name); TRACE();

                if(d_nbr_to_y < (d_nbr_to_S + d_S_to_y)){
               
                    sprintf(LOG, "Node : %s : Above inequality passed",  S->node_name); TRACE();
                     
                    SET_BIT(spf_result_y->node->p_space_protection_type, LINK_PROTECTION);

                    sprintf(LOG, "Node : %s : P-space node %s status set to LINK_PROTECTION through nbr  %s",
                            S->node_name, spf_result_y->node->node_name, nbr_node->node_name); TRACE();
                    singly_ll_add_node_by_val(ex_p_space, spf_result_y->node);
                
                    /* IF node protection is not enabled, spf_result_y->node will never be promoted to
                     * node protecting PQ node through any nbr, hence, check for next node*/
                    if(is_node_protection_enabled == FALSE) break;
                }
                else{
                    sprintf(LOG, "Node : %s : Above inequality failed",  S->node_name); TRACE();
                }
            }
        } ITERATE_NODE_PHYSICAL_NBRS_END2(S, nbr_node, level);
    } ITERATE_LIST_END;
    return ex_p_space;
}   

/* Note : here node is E, not S*/
q_space_set_t
p2p_compute_q_space(node_t *node, edge_t *failed_edge, LEVEL level){

    node_t *S = NULL, *E = NULL;
    singly_ll_node_t *list_node1 = NULL;

    unsigned int d_E_to_S = 0,
                 d_S_to_y = 0,
                 d_E_to_y = 0;

    spf_result_t *spf_result_y = NULL;
    
    assert(!is_broadcast_link(failed_edge, level));

    q_space_set_t q_space = init_singly_ll();
    singly_ll_set_comparison_fn(q_space, instance_node_comparison_fn);

    E = node;
    S = failed_edge->from.node;

    /*Compute reverse SPF for nodes S and E as roots*/
    inverse_topology(instance, level);
    Compute_and_Store_Forward_SPF(S, level);
    Compute_and_Store_Forward_SPF(E, level);
    inverse_topology(instance, level);

    d_E_to_S = DIST_X_Y(E, S, level);

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node1){

        spf_result_y = (spf_result_t *)list_node1->data;

        if(spf_result_y->node == E) /*Do not add self*/
            continue;

        /*Another way of overloading the router is to set the outgoing
         * metric on all its edges = INFINITE_METRIC. Though, we dont follow
         * this mechanism of overloading the router in this project*/

        if(IS_OVERLOADED(spf_result_y->node, level))
            continue;

        /*Now find d_S_to_y */
        d_S_to_y = spf_result_y->spf_metric;

        /*Now find d_E_to_y */
        d_E_to_y = DIST_X_Y(E, spf_result_y->node, level);
        
        sprintf(LOG, "Testing Q space inequality E = %s, Y = %s, S = %s, d_E_to_y(%u) < d_S_to_y(%u) + d_E_to_S(%u)", 
                E->node_name, spf_result_y->node->node_name, S->node_name, d_E_to_y, d_S_to_y, d_E_to_S);TRACE();

        if(d_E_to_y < (d_S_to_y + d_E_to_S))
            singly_ll_add_node_by_val(q_space, spf_result_y->node);

    }ITERATE_LIST_END;
    return q_space;
}

pq_space_set_t
Intersect_Extended_P_and_Q_Space(p_space_set_t p_space, q_space_set_t q_space){

    singly_ll_node_t *p_list_node = NULL;
    node_t *p_node = NULL;

    pq_space_set_t pq_space = init_singly_ll();
    
    ITERATE_LIST_BEGIN(p_space, p_list_node){
        p_node = (node_t*)p_list_node->data;
        if(singly_ll_search_by_key(q_space, p_node->node_name)) 
            singly_ll_add_node_by_val(pq_space, p_node);
    }ITERATE_LIST_END;

    /* we dont need p_space and q_space anymore*/

    delete_singly_ll(p_space);
    free(p_space);

    delete_singly_ll(q_space);
    free(q_space);

    return pq_space;
}


static lfa_t *
broadcast_compute_link_node_protection_rlfas(node_t *S, 
                                  edge_t *protected_link,  
                                  LEVEL level,
                                  boolean strict_down_stream_lfa){

    return NULL;
}

static lfa_t *
p2p_compute_link_node_protection_rlfas(node_t *S, 
                                  edge_t *protected_link,  
                                  LEVEL level,
                                  boolean strict_down_stream_lfa){

    singly_ll_node_t *list_node = NULL,
    *list_node1 = NULL;

    node_t *pq_node = NULL,
           *D = NULL,
           *E = NULL;

    spf_result_t *D_res = NULL;

    unsigned int d_pq_to_D = 0,
                 d_S_to_D  = 0,
                 d_pq_to_E = 0,
                 d_E_to_D  = 0;


    lfa_dest_pair_t *lfa_dest_pair = NULL;
    edge_end_t *S_E_oif_for_D = NULL;
    edge_t *S_E_oif_edge_for_D = NULL;

    lfa_t *lfa = get_new_lfa();
    lfa->protected_link = &protected_link->from;

    assert(!is_broadcast_link(protected_link, level));

    p_space_set_t ex_p_space = p2p_compute_link_node_protecting_extended_p_space(S, protected_link, level);
    sprintf(LOG, "No of nodes in ex_p_space = %u", GET_NODE_COUNT_SINGLY_LL(ex_p_space)); TRACE();
    
    if(GET_NODE_COUNT_SINGLY_LL(ex_p_space) == 0){
        free_lfa(lfa);
        return NULL;
    }

    q_space_set_t q_space    = p2p_compute_q_space(protected_link->to.node, protected_link, level);
    sprintf(LOG, "No of nodes in q_space = %u", GET_NODE_COUNT_SINGLY_LL(q_space)); TRACE();
    
    if(GET_NODE_COUNT_SINGLY_LL(q_space) == 0){
        free_lfa(lfa);
        return NULL;
    }

    /*This PQ space will not contain overloaded nodes*/
    pq_space_set_t pq_space  = Intersect_Extended_P_and_Q_Space(ex_p_space, q_space);
    sprintf(LOG, "No of nodes in pq_space = %u", GET_NODE_COUNT_SINGLY_LL(pq_space)); TRACE();

    if(GET_NODE_COUNT_SINGLY_LL(pq_space) == 0){
        free_lfa(lfa);
        return NULL;
    }

    /*Avoid double free*/
    ex_p_space = NULL;
    q_space = NULL;

    ITERATE_LIST_BEGIN(pq_space, list_node){

        pq_node = (node_t *)list_node->data;
        Compute_and_Store_Forward_SPF(pq_node, level);

    } ITERATE_LIST_END;

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node){

        D_res = list_node->data;
        D = D_res->node;
        
        if(D == S) continue;

        E = D_res->next_hop[IPNH][0];

        /*Now if S reaches D via E through protected link, then D's LFA needs to be computed*/
        S_E_oif_for_D = get_min_oif(S, E, level, NULL, IPNH);

        sprintf(LOG, "Node : %s : OIF from node= %s to Nbr %s is %s",
                S->node_name,  S->node_name, E->node_name, S_E_oif_for_D->intf_name); TRACE();

        S_E_oif_edge_for_D = GET_EGDE_PTR_FROM_EDGE_END(S_E_oif_for_D);

        if(S_E_oif_edge_for_D != protected_link){
            sprintf(LOG, "Node : %s : Source(S) = %s, DEST(D) = %s, Primary NH(E) = NOT IMPACTED, skipping this Destination", 
                          S->node_name, S->node_name, D->node_name); TRACE();
            continue;
        }

        sprintf(LOG, "Node : %s : Source(S) = %s, DEST(D) = %s, Primary NH(E) = %s(IMPACTED)", 
                          S->node_name, S->node_name, D->node_name, E->node_name); TRACE();

        /*Examining all PQ nodes for Destination D for potential link protection RLFAs*/

        d_S_to_D = DIST_X_Y(S, D, level);

        ITERATE_LIST_BEGIN(pq_space, list_node1){

            pq_node = (node_t *)list_node1->data;
            d_pq_to_D = DIST_X_Y(pq_node, D, level);

            if(strict_down_stream_lfa){
                sprintf(LOG, "Node : %s : Testing Downsream inequality between PQ node = %s, and Dest = %s, d_pq_to_D(%u) < d_S_to_D(%u)", 
                             S->node_name, pq_node->node_name, D->node_name, d_pq_to_D, d_S_to_D); TRACE();

                if(d_pq_to_D < d_S_to_D){

                    lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
                    lfa_dest_pair->lfa_type = LINK_PROTECTION_RLFA_DOWNSTREAM;
                    lfa_dest_pair->lfa = pq_node;
                    lfa_dest_pair->oif_to_lfa = NULL;
                    lfa_dest_pair->dest = D;

                    /*Record the LFA*/
                    singly_ll_add_node_by_val(lfa->lfa, lfa_dest_pair);
                    
                    sprintf(LOG, "Node : %s : Downsream inequality qualified by PQ node %s for Dest %s", 
                                    S->node_name, pq_node->node_name, D->node_name); TRACE();

#if 0
                    /*promote the recorded LFA to LINK_AND_NODE_PROTECTION_RLFA if Node protection inequality meets*/
                    
                    sprintf(LOG, "Node : %s : Testing inequality for node protection on PQ node %s for Dest %s : \
                                   d_pq_to_D(%u) < d_pq_to_E(%u) + d_E_to_D(%u)", 
                                    S->node_name, pq_node->node_name, D->node_name, d_pq_to_D, d_pq_to_E, d_E_to_D); TRACE();

                    /* Here : E = protected_link->to.node */
                    d_pq_to_E = DIST_X_Y(pq_node, protected_link->to.node, level);
                    d_E_to_D = DIST_X_Y(protected_link->to.node, D, level);

                    if(d_pq_to_D < d_pq_to_E + d_E_to_D){
                        
                        lfa_dest_pair->lfa_type = LINK_AND_NODE_PROTECTION_RLFA;
                        sprintf(LOG, "Node : %s : Node protection inequality qualified, PQ node = %s promoted to %s for Dest %s", 
                            S->node_name, pq_node->node_name, get_str_lfa_type(lfa_dest_pair->lfa_type),  D->node_name); TRACE();
                    }
#endif
                }
            }
            else{
                /* no need to check downstream criterion, it improves the coverage, but the cost of loops*/
                sprintf(LOG, "Node : %s : Downsream inequality skipped for all PQ nodes", S->node_name); TRACE();
                lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
                lfa_dest_pair->lfa_type = LINK_PROTECTION_RLFA;
                lfa_dest_pair->lfa = pq_node;
                lfa_dest_pair->oif_to_lfa = NULL;
                lfa_dest_pair->dest = D;
                /*Record the LFA*/
                singly_ll_add_node_by_val(lfa->lfa, lfa_dest_pair);
                sprintf(LOG, "Node : %s : Downsream inequality qualified by PQ node %s for Dest %s", 
                        S->node_name, pq_node->node_name, D->node_name); TRACE();

                /*promote the recorded LFA to LINK_AND_NODE_PROTECTION_RLFA if Node protection inequality meets*/

                sprintf(LOG, "Node : %s : Testing inequality for node protection on PQ node %s for Dest %s : \
                        d_pq_to_D(%u) < d_pq_to_E(%u) + d_E_to_D(%u)", 
                        S->node_name, pq_node->node_name, D->node_name, d_pq_to_D, d_pq_to_E, d_E_to_D); TRACE();

                /* Here : E = protected_link->to.node */
                d_pq_to_E = DIST_X_Y(pq_node, protected_link->to.node, level);
                d_E_to_D = DIST_X_Y(protected_link->to.node, D, level);

                if(d_pq_to_D < d_pq_to_E + d_E_to_D){

                    lfa_dest_pair->lfa_type = LINK_AND_NODE_PROTECTION_RLFA;
                    sprintf(LOG, "Node : %s : Node protection inequality qualified, PQ node = %s promoted to %s for Dest %s", 
                            S->node_name, pq_node->node_name, get_str_lfa_type(lfa_dest_pair->lfa_type),  D->node_name); TRACE();
                }
            }
        } ITERATE_LIST_END;
    } ITERATE_LIST_END;

    if(GET_NODE_COUNT_SINGLY_LL(lfa->lfa) == 0){
        free_lfa(lfa);
        return NULL;
    }
    return lfa;
}

/*Routine to compute RLFA of node S, wrt to failed link 
 * failed_edge at given level for destination dest. Note that
 * to compute RLFA we need to know dest also, thus RLFAs are 
 * computed per destination (Need to check for LFAs)*/

void 
p2p_compute_rlfa_for_given_dest(node_t *S, LEVEL level, edge_t *failed_edge, node_t *dest){

    /* Run SPF for protecting node S*/

    singly_ll_node_t *list_node = NULL;
    node_t *pq_node = NULL;

    unsigned int d_pq_to_dest = 0,
                 d_S_to_dest = 0;

    assert(!is_broadcast_link(failed_edge, level));
    p_space_set_t ex_p_space = p2p_compute_p_space(S, failed_edge, level);
    q_space_set_t q_space = p2p_compute_q_space(S, failed_edge, level);

    pq_space_set_t pq_space = Intersect_Extended_P_and_Q_Space(ex_p_space, q_space);

    /*Avoid double free*/
    ex_p_space = NULL;
    q_space = NULL;

    /* Now we have PQ nodes in the list. All PQ nodes should be 
     * downstream wrt to S. So, we need to apply downstream constraint
     * on PQ nodes, and filter those which do not satisfy this constraint.
     * Note that, p nodes identified by compute_extended_p_space() need not
     * satisfy downstream criteria wrt S, but p nodes identified by compute_p_space()
     * surely does satisfy downstream criteria wrt S*/

    /* PQ nodes are, by definition, are those nodes in the network which relay the traffic 
     * from S to E through Path S->PQ_node->E while not traversing the link S-E. But what 
     * if E node failed itself (worst than prepared failure). In this case, PQ node will 
     * route the traffic to its own LFA/RLFA. So, we need to make sure that PQ node do not 
     * loop the traffic back to S in this case. Hence, PQ node should be downstream to S 
     * wrt to Destination.
     *
     * Obviously, LFA/RLFA of a pq-node will be downstream wrt to pq-node, but if pq-node is 
     * also downstream wrt to S, then certainly pq-nodes LFA/RLFA will also be downstream wrt 
     * to S, hence, in case of node failure E, pq node will never loop back the traffic to S through its alternate.
     *
     * p-space(S) guarantees that P nodes in p-space(S) are downstream nodes to S, but same is not true for extended-p-space(S).
     *
     * */
    
    d_S_to_dest = DIST_X_Y(S, dest, level);

    ITERATE_LIST_BEGIN(pq_space, list_node){

        pq_node = (node_t *)list_node->data;

        Compute_and_Store_Forward_SPF(pq_node, level);
       
        /* Check for D_opt(pq_node, dest) < D_opt(S, dest)*/

        d_pq_to_dest = DIST_X_Y(pq_node, dest, level);
         
        if(d_pq_to_dest < d_S_to_dest){

            printf("PQ node : %s\n", pq_node->node_name);
        }
    }ITERATE_LIST_END;
}

lfa_t *
get_new_lfa(){

    lfa_t *lfa = calloc(1, sizeof(lfa_t));
    lfa->protected_link = NULL;
    lfa->lfa = init_singly_ll();
    return lfa;
}

void
free_lfa(lfa_t *lfa){

    singly_ll_node_t *list_node = NULL;
    lfa_dest_pair_t *lfa_dest_pair = NULL;

    if(!lfa)
        return;
    ITERATE_LIST_BEGIN(lfa->lfa, list_node){
        lfa_dest_pair = list_node->data;
        free(lfa_dest_pair);
        lfa_dest_pair = NULL;
    } ITERATE_LIST_END;

    delete_singly_ll(lfa->lfa);
    free(lfa->lfa);
    lfa->lfa = NULL;
    free(lfa);
    lfa = NULL;
}

char *
get_str_lfa_type(lfa_type_t lfa_type){

    switch(lfa_type){

        case LINK_PROTECTION_LFA:
            return "LINK_PROTECTION_LFA";               /*Only inequality 1 - RFC5286*/
        case LINK_PROTECTION_LFA_DOWNSTREAM:
            return "LINK_PROTECTION_LFA_DOWNSTREAM";    /*Ineq 1 and 2 - RFC5286*/
        case LINK_AND_NODE_PROTECTION_LFA:
            return "LINK_AND_NODE_PROTECTION_LFA";      /*Ineq 1 2(Op) and 3 - RFC5286*/
        case BROADCAST_LINK_PROTECTION_LFA:
            return "BROADCAST_LINK_PROTECTION_LFA";     /*Ineq 1 2(Op) and 4 - RFC5286*/
        case BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM:   
            return "BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM";   /* Ineq 1 2(Op) and 4 - RFC5286*/
        case BROADCAST_ONLY_NODE_PROTECTION_LFA:
            return "BROADCAST_ONLY_NODE_PROTECTION_LFA";
        case BROADCAST_LINK_AND_NODE_PROTECTION_LFA:
            return "BROADCAST_LINK_AND_NODE_PROTECTION_LFA";
        case LINK_PROTECTION_RLFA:
            return "LINK_PROTECTION_RLFA";
        case LINK_PROTECTION_RLFA_DOWNSTREAM:
            return "LINK_PROTECTION_RLFA_DOWNSTREAM";
        case LINK_AND_NODE_PROTECTION_RLFA:
            return "LINK_AND_NODE_PROTECTION_RLFA";
        case BROADCAST_LINK_PROTECTION_RLFA:
            return "BROADCAST_LINK_PROTECTION_RLFA";
        case BROADCAST_LINK_PROTECTION_RLFA_DOWNSTREAM:
            return "BROADCAST_LINK_PROTECTION_RLFA_DOWNSTREAM";
        case BROADCAST_LINK_AND_NODE_PROTECTION_RLFA:
            return "BROADCAST_LINK_AND_NODE_PROTECTION_RLFA";
        default:
            return "UNKNOWN_LFA_TYPE";
    }
}


/* In case of LFAs, the LFA is promoted to Node protecting LFA if they
 * meet the node protecting criteria*/

static lfa_t *
broadcast_compute_link_node_protection_lfas(node_t * S, edge_t *protected_link, 
                           LEVEL level, 
                           boolean strict_down_stream_lfa){

     
    node_t *PN = NULL, 
           *N = NULL, 
           *D = NULL,
           *E = NULL;

    edge_t *edge1 = NULL, *edge2 = NULL,
            *S_E_oif_edge_for_D = NULL;

    spf_result_t *D_res = NULL;
    singly_ll_node_t *list_node = NULL;
    boolean LFA_reach_via_broadcast_link = FALSE;
    edge_end_t *S_E_oif_for_D = NULL;

    unsigned int dist_N_D  = 0, 
                 dist_N_S  = 0, 
                 dist_S_D  = 0,
                 dist_PN_D = 0,
                 dist_N_PN = 0,
                 dist_N_E  = 0,
                 dist_E_D  = 0;


    lfa_dest_pair_t *lfa_dest_pair = NULL;

    assert(is_broadcast_link(protected_link, level));

    lfa_t *lfa = get_new_lfa();
    lfa->protected_link = &protected_link->from;

    PN = protected_link->to.node;
    Compute_and_Store_Forward_SPF(PN, level);  
    
    ITERATE_NODE_PHYSICAL_NBRS_BEGIN2(S, N, edge1, edge2, level){

        if(edge1 == protected_link){
            ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
        }
        
        sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s edge2 = %s for LFA candidature",/*Strange : adding edge2 in log solves problem of this macro looping*/ 
                S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();
    
        if(IS_OVERLOADED(N, level)){
            sprintf(LOG, "Node : %s : Nbr %s failed for LFA candidature, reason - Overloaded", S->node_name, N->node_name); TRACE();
            ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
        }
      
       dist_N_S = DIST_X_Y(N, S, level);
       dist_N_PN =  DIST_X_Y(N, PN, level);

       if(is_broadcast_member_node(S, protected_link, N , level))
           LFA_reach_via_broadcast_link = TRUE;

       sprintf(LOG, "Node : %s : Source(S) = %s, probable LFA(N) = %s reachability via broadcast link : %s",
                S->node_name,  S->node_name, N->node_name, LFA_reach_via_broadcast_link ? "TRUE": "FALSE"); TRACE(); 

       ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node){
           
           D_res = list_node->data;
           D = D_res->node;

           if(D == S){
                 ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);       
           }

           if(D->node_type[level] == PSEUDONODE){
               ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
           }

           E = D_res->next_hop[IPNH][0];
           
           /*Now if S reaches D via E through protected link, then D's LFA needs to be computed*/
           S_E_oif_for_D = get_min_oif(S, E, level, NULL, IPNH);

           sprintf(LOG, "Node : %s : OIF from node= %s to Nbr %s is %s",  
            S->node_name,  S->node_name, E->node_name, S_E_oif_for_D->intf_name); TRACE();

           S_E_oif_edge_for_D = GET_EGDE_PTR_FROM_EDGE_END(S_E_oif_for_D);
           
           if(S_E_oif_edge_for_D != protected_link){
               sprintf(LOG, "Node : %s : Source(S) = %s, probable LFA(N) = %s, DEST(D) = %s, Primary NH(E) = NOT IMPACTED, skipping this Destination", 
                             S->node_name, S->node_name, N->node_name, D->node_name); TRACE();
               ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
           }

            sprintf(LOG, "Node : %s : Source(S) = %s, probable LFA(N) = %s, DEST(D) = %s, Primary NH(E) = %s(IMPACTED)", 
                            S->node_name, S->node_name, N->node_name, D->node_name, E->node_name); TRACE();


            dist_N_D = DIST_X_Y(N, D, level);
            dist_S_D = D_res->spf_metric;

            /* Apply inequality 1*/
            sprintf(LOG, "Node : %s : Testing inequality 1 : dist_N_D(%u) < dist_N_S(%u) + dist_S_D(%u)",
                    S->node_name, dist_N_D, dist_N_S, dist_S_D); TRACE();

            if(!(dist_N_D < dist_N_S + dist_S_D)){
                sprintf(LOG, "Node : %s : Inequality 1 failed", S->node_name); TRACE();
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
            }

            sprintf(LOG, "Node : %s : Inequality 1 passed", S->node_name); TRACE();

            dist_PN_D = DIST_X_Y(PN, D, level);

            
            sprintf(LOG, "Node : %s : Testing inequality 4 : dist_N_D(%u) < dist_N_PN(%u) + dist_PN_D(%u)",
                    S->node_name, dist_N_D, dist_N_PN, dist_PN_D); TRACE();

            /*Apply inequality 4*/
            if(dist_N_D < dist_N_PN + dist_PN_D){
                sprintf(LOG, "Node : %s : Inequality 4 passed, LFA created with %s", 
                    S->node_name, get_str_lfa_type(BROADCAST_LINK_PROTECTION_LFA)); TRACE();

                lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
                lfa_dest_pair->lfa_type = BROADCAST_LINK_PROTECTION_LFA;
                lfa_dest_pair->lfa = N;
                lfa_dest_pair->oif_to_lfa = &edge1->from;
                lfa_dest_pair->dest = D;

                /*Record the LFA*/
                singly_ll_add_node_by_val(lfa->lfa, lfa_dest_pair);

                /*Apply inequality 2*/

                /* Inequality 2 has been controlled by additon boolean argument because, inequality should be
                 * explicitely applied under administrator's control. Inequality 2 has following drawbacks :
                 * 1. It drastically reduces the LFA coverage
                 * 2. It helps in avoid microloops only when multiple link fails at the same time, What is the probablity when multiple link
                 * failures happen at the same time
                 * 3. It helps in avoid microloops when node completely fails, and in specific topologies
                 *
                 * I feel the drawbacks of this inequality weighs far more than benefits we get from it. We are not totally discarding this
                 * inequality from implementation, but giving explitely knob if admin wants to harness the advantages Or disadvantages
                 * of this inequality at his own will
                 * */

                if(strict_down_stream_lfa){
                    /* 4. Narrow down the subset further using inequality 2 */

                    sprintf(LOG, "Node : %s : Testing inequality 2 : dist_N_D(%u) < dist_S_D(%u)", 
                            S->node_name, dist_N_D, dist_S_D); TRACE();

                    if(!(dist_N_D < dist_S_D)){
                        sprintf(LOG, "Node : %s : Inequality 2 failed", S->node_name); TRACE();
                        ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
                    }

                    sprintf(LOG, "Node : %s : Inequality 2 passed, LFA promoted from %s to %s", S->node_name, 
                    get_str_lfa_type(lfa_dest_pair->lfa_type), get_str_lfa_type(BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM)); TRACE(); 
                    lfa_dest_pair->lfa_type = BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM;

                     
                    /* Inequality 3 : Node protecting LFA */
                    dist_N_E = DIST_X_Y(N, E, level);
                    dist_E_D = DIST_X_Y(E, D, level); 

                    sprintf(LOG, "Node : %s : Testing inequality 3 : dist_N_D(%u) < dist_N_E(%u) + dist_E_D(%u)",
                            S->node_name, dist_N_D, dist_N_E, dist_E_D); TRACE();

                    if(dist_N_D < dist_N_E + dist_E_D){
                        sprintf(LOG, "Node : %s : inequality 3 Passed, LFA promoted from %s to %s",
                                 S->node_name, get_str_lfa_type(lfa_dest_pair->lfa_type), 
                                 get_str_lfa_type(BROADCAST_LINK_AND_NODE_PROTECTION_LFA)); TRACE();
                        lfa_dest_pair->lfa_type = BROADCAST_LINK_AND_NODE_PROTECTION_LFA;
                    }else{
                        sprintf(LOG, "Node : %s : inequality 3 Failed", S->node_name); TRACE();
                    }
                    ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
                }
            }
            else{
                sprintf(LOG, "Node : %s : Inequality 4 failed", S->node_name); TRACE(); 
                if(strict_down_stream_lfa){
                    /* 4. Narrow down the subset further using inequality 2 */

                    sprintf(LOG, "Node : %s : Testing inequality 2 : dist_N_D(%u) < dist_S_D(%u)", 
                            S->node_name, dist_N_D, dist_S_D); TRACE();

                    if(!(dist_N_D < dist_S_D)){
                        sprintf(LOG, "Node : %s : Inequality 2 failed", S->node_name); TRACE();
                        ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
                    }

                    sprintf(LOG, "Node : %s : Inequality 2 passed", S->node_name); TRACE(); 

                    /* Inequality 3 : Node protecting LFA */
                    dist_N_E = DIST_X_Y(N, E, level);
                    dist_E_D = DIST_X_Y(E, D, level); 

                    sprintf(LOG, "Node : %s : Testing inequality 3 : dist_N_D(%u) < dist_N_E(%u) + dist_E_D(%u)",
                            S->node_name, dist_N_D, dist_N_E, dist_E_D); TRACE();

                    if(dist_N_D < dist_N_E + dist_E_D){
                        lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
                        lfa_dest_pair->lfa = N;
                        lfa_dest_pair->oif_to_lfa = &edge1->from;
                        lfa_dest_pair->dest = D;

                        lfa_dest_pair->lfa_type = LFA_reach_via_broadcast_link ? BROADCAST_ONLY_NODE_PROTECTION_LFA:BROADCAST_LINK_AND_NODE_PROTECTION_LFA; 

                        sprintf(LOG, "Node : %s : inequality 3 Passed, LFA created with type = %s",
                                S->node_name, get_str_lfa_type(lfa_dest_pair->lfa_type)); TRACE();
                    }else{
                        sprintf(LOG, "Node : %s : inequality 3 Failed", S->node_name); TRACE();
                        ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
                    }
                }
            }
       } ITERATE_LIST_END;
    }
    ITERATE_NODE_PHYSICAL_NBRS_END2(S, N, level); 
    
    if(GET_NODE_COUNT_SINGLY_LL(lfa->lfa) == 0){
        free_lfa(lfa);
        return NULL;
    }

    return lfa;
}


static lfa_t *
_broadcast_compute_link_node_protection_lfas(node_t * S, edge_t *protected_link, 
                            LEVEL level, 
                            boolean strict_down_stream_lfa){


    node_t *N = NULL, *PN = NULL;

    edge_t *edge1 = NULL, 
           *edge2 = NULL;

    unsigned int dist_N_D = 0, 
                 dist_N_S = 0, 
                 dist_S_D = 0,
                 dist_N_E = 0,
                 dist_E_D = 0,
                 nh_count = 0;

    ITERATE_NODE_PHYSICAL_NBRS_BEGIN2(S, N, edge1, edge2, level){

        printf("N = %s, %s, %s\n", 
            N->node_name, edge1->from.intf_name, edge2->from.intf_name);

        ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
        ITERATE_NODE_PHYSICAL_NBRS_BREAK2(S, N, level);

    } ITERATE_NODE_PHYSICAL_NBRS_END2(S, N, level);

    return NULL;

}

/* In case of LFAs, the LFA is promoted to Node protecting LFA if they
 * meet the node protecting criteria*/

static lfa_t *
p2p_compute_link_node_protection_lfas(node_t * S, edge_t *protected_link, 
                            LEVEL level, 
                            boolean strict_down_stream_lfa){

    node_t *E = NULL, 
    *N = NULL, 
    *D = NULL;

    edge_t *edge1 = NULL, *edge2 = NULL;
    spf_result_t *D_res = NULL;
    singly_ll_node_t *list_node = NULL;

    unsigned int dist_N_D = 0, 
                 dist_N_S = 0, 
                 dist_S_D = 0,
                 dist_N_E = 0,
                 dist_E_D = 0,
                 nh_count = 0;

    lfa_dest_pair_t *lfa_dest_pair = NULL;
    lfa_type_t lfa_type = UNKNOWN_LFA_TYPE;
     
    lfa_t *lfa = get_new_lfa(); 

    /* 3. Filter nbrs of S using inequality 1 */
    E = protected_link->to.node;

    lfa->protected_link = &protected_link->from;

      ITERATE_NODE_PHYSICAL_NBRS_BEGIN2(S, N, edge1, edge2, level){

        printf("N = %s, %s, %s\n", N->node_name, edge1->from.intf_name, edge2->from.intf_name);

        sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s, edge2 = %s for LFA candidature",/*Strange : adding edge2 in log solves problem of this macro looping*/ 
                S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();

        /*Do not consider the link being protected to find LFA*/
        if(N == E && edge1 == protected_link){
            sprintf(LOG, "Node : %s : Nbr %s with OIF %s is same as protected link %s, skipping this nbr from LFA candidature", 
                    S->node_name, N->node_name, edge1->from.intf_name, protected_link->from.intf_name); TRACE();
            ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
        }

        if(IS_OVERLOADED(N, level)){
            sprintf(LOG, "Node : %s : Nbr %s failed for LFA candidature, reason - Overloaded", S->node_name, N->node_name); TRACE();
            ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
        }

        dist_N_S = DIST_X_Y(N, S, level);
        ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node){

            D_res = list_node->data;
            D = D_res->node;

            if(D == S) {
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
            };

            sprintf(LOG, "Node : %s : Source(S) = %s, probable LFA(N) = %s, DEST(D) = %s, Primary NH(E) = %s", 
                    S->node_name, S->node_name, N->node_name, D->node_name, E->node_name); TRACE();


            /* If to reach D from S , primary nexthop is not E, then skip D*/
            if(!is_present(&D_res->next_hop[IPNH][0], E)){
                sprintf(LOG, "Node : %s : skipping Dest %s as %s is not its primary nexthop",
                        S->node_name, D->node_name, E->node_name); TRACE();
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
            }
            
            sprintf(LOG, "Node : %s : Dest %s IMPACTED", S->node_name, D->node_name); TRACE();

            dist_N_D = DIST_X_Y(N, D, level);
            dist_S_D = D_res->spf_metric;

            sprintf(LOG, "Node : %s : Testing inequality 1 : dist_N_D(%u) < dist_N_S(%u) + dist_S_D(%u)",
                    S->node_name, dist_N_D, dist_N_S, dist_S_D); TRACE();

            /* Apply inequality 1*/
            if(!(dist_N_D < dist_N_S + dist_S_D)){
                sprintf(LOG, "Node : %s : Inequality 1 failed", S->node_name); TRACE();
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
            }

            sprintf(LOG, "Node : %s : Inequality 1 passed", S->node_name); TRACE();

            /* Inequality 2 has been controlled by additon boolean argument because, inequality should be
             * explicitely applied under administrator's control. Inequality 2 has following drawbacks :
             * 1. It drastically reduces the LFA coverage
             * 2. It helps in avoid microloops only when multiple link fails at the same time, What is the probablity when multiple link
             * failures happen at the same time
             * 3. It helps in avoid microloops when node completely fails, and in specific topologies
             *
             * I feel the drawbacks of this inequality weighs far more than benefits we get from it. We are not totally discarding this
             * inequality from implementation, but giving explitely knob if admin wants to harness the advantages Or disadvantages
             * of this inequality at his own will
             * */

            if(strict_down_stream_lfa){
                /* 4. Narrow down the subset further using inequality 2 */

                sprintf(LOG, "Node : %s : Testing inequality 2 : dist_N_D(%u) < dist_S_D(%u)", 
                        S->node_name, dist_N_D, dist_S_D); TRACE();

                if(!(dist_N_D < dist_S_D)){
                    sprintf(LOG, "Node : %s : Inequality 2 failed", S->node_name); TRACE();
                    ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
                }

                sprintf(LOG, "Node : %s : Inequality 2 passed", S->node_name); TRACE(); 
            }

            lfa_type = strict_down_stream_lfa ? LINK_PROTECTION_LFA_DOWNSTREAM : LINK_PROTECTION_LFA; 

            /*ToDo : Node protection criteria should be checked for all primary nexthops*/
            if(IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
                
                /* Inequality 3 : Node protecting LFA */
                dist_N_E = DIST_X_Y(N, E, level);
                dist_E_D = DIST_X_Y(E, D, level); 

                sprintf(LOG, "Node : %s : Testing inequality 3 : dist_N_D(%u) < dist_N_E(%u) + dist_E_D(%u)",
                        S->node_name, dist_N_D, dist_N_E, dist_E_D); TRACE();

                if(dist_N_D < dist_N_E + dist_E_D){
                    lfa_type = LINK_AND_NODE_PROTECTION_LFA;  
                    sprintf(LOG, "Node : %s : inequality 3 Passed, Link protecting LFA promoted to node-protecting LFA",
                            S->node_name); TRACE();
                }else{
                    sprintf(LOG, "Node : %s : inequality 3 Failed", S->node_name); TRACE();
                }
            }

            nh_count = get_nh_count(&(D_res->next_hop[IPNH][0]));

            /*If Destination has multiple ECMP primary nexthops, no need to compute LFA for D which provides only
             * link protection*/
            if((nh_count > 1) && 
                    ((lfa_type == LINK_PROTECTION_LFA ||
                      lfa_type ==  LINK_PROTECTION_LFA_DOWNSTREAM))){

                        sprintf(LOG, "Node : %s : Dest %s has %u ECMP nexthops, protection type provided by nbr %s = %s,"
                            "This LFA is not recorded", S->node_name, D->node_name, nh_count, 
                            N->node_name, get_str_lfa_type(lfa_type)); TRACE();
                        ITERATE_NODE_PHYSICAL_NBRS_CONTINUE2(S, N, level);
             }

            lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
            lfa_dest_pair->lfa_type = lfa_type;
            lfa_dest_pair->lfa = N;
            lfa_dest_pair->oif_to_lfa = &edge1->from; 
            lfa_dest_pair->dest = D;

            /*Record the LFA*/ 
            singly_ll_add_node_by_val(lfa->lfa, lfa_dest_pair);
            sprintf(LOG, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s", N->node_name, 
                    lfa_dest_pair->oif_to_lfa->intf_name, D->node_name, 
                    get_str_lfa_type(lfa_dest_pair->lfa_type)); TRACE();

        } ITERATE_LIST_END;

        sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s edge2 = %s for LFA candidature Done", 
                S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();

    } ITERATE_NODE_PHYSICAL_NBRS_END2(S, N, level);

    if(GET_NODE_COUNT_SINGLY_LL(lfa->lfa) == 0){
        free_lfa(lfa);
        return NULL;
    }

    return lfa;
}


void
print_lfa_info(lfa_t *lfa){

    singly_ll_node_t *list_node = NULL;
    lfa_dest_pair_t *lfa_dest_pair = NULL;
    
    if(!lfa)
        return;
    printf("protected link : %s\n", lfa->protected_link->intf_name);
    printf("LFAs(LFA, D) : \n");
    ITERATE_LIST_BEGIN(lfa->lfa, list_node){

        lfa_dest_pair = list_node->data;
        printf("    LFA = %s, OIF = %s, Dest = %s, lfa_type = %s\n", lfa_dest_pair->lfa->node_name, 
        lfa_dest_pair->oif_to_lfa ? lfa_dest_pair->oif_to_lfa->intf_name : "Nil", 
        lfa_dest_pair->dest->node_name, get_str_lfa_type(lfa_dest_pair->lfa_type));
          
    } ITERATE_LIST_END;
    printf("\n");
}

void
clear_lfa_result(node_t *node){

    singly_ll_node_t *list_node = NULL,
                     *list_node1 = NULL;
    lfa_dest_pair_t *lfa_dest_pair = NULL;
    lfa_t *lfa = NULL;

    ITERATE_LIST_BEGIN(node->link_protection_lfas, list_node){

        lfa = list_node->data;

        ITERATE_LIST_BEGIN(lfa->lfa, list_node1){

            lfa_dest_pair = list_node1->data;
            free(lfa_dest_pair);
            lfa_dest_pair = NULL;
        } ITERATE_LIST_END;

        delete_singly_ll(lfa->lfa);
        free(lfa->lfa);
    } ITERATE_LIST_END;

    delete_singly_ll(node->link_protection_lfas);
    free(node->link_protection_lfas);
    node->link_protection_lfas = NULL;
}

lfa_t *
compute_lfa(node_t * S, edge_t *protected_link,
            LEVEL level,
            boolean strict_down_stream_lfa){

     /*LFA computation is possible only for unicast links*/
     assert(protected_link->etype == UNICAST);

    /* Run necessary SPF runs required to compute LFAs 
     * of any type - p2p link/node protection or broadcast link
     * node protecting LFAs*/

     /* 1. Run SPF on S to know DIST(S,D) */
     Compute_and_Store_Forward_SPF(S, level);
    
     /* 2. Run SPF on all nbrs of S to know DIST(N,D) and DIST(N,S)*/
     Compute_PHYSICAL_Neighbor_SPFs(S, level); 

     if(is_broadcast_link(protected_link, level) == FALSE)
         return p2p_compute_link_node_protection_lfas(S, protected_link, level, TRUE); 
     else
         return broadcast_compute_link_node_protection_lfas(S, protected_link, level, TRUE);
}

lfa_t *
compute_rlfa(node_t * S, edge_t *protected_link,
            LEVEL level,
            boolean strict_down_stream_lfa){

     /*R LFA computation is possible only for unicast links*/
     assert(protected_link->etype == UNICAST);
     
     init_back_up_computation(S, level);

     if(is_broadcast_link(protected_link, level) == FALSE)
         return p2p_compute_link_node_protection_rlfas(S, protected_link, level, TRUE); 
     else
         return broadcast_compute_link_node_protection_rlfas(S, protected_link, level, TRUE);
}

