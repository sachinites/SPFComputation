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

static void 
clear_pq_nodes(node_t *S, LEVEL level){
    unsigned int i = 0;
    for(i=0; i < MAX_NXT_HOPS; i++){
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
       res->node->p_space_protection_type = 0;
       res->node->q_space_protection_type = 0;
       ITERATE_NH_TYPE_BEGIN(nh){
        copy_nh_list2(res->node->backup_next_hop[level][nh], 
            res->node->old_backup_next_hop[level][nh]); 
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
                        boolean *IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED){

    /*case 1 : Destination has only 1 next hop through protected_link, return TRUE*/
    unsigned int nh_count = 0;
    nh_type_t nh = NH_MAX;
    internal_nh_t *primary_nh = NULL;
    spf_result_t *D_res = NULL;
    node_t *E = protected_link->to.node;
    
    unsigned int d_prim_nh_to_D = 0,
                 d_prim_nh_to_E = 0,
                 d_E_to_D       = 0,
                 i = 0;

    assert(IS_LEVEL_SET(protected_link->level, level));
    *IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE;

    if(!IS_LINK_PROTECTION_ENABLED(protected_link) &&
            !IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
        printf("No protection enabled on the link\n");
        return FALSE;
    }

    D_res = GET_SPF_RESULT((&S->spf_info), D, level);
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
        if(primary_nh->oif == &protected_link->from){
            sprintf(impact_reason, "Dest %s only primary nxt hop oif(%s) = protected_link(%s)", 
                    D->node_name, primary_nh->oif->intf_name, protected_link->from.intf_name);
            return TRUE;
        }

        if(IS_LINK_PROTECTION_ENABLED(protected_link) &&
           !IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
            /*D 's only primary nexthop is different from protected_link*/
            sprintf(impact_reason, "Dest %s only primary nxt hop oif(%s) != protected_link(%s) AND Only LINK_PROTECTION Enabled",
                D->node_name, primary_nh->oif->intf_name, protected_link->from.intf_name);
            return FALSE;
        }

        /*if Node protection is enabled, primary nexthop is different from E and 
         * should not traverse E*/
        if(IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
            if(primary_nh->node == E){
                sprintf(impact_reason, "Dest %s only primary nxt hop node(%s) = protected_link next hop node (%s) AND LINK_NODE_PROTECTION Enabled",
                    D->node_name, primary_nh->node->node_name, E->node_name);
                return TRUE;
            }

            d_prim_nh_to_D = DIST_X_Y(primary_nh->node, D, level);
            d_prim_nh_to_E = DIST_X_Y(primary_nh->node, E, level);
            d_E_to_D = DIST_X_Y(E, D, level);

            if(d_prim_nh_to_D < d_prim_nh_to_E + d_E_to_D){
                sprintf(impact_reason, "Dest %s only primary nxt hop node(%s) do not traverse protected_link next hop node (%s) AND LINK_NODE_PROTECTION Enabled",
                    D->node_name, primary_nh->node->node_name, E->node_name);
                return FALSE;
            }
            else{
                sprintf(impact_reason, "Dest %s only primary nxt hop node(%s) traverses protected_link next hop node (%s) AND LINK_NODE_PROTECTION Enabled",
                    D->node_name, primary_nh->node->node_name, E->node_name);
                return TRUE;    
            }
        }
    }

    if(nh_count > 1){
        if(IS_LINK_PROTECTION_ENABLED(protected_link) &&
                !IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
            sprintf(impact_reason, "Dest %s has ECMP primary nxt hop count = %u AND Only LINK_PROTECTION Enabled",
                        D->node_name, nh_count);
            return FALSE;/*ECMP case with only link protection*/
        }
        if(IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){
            /*None of primary next hops should traverse the E*/
            ITERATE_NH_TYPE_BEGIN(nh){
                for(i = 0; i < MAX_NXT_HOPS; i++){
                    primary_nh = &D_res->next_hop[nh][i];
                    if(is_nh_list_empty2(&(D_res->next_hop[nh][i])))
                        break;
                    d_prim_nh_to_D = DIST_X_Y(primary_nh->node, D, level);
                    d_prim_nh_to_E = DIST_X_Y(primary_nh->node, E, level);
                    d_E_to_D = DIST_X_Y(E, D, level);
                    /*Atleast one primary nxt hop do not traverse E, ECMP case*/
                    if(d_prim_nh_to_D < d_prim_nh_to_E + d_E_to_D){
                        sprintf(impact_reason, "Dest %s has ECMP primary nxt hop count = %u," 
                            "but primary nxt hop node(%s) do not traverse protected_link next hop"
                            "node (%s) AND LINK_NODE_PROTECTION Enabled",
                            D->node_name, nh_count, primary_nh->node->node_name, E->node_name);
                        return FALSE;
                    }
                }
            }ITERATE_NH_TYPE_END;
            /* ToDo : We must not compute Link protection LFA/RLFA for such destination, only node
             * protection back up is needed*/
            sprintf(impact_reason, "Dest %s has ECMP primary nxt hop count = %u,"
                    "but all primary nxt hop nodes traverses protected_link next hop"
                    "node (%s) AND LINK_NODE_PROTECTION Enabled. No only-link protecting backup needed",
                    D->node_name, nh_count, E->node_name);
            *IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = FALSE;
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


/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the p-space of 'node' wrt to 'edge'.
 *  Note that, 'edge' need not be directly connected edge of 'node'
 *-----------------------------------------------------------------------------*/

p_space_set_t 
p2p_compute_p_space(node_t *S, edge_t *protected_link, LEVEL level){
    
    spf_result_t *res = NULL;
    singly_ll_node_t* list_node = NULL;
    node_t *E = NULL;

    unsigned int dist_E_to_y = 0,
                 dist_S_to_y = 0,
                 dist_S_to_E = 0;
    
    assert(!is_broadcast_link(protected_link, level));

    E = protected_link->to.node;
    Compute_and_Store_Forward_SPF(S, level);
    Compute_and_Store_Forward_SPF(E, level);

    ll_t *spf_result = S->spf_run_result[level];

    p_space_set_t p_space = init_singly_ll();
    singly_ll_set_comparison_fn(p_space, instance_node_comparison_fn);

    /*Iterate over spf_result and filter out all those nodes whose next hop is 
     * protected_link->to.node */

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
p_space_set_t
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
    p_space_set_t ex_p_space = NULL;
    singly_ll_node_t *list_node1 = NULL;

    unsigned int d_nbr_to_p_node = 0,
                 d_nbr_to_S = 0,
                 d_S_to_p_node = 0,
                 d_nbr_to_PN = 0,
                 d_PN_to_p_node = 0;


    spf_result_t *spf_result_p_node = NULL;

    if(!IS_LEVEL_SET(protected_link->level, level))
        return NULL;

    assert(is_broadcast_link(protected_link, level));

    ex_p_space = init_singly_ll();
    singly_ll_set_comparison_fn(ex_p_space, instance_node_comparison_fn);

    boolean is_node_protection_enabled = 
        IS_LINK_NODE_PROTECTION_ENABLED(protected_link);
    boolean is_link_protection_enabled = 
        IS_LINK_PROTECTION_ENABLED(protected_link);

    /*run spf on self*/
    Compute_and_Store_Forward_SPF(S, level); 
    /*Run SPF on all logical nbrs of S*/
    Compute_PHYSICAL_Neighbor_SPFs(S, level);

    /*iterate over entire network. Note that node->spf_run_result list
     * carries all nodes of the network reachable from source at level l.
     * We deem this list as the "entire network"*/ 
    PN = protected_link->to.node;

    sprintf(LOG, "Node : %s : Begin ext-pspace computation for S=%s, protected-link = %s, LEVEL = %s",
            S->node_name, S->node_name, protected_link->from.intf_name, get_str_level(level)); TRACE();

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
            d_nbr_to_p_node = DIST_X_Y(nbr_node, P_node, level);
            d_PN_to_p_node = DIST_X_Y(PN, P_node, level);

            if(is_node_protection_enabled == TRUE){

                /*If node protection is enabled*/
                d_PN_to_p_node = DIST_X_Y(PN, P_node, level);

                /*Loop free inequality 1 : N should be Loop free wrt S and PN*/
                sprintf(LOG, "Node : %s : Testing inequality 1 : checking loop free wrt S = %s, Nbr = %s(oif = %s), P_node = %s",
                        S->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name, P_node->node_name); TRACE(); 

                d_nbr_to_S = DIST_X_Y(nbr_node, S, level);
                sprintf(LOG, "Node : %s : d_nbr_to_p_node(%u) < d_nbr_to_S(%u) + d_S_to_p_node(%u)",
                        S->node_name, d_nbr_to_p_node, d_nbr_to_S, d_S_to_p_node); TRACE();

                if(!(d_nbr_to_p_node < d_nbr_to_S + d_S_to_p_node)){
                    sprintf(LOG, "Node : %s : inequality 1 failed, Nbr = %s(oif = %s) is not loop free wrt S", 
                            S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                    continue;
                }
                sprintf(LOG, "Node : %s : above inequality 1 passed", S->node_name); TRACE();
                sprintf(LOG, "Node : %s : Testing node protection inequality for Broadcast link: S = %s, nbr = %s, P_node = %s, PN = %s",
                        S->node_name, S->node_name, nbr_node->node_name, P_node->node_name, PN->node_name); TRACE();
                /*Node protection criteria for broadcast link should be : Nbr should be able to send traffic to P_node wihout 
                 * passing through any node attached to broadcast segment*/

                if(broadcast_node_protection_critera(S, level, protected_link, P_node, nbr_node) == TRUE){
                    sprintf(LOG, "Node : %s : Above node protection inequality passed", S->node_name); TRACE();
                    SET_BIT(P_node->p_space_protection_type, ONLY_NODE_PROTECTION);
                    
                    /*Check for link protection, nbr_node should be loop free wrt to PN*/
                    sprintf(LOG, "Node : %s : Checking if potential P_node = %s provide broadcast link protection to S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                    sprintf(LOG, "Node : %s : Checking if Nbr  = %s(oif=%s) is  loop free wrt to PN = %s", 
                            S->node_name, nbr_node->node_name, edge1->from.intf_name, PN->node_name); TRACE();
                    singly_ll_add_node_by_val(ex_p_space, P_node);
                    /*For link protection, Nbr should be loop free wrt to PN*/
                    if(d_nbr_to_p_node < (d_nbr_to_PN + d_PN_to_p_node)){
                        UNSET_BIT(P_node->p_space_protection_type, ONLY_NODE_PROTECTION);
                        SET_BIT(P_node->p_space_protection_type, LINK_NODE_PROTECTION);
                        sprintf(LOG, "Node : %s : P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                                S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                    }
                    continue;
                }

                sprintf(LOG, "Node : %s : Above node protection inequality failed", S->node_name); TRACE();

                if(is_link_protection_enabled == FALSE){
                    sprintf(LOG, "Node : %s :  Link degradation is disabled, candidate P_node = %s"
                            " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)", 
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                    continue;
                }
                /*P_node could not provide node protection, check for link protection*/
                if(is_link_protection_enabled == TRUE){
                    sprintf(LOG, "Node : %s : Checking if potential P_node = %s provide broadcast link protection to S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                    sprintf(LOG, "Node : %s : Checking if Nbr  = %s(oif=%s) is  loop free wrt to PN = %s", 
                            S->node_name, nbr_node->node_name, edge1->from.intf_name, PN->node_name); TRACE();
                    
                    /*For link protection, Nbr should be loop free wrt to PN*/
                    if(d_nbr_to_p_node < (d_nbr_to_PN + d_PN_to_p_node)){
                        SET_BIT(P_node->p_space_protection_type, LINK_PROTECTION);
                        singly_ll_add_node_by_val(ex_p_space, P_node);
                        sprintf(LOG, "Node : %s : P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                                S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                        continue;
                    }
                    sprintf(LOG, "Node : %s : candidate P_node = %s do not provide link protection" 
                            " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)",
                            S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                }
            }
        } ITERATE_LIST_END;
    } ITERATE_NODE_PHYSICAL_NBRS_END(S, nbr_node, pn_node, level);
    return ex_p_space;
}   

/*-----------------------------------------------------------------------------
 *  This routine returns the set of routers in the extended p-space of 'node' wrt to
 *  'edge'. Note that, 'edge' need not be directly connected edge of 'node'.
 *-----------------------------------------------------------------------------*/
p_space_set_t
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
    p_space_set_t ex_p_space = NULL;
    singly_ll_node_t *list_node1 = NULL;
    internal_nh_t *rlfa = NULL;

    unsigned int d_nbr_to_p_node = 0,
                 d_nbr_to_S = 0,
                 d_nbr_to_E = 0,
                 d_E_to_p_node = 0,
                 d_S_to_p_node = 0;

    spf_result_t *spf_result_p_node = NULL;

    if(!IS_LEVEL_SET(protected_link->level, level))
        return NULL;

    assert(!is_broadcast_link(protected_link, level));

    ex_p_space = init_singly_ll();
    singly_ll_set_comparison_fn(ex_p_space, instance_node_comparison_fn);

    boolean is_node_protection_enabled = 
        IS_LINK_NODE_PROTECTION_ENABLED(protected_link);
    boolean is_link_protection_enabled = 
        IS_LINK_PROTECTION_ENABLED(protected_link);

    /*run spf on self*/
    Compute_and_Store_Forward_SPF(S, level); 
    /*Run SPF on all logical nbrs of S*/
    Compute_PHYSICAL_Neighbor_SPFs(S, level);

    /*iterate over entire network. Note that node->spf_run_result list
     * carries all nodes of the network reachable from source at level l.
     * We deem this list as the "entire network"*/ 
    E = protected_link->to.node;

    sprintf(LOG, "Node : %s : Begin ext-pspace computation for S=%s, protected-link = %s, LEVEL = %s",
                  S->node_name, S->node_name, protected_link->from.intf_name, get_str_level(level)); TRACE();

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
                d_nbr_to_p_node = DIST_X_Y(nbr_node, P_node, level);

                if(is_node_protection_enabled == TRUE){

                    /*In case of link protection, we skip S---E link but not E, 
                     * in case of node protection we skip E altogether*/
                    if(nbr_node == E){
                        if(is_link_protection_enabled == TRUE){
                            sprintf(LOG, "Node : %s : Checking if potential P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                                    S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                            
                            d_nbr_to_S = DIST_X_Y(nbr_node, S, level);
                            if(d_nbr_to_p_node < (d_nbr_to_S + protected_link->metric[level])){
                                SET_BIT(P_node->p_space_protection_type, LINK_PROTECTION);
                                {
                                    rlfa = get_next_hop_empty_slot(S->pq_nodes[level]);
                                    rlfa->level = level;     
                                    rlfa->oif = &edge1->from;
                                    rlfa->node = NULL;
                                    if(edge1->etype == UNICAST)
                                        set_next_hop_gw_pfx(*rlfa, edge1->to.prefix[level]->prefix);
                                    rlfa->nh_type = LSPNH;
                                    rlfa->lfa_type = LINK_PROTECTION_RLFA;
                                    rlfa->proxy_nbr = nbr_node;
                                    rlfa->rlfa = P_node;
                                    rlfa->ldplabel = 1;
                                    rlfa->root_metric = edge1->metric[level] + d_nbr_to_p_node;
                                    rlfa->dest_metric = 0; /*Not known yet*/ 
                                    rlfa->is_eligible = TRUE; /*Not known yet*/
                                }
                                singly_ll_add_node_by_val(ex_p_space, P_node);
                                sprintf(LOG, "Node : %s : P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                                        S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                                continue;
                            }
                            sprintf(LOG, "Node : %s : candidate P_node = %s do not provide link protection" 
                                    " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)",
                                    S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                        }
                        continue;   
                    }
                    /*If node protection is enabled*/
                    d_E_to_p_node = DIST_X_Y(E, P_node, level);

                    /*Loop free inequality 1 : N should be Loop free wrt S*/
                    sprintf(LOG, "Node : %s : Testing inequality 1 : S = %s, Nbr = %s(oif = %s), P_node = %s",
                            S->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name, P_node->node_name); TRACE(); 

                    d_nbr_to_S = DIST_X_Y(nbr_node, S, level);
                    sprintf(LOG, "Node : %s : d_nbr_to_p_node(%u) < d_nbr_to_S(%u) + d_S_to_p_node(%u)",
                            S->node_name, d_nbr_to_p_node, d_nbr_to_S, d_S_to_p_node); TRACE();

                    if(!(d_nbr_to_p_node < d_nbr_to_S + d_S_to_p_node)){
                        sprintf(LOG, "Node : %s : inequality 1 failed", S->node_name); TRACE();
                        continue;
                    }
                    sprintf(LOG, "Node : %s : above inequality 1 passed", S->node_name); TRACE();

                    /*condition for node protection RLFA - RFC : 
                     * draft-ietf-rtgwg-rlfa-node-protection-13 - section 2.2.6.2*/
                    sprintf(LOG, "Node : %s : Testing node protection inequality : S = %s, nbr = %s, P_node = %s, E = %s",
                            S->node_name, S->node_name, nbr_node->node_name, P_node->node_name, E->node_name); TRACE();
                    sprintf(LOG, "Node : %s : d_nbr_to_p_node(%u) < d_nbr_to_E(%u) + d_E_to_p_node(%u)", 
                            S->node_name, d_nbr_to_p_node, d_nbr_to_E, d_E_to_p_node); TRACE();

                    if(d_nbr_to_p_node < (d_nbr_to_E + d_E_to_p_node)){
                        sprintf(LOG, "Node : %s : Above node protection inequality passed", S->node_name); TRACE();
                        /*Node has been added to extended p-space, no need to check for link protection
                         * as node-protecting node in extended pspace is automatically link protecting node for P2P links*/
                        SET_BIT(P_node->p_space_protection_type, LINK_NODE_PROTECTION);
                        singly_ll_add_node_by_val(ex_p_space, P_node);
                        {
                            rlfa = get_next_hop_empty_slot(S->pq_nodes[level]);
                            rlfa->level = level;     
                            rlfa->oif = &edge1->from;
                            rlfa->node = NULL;
                            if(edge1->etype == UNICAST)
                                set_next_hop_gw_pfx(*rlfa, edge1->to.prefix[level]->prefix);
                            rlfa->nh_type = LSPNH;
                            rlfa->lfa_type = LINK_AND_NODE_PROTECTION_RLFA;
                            rlfa->proxy_nbr = nbr_node;
                            rlfa->rlfa = P_node;
                            rlfa->ldplabel = 1;
                            rlfa->root_metric = edge1->metric[level] + d_nbr_to_p_node;
                            rlfa->dest_metric = 0; /*Not known yet*/ 
                            rlfa->is_eligible = TRUE; /*Not known yet*/
                        }
                        continue;
                    }
                    sprintf(LOG, "Node : %s : Above node protection inequality failed", S->node_name); TRACE();
                    if(is_link_protection_enabled == FALSE){
                        sprintf(LOG, "Node : %s :  Link degradation is disabled, candidate P_node = %s"
                                " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)", 
                                S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                        continue;
                    }
                    /*P_node could not provide node protection, check for link protection*/
                    if(is_link_protection_enabled == TRUE){
                        sprintf(LOG, "Node : %s : Checking if potential P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                                S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();

                        if(d_nbr_to_p_node < (d_nbr_to_S + protected_link->metric[level])){
                            SET_BIT(P_node->p_space_protection_type, LINK_PROTECTION);
                            {
                                rlfa = get_next_hop_empty_slot(S->pq_nodes[level]);
                                rlfa->level = level;     
                                rlfa->oif = &edge1->from;
                                rlfa->node = NULL;
                                if(edge1->etype == UNICAST)
                                    set_next_hop_gw_pfx(*rlfa, edge1->to.prefix[level]->prefix);
                                rlfa->nh_type = LSPNH;
                                rlfa->lfa_type = LINK_PROTECTION_RLFA;
                                rlfa->proxy_nbr = nbr_node;
                                rlfa->rlfa = P_node;
                                rlfa->ldplabel = 1;
                                rlfa->root_metric = edge1->metric[level] + d_nbr_to_p_node;
                                rlfa->dest_metric = 0; /*Not known yet*/ 
                                rlfa->is_eligible = TRUE; /*Not known yet*/
                            }
                            singly_ll_add_node_by_val(ex_p_space, P_node);
                            sprintf(LOG, "Node : %s : P_node = %s provide link protection to S = %s, Nbr = %s(oif=%s)",
                                    S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                            continue;
                        }
                        sprintf(LOG, "Node : %s : candidate P_node = %s do not provide link protection" 
                                     " rejected to qualify as p-node for S = %s, Nbr = %s(oif=%s)",
                                     S->node_name, P_node->node_name, S->node_name, nbr_node->node_name, edge1->from.intf_name); TRACE();
                    }
                }
            } ITERATE_LIST_END;
        } ITERATE_NODE_PHYSICAL_NBRS_END(S, nbr_node, pn_node, level);
    return ex_p_space;
}   

q_space_set_t
broadcast_filter_select_pq_nodes_from_ex_pspace(node_t *S, edge_t *protected_link, 
        LEVEL level, p_space_set_t ex_p_space){

    unsigned int d_p_to_E = 0,
                 d_p_to_D = 0,
                 d_E_to_D = 0;

    char impact_reason[STRING_REASON_LEN];
    node_t *E = protected_link->to.node,
           *p_node = NULL;
    
    spf_result_t *D_res = NULL;
    singly_ll_node_t *list_node = NULL,
                     *list_node1 = NULL;

    assert(is_broadcast_link(protected_link, level));

    q_space_set_t pq_space = init_singly_ll();
    singly_ll_set_comparison_fn(pq_space, instance_node_comparison_fn);

    /*Compute reverse SPF for nodes S and E as roots*/
    inverse_topology(instance, level);
    Compute_and_Store_Forward_SPF(S, level);
    Compute_and_Store_Forward_SPF(E, level);
    inverse_topology(instance, level);

    ITERATE_LIST_BEGIN(ex_p_space, list_node){
        p_node = list_node->data;

        assert(IS_BIT_SET(p_node->p_space_protection_type, LINK_PROTECTION) ||
                IS_BIT_SET(p_node->p_space_protection_type, LINK_NODE_PROTECTION)||
                IS_BIT_SET(p_node->p_space_protection_type, ONLY_NODE_PROTECTION));


        assert(IS_BIT_SET(p_node->p_space_protection_type, LINK_NODE_PROTECTION));
        /*For node protection, Run the Forward SPF run on PQ nodes*/
        Compute_and_Store_Forward_SPF(p_node, level);
        /*Now inspect all Destinations which are impacted by the link*/
        boolean is_dest_impacted = FALSE,
                 IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE; 

        ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node1){
            is_dest_impacted = FALSE;
            D_res = list_node1->data;

            /*if RLFA's proxy nbr itself is a destination, then no need to find
             * PQ node for such a destination. p_node->proxy_nbr will surely quality to be
             * LFA for such a destination*/
#if 0
            if(p_node->proxy_nbr == D_res->node)
                continue;
#endif
            /*Check if this is impacted destination*/
            memset(impact_reason, 0, STRING_REASON_LEN);

            IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE;
            is_dest_impacted = is_destination_impacted(S, protected_link, D_res->node, 
                        level, impact_reason, &IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED);
            sprintf(LOG, "Dest = %s Impact result = %s\n    reason : %s", D_res->node->node_name, 
                    is_dest_impacted ? "IMPACTED" : "NOT-IMPCATED", impact_reason); TRACE();

            if(is_dest_impacted == FALSE) continue;
        
            if(IS_BIT_SET(p_node->p_space_protection_type, LINK_PROTECTION)){
                /*This node cannot provide node protection, check only link protection
                 * p_node should be loop free wrt to PN*/
                if(IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED == FALSE){
                    sprintf(LOG, "Node : %s : Pnode  %s not considered for link protection RLFA as Dest %s has ECMP, failed to qualify as PQ node",
                            S->node_name, p_node->node_name, D_res->node->node_name); TRACE();
                    continue;
                }
                d_p_to_E = DIST_X_Y(E, p_node, level);
                d_p_to_D = DIST_X_Y(p_node, D_res->node, level);
                d_E_to_D = DIST_X_Y(E, D_res->node, level);
                if(!(d_p_to_D < d_p_to_E + d_E_to_D)){
                    sprintf(LOG, "Node : %s : Link protected p-node %s failed to qualify as link protection Q node",
                            S->node_name, p_node->node_name); TRACE();
                    /*p node fails to provide link protection, this do not qualifies to be pq node*/
                    continue;    
                }
                /*Doesnt matter if p_node qualifies node protection criteria, it will be link protecting only*/
                sprintf(LOG, "Node : %s : Link protected p-node %s qualify as link protection Q node",
                        S->node_name, p_node->node_name); TRACE();
                SET_BIT(p_node->q_space_protection_type, LINK_PROTECTION);
                singly_ll_add_node_by_val(pq_space, p_node);
                continue;
            }

            /*Check if p_node provides node protection*/
            if(broadcast_node_protection_critera(S, level, protected_link, D_res->node, p_node) == TRUE){
                /*This node provides node protection to Destination D*/
                sprintf(LOG, "Node : %s : Node protected p-node %s qualify as node protection Q node for for Dest %s",
                        S->node_name, p_node->node_name, D_res->node->node_name); TRACE();
                if(IS_BIT_SET(p_node->p_space_protection_type, LINK_NODE_PROTECTION)){
                    SET_BIT(p_node->q_space_protection_type, LINK_NODE_PROTECTION);
                }
                else{
                    SET_BIT(p_node->q_space_protection_type, ONLY_NODE_PROTECTION);
                }
                singly_ll_add_node_by_val(pq_space, p_node);   
                continue;
            }

            sprintf(LOG, "Node : %s : Node protected p-node %s failed to qualify as node protection Q node for Dest %s",
                S->node_name, p_node->node_name, D_res->node->node_name); TRACE();
            /*p_node fails to provide node protection, demote the p_node to LINK_PROTECTION
             * if it provides atleast link protection to Destination D*/
            if(IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED == FALSE){
                sprintf(LOG, "Node : %s : Pnode  %s not considered for link protection RLFA as Dest %s has ECMP, failed to qualify as PQ node",
                        S->node_name, p_node->node_name, D_res->node->node_name); TRACE();
                continue;
            }
            d_p_to_D = DIST_X_Y(p_node, D_res->node, level);
            d_p_to_E = DIST_X_Y(E, p_node, level);
            d_E_to_D = DIST_X_Y(E, D_res->node, level);
            if(!(d_p_to_D < d_p_to_E + d_E_to_D)){
                sprintf(LOG, "Node : %s : Node protected p-node %s failed to qualify as link protection Q node for Dest %s",
                            S->node_name, p_node->node_name, D_res->node->node_name); TRACE();
                continue;
            }
            SET_BIT(p_node->q_space_protection_type, LINK_PROTECTION);
            sprintf(LOG, "Node : %s : Node protected p-node %s qualify as link protection Q node"
                    "Demoted from LINK_NODE_PROTECTION to LINK_PROTECTION PQ node for Dest %s", 
                     S->node_name, p_node->node_name, D_res->node->node_name); TRACE();
            singly_ll_add_node_by_val(pq_space, p_node);
        }ITERATE_LIST_END;
    } ITERATE_LIST_END;
    return pq_space; 
}

q_space_set_t
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
        if(is_nh_list_empty2(p_node))
            break;
        /*This node cannot provide node protection, check only link protection*/
        d_p_to_S = DIST_X_Y(S, p_node->rlfa, level); 
        d_p_to_E = DIST_X_Y(E, p_node->rlfa, level);
        if(!(d_p_to_E < d_p_to_S + d_S_to_E)){
            sprintf(LOG, "Node : %s : p-node %s failed to qualify as link protection Q node",
                    S->node_name, p_node->rlfa->node_name); TRACE();
            /*p node fails to provide link protection, this do not qualifies to be pq node*/
            p_node->is_eligible = FALSE;
            continue;
        }
        /*Doesnt matter if p_node qualifies node protection criteria, it will be link protecting only*/
        sprintf(LOG, "Node : %s : p-node %s qualify as link protection Q node",
                S->node_name, p_node->rlfa->node_name); TRACE();
    }
    for( i = 0; i < MAX_NXT_HOPS; i++){
        p_node = &S->pq_nodes[level][i];
        if(is_nh_list_empty2(p_node)) break;
        if(p_node->is_eligible == FALSE) continue;

        /*For node protection, Run the Forward SPF run on PQ nodes*/
        Compute_and_Store_Forward_SPF(p_node->rlfa, level);
        /*Now inspect all Destinations which are impacted by the link*/
        boolean is_dest_impacted = FALSE,
                IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE;

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
            IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE;
            is_dest_impacted = is_destination_impacted(S, protected_link, D_res->node, 
                    level, impact_reason, &IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED);
            sprintf(LOG, "Dest = %s Impact result = %s\n    reason : %s", D_res->node->node_name, 
                    is_dest_impacted ? "IMPACTED" : "NOT-IMPCATED", impact_reason); TRACE();

            if(is_dest_impacted == FALSE) continue;

            /*Check if p_node provides node protection*/
            if(p_node->lfa_type == LINK_AND_NODE_PROTECTION_RLFA){
                d_p_to_D = DIST_X_Y(p_node->rlfa, D_res->node, level);
                d_E_to_D = DIST_X_Y(E, D_res->node, level);
                if(d_p_to_D < d_p_to_E + d_E_to_D){
                    /*This node provides node protection to Destination D*/
                    sprintf(LOG, "Node : %s : Node protected p-node %s qualify as node protection Q node for for Dest %s",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); TRACE();
                    p_node->dest_metric = d_p_to_D;
                    rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
                    copy_internal_nh_t(*p_node, *rlfa);
                    continue;
                }

                sprintf(LOG, "Node : %s : Node protected p-node %s failed to qualify as node protection Q node for Dest %s",
                        S->node_name, p_node->rlfa->node_name, D_res->node->node_name); TRACE();
                /*p_node fails to provide node protection, demote the p_node to LINK_PROTECTION
                 * if it provides atleast link protection to Destination D*/
                if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                    sprintf(LOG, "Node : %s : node link degradation is not enabled", S->node_name);
                    continue;
                }

                if(IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED == FALSE){
                    sprintf(LOG, "Node : %s : Pnode  %s not considered for link protection RLFA as Dest %s has ECMP, failed to qualify as PQ node",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); TRACE();
                    continue;
                }
                d_p_to_S = DIST_X_Y(S, p_node->rlfa, level);
                d_p_to_E = DIST_X_Y(E, p_node->rlfa, level);              
                if(!(d_p_to_D < d_p_to_S + protected_link->metric[level])){
                    sprintf(LOG, "Node : %s : Node protected p-node %s failed to qualify as link protection Q node for Dest %s",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); TRACE();
                    continue;
                }
                p_node->dest_metric = d_p_to_D;
                rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
                copy_internal_nh_t(*p_node, *rlfa);
                rlfa->lfa_type = LINK_PROTECTION_RLFA;
                sprintf(LOG, "Node : %s : Node protected p-node %s qualify as link protection Q node"
                        "Demoted from LINK_AND_NODE_PROTECTION_RLFA to LINK_PROTECTION_RLFA PQ node for Dest %s", 
                        S->node_name, p_node->rlfa->node_name, D_res->node->node_name); TRACE();
            }else if(p_node->lfa_type == LINK_PROTECTION_RLFA ||
                    p_node->lfa_type == LINK_PROTECTION_RLFA_DOWNSTREAM){
                if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                    continue;
                }
                if(IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED == FALSE){
                    sprintf(LOG, "Node : %s : Pnode  %s not considered for link protection RLFA as Dest %s has ECMP, failed to qualify as PQ node",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); TRACE();
                    continue;
                }
                d_p_to_S = DIST_X_Y(S, p_node->rlfa, level);
                d_p_to_E = DIST_X_Y(E, p_node->rlfa, level);              
                if(!(d_p_to_D < d_p_to_S + protected_link->metric[level])){
                    sprintf(LOG, "Node : %s : Link protected p-node %s failed to qualify as link protection Q node for Dest %s",
                            S->node_name, p_node->rlfa->node_name, D_res->node->node_name); TRACE();
                    continue;
                }
                p_node->dest_metric = d_p_to_D;
                rlfa = get_next_hop_empty_slot(D_res->node->backup_next_hop[level][LSPNH]);
                copy_internal_nh_t(*p_node, *rlfa);
                rlfa->lfa_type = LINK_PROTECTION_RLFA;
                sprintf(LOG, "Node : %s : link protected p-node %s qualify as link protection Q node for Dest %s",
                        S->node_name, p_node->rlfa->node_name, D_res->node->node_name); TRACE();
            }else{
                assert(0);
            }
        }ITERATE_LIST_END;
    }
    return NULL; 
}

/* Note : here node is E, not S*/
q_space_set_t
p2p_compute_q_space(node_t *node, edge_t *protected_link, LEVEL level){

    node_t *S = NULL, *E = NULL;
    singly_ll_node_t *list_node1 = NULL;

    unsigned int d_E_to_S = 0,
                 d_S_to_y = 0,
                 d_E_to_y = 0;

    spf_result_t *spf_result_y = NULL;
    
    assert(!is_broadcast_link(protected_link, level));

    q_space_set_t q_space = init_singly_ll();
    singly_ll_set_comparison_fn(q_space, instance_node_comparison_fn);

    E = node;
    S = protected_link->from.node;

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

    singly_ll_node_t *list_node = NULL;
    node_t *pq_node = NULL;
    assert(!is_broadcast_link(protected_link, level));

    p_space_set_t ex_p_space = p2p_compute_link_node_protecting_extended_p_space(S, protected_link, level);
    sprintf(LOG, "No of nodes in ex_p_space = %u", GET_NODE_COUNT_SINGLY_LL(ex_p_space)); TRACE();
    
    if(GET_NODE_COUNT_SINGLY_LL(ex_p_space) == 0){
        free(ex_p_space);
        return NULL;
    }

    /*This PQ space will not contain overloaded nodes*/
    pq_space_set_t pq_space  = p2p_filter_select_pq_nodes_from_ex_pspace(S, protected_link, 
                                                                         level);
    sprintf(LOG, "No of nodes in pq_space = %u", GET_NODE_COUNT_SINGLY_LL(pq_space)); TRACE();

    if(GET_NODE_COUNT_SINGLY_LL(pq_space) == 0){
        free(pq_space);
	delete_singly_ll(ex_p_space);
	free(ex_p_space);
	ex_p_space = NULL;
        return NULL;
    }

    printf("PQ nodes : \n");
    ITERATE_LIST_BEGIN(pq_space, list_node){
        pq_node = list_node->data;
        printf("%s (%s)\n", pq_node->node_name, IS_BIT_SET(pq_node->q_space_protection_type, \
                LINK_NODE_PROTECTION) ? "LINK_NODE_PROTECTION" : "LINK_PROTECTION"); 
    } ITERATE_LIST_END;

    delete_singly_ll(ex_p_space);
    delete_singly_ll(pq_space);
    free(ex_p_space);
    free(pq_space);
    return NULL;
}

/*Routine to compute RLFA of node S, wrt to failed link 
 * protected_link at given level for destination dest. Note that
 * to compute RLFA we need to know dest also, thus RLFAs are 
 * computed per destination (Need to check for LFAs)*/

void 
p2p_compute_rlfa_for_given_dest(node_t *S, LEVEL level, edge_t *protected_link, node_t *dest){

    /* Run SPF for protecting node S*/

    singly_ll_node_t *list_node = NULL;
    node_t *pq_node = NULL;

    unsigned int d_pq_to_dest = 0,
                 d_S_to_dest = 0;

    assert(!is_broadcast_link(protected_link, level));
    p_space_set_t ex_p_space = p2p_compute_p_space(S, protected_link, level);
    q_space_set_t q_space = p2p_compute_q_space(S, protected_link, level);

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

lfa_node_t *
get_new_lfa_node(){

    lfa_node_t *lfa_info = calloc(1, sizeof(lfa_node_t));
    lfa_info->dst_lst = init_singly_ll();
    return lfa_info;
}

void
free_lfa_node(lfa_node_t *lfa_node){

    delete_singly_ll(lfa_node->dst_lst);
    free(lfa_node);
    lfa_node = NULL;
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

static lfa_t *
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

    lfa_dest_pair_t *lfa_dest_pair = NULL;

    assert(is_broadcast_link(protected_link, level));
    boolean is_dest_impacted = FALSE,
             IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE;

    lfa_t *lfa = get_new_lfa();
    lfa->protected_link = &protected_link->from;

    PN = protected_link->to.node;
    Compute_and_Store_Forward_SPF(PN, level);  

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node){

        D_res = list_node->data;
        D = D_res->node;
        if(D == S) continue;

        memset(impact_reason, 0, STRING_REASON_LEN);

        sprintf(LOG, "Node : %s : LFA computation for Destination %s begin", S->node_name, D->node_name); TRACE();
        
        IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE;
        is_dest_impacted = is_destination_impacted(S, protected_link, D, level, impact_reason,
                            &IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED);
        sprintf(LOG, "Dest = %s Impact result = %s\n    reason : %s", D->node_name, 
                    is_dest_impacted ? "IMPACTED" : "NOT-IMPCATED", impact_reason); TRACE();

        if(is_dest_impacted == FALSE) continue;
        
        dist_S_D = D_res->spf_metric;
        dist_PN_D = DIST_X_Y(PN, D, level);

        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(S, N, pn_node, edge1, edge2, level){
            
            lfa_type = UNKNOWN_LFA_TYPE;
            sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s, edge2 = %s for LFA candidature",
                    S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();

            /*Do not consider the link being protected to find LFA*/
            if(edge1 == protected_link){
                sprintf(LOG, "Node : %s : Nbr %s with OIF %s is same as protected link %s, skipping this nbr from LFA candidature", 
                        S->node_name, N->node_name, edge1->from.intf_name, protected_link->from.intf_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }

            if(IS_OVERLOADED(N, level)){
                sprintf(LOG, "Node : %s : Nbr %s failed for LFA candidature, reason - Overloaded", S->node_name, N->node_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }

            dist_N_S = DIST_X_Y(N, S, level);
            sprintf(LOG, "Node : %s : Source(S) = %s, probable LFA(N) = %s, DEST(D) = %s", 
                    S->node_name, S->node_name, N->node_name, D->node_name); TRACE();

            dist_N_D = DIST_X_Y(N, D, level);
            sprintf(LOG, "Node : %s : Testing inequality 1 : dist_N_D(%u) < dist_N_S(%u) + dist_S_D(%u)",
                    S->node_name, dist_N_D, dist_N_S, dist_S_D); TRACE();

            /* Apply inequality 1*/
            if(!(dist_N_D < dist_N_S + dist_S_D)){
                sprintf(LOG, "Node : %s : Inequality 1 failed", S->node_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }

            sprintf(LOG, "Node : %s : Inequality 1 passed", S->node_name); TRACE();
             
            /* Inequality 3 : Node protecting LFA 
             * All primary nexthop MUST qualify node protection inequality # 3*/
            if(IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){

                sprintf(LOG, "Node : %s : Testing node protecting inequality 3 with primary nexthops of %s through potential LFA %s",
                        S->node_name, D->node_name, N->node_name); TRACE();

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
                            sprintf(LOG, "Node : %s : inequality 3 Passed with #%u next hop %s(%s)",
                                    S->node_name, i, prim_nh->node_name, nh == IPNH ? "IPNH" : "LSPNH"); TRACE();
                        }else{
                            lfa_type = UNKNOWN_LFA_TYPE;
                            sprintf(LOG, "Node : %s : inequality 3 Failed with #%u next hop %s(%s), ", 
                                    S->node_name, i, prim_nh->node_name, nh == IPNH ? "IPNH" : "LSPNH"); TRACE();
                            break;
                        }
                    }
                    if(lfa_type == UNKNOWN_LFA_TYPE ) break;
                } ITERATE_NH_TYPE_END;
            }

            if(lfa_type == BROADCAST_ONLY_NODE_PROTECTION_LFA){
                lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
                lfa_dest_pair->lfa_type = lfa_type;
                lfa_dest_pair->lfa = N;
                lfa_dest_pair->oif_to_lfa = &edge1->from; 
                lfa_dest_pair->dest = D;

                /*Record the LFA*/ 
                singly_ll_add_node_by_val(lfa->lfa, lfa_dest_pair);

                /*code to record the back up next hop*/
                backup_nh_type = edge1->etype == UNICAST ? IPNH : LSPNH;
                backup_nh = get_next_hop_empty_slot(D->backup_next_hop[level][backup_nh_type]);
                backup_nh->level = level;
                backup_nh->oif = &edge1->from;
                backup_nh->node = N;
                if(backup_nh_type == IPNH)
                    set_next_hop_gw_pfx(*backup_nh, edge2->to.prefix[level]->prefix);
                backup_nh->nh_type = backup_nh_type;
                backup_nh->lfa_type = lfa_type;
                backup_nh->proxy_nbr = NULL;
                backup_nh->rlfa = NULL;
                backup_nh->ldplabel = 0;
                backup_nh->root_metric = DIST_X_Y(S, N, level);
                backup_nh->dest_metric = dist_N_D;
                backup_nh->is_eligible = TRUE;

                sprintf(LOG, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s," 
                              "looking to promote it to BROADCAST_LINK_AND_NODE_PROTECTION_LFA", N->node_name, 
                        lfa_dest_pair->oif_to_lfa->intf_name, D->node_name, 
                        get_str_lfa_type(lfa_dest_pair->lfa_type)); TRACE();

                /*Check for Link protection criteria*/

                sprintf(LOG, "Node : %s : Testing inequality 4 : dist_N_D(%u) < dist_N_PN(%u) + dist_PN_D(%u)",
                        S->node_name, dist_N_D, dist_N_PN, dist_PN_D); TRACE();

                /*Apply inequality 4*/
                dist_N_PN = DIST_X_Y(N, PN, level);
                if(!(dist_N_D < dist_N_PN + dist_PN_D)){
                    sprintf(LOG, "Node : %s : Inequality 4 failed, LFA not promoted to BROADCAST_LINK_AND_NODE_PROTECTION_LFA", S->node_name); TRACE();
                    goto NBR_PROCESSING_DONE;
                }
                sprintf(LOG, "Node : %s : Inequality 4 passed, LFA %s(OIF = %s) , Dest = %s promoted from %s to %s", 
                        S->node_name, N->node_name, lfa_dest_pair->oif_to_lfa->intf_name, D->node_name,
                        get_str_lfa_type(lfa_dest_pair->lfa_type),
                        get_str_lfa_type(BROADCAST_LINK_AND_NODE_PROTECTION_LFA)); TRACE();

                lfa_dest_pair->lfa_type = BROADCAST_LINK_AND_NODE_PROTECTION_LFA;
                backup_nh->lfa_type = BROADCAST_LINK_AND_NODE_PROTECTION_LFA;
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, N, pn_node, level);
            }

            /*We are here because LFA is not node protecting, try for link protection LFA only*/
            if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                sprintf(LOG, "Node : %s : Node-link-degradation Disabled, Nbr %s not considered for link protection LFA", 
                        S->node_name, N->node_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }
           
            if(IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED == FALSE){
                sprintf(LOG, "Node : %s : Nbr %s not considered for link protection LFA as it has ECMP",
                            S->node_name, N->node_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }
            
            if(strict_down_stream_lfa){
                /* 4. Narrow down the subset further using inequality 2 */
                sprintf(LOG, "Node : %s : Testing inequality 2 : dist_N_D(%u) < dist_S_D(%u)", 
                        S->node_name, dist_N_D, dist_S_D); TRACE();

                if(!(dist_N_D < dist_S_D)){
                    sprintf(LOG, "Node : %s : Inequality 2 failed", S->node_name); TRACE();
                    goto NBR_PROCESSING_DONE;
                }
                sprintf(LOG, "Node : %s : Inequality 2 passed, lfa promoted from %s to %s", S->node_name, 
                                get_str_lfa_type(lfa_type), get_str_lfa_type(LINK_PROTECTION_LFA_DOWNSTREAM)); TRACE(); 
            }

            /*Now check inequality 4*/ 
            dist_N_PN = DIST_X_Y(N, PN, level);
            sprintf(LOG, "Node : %s : Testing inequality 4 : dist_N_D(%u) < dist_N_PN(%u) + dist_PN_D(%u)",
                    S->node_name, dist_N_D, dist_N_PN, dist_PN_D); TRACE();

            /*Apply inequality 4*/
            if(!(dist_N_D < dist_N_PN + dist_PN_D)){
                sprintf(LOG, "Node : %s : Inequality 4 failed, LFA candidature failed for nbr %s, Dest = %s",
                              S->node_name, N->node_name, D->node_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }

            sprintf(LOG, "Node : %s : Inequality 4 passed for Nbr %s is LFA for Dest =  %s",
                        S->node_name, N->node_name, D->node_name); TRACE();

            lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
            lfa_dest_pair->lfa_type = strict_down_stream_lfa ? BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM :
                        BROADCAST_LINK_PROTECTION_LFA;
            lfa_dest_pair->lfa = N;
            lfa_dest_pair->oif_to_lfa = &edge1->from; 
            lfa_dest_pair->dest = D;

            /*Record the LFA*/ 
            singly_ll_add_node_by_val(lfa->lfa, lfa_dest_pair);

            /*Record the LFA*/
            backup_nh_type = edge1->etype == UNICAST ? IPNH : LSPNH;
            backup_nh = get_next_hop_empty_slot(D->backup_next_hop[level][backup_nh_type]);
            backup_nh->level = level;
            backup_nh->oif = &edge1->from;
            backup_nh->node = N;
            if(backup_nh_type == IPNH)
                set_next_hop_gw_pfx(*backup_nh, edge2->to.prefix[level]->prefix);
            backup_nh->nh_type = backup_nh_type;
            backup_nh->lfa_type = lfa_type;
            backup_nh->proxy_nbr = NULL;
            backup_nh->rlfa = NULL;
            backup_nh->ldplabel = 0;
            backup_nh->root_metric = DIST_X_Y(S, N, level);
            backup_nh->dest_metric = dist_N_D;
            backup_nh->is_eligible = TRUE;

            sprintf(LOG, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s", N->node_name, 
                    lfa_dest_pair->oif_to_lfa->intf_name, D->node_name, 
                    get_str_lfa_type(lfa_dest_pair->lfa_type)); TRACE();

NBR_PROCESSING_DONE:
            sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s edge2 = %s for LFA candidature Done", 
                S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();
        } ITERATE_NODE_PHYSICAL_NBRS_END(S, N, pn_node, level);
        
    } ITERATE_LIST_END;

    if(GET_NODE_COUNT_SINGLY_LL(lfa->lfa) == 0){
        free_lfa(lfa);
        return NULL;
    }

    return lfa;
}

/* In case of LFAs, the LFA is promoted to Node protecting LFA if they
 * meet the node protecting criteria*/

static lfa_t *
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
            IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE;
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
    lfa_dest_pair_t *lfa_dest_pair = NULL;
    lfa_type_t lfa_type = UNKNOWN_LFA_TYPE;

    lfa_t *lfa = get_new_lfa(); 

    /* 3. Filter nbrs of S using inequality 1 */
    E = protected_link->to.node;

    lfa->protected_link = &protected_link->from;

    ITERATE_LIST_BEGIN(S->spf_run_result[level], list_node){
        D_res = list_node->data;
        D = D_res->node;
        is_dest_impacted = FALSE;

        if(D == S) continue; 
        memset(impact_reason, 0, STRING_REASON_LEN);

        sprintf(LOG, "Node : %s : LFA computation for Destination %s begin", S->node_name, D->node_name); TRACE();
        
        IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED = TRUE;
        is_dest_impacted = is_destination_impacted(S, protected_link, D, level, impact_reason, 
                             &IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED);
        sprintf(LOG, "Dest = %s Impact result = %s\n    reason : %s", D->node_name, 
                    is_dest_impacted ? "IMPACTED" : "NOT-IMPCATED", impact_reason); TRACE();
                    
        if(is_dest_impacted == FALSE) continue;

        dist_S_D = D_res->spf_metric;
        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(S, N, pn_node, edge1, edge2, level){

            sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s, edge2 = %s for LFA candidature",
                    S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();

            /*Do not consider the link being protected to find LFA*/
            if(edge1 == protected_link){
                sprintf(LOG, "Node : %s : Nbr %s with OIF %s is same as protected link %s, skipping this nbr from LFA candidature", 
                        S->node_name, N->node_name, edge1->from.intf_name, protected_link->from.intf_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }

            if(IS_OVERLOADED(N, level)){
                sprintf(LOG, "Node : %s : Nbr %s failed for LFA candidature, reason - Overloaded", S->node_name, N->node_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }

            dist_N_S = DIST_X_Y(N, S, level);
            sprintf(LOG, "Node : %s : Source(S) = %s, probable LFA(N) = %s, DEST(D) = %s, Primary NH(E) = %s", 
                    S->node_name, S->node_name, N->node_name, D->node_name, E->node_name); TRACE();

            dist_N_D = DIST_X_Y(N, D, level);
            sprintf(LOG, "Node : %s : Testing inequality 1 : dist_N_D(%u) < dist_N_S(%u) + dist_S_D(%u)",
                    S->node_name, dist_N_D, dist_N_S, dist_S_D); TRACE();

            /* Apply inequality 1*/
            if(!(dist_N_D < dist_N_S + dist_S_D)){
                sprintf(LOG, "Node : %s : Inequality 1 failed", S->node_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }

            sprintf(LOG, "Node : %s : Inequality 1 passed", S->node_name); TRACE();
            
            /* Inequality 3 : Node protecting LFA 
             * All primary nexthop MUST qualify node protection inequality # 3*/
            if(IS_LINK_NODE_PROTECTION_ENABLED(protected_link)){

                sprintf(LOG, "Node : %s : Testing node protecting inequality 3 with primary nexthops of %s through potential LFA %s",
                        S->node_name, D->node_name, N->node_name); TRACE();

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
                            sprintf(LOG, "Node : %s : inequality 3 Passed with #%u next hop %s(%s), lfa_type = %s",
                                    S->node_name, i, prim_nh->node_name, nh == IPNH ? "IPNH" : "LSPNH", get_str_lfa_type(lfa_type)); TRACE();
                        }else{
                            lfa_type = UNKNOWN_LFA_TYPE; 
                            sprintf(LOG, "Node : %s : inequality 3 Failed with #%u next hop %s(%s), lfa_type = %s", 
                                    S->node_name, i, prim_nh->node_name, nh == IPNH ? "IPNH" : "LSPNH", get_str_lfa_type(lfa_type)); TRACE();
                            break;
                        }
                    }
                    if(lfa_type == UNKNOWN_LFA_TYPE) break;
                } ITERATE_NH_TYPE_END;
            }

            if(lfa_type == LINK_AND_NODE_PROTECTION_LFA){
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

                {
                    /*code to record the back up next hop*/
                    nh_type_t backup_nh_type = edge1->etype == UNICAST ? IPNH : LSPNH;
                    internal_nh_t *backup_nh = 
                            get_next_hop_empty_slot(D->backup_next_hop[level][backup_nh_type]);

                    backup_nh->level = level;
                    backup_nh->oif = &edge1->from;
                    backup_nh->node = N;
                    if(backup_nh_type == IPNH)
                        set_next_hop_gw_pfx(*backup_nh, edge1->to.prefix[level]->prefix);
                    backup_nh->nh_type = backup_nh_type;
                    backup_nh->lfa_type = lfa_type;
                    backup_nh->proxy_nbr = NULL;
                    backup_nh->rlfa = NULL;
                    backup_nh->ldplabel = 0;
                    backup_nh->root_metric = DIST_X_Y(S, N, level);
                    backup_nh->dest_metric = dist_N_D;
                    backup_nh->is_eligible = TRUE;
                }
                goto NBR_PROCESSING_DONE;
            }

            if(!IS_LINK_PROTECTION_ENABLED(protected_link)){
                sprintf(LOG, "Node : %s : Node-link-degradation Disabled, Nbr %s not considered for link protection LFA", 
                        S->node_name, N->node_name); TRACE();
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
            if(IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED == FALSE){
                sprintf(LOG, "Node : %s : Nbr %s not considered for link protection LFA as it has ECMP",
                            S->node_name, N->node_name); TRACE();
                goto NBR_PROCESSING_DONE;
            }

            if(strict_down_stream_lfa){
                /* 4. Narrow down the subset further using inequality 2 */
                sprintf(LOG, "Node : %s : Testing inequality 2 : dist_N_D(%u) < dist_S_D(%u)", 
                        S->node_name, dist_N_D, dist_S_D); TRACE();

                if(!(dist_N_D < dist_S_D)){
                    sprintf(LOG, "Node : %s : Inequality 2 failed", S->node_name); TRACE();
                    ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(S, N, pn_node, level);
                }
                sprintf(LOG, "Node : %s : Inequality 2 passed, lfa promoted from %s to %s", S->node_name, 
                                get_str_lfa_type(lfa_type), get_str_lfa_type(LINK_PROTECTION_LFA_DOWNSTREAM)); TRACE(); 
                lfa_type = LINK_PROTECTION_LFA_DOWNSTREAM;
            }
            
            /*We are here because inequality 1 and 2 passed, but 3 fails*/ 
            lfa_dest_pair = calloc(1, sizeof(lfa_dest_pair_t));
            lfa_dest_pair->lfa_type = lfa_type;
            lfa_dest_pair->lfa = N;
            lfa_dest_pair->oif_to_lfa = &edge1->from; 
            lfa_dest_pair->dest = D;

            /*Record the LFA*/ 
            singly_ll_add_node_by_val(lfa->lfa, lfa_dest_pair);
            {
                /*code to record the back up next hop*/
                nh_type_t backup_nh_type = edge1->etype == UNICAST ? IPNH : LSPNH;
                internal_nh_t *backup_nh = 
                    get_next_hop_empty_slot(D->backup_next_hop[level][backup_nh_type]);

                backup_nh->level = level;
                backup_nh->oif = &edge1->from;
                backup_nh->node = N;
                if(backup_nh_type == IPNH)
                    set_next_hop_gw_pfx(*backup_nh, edge1->to.prefix[level]->prefix);
                backup_nh->nh_type = backup_nh_type;
                backup_nh->lfa_type = lfa_type;
                backup_nh->proxy_nbr = NULL;
                backup_nh->rlfa = NULL;
                backup_nh->ldplabel = 0;
                backup_nh->root_metric = DIST_X_Y(S, N, level);
                backup_nh->dest_metric = dist_N_D;
                backup_nh->is_eligible = TRUE;
            }
            sprintf(LOG, "lfa pair computed : %s(OIF = %s),%s, lfa_type = %s", N->node_name, 
                    lfa_dest_pair->oif_to_lfa->intf_name, D->node_name, 
                    get_str_lfa_type(lfa_dest_pair->lfa_type)); TRACE();

NBR_PROCESSING_DONE:
        sprintf(LOG, "Node : %s : Testing nbr %s via edge1 = %s edge2 = %s for LFA candidature Done", 
                S->node_name, N->node_name, edge1->from.intf_name, edge2->from.intf_name); TRACE();

        } ITERATE_NODE_PHYSICAL_NBRS_END(S, N, pn_node, level);

    } ITERATE_LIST_END;

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
     init_back_up_computation(S, level);

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

/*Comparison function for search*/

boolean
dst_lfa_db_t_search_comparison_fn(void *dst_lfa_db, void *dst){

    if(((dst_lfa_db_t *)dst_lfa_db)->dst == (node_t *)dst)  return TRUE;
    return FALSE;
}


dst_lfa_db_t *get_new_dst_lfa_db_node(){

    dst_lfa_db_t *dst_lfa_db = calloc(1, sizeof(dst_lfa_db_t));
    dst_lfa_db->alternates.lfas = init_singly_ll();
    return dst_lfa_db;
}

void drain_dst_lfa_db_node(dst_lfa_db_t *dst_lfa_db){

     singly_ll_node_t *list_node = NULL;
     alt_node_t *alt_node = NULL;

     ITERATE_LIST_BEGIN(dst_lfa_db->alternates.lfas, list_node){
        alt_node = list_node->data;   
        free(alt_node);
        alt_node = NULL;
     } ITERATE_LIST_END;
}

alt_node_t *get_new_alt_node(){

    alt_node_t *alt_node = calloc(1, sizeof(alt_node_t));
    return alt_node;
}

void collect_destination_lfa(dst_lfa_db_t *dst, alt_node_t *lfa){
    singly_ll_add_node_by_val(dst->alternates.lfas, lfa);
}


void collect_destination_rlfa(dst_lfa_db_t *dst, alt_node_t *rlfa){
    singly_ll_add_node_by_val(dst->alternates.rlfas, rlfa);
}



