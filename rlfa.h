/*
 * =====================================================================================
 *
 *       Filename:  rlfa.h
 *
 *    Description:  This file declares the data structures used for computing Remote Loop free alternates
 *
 *        Version:  1.0
 *        Created:  Thursday 07 September 2017 03:35:42  IST
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

#ifndef __RLFA__
#define __RLFA__

#include "LinkedListApi.h"
#include "instanceconst.h"
#include "spfcomputation.h"

typedef ll_t * p_space_set_t;
typedef ll_t * q_space_set_t;
typedef ll_t * pq_space_set_t;

typedef struct _node_t node_t;
typedef struct _edge_t edge_t;

/*Flags to enable the type of protection enabled on protected link*/
#define LINK_PROTECTION         0
#define LINK_NODE_PROTECTION    1
#define ONLY_NODE_PROTECTION    2

#define SET_LINK_PROTECTION_TYPE(edge_ptr, protection_bit)      \
    (SET_BIT(edge_ptr->from.edge_config_flags, protection_bit))

#define UNSET_LINK_PROTECTION_TYPE(edge_ptr, protection_bit)    \
    (UNSET_BIT(edge_ptr->from.edge_config_flags, protection_bit))

#define IS_LINK_PROTECTION_ENABLED(edge_ptr)                    \
    (IS_BIT_SET(edge_ptr->from.edge_config_flags, LINK_PROTECTION))

#define IS_LINK_NODE_PROTECTION_ENABLED(edge_ptr)               \
    (IS_BIT_SET(edge_ptr->from.edge_config_flags, LINK_NODE_PROTECTION))

char *
get_str_lfa_type(lfa_type_t lfa_type);

typedef struct lfa_dest_pair_{
    node_t *lfa;
    edge_end_t *oif_to_lfa;
    node_t *dest;
    lfa_type_t lfa_type;
} lfa_dest_pair_t;

typedef struct lfa_{
    edge_end_t *protected_link;
    ll_t *lfa;
} lfa_t;

/*New Unified Data structures used for both LFAs and RLFAs*/

typedef struct lfa_dst_info_{
    node_t *dst;               /* Destination protected*/
    lfa_type_t lfa_type;       /* protection type provided to this Destination*/
    unsigned int dst_metric;   /* Distance of this Dest from LFA|RLFA*/
} lfa_dst_info_t;

typedef struct lfa_node_{
    union{
    node_t *pq_node;            /*RLFA - pq node*/
    node_t *lfa;                /*LFA - Immediate nbr*/
    } lfa_node;
    edge_end_t *oif_to_lfa1;    /*LFA - oif to LFA, RLFA - oif to nbr which is proxy for pq node*/
    edge_end_t *oif_to_lfa2;    /* In case proxy nbr is through PN : LFA - oif to LFA, RLFA - oif to nbr which is proxy for pq node*/
    unsigned int root_metric;   /*LFA - metric of oif_to_lfa, RLFA -  Root metric of this PQ node*/
    ll_t *dst_lst;
} lfa_node_t;

lfa_t *
get_new_lfa();

lfa_node_t *
get_new_lfa_node();

/*Attempt 3 */
typedef struct dst_lfa_db_{

    node_t *dst;
    union{
        ll_t *lfas;
        ll_t *rlfas;
    } alternates;
    unsigned int dst_metric;
} dst_lfa_db_t;

typedef struct alt_node_{
    union{
        node_t *lfa;
        node_t *pq_node;
    } u;
    edge_end_t *oif_to_lfa1;    /*LFA - oif to LFA, RLFA - oif to nbr which is proxy for pq node*/
    edge_end_t *oif_to_lfa2;    /* In case proxy nbr is through PN : LFA - oif from PN to LFA, RLFA - oif from PN to nbr which is proxy for pq node*/
    node_t *pq_node_proxy;
    unsigned int root_metric;
    char *rejection_reason;
    char *policy_selection_reason;
    boolean is_lfa_enabled;
} alt_node_t;

dst_lfa_db_t *get_new_dst_lfa_db_node();
void drain_dst_lfa_db_node(dst_lfa_db_t *dst_lfa_db);
alt_node_t *get_new_alt_node();
void collect_destination_lfa(dst_lfa_db_t *dst, alt_node_t *lfa);
void collect_destination_rlfa(dst_lfa_db_t *dst, alt_node_t *lfa);
//void delete_destination_lfa(dst_lfa_db_t *dst, node_t *lfa);
//void delete_destination_rlfa(dst_lfa_db_t *dst, node_t *pq_node, node_t *pq_node_proxy);
dst_lfa_db_t *get_dst_lfa_db_node(edge_end_t *protected_link, node_t *dst);
boolean dst_lfa_db_t_search_comparison_fn(void *dst_lfa_db, void *dst);

/*Attemp 3 end*/

/*Attempt 4*/
typedef struct loop_free_alt_{
    char rejection_reason[STRING_REASON_LEN];
    boolean is_lfa_enabled;
    edge_end_t *protected_link;
    node_t *destination;
    internal_nh_t *backup_nxt_hop;
} loop_free_alt_t;

#define GET_LFA_NODE_FROM_BACKUP_NH(internal_nh_t_ptr)  \
    (loop_free_alt_t *)((char *)internal_nh_t_ptr - (char *)&((loop_free_alt_t *)0->backup))

#define RECORD_LFA(node_ptr, loop_free_alt_t_ptr, _lvl)   \
    singly_ll_add_node_by_val(node_ptr->lfa_list[_lvl], loop_free_alt_t_ptr)

void
free_lfa_node(lfa_node_t *lfa_node);

void
clear_lfa_result(node_t *node, LEVEL level);

void
free_lfa();


void
print_lfa_info(lfa_t *lfa);

void
Compute_and_Store_Forward_SPF(node_t *spf_root,
                              LEVEL level);
void
Compute_PHYSICAL_Neighbor_SPFs(node_t *spf_root, LEVEL level);

void
Compute_LOGICAL_Neighbor_SPFs(node_t *spf_root, LEVEL level);

p_space_set_t 
p2p_compute_p_space(node_t *node, edge_t *failed_edge, LEVEL level);

p_space_set_t 
p2p_compute_link_node_protecting_extended_p_space(node_t *node, edge_t *failed_edge, LEVEL level);

p_space_set_t 
broadcast_compute_link_node_protecting_extended_p_space(node_t *node, edge_t *failed_edge, LEVEL level);

q_space_set_t
p2p_compute_q_space(node_t *node, edge_t *failed_edge, LEVEL level);

pq_space_set_t
Intersect_Extended_P_and_Q_Space(p_space_set_t pset, q_space_set_t qset);

void
p2p_compute_rlfa_for_given_dest(node_t *node, LEVEL level, edge_t *failed_edge, node_t *dest);

q_space_set_t
p2p_filter_select_pq_nodes_from_ex_pspace(node_t *S, 
                                          edge_t *failed_edge, 
                                          LEVEL level, 
                                          p_space_set_t ex_p_space);

q_space_set_t
broadcast_filter_select_pq_nodes_from_ex_pspace(node_t *S, 
                                          edge_t *failed_edge, 
                                          LEVEL level, 
                                          p_space_set_t ex_p_space);
/*
LFA Link/Link-and-node Protection
====================================

    Inequality 1 : Loop Free Criteria
    DIST(N,D) < DIST(N,S) + DIST(S,D)  - Means, the LFA N will not send the traffic back to S in case of primary link failure (S-E link failure)

    Inequality 2 :  Downstream Path Criteria
    DIST(N,D) < DIST(S,D)              - Means, Select LFA among nbrs such that, N is a downstream router to Destination D

    Inequality 3 : Node protection Criteria
    DIST(N,D) < DIST(N,E) + DIST(E,D)  - Means, S can safely re-route traffic to N, since N's path to D do not contain failed Hop E.

              +-----+                      +-----+
              |  S  +----------------------+  N  |
              +--+--+                      +-+---+
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |                           |
                 |         +---------+       |
                 |         |         |       |
                 |         |    D    |       |
                 +---------+         +-------+
                           |         |
                           +----+----+
                                |
                                |
                                |
                                |
                                |
                                |
                                |
                                |
                                |
                                |
                                |
                          +-----+-----+
                          |           |
                          |    E      |
                          |           |
                          |           |
                          +-----------+



STEPS : To compute LFA, do the following steps
    1. Run SPF on S to know DIST(S,D)
    2. Run SPF on all nbrs of S except primary-NH(S) to know DIST(N,D) and DIST(N,S)
    3. Filter nbrs of S using inequality 1
    4. Narrow down the subset further using inequality 2
    5. Remain is the potential set of LFAs which each surely provides Link protection, but do not guarantee node protection
    6. We need to investigate this subset of potential LFAs to possibly find one LFA which provide node protection(hence link protection)
    7. Test the remaining set for inequality 3

*/

void
init_back_up_computation(node_t *S, LEVEL level);

lfa_t *
compute_lfa(node_t * S, edge_t *protected_link, LEVEL level, boolean strict_down_stream_lfa);

lfa_t *
compute_rlfa(node_t * S, edge_t *protected_link, LEVEL level, boolean strict_down_stream_lfa);

boolean
is_destination_impacted(node_t *S, edge_t *failed_edge,
        node_t *D, LEVEL level,
        char impact_reason[]);

#endif /* __RLFA__ */
