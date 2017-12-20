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

extern instance_t *instance;

extern int
instance_node_comparison_fn(void *_node, void *input_node_name);

void
Compute_and_Store_Forward_SPF(node_t *spf_root,
                              LEVEL level){

    spf_computation(spf_root, &spf_root->spf_info, level, SKELETON_RUN);
}


void
Compute_Neighbor_SPFs(node_t *spf_root, LEVEL level){

    
    /*-----------------------------------------------------------------------------
     *   Iterate over all nbrs of S except E, and run SPF
     *-----------------------------------------------------------------------------*/

    node_t *nbr_node = NULL;
    edge_t *edge1 = NULL, *edge2 = NULL;

    ITERATE_NODE_PHYSICAL_NBRS_BEGIN(spf_root, nbr_node, edge1, edge2, level){

        Compute_and_Store_Forward_SPF(nbr_node, level);
    }
    ITERATE_NODE_PHYSICAL_NBRS_END;
}



/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the p-space of 'node' wrt to 'edge'.
 *  Note that, 'edge' need not be directly connected edge of 'node'
 *-----------------------------------------------------------------------------*/

p_space_set_t 
compute_p_space(node_t *node, edge_t *failed_edge, LEVEL level){
    
    spf_result_t *res = NULL;
    singly_ll_node_t* list_node = NULL;

    Compute_and_Store_Forward_SPF(node, level);
    ll_t *spf_result = node->spf_run_result[level];

    p_space_set_t p_space = init_singly_ll();
    singly_ll_set_comparison_fn(p_space, instance_node_comparison_fn);

    /*Iterate over spf_result and filter out all those nodes whose next hop is 
     * failed_edge->to.node */

    ITERATE_LIST_BEGIN( spf_result, list_node){
        
        res = (spf_result_t *)list_node->data;

        /*Skip all nodes in the nw which are reachable through S--E*/
         if(res->next_hop[IPNH][0] == failed_edge->to.node)
            continue;
        /*Do not add computing node itself*/
        if(res->node == node)
            continue;

        singly_ll_add_node_by_val(p_space, res->node);
    }ITERATE_LIST_END;
    
    return p_space;
}   


/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the extended p-space of 'node' wrt to
 *  'edge'. Note that, 'edge' need not be directly connected edge of 'node'.
 *-----------------------------------------------------------------------------*/
p_space_set_t
compute_extended_p_space(node_t *node, edge_t *failed_edge, LEVEL level){

    /*Compute p space of all nbrs of node except failed_edge->to.node
     * and union it. Remove duplicates from union*/

    node_t *nbr_node = NULL; 
    edge_t *edge = NULL;
    p_space_set_t ex_p_space = NULL;
    singly_ll_node_t *list_node1 = NULL, 
                     *list_node2 = NULL;

    unsigned int d_nbr_to_y = 0,
                 d_nbr_to_self = 0,
                 d_self_to_y = 0;

    spf_result_t *spf_result_y = NULL,
                 *spf_result_nbr = NULL;

    ex_p_space = init_singly_ll();
    singly_ll_set_comparison_fn(ex_p_space, instance_node_comparison_fn);

    /*run spf on self*/
    Compute_and_Store_Forward_SPF(node, level); 
    /*Run SPF on all nbrs of S except E*/
    Compute_Neighbor_SPFs(node, level);

    /*iterate over entire network. Note that node->spf_run_result list
     * carries all nodes of the network reachable from source at level l.
     * We deem this list as the "entire network"*/ 

    ITERATE_LIST_BEGIN(node->spf_run_result[level], list_node1){

        spf_result_y = (spf_result_t *)list_node1->data;

        d_self_to_y = spf_result_y->spf_metric;
        
        ITERATE_NODE_NBRS_BEGIN(node, nbr_node, edge, level){

            /*skip E */
            if(edge == failed_edge)
                continue;
            //d_nbr_to_y
            ITERATE_LIST_BEGIN(nbr_node->spf_run_result[level], list_node2){

                spf_result_nbr = (spf_result_t *)list_node2->data;

                if(spf_result_nbr->node == node)
                    d_nbr_to_self = spf_result_nbr->spf_metric;

                if(spf_result_nbr->node == spf_result_y->node)
                    d_nbr_to_y = spf_result_nbr->spf_metric;
            }ITERATE_LIST_END;

            /*Apply RFC 5286 Inequality 1*/
            if(d_nbr_to_y < (d_nbr_to_self + d_self_to_y)){

                if(singly_ll_search_by_key(ex_p_space, spf_result_y->node))
                    continue;

                singly_ll_add_node_by_val(ex_p_space, spf_result_y->node);
            }   

            d_nbr_to_self = 0;
            d_nbr_to_y = 0;
        }
        ITERATE_NODE_NBRS_END;
        d_self_to_y = 0;
    }ITERATE_LIST_END;
    return ex_p_space;
}   

/* Note : here node is E, not S*/
q_space_set_t
compute_q_space(node_t *node, edge_t *failed_edge, LEVEL level){

    node_t *S = NULL, *E = NULL;
    singly_ll_node_t *list_node1 = NULL,
                     *list_node2 = NULL;
    unsigned int d_S_to_E = 0,
                 d_S_to_y = 0,
                 d_E_to_y = 0;

    spf_result_t *spf_result_y = NULL,
                 *spf_result_e = NULL;

    q_space_set_t q_space = init_singly_ll();
    singly_ll_set_comparison_fn(q_space, instance_node_comparison_fn);

    E = node;
    S = failed_edge->from.node;

    /*Compute reverse SPF for nodes S and E as roots*/
    inverse_topology(instance, level);
    Compute_and_Store_Forward_SPF(S, level);
    Compute_and_Store_Forward_SPF(E, level);
    inverse_topology(instance, level);

    ITERATE_LIST_BEGIN(E->spf_run_result[level], list_node1){

        spf_result_e = (spf_result_t *)list_node1->data;
        if(spf_result_e->node == S){
            d_S_to_E = spf_result_e->spf_metric;
            break;
        }
    }ITERATE_LIST_END;

    /*Iterare over all nodes of the network*/

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node1){

        spf_result_y = (spf_result_t *)list_node1->data;
        
        /*Now find d_S_to_y */
        d_S_to_y = spf_result_y->spf_metric;

        /*Now find d_E_to_y */
        ITERATE_LIST_BEGIN(E->spf_run_result[level], list_node2){

            spf_result_e = (spf_result_t *)list_node2->data;
            if(spf_result_e->node == spf_result_y->node){
                d_E_to_y = spf_result_e->spf_metric;
                break;
            }
        }ITERATE_LIST_END;

        if(d_E_to_y < (d_S_to_y + d_S_to_E)){
            /*I think check for duplicates is not required
             * will remove after UT*/
            if(singly_ll_search_by_key(q_space, spf_result_y->node->node_name)){
                d_S_to_y = 0;
                d_E_to_y = 0;
                continue;
            }

            if(spf_result_y->node != E) /*Do not add self*/
                singly_ll_add_node_by_val(q_space, spf_result_y->node);
        }

        d_S_to_y = 0;
        d_E_to_y = 0;
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

/*Routine to compute RLFA of node S, wrt to failed link 
 * failed_edge at given level for destination dest. Note that
 * to compute RLFA we need to know dest also, thus RLFAs are 
 * computed per destination (Need to check for LFAs)*/

void 
compute_rlfa(node_t *node, LEVEL level, edge_t *failed_edge, node_t *dest){

    /* Run SPF for protecting node S*/

    singly_ll_node_t *list_node = NULL,
                     *list_node2 = NULL;
    node_t *pq_node = NULL;
    spf_result_t *spf_result = NULL;
    unsigned int d_pq_node_to_dest = 0,
                 d_S_node_to_dest = 0;


    //p_space_set_t ex_p_space = compute_extended_p_space(node, failed_edge, level);
    p_space_set_t ex_p_space = compute_p_space(node, failed_edge, level);
    q_space_set_t q_space = compute_q_space(node, failed_edge, level);

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
    ITERATE_LIST_BEGIN(pq_space, list_node){

        pq_node = (node_t *)list_node->data;

        Compute_and_Store_Forward_SPF(pq_node, level);
        /* Check for D_opt(pq_node, dest) < D_opt(S, dest)*/

        d_pq_node_to_dest = 0;
        d_S_node_to_dest = 0;

        ITERATE_LIST_BEGIN(pq_node->spf_run_result[level], list_node2){

            spf_result = (spf_result_t *)list_node2->data;
            if(spf_result->node != dest)
                continue;

            d_pq_node_to_dest = spf_result->spf_metric;
            break;
        }ITERATE_LIST_END;

        /*We had run run the SPF for computing node also.*/
        ITERATE_LIST_BEGIN(node->spf_run_result[level], list_node2){

            spf_result = (spf_result_t *)list_node2->data;
            if(spf_result->node != dest)
                continue;

            d_S_node_to_dest = spf_result->spf_metric;
            break;
        }ITERATE_LIST_END;

        if(d_pq_node_to_dest < d_S_node_to_dest){

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
            return "LINK_PROTECTION_LFA";
        case LINK_PROTECTION_LFA_DOWNSTREAM:
            return "LINK_PROTECTION_LFA_DOWNSTREAM";
        case LINK_AND_NODE_PROTECTION_LFA:
            return "LINK_AND_NODE_PROTECTION_LFA";
        case LINK_PROTECTION_RLFA:
            return "LINK_PROTECTION_RLFA";
        case LINK_PROTECTION_RLFA_DOWNSTREAM:
            return "LINK_PROTECTION_RLFA_DOWNSTREAM";
        case LINK_AND_NODE_PROTECTION_RLFA:
            return "LINK_AND_NODE_PROTECTION_RLFA";
         default:
            return "UNKNOWN_LFA_TYPE";
    }
}

lfa_t *
get_link_protection_lfa(node_t *S, edge_end_t *protected_link, node_t *dest_node, LEVEL level){

    spf_result_t *D_res = NULL;
    singly_ll_node_t *list_node = NULL; 
    lfa_t *lfa = NULL;

    D_res = GET_SPF_RESULT((&(S->spf_info)), dest_node, level);

    ITERATE_LIST_BEGIN(D_res->link_protection_lfas, list_node){
        lfa = list_node->data;
        if(lfa->protected_link == protected_link)
            return lfa;
    } ITERATE_LIST_END;
    return NULL;
}


static lfa_t *
broadcast_link_compute_lfa(node_t * S, edge_t *protected_link, 
                           LEVEL level, 
                           boolean strict_down_stream_lfa){

    return NULL;
}

static lfa_t *
_p2p_compute_lfa(node_t * S, edge_t *protected_link,
                            LEVEL level,
                            boolean strict_down_stream_lfa){

    edge_t *edge1 = NULL, *edge2 = NULL;
    node_t *N = NULL;

    ITERATE_NODE_PHYSICAL_NBRS_BEGIN(S, N, edge1, edge2, level){

        printf("Nbr = %s, edge1 = %s\n", N->node_name, edge1->from.intf_name);
        continue;   
    } ITERATE_NODE_PHYSICAL_NBRS_END;
    return NULL;
}

static lfa_t *
p2p_compute_lfa(node_t * S, edge_t *protected_link, 
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
                 dist_E_D = 0;

    lfa_dest_pair_t *lfa_dest_pair = NULL;

    lfa_t *lfa = get_new_lfa(); 
    lfa->protected_link = &protected_link->from;

    /* 3. Filter nbrs of S using inequality 1 */
    E = protected_link->to.node;

    /*ToDo : to be changed to ITERATE_NODE_PHYSICAL_NBRS_BEGIN*/
    ITERATE_NODE_PHYSICAL_NBRS_BEGIN(S, N, edge1, edge2, level){

        sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s edge2 = %s for LFA candidature",/*Strange : adding edge2 in log solves problem of this macro looping*/ 
            S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();
        
        /*Do not consider the link being protected to find LFA*/
        if(N == E && edge1 == protected_link)
            continue;
                
        ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node){

            D_res = list_node->data;
            D = D_res->node;

            sprintf(LOG, "Node : %s : Source(S) = %s, probable LFA(N) = %s, DEST(D) = %s, Primary NH(E) = %s", 
                S->node_name, S->node_name, N->node_name, D->node_name, E->node_name); TRACE();

            /* If to reach D from S , primary nexthop is not E, then skip D*/
            if(!is_present(&D_res->next_hop[IPNH][0], E)){
                sprintf(LOG, "Node : %s : skipping Dest %s as %s is not its primary nexthop",
                    S->node_name, D->node_name, E->node_name); TRACE();
                continue;
            }

            dist_N_D = DIST_X_Y(N, D, level);
            dist_N_S = DIST_X_Y(N, S, level);
            dist_S_D = D_res->spf_metric;

            sprintf(LOG, "Node : %s : Testing inequality 1 : dist_N_D(%u) < dist_N_S(%u) + dist_S_D(%u)",
                S->node_name, dist_N_D, dist_N_S, dist_S_D); TRACE();

            /* Apply inequality 1*/
            if(!(dist_N_D < dist_N_S + dist_S_D)){
                sprintf(LOG, "Node : %s : Inequality 1 failed", S->node_name); TRACE();
                continue;
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
                    continue;
                }

                sprintf(LOG, "Node : %s : Inequality 2 passed", S->node_name); TRACE(); 
            }

            lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
            
            lfa_dest_pair->lfa_type = strict_down_stream_lfa ? 
                                      LINK_PROTECTION_LFA_DOWNSTREAM : LINK_PROTECTION_LFA; 
            
            lfa_dest_pair->lfa = N;
            lfa_dest_pair->oif_to_lfa = &edge1->from; 
            lfa_dest_pair->dest = D;

            /* Inequality 3 : Node protecting LFA */
            dist_N_E = DIST_X_Y(N, E, level);
            dist_E_D = DIST_X_Y(E, D, level); 

            sprintf(LOG, "Node : %s : Testing inequality 3 : dist_N_E(%u) < dist_E_D(%u)",
                     S->node_name, dist_N_E, dist_E_D); TRACE();

            if(dist_N_D < dist_N_E + dist_E_D){
                lfa_dest_pair->lfa_type = LINK_AND_NODE_PROTECTION_LFA;  
                sprintf(LOG, "Node : %s : inequality 3 Passed, Link protecting LFA promoted to node-protecting LFA",
                             S->node_name); TRACE();
            }else{
                sprintf(LOG, "Node : %s : inequality 3 Failed", S->node_name); TRACE();
            }

            singly_ll_add_node_by_val(lfa->lfa, lfa_dest_pair);
             
            sprintf(LOG, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s", N->node_name, 
                          lfa_dest_pair->oif_to_lfa->intf_name, D->node_name, 
                          get_str_lfa_type(lfa_dest_pair->lfa_type)); TRACE();

        } ITERATE_LIST_END;

        sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s edge2 = %s for LFA candidature Done", 
            S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();

    } ITERATE_NODE_PHYSICAL_NBRS_END;
    return lfa;
}

lfa_t *
compute_lfa(node_t * S, edge_t *protected_link,
            LEVEL level,
            boolean strict_down_stream_lfa){

    /* Run necessary SPF runs required to compute LFAs 
     * of any type - p2p link/node protection or broadcast link
     * node protecting LFAs*/

     /* 1. Run SPF on S to know DIST(S,D) */
     Compute_and_Store_Forward_SPF(S, level);
    
     /* 2. Run SPF on all nbrs of S to know DIST(N,D) and DIST(N,S)*/
     Compute_Neighbor_SPFs(S, level); 

     if(is_broadcast_link(protected_link, level) == FALSE)
         return p2p_compute_lfa(S, protected_link, level, TRUE); 
     else
         return broadcast_link_compute_lfa(S, protected_link, level, TRUE);
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
        lfa_dest_pair->oif_to_lfa->intf_name, lfa_dest_pair->dest->node_name, get_str_lfa_type(lfa_dest_pair->lfa_type));
          
    } ITERATE_LIST_END;
    printf("\n");
}

