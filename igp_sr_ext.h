/*
 * =====================================================================================
 *
 *       Filename:  igp_sr_ext.h
 *
 *    Description:  HEader defining common SR definitions
 *
 *        Version:  1.0
 *        Created:  Saturday 24 February 2018 11:32:36  IST
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

#ifndef __IGP_SR_EXT__
#define __IGP_SR_EXT__

#include "sr.h"
#include "instanceconst.h"

typedef enum {

    SHORTEST_PATH,
    STRICT_SHORTEST_PATH
} sr_algo_t;

/*prefix SID */
typedef struct _prefix_sid_t{

    unsigned int sid;
    sr_algo_t algo_id;
    unsigned int prefix;
    char mask;
    /* the IGP signaling extension for IGP-Prefix segment includes a flag
     * to indicate whether directly connected neighbors of the node on
     * which the prefix is attached should perform the NEXT operation or
     * the CONTINUE operation when processing the SID. This behavior is
     * equivalent to Penultimate Hop Popping (NEXT) or Ultimate Hop
     * Popping (CONTINUE) in MPLS.
     * pg 10 - Segment Routing Architecture-2*/
    HOP_POPPING hopping_flag;
} prefix_sid_t;

/*Adjacecncy SID*/

/*If this bit set, this Adj is eligible for protection using IPFRR or MPLS-FRR*/
#define ADJ_SID_FLAG_ELIGIBLE_FOR_PROTECTION    0
/*If this bit set, this Adj has global scope, else local scope*/
#define ADJ_SID_FLAG_SCOPE  1

/*All these members are advertised by IGP SR TLVs*/
typedef struct _adj_sid_t{

   unsigned int sid;
   FLAG adj_sid_flags;
   /*draft-ietf-spring-segment-routing-13 pg 15*/
   unsigned int weight; /*Used for parallel Adjacencies, section 3.4.1*/
} ajd_sid_t;


boolean
is_pfx_sid_lies_within_srgb(prefix_sid_t *pfx_sid, srgb_t *srgb);

typedef struct _node_t node_t;
typedef struct prefix_ prefix_t;
typedef struct edge_end_ edge_end_t;

/*
If a node N advertises Prefix-SID SID-R for a prefix R that is
attached to N, if N specifies CONTINUE as the operation to be
performed by directly connected neighbors, N MUST maintain the
following FIB entry:
Incoming Active Segment: SID-R
Ingress Operation: NEXT
Egress interface: NULL
    Segment routing Architecture-2, pg11
*/

void
sr_install_local_prefix_mpls_fib_entry(node_t *node, prefix_t *prefix);

/*
A remote node M MUST maintain the following FIB entry for any
learned Prefix-SID SID-R attached to IP prefix R:
Incoming Active Segment: SID-R
Ingress Operation:
If the next-hop of R is the originator of R
and instructed to remove the active segment: NEXT
Else: CONTINUE
Egress interface: the interface towards the next-hop along the
path computed using the algorithm advertised with
the SID toward prefix R.
    Segment routing Architecture-2, pg11
*/

void
sr_install_remote_prefix_mpls_fib_entry(node_t *node, prefix_t *prefix);

/*For ipv6 Data plane*/
/*A node N advertising an IPv6 address R usable as a segment identifier
 * MUST maintain the following FIB entry:
 * Incoming Active Segment: R
 * Ingress Operation: NEXT
 * Egress interface: NULL
 * Segment routing Architecture-2, pg11
 * */

void
sr_install_local_prefix_ipv6_fib_entry(node_t *node, prefix_t *prefix);

/*
Independent of Segment Routing support, any remote IPv6 node will
maintain a plain IPv6 FIB entry for any prefix, no matter if the
represent a segment or not. This allows forwarding of packets to the
node which owns the SID even by nodes which do not support Segment
Routing. */
/*So, this is simple ipv6 unicast forwarding behavior*/
void
sr_install_remote_prefix_ipv6_fib_entry(node_t *node, prefix_t *prefix);

void
allocate_local_adj_sid(node_t *node, edge_end_t *adjacency);

void
allocate_global_adj_sid(node_t *node, edge_end_t *adjacency, srgb_t *srgb);

/*
 *When a node binds an Adj-SID to a local data-link L, the node MUST
 install the following FIB entry:
 Incoming Active Segment: V
 Ingress Operation: NEXT
 Egress Interface: L
 Also take care of section 3.4.1 pg 15
 * */
void
sr_install_local_adj_mpls_fib_entry(node_t *node, edge_end_t *adjacency);

void
sr_install_global_adj_mpls_fib_entry(node_t *node, edge_end_t *adjacency, srgb_t *srgb);
#endif /* __SR__ */ 
