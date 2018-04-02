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
#include "bitsop.h"
#include "spftrace.h"
#include "routes.h"

extern instance_t *instance;

extern int
instance_node_comparison_fn(void *_node, void *input_node_name);

void 
clear_pq_nodes(node_t *S, LEVEL level){
    
    unsigned int i = 0;
    if(is_internal_nh_t_empty(S->pq_nodes[level][0]))
        return;
    for(i = 0; i < MAX_NXT_HOPS; i++){
        init_internal_nh_t(S->pq_nodes[level][i]);
    }
}

void
init_back_up_computation(node_t *S, LEVEL level){

   singly_ll_node_t* list_node = NULL;
   spf_result_t *res = NULL;
   nh_type_t nh = NH_MAX;
   unsigned int i = 0;
   ll_t *spf_result = S->spf_run_result[level];
   
   ITERATE_LIST_BEGIN(spf_result, list_node){
       
       res = (spf_result_t *)list_node->data;
       ITERATE_NH_TYPE_BEGIN(nh){
#if 0
        copy_nh_list2(res->node->backup_next_hop[level][nh], 
            res->node->old_backup_next_hop[level][nh]); 
#endif
        if(is_internal_nh_t_empty(res->node->backup_next_hop[level][nh][0]))
            continue;
        for(i=0; i < MAX_NXT_HOPS; i++){
            init_internal_nh_t(res->node->backup_next_hop[level][nh][i]);    
        }
       } ITERATE_NH_TYPE_END;
   } ITERATE_LIST_END;
   clear_pq_nodes(S, level);
}

/*It should work for both broadcast and non-broadcast links*/
boolean
is_destination_impacted(node_t *S, edge_t *protected_link, 
                        node_t *D, LEVEL level, 
                        char impact_reason[],
                        boolean *MANDATORY_NODE_PROTECTION){

    /*case 1 : Destination has only 1 next hop through protected_link, return TRUE*/
    unsigned int nh_count = 0;
    nh_type_t nh = NH_MAX;
    internal_nh_t *primary_nh = NULL;
    spf_result_t *D_res = NULL;
    node_t *E = protected_link->to.node;
    
    assert(IS_LEVEL_SET(protected_link->level, level));

    /* If it is false, it dont mean anything, but if it is true then
     * Destination MUST need to have node protection backup. In the rest of
     * code we must check only for TRUE-ness of this variable. This variable
     * is meant for Destination which has ECMP but yet those ECMPs fails to
     * provide node protection to the node. For Dest which have only 1 Primary
     * Nexthops, ofcourse they have to be happy with any type of protection (they are poor).
     * Hence, this variable is not meant for Dest with 1 primary nexthops. The word 
     * MANDATORY means the destination do not needs link-only protecting backups (as they
     * already have ECMP)*/
    *MANDATORY_NODE_PROTECTION = FALSE;

    if(!IS_LINK_PROTECTION_ENABLED(protected_link) &&
            !IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
        printf("No protection enabled on the link\n");
        return FALSE;
    }

    D_res = GET_SPF_RESULT((&S->spf_info), D, level);

    if(D_res->backup_requirement[level] == NO_BACKUP_REQUIRED){
        sprintf(impact_reason, "Dest %s has Independant primary nexthops", D->node_name);
        return FALSE;
    }

    ITERATE_NH_TYPE_BEGIN(nh){
        nh_count += get_nh_count(&(D_res->next_hop[nh][0]));
    }ITERATE_NH_TYPE_END;

    if(nh_count == 0){
        sprintf(impact_reason, "Dest %s has no Primary Nexthops", D->node_name);
        return FALSE;
    }

    if(nh_count == 1){
        primary_nh = is_nh_list_empty2(&(D_res->next_hop[IPNH][0])) ? &(D_res->next_hop[LSPNH][0]):
                                            &(D_res->next_hop[IPNH][0]);
        if(primary_nh->oif != &protected_link->from){
            sprintf(impact_reason, "Dest %s only primary nxt hop oif(%s) != protected_link(%s)", 
                    D->node_name, primary_nh->oif->intf_name, protected_link->from.intf_name);
            return FALSE;
        }

        sprintf(impact_reason, "Dest %s only primary nxt hop oif(%s) = protected_link(%s)",
                    D->node_name, primary_nh->oif->intf_name, protected_link->from.intf_name);
        return TRUE;
    }

    if(nh_count > 1){
        if(IS_LINK_PROTECTION_ENABLED(protected_link) &&
                !IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
            sprintf(impact_reason, "Dest %s has ECMP primary nxt hop count = %u AND Only LINK_PROTECTION Enabled",
                        D->node_name, nh_count);
            return FALSE;/*ECMP case with only link protection*/
        }
        if(IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
            sprintf(impact_reason, "Dest %s has ECMP primary nxt hop count = %u,"
                    "but all primary nxt hop nodes traverses protected_link next hop"
                    "node (%s) AND LINK_NODE_PROTECTION Enabled. No only-link protecting backup needed",
                    D->node_name, nh_count, E->node_name);
            *MANDATORY_NODE_PROTECTION = TRUE;
            return TRUE;
        }
    }
    assert(0); 
    return FALSE;
}

void
Compute_and_Store_Forward_SPF(node_t *spf_root,
                              LEVEL level){

    spf_computation(spf_root, &spf_root->spf_info, level, FORWARD_RUN);
}


void
Compute_PHYSICAL_Neighbor_SPFs(node_t *spf_root, LEVEL level){

    
    node_t *nbr_node = NULL,
           *pn_node = NULL;
    edge_t *edge1 = NULL, *edge2 = NULL;

    ITERATE_NODE_PHYSICAL_NBRS_BEGIN(spf_root, nbr_node, pn_node, edge1, edge2, level){

        Compute_and_Store_Forward_SPF(nbr_node, level);
    }
    ITERATE_NODE_PHYSICAL_NBRS_END(spf_root, nbr_node, pn_node, level);
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

static boolean
broadcast_node_protection_critera(node_t *S, 
                                  LEVEL level, edge_t *protected_link, 
                                  node_t *dest, node_t *nbr_node){

    /* nbr_node Must be able to send traffic to dest by-passing all nodes directly
     * attached to LAN segment of protected_link*/
    /* This routine assumes all required SPF runs has been run*/

    assert(is_broadcast_link(protected_link, level));
    node_t *PN = protected_link->to.node;
    node_t *PN_nbr = NULL;
    edge_t *edge = NULL;

    unsigned int d_nbr_node_to_dest = 0,
                 d_nbr_node_to_PN_nbr = 0,
                 d_PN_nbr_to_dest = 0;

    d_nbr_node_to_dest = DIST_X_Y(nbr_node, dest, level);
    ITERATE_NODE_LOGICAL_NBRS_BEGIN(PN, PN_nbr, edge, level){

        d_nbr_node_to_PN_nbr = DIST_X_Y(nbr_node, PN_nbr, level);
        d_PN_nbr_to_dest = DIST_X_Y(PN_nbr, dest, level);

        if(d_nbr_node_to_dest < d_nbr_node_to_PN_nbr + d_PN_nbr_to_dest){
            continue;   
        }
        else{
            return FALSE;
        }
    } ITERATE_NODE_LOGICAL_NBRS_END;
    return TRUE;
}

/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the extended p-space of 'node' wrt to
 *  'edge'. Note that, 'edge' need not be directly connected edge of 'node'.
 *-----------------------------------------------------------------------------*/
void
broadcast_compute_link_node_protecting_extended_p_space(node_t *S, 
        edge_t *protected_link, 
        LEVEL level){

    /*Compute p space of all nbrs of node except protected_link->to.node
     * and union it. Remove duplicates from union*/

    node_t *nbr_node = NULL,
    *PN = NULL,
    *pn_node = NULL,
    *P_node = NULL;

    edge_t *edge1 = NULL, *edge2 = NULL;
    singly_ll_node_t *list_node1 = NULL;

    unsigned int d_nbr_to_p_node = 0,
                 d_nbr_to_S = 0,
                 d_S_to_p_node = 0,
                 d_nbr_to_PN = 0,
                 d_PN_to_p_node = 0,
                 d_S_to_nbr = 0,
                 d_S_to_PN = 0,
                 d_PN_to_nbr =0;


    spf_result_t *spf_result_p_node = NULL;

    if(!IS_LEVEL_SET(protected_link->level, level))
        return;

    internal_nh_t *rlfa = NULL;
    assert(is_broadcast_link(protected_link, level));

    boolean is_node_protection_enabled = 
        IS_LINK_NODE_PROTECTION_ENABLED(protected_link);
    boolean is_link_protection_enabled = 
        IS_LINK_PROTECTION_ENABLED(protected_link);

#if 0/* Caller is suppose to run these SPF runs*/
    /*run spf on self*/
    Compute_and_Store_Forward_SPF(S, level); 
    /*Run SPF on all logical nbrs of S*/
    Compute_PHYSICAL_Neighbor_SPFs(S, level);
#endif
    /*iterate over entire network. Note that node->spf_run_result list
     * carries all nodes of the network reachable from source at level l.
     * We deem this list as the "entire network"*/ 
    PN = protected_link->to.node;


    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node1){
        spf_result_p_node = (spf_result_t *)list_node1->data;
        P_node = spf_result_p_node->node;

        if(P_node == S || IS_OVERLOADED(P_node, level) ||
                P_node == nbr_node) continue;

        /* ToDo : RFC : Remote-LFA Node Protection and Manageability
         * draft-ietf-rtgwg-rlfa-node-protection-13 - section 2.2.1*/
        /* Testing ECMP equality : Nbr N should not have ECMP path to P_node 
         * which traverses the S----E link -- wonder criteria of selecting the
         * P node via N automatically subsume this */

        d_S_to_p_node = spf_result_p_node->spf_metric;
        d_PN_to_p_node = DIST_X_Y(PN, P_node, level);

        sprintf(instance->traceopts->b, "Node : %s : Begin ext-pspace computation for S=%s, protected-link = %s, LEVEL = %s",
                S->node_name, S->node_name, protected_link->from.intf_name, get_str_level(level)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(S, nbr_node, pn_node, edge1, edge2, level){

            /*skip protected link itself */
            if(edge1 == protected_link){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
            }

            /*RFC 7490 section 5.4 : skip neighbors in computation of PQ nodes(extended p space) 
             * which are either overloaded or reachable through infinite metric*/
            if(edge1->metric[level] >= INFINITE_METRIC){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
            }
            if(IS_OVERLOADED(nbr_node, level)){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
            }

            d_nbr_to_PN = DIST_X_Y(nbr_node, PN, level);
            d_S_to_nbr = DIST_X_Y(S, nbr_node, level);
            d_PN_to_nbr = DIST_X_Y(PN, nbr_node, level);

            if(!(d_S_to_nbr <  d_S_to_PN + d_PN_to_nbr)){
                sprintf(instance->traceopts->b, "Node : %s : Nbr %s will not be considered for computing P-space," 
                        "nbr traverses protected link", S->node_name, nbr_node->node_name); 
                trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
            }   

            d_nbr_to_p_node = DIST_X_Y(nbr_node, P_node, level);
            if(is_node_protection_enabled == TRUE){

                /*If node protection is enabled*/
                d_PN_to_p_node = DIST_X_Y(PN, P_node, level);

                /*Loop free inequality 1 : N should be Loop free wrt S and PN*/
                sprintf(instance->traceopts->b, "Node : %s : Testing inequality 1 : checking loop free wrt S = %s, Nbr = %s(oif = %s), P_node = %s",
                        S->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name, P_node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT); 

                d_nbr_to_S = DIST_X_Y(nbr_node, S, level);
                sprintf(instance->traceopts->b, "Node : %s : d_nbr_to_p_node(%u) < d_nbr_to_S(%u) + d_S_to_p_node(%u)",
                        S->node_name, d_nbr_to_p_node, d_nbr_to_S, d_S_to_p_node); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                if(!(d_nbr_to_p_node < d_nbr_to_S + d_S_to_p_node)){
                    sprintf(instance->traceopts->b, "Node : %s : inequality 1 failed, Nbr = %s(oif = %s) is not loop free wrt S", 
                            S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    continue;
                }
                sprintf(instance->traceopts->b, "Node : %s : above inequality 1 passed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                sprintf(instance->traceopts->b, "Node : %s : Testing node protection inequality for Broadcast link: S = %s, nbr = %s, P_node = %s, PN = %s",
                        S->node_name, S->node_name, nbr_node->node_name, P_node->node_name, PN->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                /*Node protection criteria for broadcast link should be : Nbr should be able to send traffic to P_node wihout 
                 * passing through any node attached to broadcast segment*/

                if(broadcast_node_protection_critera(S, level, protected_link, P_node, nbr_node) == TRUE){
                    sprintf(instance->traceopts->b, "Node : %s : Above node protection inequality passed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                    rlfa = get_next_hop_empty_slot(S->pq_nodes[level]);
                    rlfa->lfa_type = BROADCAST_NODE_PROTECTION_RLFA;
                    /*Check for link protection, nbr_node should be loop free wrt to PN*/
                    sprintf(instance->traceopts->b, "Node : %s : Checking if potential P_node = %s provide broadcast link protection to S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    sprintf(instance->traceopts->b, "Node : %s : Checking if Nbr  = %s(oif=%s) is  loop free wrt to PN = %s", 
                            S->node_name, nbr_node->node_name, edge1->from.intf_name, PN->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    /*For link protection, Nbr should be loop free wrt to PN*/
                    if(d_nbr_to_p_node < (d_nbr_to_PN + d_PN_to_p_node)){
                        rlfa->lfa_type = BROADCAST_LINK_AND_NODE_PROTECTION_RLFA;
                        sprintf(instance->traceopts->b, "Node : %s : P_node = %s provide node-link protection to S = %s, Nbr = %s(oif=%s)",
                                S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    }
                    rlfa->level = level;     
                    rlfa->oif = &edge1->from;
                    rlfa->protected_link = &protected_link->from;
                    rlfa->node = NULL;
                    if(edge1->etype == UNICAST)
                        set_next_hop_gw_pfx(*rlfa, edge1->to.prefix[level]->prefix);
                    rlfa->nh_type = LSPNH;
                    rlfa->proxy_nbr = nbr_node;
                    rlfa->rlfa = P_node;
                    //rlfa->mpls_label_in = 1;
                    rlfa->root_metric = d_S_to_p_node;
                    rlfa->dest_metric = 0; /*Not known yet*/ 
                    rlfa->is_eligible = TRUE; /*Not known yet*/
                    ITERATE_NODE_PHYSICAL_NBRS_BREAK(S, nbr_node, pn_node, level);
                }

                sprintf(instance->traceopts->b, "Node : %s : Above node protection inequality failed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                if(is_link_protection_enabled == FALSE){
                    sprintf(instance->traceopts->b, "Node : %s :  Link degradation is disabled, candidate P_node = %s"
                            " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)", 
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
                }
                /*P_node could not provide node protection, check for link protection*/
                if(is_link_protection_enabled == TRUE){
                    sprintf(instance->traceopts->b, "Node : %s : Checking if potential P_node = %s provide broadcast link protection to S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    sprintf(instance->traceopts->b, "Node : %s : Checking if Nbr  = %s(oif=%s) is  loop free wrt to PN = %s", 
                            S->node_name, nbr_node->node_name, edge1->from.intf_name, PN->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                    /*For link protection, Nbr should be loop free wrt to PN*/
                    if(d_nbr_to_p_node < (d_nbr_to_PN + d_PN_to_p_node)){
                        sprintf(instance->traceopts->b, "Node : %s : P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                                S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                        rlfa = get_next_hop_empty_slot(S->pq_nodes[level]);
                        rlfa->level = level;     
                        rlfa->oif = &edge1->from;
                        rlfa->protected_link = &protected_link->from;
                        rlfa->node = NULL;
                        if(edge1->etype == UNICAST)
                            set_next_hop_gw_pfx(*rlfa, edge1->to.prefix[level]->prefix);
                        rlfa->nh_type = LSPNH;
                        rlfa->lfa_type = BROADCAST_LINK_PROTECTION_RLFA;
                        rlfa->proxy_nbr = nbr_node;
                        rlfa->rlfa = P_node;
                        //rlfa->mpls_label_in = 1;
                        rlfa->root_metric = d_S_to_p_node;
                        rlfa->dest_metric = 0; /*Not known yet*/ 
                        rlfa->is_eligible = TRUE; /*Not known yet*/
                        ITERATE_NODE_PHYSICAL_NBRS_BREAK(S, nbr_node, pn_node, level);
                    }
                    sprintf(instance->traceopts->b, "Node : %s : candidate P_node = %s do not provide link protection" 
                            " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                }
            }else if(is_link_protection_enabled == TRUE){
                if(d_nbr_to_p_node < (d_nbr_to_PN + d_PN_to_p_node)){
                    sprintf(instance->traceopts->b, "Node : %s : P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    rlfa = get_next_hop_empty_slot(S->pq_nodes[level]);
                    rlfa->level = level;     
                    rlfa->oif = &edge1->from;
                    rlfa->protected_link = &protected_link->from;
                    rlfa->node = NULL;
                    if(edge1->etype == UNICAST)
                        set_next_hop_gw_pfx(*rlfa, edge1->to.prefix[level]->prefix);
                    rlfa->nh_type = LSPNH;
                    rlfa->lfa_type = BROADCAST_LINK_PROTECTION_RLFA;
                    rlfa->proxy_nbr = nbr_node;
                    rlfa->rlfa = P_node;
                    //rlfa->mpls_label_in = 1;
                    rlfa->root_metric = d_S_to_p_node;
                    rlfa->dest_metric = 0; /*Not known yet*/ 
                    rlfa->is_eligible = TRUE; /*Not known yet*/
                }
            }
            else{
                assert(0);
            }
        } ITERATE_NODE_PHYSICAL_NBRS_END(S, nbr_node, pn_node, level);
    } ITERATE_LIST_END;
}   

/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the extended p-space of 'node' wrt to
 *  'edge'. Note that, 'edge' need not be directly connected edge of 'node'.
 *-----------------------------------------------------------------------------*/
void
p2p_compute_link_node_protecting_extended_p_space(node_t *S, 
                                                  edge_t *protected_link, 
                                                  LEVEL level){

    /*Compute p space of all nbrs of node except protected_link->to.node
     * and union it. Remove duplicates from union*/

    node_t *nbr_node = NULL,
    *E = NULL,
    *pn_node = NULL,
    *P_node = NULL;

    edge_t *edge1 = NULL, *edge2 = NULL;
    singly_ll_node_t *list_node1 = NULL;
    internal_nh_t *rlfa = NULL;

    unsigned int d_nbr_to_p_node = 0,
                 d_nbr_to_S = 0,
                 d_nbr_to_E = 0,
                 d_E_to_nbr =0,
                 d_E_to_p_node = 0,
                 d_S_to_p_node = 0,
                 d_S_to_nbr = 0,
                 d_S_to_E = 0;

    spf_result_t *spf_result_p_node = NULL;

    if(!IS_LEVEL_SET(protected_link->level, level))
        return;

    assert(!is_broadcast_link(protected_link, level));

    boolean is_node_protection_enabled = 
        IS_LINK_NODE_PROTECTION_ENABLED(protected_link);
    boolean is_link_protection_enabled = 
        IS_LINK_PROTECTION_ENABLED(protected_link);


#if 0 /*Caller suppose to run these SPF runs*/
    /*run spf on self*/
    Compute_and_Store_Forward_SPF(S, level); 
    /*Run SPF on all logical nbrs of S*/
    Compute_PHYSICAL_Neighbor_SPFs(S, level);
#endif
    /*iterate over entire network. Note that node->spf_run_result list
     * carries all nodes of the network reachable from source at level l.
     * We deem this list as the "entire network"*/ 
    E = protected_link->to.node;
    d_S_to_E = DIST_X_Y(S, E, level);

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node1){
        spf_result_p_node = (spf_result_t *)list_node1->data;
        P_node = spf_result_p_node->node;

        if(P_node == S || IS_OVERLOADED(P_node, level) || 
                P_node == nbr_node || 
                P_node == E) /*RFC 7490, section 4.2*/
            continue;

        /* ToDo : RFC : Remote-LFA Node Protection and Manageability
         * draft-ietf-rtgwg-rlfa-node-protection-13 - section 2.2.1*/
        /* Testing ECMP equality : Nbr N should not have ECMP path to P_node 
         * which traverses the S----E link -- wonder criteria of selecting the
         * P node via N automatically subsume this */

        d_S_to_p_node = spf_result_p_node->spf_metric;

        sprintf(instance->traceopts->b, "Node : %s : Begin ext-pspace computation for S=%s, protected-link = %s, LEVEL = %s",
                S->node_name, S->node_name, protected_link->from.intf_name, get_str_level(level)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(S, nbr_node, pn_node, edge1, edge2, level){

            /*skip protected link itself */
            if(edge1 == protected_link){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
            }

            /*RFC 7490 section 5.4 : skip neighbors in computation of PQ nodes(extended p space) 
             * which are either overloaded or reachable through infinite metric*/
            if(edge1->metric[level] >= INFINITE_METRIC){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
            }
            if(IS_OVERLOADED(nbr_node, level)){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
            }

            d_nbr_to_E = DIST_X_Y(nbr_node, E, level);
            d_S_to_nbr = DIST_X_Y(S, nbr_node, level);
            d_E_to_nbr = DIST_X_Y(E, nbr_node, level);

            /*  skip nbrs which are reachable from S from protected link*/
            /* nbr should not be reachable via E. Examine
             * only the node protecting nbrs since S need to establish the
             * tunnel to P node and this tunnel should reach P via shortest path
             * not passing through protected-link*/

            if(!(d_S_to_nbr <  d_S_to_E + d_E_to_nbr)){
                sprintf(instance->traceopts->b, "Node : %s : Nbr %s will not be considered for computing P-space," 
                        "nbr traverses protected link", S->node_name, nbr_node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
            }

            d_nbr_to_p_node = DIST_X_Y(nbr_node, P_node, level);
            if(is_node_protection_enabled == TRUE){

                /*If node protection is enabled*/
                d_E_to_p_node = DIST_X_Y(E, P_node, level);

                /*Loop free inequality 1 : N should be Loop free wrt S*/
                sprintf(instance->traceopts->b, "Node : %s : Testing inequality 1 : S = %s, Nbr = %s(oif = %s), P_node = %s",
                        S->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name, P_node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT); 

                d_nbr_to_S = DIST_X_Y(nbr_node, S, level);
                sprintf(instance->traceopts->b, "Node : %s : d_nbr_to_p_node(%u) < d_nbr_to_S(%u) + d_S_to_p_node(%u)",
                        S->node_name, d_nbr_to_p_node, d_nbr_to_S, d_S_to_p_node); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                if(!(d_nbr_to_p_node < d_nbr_to_S + d_S_to_p_node)){
                    sprintf(instance->traceopts->b, "Node : %s : inequality 1 failed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
                }
                sprintf(instance->traceopts->b, "Node : %s : above inequality 1 passed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                /*condition for node protection RLFA - RFC : 
                 * draft-ietf-rtgwg-rlfa-node-protection-13 - section 2.2.6.2*/
                sprintf(instance->traceopts->b, "Node : %s : Testing node protection inequality : S = %s, nbr = %s, P_node = %s, E = %s",
                        S->node_name, S->node_name, nbr_node->node_name, P_node->node_name, E->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                sprintf(instance->traceopts->b, "Node : %s : d_nbr_to_p_node(%u) < d_nbr_to_E(%u) + d_E_to_p_node(%u)", 
                        S->node_name, d_nbr_to_p_node, d_nbr_to_E, d_E_to_p_node); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                if(d_nbr_to_p_node < (d_nbr_to_E + d_E_to_p_node)){
                    sprintf(instance->traceopts->b, "Node : %s : Above node protection inequality passed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    /*Node has been added to extended p-space, no need to check for link protection
                     * as node-protecting node in extended pspace is automatically link protecting node for P2P links*/
                    {
                        rlfa = get_next_hop_empty_slot(S->pq_nodes[level]);
                        rlfa->level = level;     
                        rlfa->oif = &edge1->from;
                        rlfa->protected_link = &protected_link->from;
                        rlfa->node = NULL;
                        if(edge1->etype == UNICAST)
                            set_next_hop_gw_pfx(*rlfa, edge1->to.prefix[level]->prefix);
                        rlfa->nh_type = LSPNH;
                        rlfa->lfa_type = LINK_AND_NODE_PROTECTION_RLFA;
                        rlfa->proxy_nbr = nbr_node;
                        rlfa->rlfa = P_node;
                        //rlfa->mpls_label_in = 1;
                        rlfa->root_metric = d_S_to_p_node;
                        rlfa->dest_metric = 0; /*Not known yet*/ 
                        rlfa->is_eligible = TRUE; /*Not known yet*/
                    }
                    ITERATE_NODE_PHYSICAL_NBRS_BREAK(S, nbr_node, pn_node, level);
                }
                sprintf(instance->traceopts->b, "Node : %s : Above node protection inequality failed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                if(is_link_protection_enabled == FALSE){
                    sprintf(instance->traceopts->b, "Node : %s :  Link degradation is disabled, candidate P_node = %s"
                            " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)", 
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, nbr_node, pn_node, level);
                }
                /*P_node could not provide node protection, check for link protection*/
                if(is_link_protection_enabled == TRUE){
                    sprintf(instance->traceopts->b, "Node : %s : Checking if potential P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                    if(d_nbr_to_p_node < (d_nbr_to_S + protected_link->metric[level])){
                        {
                            rlfa = get_next_hop_empty_slot(S->pq_nodes[level]);
                            rlfa->level = level;     
                            rlfa->oif = &edge1->from;
                            rlfa->protected_link = &protected_link->from;
                            rlfa->node = NULL;
                            if(edge1->etype == UNICAST)
                                set_next_hop_gw_pfx(*rlfa, edge1->to.prefix[level]->prefix);
                            rlfa->nh_type = LSPNH;
                            rlfa->lfa_type = LINK_PROTECTION_RLFA;
                            rlfa->proxy_nbr = nbr_node;
                            rlfa->rlfa = P_node;
                            //rlfa->mpls_label_in = 1;
                            rlfa->root_metric = d_S_to_p_node;
                            rlfa->dest_metric = 0; /*Not known yet*/ 
                            rlfa->is_eligible = TRUE; /*Not known yet*/
                        }
                        sprintf(instance->traceopts->b, "Node : %s : P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                                S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); 
                        trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                        ITERATE_NODE_PHYSICAL_NBRS_BREAK(S, nbr_node, pn_node, level);
                    }
                    sprintf(instance->traceopts->b, "Node : %s : candidate P_node = %s do not provide link protection" 
                            " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); 
                    trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                }
            }
        } ITERATE_NODE_PHYSICAL_NBRS_END(S, nbr_node, pn_node, level);
    } ITERATE_LIST_END;
}   

void
broadcast_filter_select_pq_nodes_from_ex_pspace(node_t *S, edge_t *protected_link, 
        LEVEL level){

    unsigned int d_p_to_E = 0,
                 d_p_to_D = 0,
                 d_E_to_D = 0,
                 i = 0;

    char impact_reason[STRING_REASON_LEN];
    node_t *E = protected_link->to.node;
    internal_nh_t *p_node = NULL,
                  *rlfa = NULL;
    
    spf_result_t *D_res = NULL;
    singly_ll_node_t *list_node1 = NULL;

    assert(is_broadcast_link(protected_link, level));

    /*Compute reverse SPF for nodes S and E as roots*/
    inverse_topology(instance, level);
    Compute_and_Store_Forward_SPF(S, level);
    Compute_and_Store_Forward_SPF(E, level);
    inverse_topology(instance, level);
    
    for( i = 0; i < MAX_NXT_HOPS; i++){
        p_node = &S->pq_nodes[level][i];

        //assert(IS_BIT_SET(p_node->p_space_protection_type, LINK_NODE_PROTECTION));
        /*For node protection, Run the Forward SPF run on PQ nodes*/
        Compute_and_Store_Forward_SPF(p_node->rlfa, level);
        /*Now inspect all Destinations which are impacted by the link*/
        boolean is_dest_impacted = FALSE,
                 MANDATORY_NODE_PROTECTION = FALSE; 

        ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node1){
            is_dest_impacted = FALSE;
            D_res = list_node1->data;

            /*if RLFA's proxy nbr itself is a destination, then no need to find
             * PQ node for such a destination. p_node->proxy_nbr will surely quality to be
             * LFA for such a destination*/
            if(p_node->proxy_nbr == D_res->node)
                continue;
            
            /*Check if this is impacted destination*/
            memset(impact_reason, 0, STRING_REASON_LEN);

            MANDATORY_NODE_PROTECTION = FALSE;
            is_dest_impacted = is_destination_impacted(S, protected_link, D_res->node, 
                        level, impact_reason, &MANDATORY_NODE_PROTECTION);
            sprintf(instance->traceopts->b, "Dest = %s Impact result = %s\n    reason : %s", D_res->node->node_name, 
                    is_dest_impacted ? "IMPACTED" : "NOT-IMPCATED", impact_reason); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            if(is_dest_impacted == FALSE) continue;
        
            if(p_node->lfa_type == BROADCAST_LINK_PROTECTION_RLFA ||
                    p_node->lfa_type == BROADCAST_LINK_PROTECTION_RLFA_DOWNSTREAM){
                /*This node cannot provide node protection, check only link protection
                 * p_node should be loop free wrt to PN*/
                if(MANDATORY_NODE_PROTECTION == TRUE){
                    sprintf(instance->traceopts->b, "Node : %s : Pnode  %s not considered for link protection RLFA as Dest %s has ECMP, failed to qualify as PQ node",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    continue;
                }
                d_p_to_E = DIST_X_Y(E, p_node->rlfa, level);
                d_p_to_D = DIST_X_Y(p_node->rlfa, D_res->node, level);
                d_E_to_D = DIST_X_Y(E, D_res->node, level);
                if(!(d_p_to_D < d_p_to_E + d_E_to_D)){
                    sprintf(instance->traceopts->b, "Node : %s : Link protected p-node %s failed to qualify as link protection Q node",
                            S->node_name, p_node->rlfa->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    /*p node fails to provide link protection, this do not qualifies to be pq node*/
                    continue;    
                }
                /*Doesnt matter if p_node qualifies node protection criteria, it will be link protecting only*/
                sprintf(instance->traceopts->b, "Node : %s : Link protected p-node %s qualify as link protection Q node",
                        S->node_name, p_node->rlfa->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
                //(*(p_node->ref_count))++;
                copy_internal_nh_t(*p_node, *rlfa);
                rlfa->dest_metric = d_p_to_D;
                continue;
            }

            /*Check if p_node provides node protection*/
            if(broadcast_node_protection_critera(S, level, protected_link, D_res->node, p_node->rlfa) == TRUE){
                /*This node provides node protection to Destination D*/
                sprintf(instance->traceopts->b, "Node : %s : Node protected p-node %s qualify as node protection Q node for for Dest %s",
                        S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                
                /*When tested for P nodes, node protecting p-nodes are automatically link protecting 
                 * p nodes also for given Destination*/
                rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
                //(*(p_node->ref_count))++;
                copy_internal_nh_t(*p_node, *rlfa);
                continue;
            }

            sprintf(instance->traceopts->b, "Node : %s : Node protected p-node %s failed to qualify as node protection Q node for Dest %s",
                S->node_name, p_node->rlfa->node_name, D_res->node->node_name); 
            trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
            /*p_node fails to provide node protection, demote the p_node to LINK_PROTECTION
             * if it provides atleast link protection to Destination D*/
            if(MANDATORY_NODE_PROTECTION == TRUE){
                sprintf(instance->traceopts->b, "Node : %s : Pnode  %s not considered for link protection RLFA as Dest %s has ECMP, failed to qualify as PQ node",
                        S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                continue;
            }

            if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                sprintf(instance->traceopts->b, "Node : %s : node link degradation is not enabled", S->node_name);
                continue;
            }

            d_p_to_D = DIST_X_Y(p_node->rlfa, D_res->node, level);
            d_p_to_E = DIST_X_Y(E, p_node->rlfa, level);
            d_E_to_D = DIST_X_Y(E, D_res->node, level);
            if(!(d_p_to_D < d_p_to_E + d_E_to_D)){
                sprintf(instance->traceopts->b, "Node : %s : Node protected p-node %s failed to qualify as link protection Q node for Dest %s",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                continue;
            }
            sprintf(instance->traceopts->b, "Node : %s : Node protected p-node %s qualify as link protection Q node"
                    "Demoted from LINK_NODE_PROTECTION to LINK_PROTECTION PQ node for Dest %s", 
                     S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
            rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
            //(*(p_node->ref_count))++;
            copy_internal_nh_t(*p_node, *rlfa);
            rlfa->lfa_type = BROADCAST_LINK_PROTECTION_RLFA;
        }ITERATE_LIST_END;
    } 
}

void
p2p_filter_select_pq_nodes_from_ex_pspace(node_t *S, 
                            edge_t *protected_link, 
                            LEVEL level){

    unsigned int d_S_to_E = 0,
                 d_p_to_S = 0,
                 d_p_to_E = 0,
                 d_p_to_D = 0,
                 d_E_to_D = 0,
                 i = 0;

    char impact_reason[STRING_REASON_LEN];
    node_t *E = protected_link->to.node;
    internal_nh_t *p_node = NULL,
                  *rlfa = NULL;

    spf_result_t *D_res = NULL;
    singly_ll_node_t *list_node1 = NULL;
    assert(!is_broadcast_link(protected_link, level));

    /*Compute reverse SPF for nodes S and E as roots*/
    inverse_topology(instance, level);
    Compute_and_Store_Forward_SPF(S, level);
    Compute_and_Store_Forward_SPF(E, level);
    inverse_topology(instance, level);

    d_S_to_E = DIST_X_Y(E, S, level);

    for( ; i < MAX_NXT_HOPS; i++){
        p_node = &S->pq_nodes[level][i];
        if(is_empty_internal_nh(p_node))
            break;
        /*This node cannot provide node protection, check only link protection*/
        d_p_to_S = DIST_X_Y(S, p_node->rlfa, level); 
        d_p_to_E = DIST_X_Y(E, p_node->rlfa, level);
        if(!(d_p_to_E < d_p_to_S + d_S_to_E)){
            sprintf(instance->traceopts->b, "Node : %s : p-node %s failed to qualify as link protection Q node",
                    S->node_name, p_node->rlfa->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
            /*p node fails to provide link protection, this do not qualifies to be pq node*/
            p_node->is_eligible = FALSE;
            continue;
        }
#if 0
        /*Doesnt matter if p_node qualifies node protection criteria, it will be link protecting only*/
        sprintf(instance->traceopts->b, "Node : %s : p-node %s qualify as link protection Q node",
                S->node_name, p_node->rlfa->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
#endif
    }
    for( i = 0; i < MAX_NXT_HOPS; i++){
        p_node = &S->pq_nodes[level][i];
        if(is_nh_list_empty2(p_node)) break;
        if(p_node->is_eligible == FALSE) continue;

        /*For node protection, Run the Forward SPF run on PQ nodes*/
        Compute_and_Store_Forward_SPF(p_node->rlfa, level);
        /*Now inspect all Destinations which are impacted by the link*/
        boolean is_dest_impacted = FALSE,
                MANDATORY_NODE_PROTECTION = FALSE;

        d_p_to_E = DIST_X_Y(E, p_node->rlfa, level); 
        d_p_to_S = DIST_X_Y(S, p_node->rlfa, level);
        ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node1){
            is_dest_impacted = FALSE;
            D_res = list_node1->data;

            /*if RLFA's proxy nbr itself is a destination, then no need to find
             * PQ node for such a destination. p_node->proxy_nbr will surely quality to be
             * LFA for such a destination*/
            if(p_node->proxy_nbr == D_res->node)
                continue;

            /*Check if this is impacted destination*/
            memset(impact_reason, 0, STRING_REASON_LEN);
            MANDATORY_NODE_PROTECTION = FALSE;
            is_dest_impacted = is_destination_impacted(S, protected_link, D_res->node, 
                    level, impact_reason, &MANDATORY_NODE_PROTECTION);
            sprintf(instance->traceopts->b, "Dest = %s Impact result = %s\n    reason : %s", D_res->node->node_name, 
                    is_dest_impacted ? "IMPACTED" : "NOT-IMPCATED", impact_reason); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            if(is_dest_impacted == FALSE) continue;

            /*Check if p_node provides node protection*/
            if(p_node->lfa_type == LINK_AND_NODE_PROTECTION_RLFA){
                d_p_to_D = DIST_X_Y(p_node->rlfa, D_res->node, level);
                d_E_to_D = DIST_X_Y(E, D_res->node, level);
                sprintf(instance->traceopts->b, "Node : %s : Cheking if Node-protected p-node %s  qualify as node protection Q node for Dest %s",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                sprintf(instance->traceopts->b, "Node : %s : d_p_to_D(%u) < d_p_to_E(%u) + d_E_to_D(%u)", 
                            S->node_name, d_p_to_D, d_p_to_E, d_E_to_D); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                if(d_p_to_D < d_p_to_E + d_E_to_D){
                    /*This node provides node protection to Destination D*/
                    sprintf(instance->traceopts->b, "Node : %s : Node protected p-node %s qualify as node protection Q node for Dest %s",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    p_node->dest_metric = d_p_to_D;
                    rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
                    //(*(p_node->ref_count))++;
                    copy_internal_nh_t(*p_node, *rlfa);
                    continue;
                }

                sprintf(instance->traceopts->b, "Node : %s : Node protected p-node %s failed to qualify as node protection Q node for Dest %s",
                        S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                /*p_node fails to provide node protection, demote the p_node to LINK_PROTECTION
                 * if it provides atleast link protection to Destination D*/
                if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                    sprintf(instance->traceopts->b, "Node : %s : node link degradation is not enabled", S->node_name);
                    continue;
                }

                if(MANDATORY_NODE_PROTECTION == TRUE){
                    sprintf(instance->traceopts->b, "Node : %s : Pnode  %s not considered for link protection RLFA as Dest %s has ECMP, failed to qualify as PQ node",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    continue;
                }
                if(!(d_p_to_D < d_p_to_S + protected_link->metric[level])){
                    sprintf(instance->traceopts->b, "Node : %s : Node protected p-node %s failed to qualify as link protection Q node for Dest %s",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    continue;
                }
                p_node->dest_metric = d_p_to_D;
                rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
                //(*(p_node->ref_count))++;
                copy_internal_nh_t(*p_node, *rlfa);
                rlfa->lfa_type = LINK_PROTECTION_RLFA;
                sprintf(instance->traceopts->b, "Node : %s : Node protected p-node %s qualify as link protection Q node"
                        "Demoted from LINK_AND_NODE_PROTECTION_RLFA to LINK_PROTECTION_RLFA PQ node for Dest %s", 
                        S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
            }else if(p_node->lfa_type == LINK_PROTECTION_RLFA ||
                    p_node->lfa_type == LINK_PROTECTION_RLFA_DOWNSTREAM){
                if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                    continue;
                }
                if(MANDATORY_NODE_PROTECTION == TRUE){
                    sprintf(instance->traceopts->b, "Node : %s : Pnode  %s not considered for link protection RLFA as Dest %s has ECMP, failed to qualify as PQ node",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    continue;
                }

                d_p_to_D = DIST_X_Y(p_node->rlfa, D_res->node, level);
                if(!(d_p_to_D < d_p_to_S + protected_link->metric[level])){
                    sprintf(instance->traceopts->b, "Node : %s : Link protected p-node %s failed to qualify as link protection Q node for Dest %s",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    continue;
                }
                p_node->dest_metric = d_p_to_D;
                rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
                //(*(p_node->ref_count))++;
                copy_internal_nh_t(*p_node, *rlfa);
                rlfa->lfa_type = LINK_PROTECTION_RLFA;
                sprintf(instance->traceopts->b, "Node : %s : link protected p-node %s qualify as link protection Q node for Dest %s",
                        S->node_name, p_node->rlfa->node_name, D_res->node->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
            }else{
                assert(0);
            }
        }ITERATE_LIST_END;
    }
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

/* Algorithm revised
 
 * if N is node protecting LFA (ineq 1 2 and 3)
 *   N is BROADCAST_ONLY_NODE_PROTECTION_LFA
 *   if N satisfies ineq 4 and N do not traverse PN from S
 *      N is BROADCAST_LINK_AND_NODE_PROTECTION_LFA
 *      return
 *   else
 *      N BROADCAST_ONLY_NODE_PROTECTION_LFA 
 *      return
 * else
 *   if LINK PROTECTION is not enabled
 *      N is not LFA
 *      return
 *   if N satisfies ineq 4 and N do not traverse PN from S
 *      N is BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM
 *      return
 *   else
 *      N is not LFA
 *      return 
 *
 * */

/* Reading RFC 5286, I still dont get why LFA must not traverse PN from S. I have not put this check
 * while finding out LFAs*/

static void
broadcast_compute_link_node_protection_lfas(node_t * S, edge_t *protected_link, 
                           LEVEL level, 
                           boolean strict_down_stream_lfa){


    node_t *PN = NULL, 
    *N = NULL, 
    *D = NULL,
    *pn_node = NULL,
    *prim_nh = NULL;

    edge_t *edge1 = NULL, 
            *edge2 = NULL;

    spf_result_t *D_res = NULL;
    singly_ll_node_t *list_node = NULL;

    lfa_type_t lfa_type = UNKNOWN_LFA_TYPE;

    nh_type_t nh = NH_MAX, backup_nh_type = NH_MAX;
    internal_nh_t *backup_nh = NULL;

    char impact_reason[STRING_REASON_LEN];

    unsigned int dist_N_D  = 0, 
                 dist_N_S  = 0, 
                 dist_S_D  = 0,
                 dist_PN_D = 0,
                 dist_N_PN = 0,
                 dist_N_E  = 0,
                 dist_E_D  = 0,
                 i = 0;

    assert(is_broadcast_link(protected_link, level));
    boolean is_dest_impacted = FALSE,
             MANDATORY_NODE_PROTECTION = FALSE;

    PN = protected_link->to.node;
    Compute_and_Store_Forward_SPF(PN, level);  

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node){

        D_res = list_node->data;
        D = D_res->node;
        if(D == S) continue;

        memset(impact_reason, 0, STRING_REASON_LEN);

        sprintf(instance->traceopts->b, "Node : %s : LFA computation for Destination %s begin", S->node_name, D->node_name); 
        trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
        
        MANDATORY_NODE_PROTECTION = FALSE;
        is_dest_impacted = is_destination_impacted(S, protected_link, D, level, impact_reason,
                            &MANDATORY_NODE_PROTECTION);
        sprintf(instance->traceopts->b, "Dest = %s Impact result = %s\n    reason : %s", D->node_name, 
                    is_dest_impacted ? "IMPACTED" : "NOT-IMPCATED", impact_reason); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

        if(is_dest_impacted == FALSE) continue;
        
        dist_S_D = D_res->spf_metric;
        dist_PN_D = DIST_X_Y(PN, D, level);

        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(S, N, pn_node, edge1, edge2, level){
            
            lfa_type = UNKNOWN_LFA_TYPE;
            sprintf(instance->traceopts->b, "Node : %s : Testing nbr %s via edge1 = %s, edge2 = %s for LFA candidature",
                    S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); 
            trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            /*Do not consider the link being protected to find LFA*/
            if(edge1 == protected_link){
                sprintf(instance->traceopts->b, "Node : %s : Nbr %s with OIF %s is same as protected link %s, skipping this nbr from LFA candidature", 
                        S->node_name, N->node_name, edge1->from.intf_name, protected_link->from.intf_name); 
                trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }

            if(IS_OVERLOADED(N, level)){
                sprintf(instance->traceopts->b, "Node : %s : Nbr %s failed for LFA candidature, reason - Overloaded", S->node_name, N->node_name); 
                trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }

            dist_N_S = DIST_X_Y(N, S, level);
            sprintf(instance->traceopts->b, "Node : %s : Source(S) = %s, probable LFA(N) = %s, DEST(D) = %s", 
                    S->node_name, S->node_name, N->node_name, D->node_name); 
            trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            dist_N_D = DIST_X_Y(N, D, level);
            sprintf(instance->traceopts->b, "Node : %s : Testing inequality 1 : dist_N_D(%u) < dist_N_S(%u) + dist_S_D(%u)",
                    S->node_name, dist_N_D, dist_N_S, dist_S_D); 
            trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            /* Apply inequality 1*/
            if(!(dist_N_D < dist_N_S + dist_S_D)){
                sprintf(instance->traceopts->b, "Node : %s : Inequality 1 failed", S->node_name); 
                trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }

            sprintf(instance->traceopts->b, "Node : %s : Inequality 1 passed", S->node_name); 
            trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
             
            /* Inequality 3 : Node protecting LFA 
             * All primary nexthop MUST qualify node protection inequality # 3*/
            if(IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){

                sprintf(instance->traceopts->b, "Node : %s : Testing node protecting inequality 3 with primary nexthops of %s through potential LFA %s",
                        S->node_name, D->node_name, N->node_name); 
                trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                /*N is node protecting LFA if it could send traffic to D without passing
                 * through ALL primary next hops of D*/
                ITERATE_NH_TYPE_BEGIN(nh){
                    for(i = 0; i < MAX_NXT_HOPS; i++){
                        prim_nh = D_res->next_hop[nh][i].node;
                        if(!prim_nh) break;     
                        dist_N_E = DIST_X_Y(N, prim_nh, level);
                        dist_E_D = DIST_X_Y(prim_nh, D, level);

                        if(dist_N_D < dist_N_E + dist_E_D){
                            lfa_type = BROADCAST_ONLY_NODE_PROTECTION_LFA;  
                            sprintf(instance->traceopts->b, "Node : %s : inequality 3 Passed with #%u next hop %s(%s)",
                                    S->node_name, i, prim_nh->node_name, nh == IPNH ? "IPNH" : "LSPNH"); 
                            trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                        }else{
                            lfa_type = UNKNOWN_LFA_TYPE;
                            sprintf(instance->traceopts->b, "Node : %s : inequality 3 Failed with #%u next hop %s(%s), ", 
                                    S->node_name, i, prim_nh->node_name, nh == IPNH ? "IPNH" : "LSPNH"); 
                            trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                            break;
                        }
                    }
                    if(lfa_type == UNKNOWN_LFA_TYPE ) break;
                } ITERATE_NH_TYPE_END;
            }

            if(lfa_type == BROADCAST_ONLY_NODE_PROTECTION_LFA){
                /*code to record the back up next hop*/
                backup_nh_type = edge1->etype == UNICAST ? IPNH : LSPNH;
                backup_nh = get_next_hop_empty_slot(D->backup_next_hop[level][backup_nh_type]);
                backup_nh->level = level;
                backup_nh->oif = &edge1->from;
                backup_nh->protected_link = &protected_link->from;
                backup_nh->node = N;
                if(backup_nh_type == IPNH)
                    set_next_hop_gw_pfx(*backup_nh, edge2->to.prefix[level]->prefix);
                backup_nh->nh_type = backup_nh_type;
                backup_nh->lfa_type = lfa_type;
                backup_nh->proxy_nbr = NULL;
                backup_nh->rlfa = NULL;
                //backup_nh->mpls_label_in = 0;
                backup_nh->root_metric = DIST_X_Y(S, N, level);
                backup_nh->dest_metric = dist_N_D;
                backup_nh->is_eligible = TRUE;

                sprintf(instance->traceopts->b, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s," 
                              "looking to promote it to BROADCAST_LINK_AND_NODE_PROTECTION_LFA", N->node_name, 
                        backup_nh->oif->intf_name, backup_nh->node->node_name, 
                        get_str_lfa_type(backup_nh->lfa_type)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                /*Check for Link protection criteria*/
                sprintf(instance->traceopts->b, "Node : %s : Testing inequality 4 : dist_N_D(%u) < dist_N_PN(%u) + dist_PN_D(%u)",
                        S->node_name, dist_N_D, dist_N_PN, dist_PN_D); 
                trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                /*Apply inequality 4*/
                dist_N_PN = DIST_X_Y(N, PN, level);
                if(!(dist_N_D < dist_N_PN + dist_PN_D)){
                    sprintf(instance->traceopts->b, "Node : %s : Inequality 4 failed, LFA not promoted to BROADCAST_LINK_AND_NODE_PROTECTION_LFA", S->node_name); 
                    trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    goto NBR_PROCESSING_DONE;
                }
                sprintf(instance->traceopts->b, "Node : %s : Inequality 4 passed, LFA %s(OIF = %s) , Dest = %s promoted from %s to %s", 
                        S->node_name, N->node_name, backup_nh->oif->intf_name, backup_nh->node->node_name,
                        get_str_lfa_type(backup_nh->lfa_type),
                        get_str_lfa_type(BROADCAST_LINK_AND_NODE_PROTECTION_LFA)); 
                trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                backup_nh->lfa_type = BROADCAST_LINK_AND_NODE_PROTECTION_LFA;
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, N, pn_node, level);
            }

            /*We are here because LFA is not node protecting, try for link protection LFA only*/
            if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                sprintf(instance->traceopts->b, "Node : %s : Node-link-degradation Disabled, Nbr %s not considered for link protection LFA", 
                        S->node_name, N->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }
           
            if(MANDATORY_NODE_PROTECTION == TRUE){
                sprintf(instance->traceopts->b, "Node : %s : Nbr %s not considered for link protection LFA as it has ECMP",
                            S->node_name, N->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }
            
            if(strict_down_stream_lfa){
                /* 4. Narrow down the subset further using inequality 2 */
                sprintf(instance->traceopts->b, "Node : %s : Testing inequality 2 : dist_N_D(%u) < dist_S_D(%u)", 
                        S->node_name, dist_N_D, dist_S_D); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                if(!(dist_N_D < dist_S_D)){
                    sprintf(instance->traceopts->b, "Node : %s : Inequality 2 failed", S->node_name); 
                    trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    goto NBR_PROCESSING_DONE;
                }
                sprintf(instance->traceopts->b, "Node : %s : Inequality 2 passed, lfa promoted from %s to %s", S->node_name, 
                                get_str_lfa_type(lfa_type), get_str_lfa_type(LINK_PROTECTION_LFA_DOWNSTREAM)); 
                trace(instance->traceopts, BACKUP_COMPUTATION_BIT); 
            }

            /*Now check inequality 4*/ 
            dist_N_PN = DIST_X_Y(N, PN, level);
            sprintf(instance->traceopts->b, "Node : %s : Testing inequality 4 : dist_N_D(%u) < dist_N_PN(%u) + dist_PN_D(%u)",
                    S->node_name, dist_N_D, dist_N_PN, dist_PN_D); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            /*Apply inequality 4*/
            if(!(dist_N_D < dist_N_PN + dist_PN_D)){
                sprintf(instance->traceopts->b, "Node : %s : Inequality 4 failed, LFA candidature failed for nbr %s, Dest = %s",
                              S->node_name, N->node_name, D->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }

            sprintf(instance->traceopts->b, "Node : %s : Inequality 4 passed for Nbr %s is LFA for Dest =  %s",
                        S->node_name, N->node_name, D->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            /*Record the LFA*/
            backup_nh_type = edge1->etype == UNICAST ? IPNH : LSPNH;
            backup_nh = get_next_hop_empty_slot(D->backup_next_hop[level][backup_nh_type]);
            backup_nh->level = level;
            backup_nh->oif = &edge1->from;
            backup_nh->protected_link = &protected_link->from;
            backup_nh->node = N;
            if(backup_nh_type == IPNH)
                set_next_hop_gw_pfx(*backup_nh, edge2->to.prefix[level]->prefix);
            backup_nh->nh_type = backup_nh_type;
            backup_nh->lfa_type = lfa_type;
            backup_nh->proxy_nbr = NULL;
            backup_nh->rlfa = NULL;
            //backup_nh->mpls_label_in = 0;
            backup_nh->root_metric = DIST_X_Y(S, N, level);
            backup_nh->dest_metric = dist_N_D;
            backup_nh->is_eligible = TRUE;

            sprintf(instance->traceopts->b, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s", N->node_name, 
                    backup_nh->oif->intf_name, backup_nh->node->node_name, 
                    get_str_lfa_type(backup_nh->lfa_type)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

NBR_PROCESSING_DONE:
            sprintf(instance->traceopts->b, "Node : %s : Testing nbr %s via edge1 = %s edge2 = %s for LFA candidature Done", 
                S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
        } ITERATE_NODE_PHYSICAL_NBRS_END(S, N, pn_node, level);
        
    } ITERATE_LIST_END;
}

/* In case of LFAs, the LFA is promoted to Node protecting LFA if they
 * meet the node protecting criteria*/

static void
p2p_compute_link_node_protection_lfas(node_t * S, edge_t *protected_link, 
                            LEVEL level, 
                            boolean strict_down_stream_lfa){

    node_t *E = NULL, 
    *N = NULL, 
    *D = NULL,
    *prim_nh = NULL,
    *pn_node = NULL;

    edge_t *edge1 = NULL, *edge2 = NULL;

    boolean is_dest_impacted = FALSE,
            MANDATORY_NODE_PROTECTION = FALSE;
    char impact_reason[STRING_REASON_LEN];

    spf_result_t *D_res = NULL;
    singly_ll_node_t *list_node = NULL;

    unsigned int dist_N_D = 0, 
                 dist_N_S = 0, 
                 dist_S_D = 0,
                 dist_N_E = 0,
                 dist_E_D = 0,
                 i = 0;

    nh_type_t nh = NH_MAX;
    lfa_type_t lfa_type = UNKNOWN_LFA_TYPE;

    /* 3. Filter nbrs of S using inequality 1 */
    E = protected_link->to.node;

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node){
        D_res = list_node->data;
        D = D_res->node;
        is_dest_impacted = FALSE;

        if(D == S) continue; 
        memset(impact_reason, 0, STRING_REASON_LEN);

        sprintf(instance->traceopts->b, "Node : %s : LFA computation for Destination %s begin for protected link (%s)", 
            S->node_name, D->node_name, protected_link->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
        
        MANDATORY_NODE_PROTECTION = FALSE;
        is_dest_impacted = is_destination_impacted(S, protected_link, D, level, impact_reason, 
                             &MANDATORY_NODE_PROTECTION);
        sprintf(instance->traceopts->b, "Dest = %s Impact result = %s\n    reason : %s", D->node_name, 
                    is_dest_impacted ? "IMPACTED" : "NOT-IMPCATED", impact_reason); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    
        if(is_dest_impacted == FALSE) continue;

        dist_S_D = D_res->spf_metric;
        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(S, N, pn_node, edge1, edge2, level){

            sprintf(instance->traceopts->b, "Node : %s : Testing nbr %s via edge1(%s) = %s, edge2(%s) = %s for LFA candidature",
                    S->node_name, N->node_name, edge1->status == 1 ? "UP" : "DOWN", 
                    edge1->from.intf_name,
                    edge2->status == 1 ? "UP" : "DOWN", 
                    edge2->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
            
            /*Do not consider the link being protected to find LFA*/
            if(edge1 == protected_link){
                sprintf(instance->traceopts->b, "Node : %s : Nbr %s with OIF %s is same as protected link %s, skipping this nbr from LFA candidature", 
                        S->node_name, N->node_name, edge1->from.intf_name, protected_link->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }

            if(IS_OVERLOADED(N, level)){
                sprintf(instance->traceopts->b, "Node : %s : Nbr %s failed for LFA candidature, reason - Overloaded", 
                S->node_name, N->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }

            dist_N_S = DIST_X_Y(N, S, level);
            sprintf(instance->traceopts->b, "Node : %s : Source(S) = %s, probable LFA(N) = %s, DEST(D) = %s, Primary NH(E) = %s", 
                    S->node_name, S->node_name, N->node_name, D->node_name, E->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            dist_N_D = DIST_X_Y(N, D, level);
            sprintf(instance->traceopts->b, "Node : %s : Testing inequality 1 : dist_N_D(%u) < dist_N_S(%u) + dist_S_D(%u)",
                    S->node_name, dist_N_D, dist_N_S, dist_S_D); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

            /* Apply inequality 1*/
            if(!(dist_N_D < dist_N_S + dist_S_D)){
                sprintf(instance->traceopts->b, "Node : %s : Inequality 1 failed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }

            sprintf(instance->traceopts->b, "Node : %s : Inequality 1 passed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
            
            /* Inequality 3 : Node protecting LFA 
             * All primary nexthop MUST qualify node protection inequality # 3*/
            if(IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){

                sprintf(instance->traceopts->b, "Node : %s : Testing node protecting inequality 3 with primary nexthops of %s through potential LFA %s",
                        S->node_name, D->node_name, N->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                /*N is node protecting LFA if it could send traffic to D without passing
                 * through ALL primary next hops of D*/
                ITERATE_NH_TYPE_BEGIN(nh){
                    for(i = 0; i < MAX_NXT_HOPS; i++){
                        prim_nh = D_res->next_hop[nh][i].node;
                        if(!prim_nh) break;     
                        dist_N_E = DIST_X_Y(N, prim_nh, level);
                        dist_E_D = DIST_X_Y(prim_nh, D, level);

                        if(dist_N_D < dist_N_E + dist_E_D){
                            lfa_type = LINK_AND_NODE_PROTECTION_LFA;  
                            sprintf(instance->traceopts->b, "Node : %s : inequality 3 Passed with #%u next hop %s(%s), lfa_type = %s",
                                    S->node_name, i, prim_nh->node_name, nh == IPNH ? "IPNH" : "LSPNH", get_str_lfa_type(lfa_type)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                        }else{
                            lfa_type = UNKNOWN_LFA_TYPE; 
                            sprintf(instance->traceopts->b, "Node : %s : inequality 3 Failed with #%u next hop %s(%s), lfa_type = %s", 
                                    S->node_name, i, prim_nh->node_name, nh == IPNH ? "IPNH" : "LSPNH", get_str_lfa_type(lfa_type)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                            break;
                        }
                    }
                    if(lfa_type == UNKNOWN_LFA_TYPE) break;
                } ITERATE_NH_TYPE_END;
            }

            if(lfa_type == LINK_AND_NODE_PROTECTION_LFA){
                /*Record the LFA*/ 
                sprintf(instance->traceopts->b, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s", N->node_name, 
                        edge1->from.intf_name, D->node_name, 
                        get_str_lfa_type(lfa_type)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                {
                    /*code to record the back up next hop*/
                    nh_type_t backup_nh_type = edge1->etype == UNICAST ? IPNH : LSPNH;
                    internal_nh_t *backup_nh = 
                            get_next_hop_empty_slot(D->backup_next_hop[level][backup_nh_type]);

                    backup_nh->level = level;
                    backup_nh->oif = &edge1->from;
                    backup_nh->protected_link = &protected_link->from;
                    backup_nh->node = N;
                    if(backup_nh_type == IPNH)
                        set_next_hop_gw_pfx(*backup_nh, edge1->to.prefix[level]->prefix);
                    backup_nh->nh_type = backup_nh_type;
                    backup_nh->lfa_type = lfa_type;
                    backup_nh->proxy_nbr = NULL;
                    backup_nh->rlfa = NULL;
                    //backup_nh->mpls_label_in = 0;
                    backup_nh->root_metric = DIST_X_Y(S, N, level);
                    backup_nh->dest_metric = dist_N_D;
                    backup_nh->is_eligible = TRUE;
                }
                goto NBR_PROCESSING_DONE;
            }

            if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                sprintf(instance->traceopts->b, "Node : %s : Node-link-degradation Disabled, Nbr %s not considered for link protection LFA", 
                        S->node_name, N->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }
          
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
            if(MANDATORY_NODE_PROTECTION == TRUE){
                sprintf(instance->traceopts->b, "Node : %s : Nbr %s not considered for link protection LFA as it has ECMP",
                            S->node_name, N->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                goto NBR_PROCESSING_DONE;
            }

            if(strict_down_stream_lfa){
                /* 4. Narrow down the subset further using inequality 2 */
                sprintf(instance->traceopts->b, "Node : %s : Testing inequality 2 : dist_N_D(%u) < dist_S_D(%u)", 
                        S->node_name, dist_N_D, dist_S_D); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

                if(!(dist_N_D < dist_S_D)){
                    sprintf(instance->traceopts->b, "Node : %s : Inequality 2 failed", S->node_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
                    ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, N, pn_node, level);
                }
                sprintf(instance->traceopts->b, "Node : %s : Inequality 2 passed, lfa promoted from %s to %s", S->node_name, 
                                get_str_lfa_type(lfa_type), get_str_lfa_type(LINK_PROTECTION_LFA_DOWNSTREAM)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT); 
                lfa_type = LINK_PROTECTION_LFA_DOWNSTREAM;
            }
            
            /*We are here because inequality 1 and 2 passed, but 3 fails*/ 
            /*Record the LFA*/ 
            {
                /*code to record the back up next hop*/
                nh_type_t backup_nh_type = edge1->etype == UNICAST ? IPNH : LSPNH;
                internal_nh_t *backup_nh = 
                    get_next_hop_empty_slot(D->backup_next_hop[level][backup_nh_type]);

                backup_nh->level = level;
                backup_nh->oif = &edge1->from;
                backup_nh->protected_link = &protected_link->from;
                backup_nh->node = N;
                if(backup_nh_type == IPNH)
                    set_next_hop_gw_pfx(*backup_nh, edge1->to.prefix[level]->prefix);
                backup_nh->nh_type = backup_nh_type;
                backup_nh->lfa_type = lfa_type;
                backup_nh->proxy_nbr = NULL;
                backup_nh->rlfa = NULL;
                //backup_nh->mpls_label_in = 0;
                backup_nh->root_metric = DIST_X_Y(S, N, level);
                backup_nh->dest_metric = dist_N_D;
                backup_nh->is_eligible = TRUE;
            }
            sprintf(instance->traceopts->b, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s", N->node_name, 
                    edge1->from.intf_name, D->node_name, get_str_lfa_type(lfa_type)); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

NBR_PROCESSING_DONE:
        sprintf(instance->traceopts->b, "Node : %s : Testing nbr %s via edge1 = %s edge2 = %s for LFA candidature Done", 
                S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); trace(instance->traceopts, BACKUP_COMPUTATION_BIT);

        } ITERATE_NODE_PHYSICAL_NBRS_END(S, N, pn_node, level);

    } ITERATE_LIST_END;
}

void 
compute_lfa(node_t * S, edge_t *protected_link,
            LEVEL level,
            boolean strict_down_stream_lfa){

     /*LFA computation is possible only for unicast links*/
     assert(protected_link->etype == UNICAST);
#if 0 // caller suppose to run all necessary SPF runs
    /* Run necessary SPF runs required to compute LFAs 
     * of any type - p2p link/node protection or broadcast link
     * node protecting LFAs*/

     /* 1. Run SPF on S to know DIST(S,D) */
     Compute_and_Store_Forward_SPF(S, level);
     /* 2. Run SPF on all nbrs of S to know DIST(N,D) and DIST(N,S)*/
     Compute_PHYSICAL_Neighbor_SPFs(S, level); 
#endif
     if(is_broadcast_link(protected_link, level) == FALSE)
         p2p_compute_link_node_protection_lfas(S, protected_link, level, TRUE); 
     else
         broadcast_compute_link_node_protection_lfas(S, protected_link, level, TRUE);
}

void
compute_rlfa(node_t * S, edge_t *protected_link,
            LEVEL level,
            boolean strict_down_stream_lfa){

     /*R LFA computation is possible only for unicast links*/
     assert(protected_link->etype == UNICAST);
     Compute_and_Store_Forward_SPF(S, level);
     Compute_PHYSICAL_Neighbor_SPFs(S, level); 
     init_back_up_computation(S, level);

     if(is_broadcast_link(protected_link, level) == FALSE){
        p2p_compute_link_node_protecting_extended_p_space(S, protected_link, level);
        p2p_filter_select_pq_nodes_from_ex_pspace(S, protected_link, level);
     }
     else{
         broadcast_compute_link_node_protecting_extended_p_space(S, protected_link, level);
         broadcast_filter_select_pq_nodes_from_ex_pspace(S, protected_link, level); 
     }
}


