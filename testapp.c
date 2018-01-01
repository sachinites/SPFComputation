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
    // instance = build_multi_area_topo();
    instance = build_ring_topo();
    //instance = build_cisco_example_topo();
    //instance = broadcast_link_protecting_lfa();
    //instance = overload_router_topo();
    //leak_prefix("R3", "100.1.1.1", 24, LEVEL2, LEVEL1);

    start_shell();
    return 0;
}
