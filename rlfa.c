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

#include "rlfa.h"
#include <stdlib.h>
#include <stdio.h>
#include "instance.h"
#include "spfutil.h"

extern instance_t *instance;

extern int
instance_node_comparison_fn(void *_node, void *input_node_name);

void
Compute_and_Store_Forward_SPF(node_t *spf_root,
                            spf_info_t *spf_info,
                            LEVEL level){

    spf_computation(spf_root, spf_info, level, SKELETON_RUN);
}


void
Compute_Neighbor_SPFs(node_t *spf_root, edge_t *failed_edge,
                      LEVEL level){

    
    /*-----------------------------------------------------------------------------
     *   Iterate over all nbrs of S except E, and run SPF
     *-----------------------------------------------------------------------------*/

    node_t *nbr_node = NULL;
    edge_t *edge = NULL;

    ITERATE_NODE_NBRS_BEGIN(spf_root, nbr_node, edge, level){

        if(failed_edge->to.node == edge->to.node)
            continue;

        Compute_and_Store_Forward_SPF(nbr_node, &nbr_node->spf_info, level);

    }
    ITERATE_NODE_NBRS_END;
}



/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the p-space of 'node' wrt to 'edge'.
 *  Note that, 'edge' need not be directly connected edge of 'node'
 *-----------------------------------------------------------------------------*/

p_space_set_t 
compute_p_space(node_t *node, edge_t *failed_edge, LEVEL level){
    
    spf_result_t *res = NULL;
    singly_ll_node_t* list_node = NULL;

    Compute_and_Store_Forward_SPF(node, &node->spf_info, level);
    ll_t *spf_result = node->spf_run_result[level];

    p_space_set_t p_space = init_singly_ll();
    singly_ll_set_comparison_fn(p_space, instance_node_comparison_fn);

    /*Iterate over spf_result and filter out all those nodes whose next hop is 
     * failed_edge->to.node */

    ITERATE_LIST( spf_result, list_node){
        
        res = (spf_result_t *)list_node->data;

        /*Skip all nodes in the nw which are reachable through S--E*/
         if(res->node->next_hop[level][0] == failed_edge->to.node)
            continue;
        /*Do not add computing node itself*/
        if(res->node == node)
            continue;

        singly_ll_add_node_by_val(p_space, res->node);
    }
    
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
    Compute_and_Store_Forward_SPF(node, &node->spf_info, level); 
    /*Run SPF on all nbrs of S except E*/
    Compute_Neighbor_SPFs(node, failed_edge, level);

    /*iterate over entire network. Note that node->spf_run_result list
     * carries all nodes of the network reachable from source at level l.
     * We deem this list as the "entire network"*/ 

    ITERATE_LIST(node->spf_run_result[level], list_node1){

        spf_result_y = (spf_result_t *)list_node1->data;

        d_self_to_y = spf_result_y->spf_metric;
        
        ITERATE_NODE_NBRS_BEGIN(node, nbr_node, edge, level){

            /*skip E */
            if(edge == failed_edge)
                continue;
            //d_nbr_to_y
            ITERATE_LIST(nbr_node->spf_run_result[level], list_node2){

                spf_result_nbr = (spf_result_t *)list_node2->data;

                if(spf_result_nbr->node == node)
                    d_nbr_to_self = spf_result_nbr->spf_metric;

                if(spf_result_nbr->node == spf_result_y->node)
                    d_nbr_to_y = spf_result_nbr->spf_metric;
            }

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
    }
    return ex_p_space;
}   

/* Note : here node is E, not S*/
q_space_set_t
compute_q_space(node_t *node, edge_t *failed_edge, LEVEL level){

    /* Need to check how to compute Q space*/

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
    Compute_and_Store_Forward_SPF(S, &S->spf_info, level);
    Compute_and_Store_Forward_SPF(E, &E->spf_info, level);
    inverse_topology(instance, level);

    ITERATE_LIST(E->spf_run_result[level], list_node1){

        spf_result_e = (spf_result_t *)list_node1->data;
        if(spf_result_e->node == S){
            d_S_to_E = spf_result_e->spf_metric;
            break;
        }
    }

    /*Iterare over all nodes of the network*/

    ITERATE_LIST(S->spf_run_result[level], list_node1){

        spf_result_y = (spf_result_t *)list_node1->data;
        
        /*Now find d_S_to_y */
        d_S_to_y = spf_result_y->spf_metric;

        /*Now find d_E_to_y */
        ITERATE_LIST(E->spf_run_result[level], list_node2){

            spf_result_e = (spf_result_t *)list_node2->data;
            if(spf_result_e->node == spf_result_y->node){
                d_E_to_y = spf_result_e->spf_metric;
                break;
            }
        }

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
    }
    return q_space;
}

pq_space_set_t
Intersect_Extended_P_and_Q_Space(p_space_set_t p_space, q_space_set_t q_space){

    singly_ll_node_t *p_list_node = NULL;
    node_t *p_node = NULL;

    pq_space_set_t pq_space = init_singly_ll();
    
    ITERATE_LIST(p_space, p_list_node){
        p_node = (node_t*)p_list_node->data;
        if(singly_ll_search_by_key(q_space, p_node->node_name)) 
            singly_ll_add_node_by_val(pq_space, p_node);
    }

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
    //Compute_and_Store_Forward_SPF(node, &node->spf_info, level);    
    //Compute_and_Store_Forward_SPF(failed_edge->to.node, &failed_edge->to.node->spf_info, level);    

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
    ITERATE_LIST(pq_space, list_node){

        pq_node = (node_t *)list_node->data;

        Compute_and_Store_Forward_SPF(pq_node, &pq_node->spf_info, level);
        /* Check for D_opt(pq_node, dest) < D_opt(S, dest)*/

        d_pq_node_to_dest = 0;
        d_S_node_to_dest = 0;

        ITERATE_LIST(pq_node->spf_run_result[level], list_node2){

            spf_result = (spf_result_t *)list_node2->data;
            if(spf_result->node != dest)
                continue;

            d_pq_node_to_dest = spf_result->spf_metric;
            break;
        }

        /*We had run run the SPF for computing node also.*/
        ITERATE_LIST(node->spf_run_result[level], list_node2){

            spf_result = (spf_result_t *)list_node2->data;
            if(spf_result->node != dest)
                continue;

            d_S_node_to_dest = spf_result->spf_metric;
            break;
        }

        if(d_pq_node_to_dest < d_S_node_to_dest){

            printf("PQ node : %s\n", pq_node->node_name);
        }
    }
}

