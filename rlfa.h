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

/*Back up SPF options */

/*config node <node-name> backup-spf-options*/
#define SPF_BACKUP_OPTIONS_ENABLED     0 
/*config node <node-name> backup-spf-options remote-backup-calculation*/
#define SPF_BACKUP_OPTIONS_REMOTE_BACKUP_CALCULATION    1 
/*config node <node-name> backup-spf-options node-link-degradation*/
#define SPF_BACKUP_OPTIONS_NODE_LINK_DEG    2

char *
get_str_lfa_type(lfa_type_t lfa_type);

void
Compute_and_Store_Forward_SPF(node_t *spf_root,
                              LEVEL level);
void
Compute_PHYSICAL_Neighbor_SPFs(node_t *spf_root, LEVEL level);

void
Compute_LOGICAL_Neighbor_SPFs(node_t *spf_root, LEVEL level);

void 
p2p_compute_link_node_protecting_extended_p_space(node_t *node, edge_t *failed_edge, LEVEL level);

void
broadcast_compute_link_node_protecting_extended_p_space(node_t *node, edge_t *failed_edge, LEVEL level);

void
p2p_filter_select_pq_nodes_from_ex_pspace(node_t *S, 
                                          edge_t *failed_edge, 
                                          LEVEL level); 

void
broadcast_filter_select_pq_nodes_from_ex_pspace(node_t *S, 
                                          edge_t *failed_edge, 
                                          LEVEL level);
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

void
compute_lfa(node_t * S, edge_t *protected_link, LEVEL level, boolean strict_down_stream_lfa);

void
compute_rlfa(node_t * S, edge_t *protected_link, LEVEL level, boolean strict_down_stream_lfa);

boolean
is_destination_impacted(node_t *S, edge_t *failed_edge,
        node_t *D, LEVEL level,
        char impact_reason[],
        boolean *IS_ONLY_LINK_PROTECTION_BACKUP_REQUIRED);

#endif /* __RLFA__ */
