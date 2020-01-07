/*
 * =====================================================================================
 *
 *       Filename:  testapp.c
 *
 *    Description:  Test Main Stub
 *
 *        Version:  1.0
 *        Created:  Wednesday 23 August 2017 07:38:47  IST
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

#include "instance.h"
#include <stdio.h>
#include "libcli.h"

/*import from spfdcm.c*/
extern void
spf_init_dcm();

/*import from topo.c*/
extern instance_t * build_linear_topo();
extern instance_t * build_multi_area_topo();
extern instance_t * build_ring_topo();
extern instance_t * build_cisco_example_topo();
extern instance_t * overload_router_topo();
extern instance_t * pseudonode_ecmp_topo();
extern instance_t * lsp_ecmp_topo();
extern instance_t * broadcast_link_protecting_lfa();
extern instance_t * build_ecmp_topo2();
extern instance_t * build_multi_link_topo();
extern instance_t * build_ring_topo_7nodes();
extern instance_t * build_rlfa_topo();
extern instance_t * build_lfa_topo();
extern instance_t * multi_primary_nxt_hops();
extern instance_t * one_hop_backup();
extern instance_t * lsp_as_backup_topo();
extern instance_t * tilfa_topo_parallel_links();
extern instance_t * tilfa_topo_one_hop_test();
extern instance_t * tilfa_topo_p_q_distance_1();
extern instance_t * tilfa_topo_page_408_node_protection();
extern instance_t * tilfa_topo_2_adj_segment_example();

/*Globals */
instance_t *instance = NULL;

int
main(int argc, char **argv){

    /* Lib cli initialization */
    spf_init_dcm();

    /* Topology Initialization*/
    //instance = build_linear_topo();
    //instance = pseudonode_ecmp_topo();
    //instance = lsp_ecmp_topo();
    //instance = build_multi_area_topo();
    //instance = build_ring_topo();
    //instance = build_ring_topo_7nodes();
    //instance = build_ecmp_topo2();
    //instance = build_cisco_example_topo();
    //instance = broadcast_link_protecting_lfa();
    //instance = build_multi_link_topo();
    //instance = lsp_as_backup_topo();
    //instance = build_rlfa_topo();
    //instance = build_lfa_topo();
    //instance = overload_router_topo();
    //instance = multi_primary_nxt_hops();
    //instance = one_hop_backup();
    //instance = tilfa_topo_parallel_links();
    //instance = tilfa_topo_one_hop_test();
    instance = tilfa_topo_p_q_distance_1();
    //instance = tilfa_topo_page_408_node_protection();
    //instance = tilfa_topo_2_adj_segment_example();
    start_shell();
    return 0;
}
