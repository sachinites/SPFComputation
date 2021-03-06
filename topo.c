/*
 * =====================================================================================
 *
 *       Filename:  topo.c
 *
 *    Description:  This file builds the network topology
 *
 *        Version:  1.0
 *        Created:  Thursday 24 August 2017 01:07:07  IST
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
#include "cmdtlv.h"
#include "libcli.h"
#include "spfcmdcodes.h"
#include "spfclihandler.h"
#include "LinuxMemoryManager/uapi_mm.h"

extern instance_t *instance;

instance_t *
build_ecmp_topo2(){

/*                         +--------+
                6      0/0 |        |0/1       1
         +-----------------+   B    +-------------------+      
         |      10.1.1.2/24|        |50.1.1.1/24        |
         |                 +--------+                   |
         |                                              |
         |                                              |
         |0/0                                           |0/1
 *       |10.1.1.1/24                                   |50.1.1.2/24
 *  +----+----+              +-------+              +---+---+
 *  |         |0/1    5   0/1|       |0/2    2    .2|       |
 *  |   S     +--------------|   E   +--------------+  D    |
 *  |         |20.1.1.1/24 .2|       |30.1.1.1   0/2|       |
 *  |         |              |       |              +---+---+
 *  +----+----+              +-------+                  |40.1.1.2/24
 *       |40.1.1.1/24                                   |0/3
 *       |0/2                                           |
 *       |                                              |
 *       |                                              |
 *       |                                              |
 *       |               +----------+                   |
         |               |          |                   |
         |         7     |   PN     |      0            |
         +---------------+          +-------------------+
                      0/2|          |0/3
                         +----------+

*/

    instance_t *instance = get_new_instance();

    node_t *S = create_new_node(instance, "S", AREA1, "192.168.0.1");
    node_t *E = create_new_node(instance, "E", AREA1, "192.168.0.2");
    node_t *B = create_new_node(instance, "B", AREA1, "192.168.0.3");
    node_t *PN = create_new_node(instance, "PN", AREA1, ZERO_IP);
    node_t *D = create_new_node(instance, "D", AREA1, "192.168.0.4");

    prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL1);
    prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL1);
    prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL1);
    prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL1);
    prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
    prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);
    prefix_t *prefix_40_1_1_1_24 = create_new_prefix("40.1.1.1", 24, LEVEL1);
    prefix_t *prefix_40_1_1_2_24 = create_new_prefix("40.1.1.2", 24, LEVEL1);
    prefix_t *prefix_50_1_1_1_24 = create_new_prefix("50.1.1.1", 24, LEVEL1);
    prefix_t *prefix_50_1_1_2_24 = create_new_prefix("50.1.1.2", 24, LEVEL1);
    

    edge_t *S_B_edge = create_new_edge("eth0/0", "eth0/0", 6,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL1);

    edge_t *S_E_edge = create_new_edge("eth0/1", "eth0/1", 5,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL1);

    edge_t *S_PN_edge = create_new_edge("eth0/2", "eth0/2", 7,
            prefix_40_1_1_1_24, 0, LEVEL1);

    edge_t *PN_D_edge = create_new_edge("eth0/3", "eth0/3", 0,
            0, prefix_40_1_1_2_24, LEVEL1);
    
    edge_t *E_D_edge = create_new_edge("eth0/2", "eth0/2", 2,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);
    
    edge_t *B_D_edge = create_new_edge("eth0/1", "eth0/1", 1,
            prefix_50_1_1_1_24, prefix_50_1_1_2_24, LEVEL1);

    insert_edge_between_2_nodes(S_B_edge, S, B, BIDIRECTIONAL);
    insert_edge_between_2_nodes(S_E_edge, S, E, BIDIRECTIONAL);
    insert_edge_between_2_nodes(S_PN_edge, S, PN, BIDIRECTIONAL);
    insert_edge_between_2_nodes(PN_D_edge, PN, D, BIDIRECTIONAL);
    insert_edge_between_2_nodes(E_D_edge, E, D, BIDIRECTIONAL);
    insert_edge_between_2_nodes(B_D_edge, B, D, BIDIRECTIONAL);
    mark_node_pseudonode(PN, LEVEL1);
    set_instance_root(instance, S);
    return instance;
}


instance_t *
build_multi_link_topo(){


/*
 *                                                                +----------------+                  +------------+
 *+--------------+0/0 10.1.1.1          10           0/1 10.1.1.2 |                | 0/2      20.1.1.2|            |
 *|              +------------------------------------------------+                +----------10------+            |
 *|              |                                                |                |20.1.1.1       0/0|     R2     |
 *|     R0       |                                                |      R1        |                  |            |
 *|              |0/1 30.1.1.1          10           0/0 30.1.1.2 |                |                  |            |
 *|              +------------------------------------------------+                |                  +------------+
 *+--------------+                                                |                |
 *                                                                |----------------+
 *
 *
 * */

    
    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "192.168.0.1");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.2");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.3");

    prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL1);
    prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL1);
    prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL1);
    prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL1);
    prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
    prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);

    edge_t *R0_R1_edge1 = create_new_edge("eth0/0", "eth0/1", 10,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL1);

    edge_t *R0_R1_edge2 = create_new_edge("eth0/1", "eth0/0", 10,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);

    edge_t *R1_R2_edge = create_new_edge("eth0/2", "eth0/0", 10,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL1);

    insert_edge_between_2_nodes(R0_R1_edge1, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge2, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R2_edge, R1, R2, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
build_linear_topo(){

    /*
     *                                                                                                          
     * +------+               +------+                +-------+               +-------+               +--------+
     * |      |0/0    10.1.1.2|      |0/2     20.1.1.2|       |0/4    30.1.1.2|       | 0/6   40.1.1.2|        |
     * |  R0  +-------L2------+  R1  +------L2--------+  R2   +-------L1------+  R3   +-----L1-- -----+  R4    |
     * |      |10.1.1.1    0/1|      |20.1.1.1    0/3 |       |30.1.1.1    0/5|       |40.1.1.1    0/7|        |
     * +------+               +------+                +-------+               +-------+               +--------+
     *                                                    
     * */
    
    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "192.168.0.1");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.2");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.3");
    node_t *R3 = create_new_node(instance, "R3", AREA1, "192.168.0.4");
    node_t *R4 = create_new_node(instance, "R4", AREA1, "192.168.0.5");

    prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL2);
    prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL2);
    prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL2);
    prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL2);
    prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
    prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);
    prefix_t *prefix_40_1_1_1_24 = create_new_prefix("40.1.1.1", 24, LEVEL1);
    prefix_t *prefix_40_1_1_2_24 = create_new_prefix("40.1.1.2", 24, LEVEL1);

    edge_t *R0_R1_edge = create_new_edge("eth0/0", "eth0/1", 10,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL2);

    edge_t *R1_R2_edge = create_new_edge("eth0/2", "eth0/3", 10,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL2);
    
    edge_t *R2_R3_edge = create_new_edge("eth0/4", "eth0/5", 10,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);

    edge_t *R3_R4_edge = create_new_edge("eth0/6", "eth0/7", 10,
            prefix_40_1_1_1_24, prefix_40_1_1_2_24, LEVEL1);

    insert_edge_between_2_nodes(R0_R1_edge, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R2_edge, R1, R2, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R2_R3_edge, R2, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R3_R4_edge, R3, R4, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
build_multi_area_topo(){

#if 0
 
                                                                                                      +-------------------+
                                                                                                      |                   |
 +----------------------------------------------+                                                     |     +-------+     |
 | +--------+10.1.1.2            0/0+-------+   |0/2                                         14.1.1.2 |     |       |     |
 | | R1     +------------L1---------+  R0   +---+---------------------------L2------------------------+-----+ R3    |     |
 | |        |0/0            10.1.1.1|       |   |14.1.1.1                                          0/2|     |       |     |
 | +----+---+                       ++------+   |                                                     |     +---+---+     |
 |      |0/1                         |0/1       |                                                     |      0/1|15.1.1.1 |
 |      |12.1.1.1                    |11.1.1.1  |                                                     |         |         |
 |      |                            |          |                                                     |         L12       |
 |      |                            L1         |                                                     |         |         |
 |      L1                           |          |                 Network Test Topology               |      0/1|15.1.1.2 |
 |      |          +-------+0/0      |          |                                                     |     +---+---+     |
 |      +----------+  R2   +---------+          |                                                     |     | R4    |     |
 |              0/1|       |11.1.1.2            |                                                     |     |       |     |
 |        12.1.1.2 +------++                    |                                                     |     +---+---+     |
 |                        | 0/2                 |                                                     | 16.1.1.1|0/2      |
 +------------------------+---------------------+                                                     +---------+---------+
                    AREA1 |20.1.1.1                                                                             |AREA2
                          |                                                                                     |
                          |                                                                                     |
                          |                                             +----------------------+                L2
                          |                                             |                      |                |
                          |                                             |       +------+       |                |
                          |                                             |    0/2|      |       |                |
                          +--------------L2-----------------------------+-------+ R5   |0/1    |                |
                                                                        20.1.1.2|      |-------+-------+--------+
                                                                        |       +---+--+16.1.1.2|
                                                                        |           |0/0       |
                                                                        |           |17.1.1.1  |
                                                                        |           |          |
                                                                        |           |          |
                                                                        |           L1         |
                                                                        |           |          |
                                                                        |           |          |
                                                                        |           |          |
                                                                        |        0/0|17.1.1.2  |
                                                                        |      +----+---+      |
                                                                        |      | R6     |      |
                                                                        |      |        |      |
                                                                        |      +--------+      |
                                                                        |                      |
                                                                        +----------------------+
                                                                               AREA3
 
#endif

    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "192.168.0.0");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.1");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.2");
    node_t *R3 = create_new_node(instance, "R3", AREA2, "192.168.0.3");
    node_t *R4 = create_new_node(instance, "R4", AREA2, "192.168.0.4");
    node_t *R5 = create_new_node(instance, "R5", AREA3, "192.168.0.5");
    node_t *R6 = create_new_node(instance, "R6", AREA3, "192.168.0.6");

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/0", 10, create_new_prefix("10.1.1.1", 24, LEVEL1), create_new_prefix("10.1.1.2", 24, LEVEL1), LEVEL1)),
                                R0, R1, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/0", 10, create_new_prefix("11.1.1.1", 24, LEVEL1), create_new_prefix("11.1.1.2", 24, LEVEL1), LEVEL1)),
                                R0, R2, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/1", 10, create_new_prefix("12.1.1.1", 24, LEVEL1), create_new_prefix("12.1.1.2", 24, LEVEL1), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/2", 10, create_new_prefix("14.1.1.1", 24, LEVEL2), create_new_prefix("14.1.1.2", 24, LEVEL2), LEVEL2)),
                                R0, R3, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/1", 10, create_new_prefix("15.1.1.1", 24, LEVEL12), create_new_prefix("15.1.1.2", 24, LEVEL12), LEVEL12)),
                                R3, R4, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/1", 10, create_new_prefix("16.1.1.1", 24, LEVEL2), create_new_prefix("16.1.1.2", 24, LEVEL2), LEVEL2)),
                                R4, R5, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/0", 10, create_new_prefix("17.1.1.1", 24, LEVEL1), create_new_prefix("17.1.1.2", 24, LEVEL1), LEVEL1)),
                                R5, R6, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/2", 10, create_new_prefix("20.1.1.1", 24, LEVEL2), create_new_prefix("20.1.1.2", 24, LEVEL2), LEVEL2)),
                                R2, R5, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
build_lfa_topo(){

#if 0

                          +----------+
                      0/1 |          |0/0 10.1.1.1/24
         +----------------+   R0_re  +--------------8------------+
         |    12.1.1.1/24 |          |                           |
         |                +----------+                           |
         |                                                       |
         5                                                       |
         |                                                       |
         |                                                       |
         |0/0 12.1.1.2/24                                        |0/0 10.1.1.2/24
     +---+---+                                              +----+-----+
     |       |0/1                                        0/1|          |
     | R2_re +----------------------5-----------------------+    R1_re |
     |       |11.1.1.2/24                        11.1.1.1/24|          |
     +-------+                                              +----------+

#endif


    instance_t *instance = get_new_instance();
    node_t *R0_re = create_new_node(instance, "R0_re", AREA1, "192.168.0.1");
    node_t *R1_re = create_new_node(instance, "R1_re", AREA1, "192.168.0.2");
    node_t *R2_re = create_new_node(instance, "R2_re", AREA1, "192.168.0.3");

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/0", 8, create_new_prefix("10.1.1.1", 24, LEVEL1 ), create_new_prefix("10.1.1.2", 24, LEVEL1), LEVEL1)), 
                                R0_re, R1_re, BIDIRECTIONAL);
    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/1", 5, create_new_prefix("11.1.1.1", 24, LEVEL1 ), create_new_prefix("11.1.1.2", 24, LEVEL1), LEVEL1)), 
                                R1_re, R2_re, BIDIRECTIONAL);
    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 5, create_new_prefix("12.1.1.2", 24, LEVEL1 ), create_new_prefix("12.1.1.1", 24, LEVEL1), LEVEL1)), 
                                R2_re, R0_re, BIDIRECTIONAL);
    set_instance_root(instance, R0_re);
    return instance;
}


instance_t *
build_rlfa_topo(){

#if 0
For link A---B, for Dest E, G and D should be RLFA 
                                                      |A
                                                0/11 /|\0/0
                                        15.1.1.2/30 / | \10.1.1.1/30
                                                   / 0/2 \
                                                  /1.1.1.1\
                                                 /   /30   \
                                                /     |     \
                                          0/10 /      |      \0/1 10.1.1.2/30
                                  15.1.1.1/30 /       |       \C 
                                             F        |        |0/2 11.1.1.1/30
                                          0/9|       0/0       |
                                  14.1.1.2/30|    1.1.1.2/30   |
                                             |        B        |
                                             |       0/1       |
                                             |    16.1.1.1/30  |
                                         0/8 |        |        |
                                  14.1.1.1/30|        |        |
                                            G|        |        |0/3 11.1.1.2/30
                                          0/7\        |       /D 
                                   13.1.1.2/30\       |      /0/4 12.1.1.1/30  
                                               \      |     /   
                                                \     |    /
                                                 \16.1.1.2/
                                                  \  /30 /
                                                   \ 0/0/
                                                    \ |/
                                                 0/6 \/0/5
                                          13.1.1.1/30 E 12.1.1.2/30


#endif    

    instance_t *instance = get_new_instance();

    node_t *A = create_new_node(instance, "A", AREA1, "192.168.0.1");
    node_t *C = create_new_node(instance, "C", AREA1, "192.168.0.2");
    node_t *D = create_new_node(instance, "D", AREA1, "192.168.0.3");
    node_t *E = create_new_node(instance, "E", AREA1, "192.168.0.4");
    node_t *G = create_new_node(instance, "G", AREA1, "192.168.0.5");
    node_t *F = create_new_node(instance, "F", AREA1, "192.168.0.6");
    node_t *B = create_new_node(instance, "B", AREA1, "192.168.0.7");

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("10.1.1.1", 30, LEVEL1), create_new_prefix("10.1.1.2", 30, LEVEL1), LEVEL1)),
                                A, C, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 10, create_new_prefix("11.1.1.1", 30, LEVEL1), create_new_prefix("11.1.1.2", 30, LEVEL1), LEVEL1)),
                                C, D, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/4", "eth0/5", 10, create_new_prefix("12.1.1.1", 30, LEVEL1), create_new_prefix("12.1.1.2", 30, LEVEL1), LEVEL1)),
                                D, E, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/6", "eth0/7", 10, create_new_prefix("13.1.1.1", 30, LEVEL1), create_new_prefix("13.1.1.2", 30, LEVEL1), LEVEL1)),
                                E, G, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/8", "eth0/9", 10, create_new_prefix("14.1.1.1", 30, LEVEL1), create_new_prefix("14.1.1.2", 30, LEVEL1), LEVEL1)),
                                G, F, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/10", "eth0/11", 10, create_new_prefix("15.1.1.1", 30, LEVEL1), create_new_prefix("15.1.1.2", 30, LEVEL1), LEVEL1)),
                                F, A, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/0", 10, create_new_prefix("1.1.1.1", 30, LEVEL1), create_new_prefix("1.1.1.2", 30, LEVEL1), LEVEL1)),
                                A, B, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/0", 10, create_new_prefix("16.1.1.1", 30, LEVEL1), create_new_prefix("16.1.1.2", 30, LEVEL1), LEVEL1)),
                                B, E, BIDIRECTIONAL);

    set_instance_root(instance, A);
    return instance;
}


instance_t *
build_ring_topo(){

#if 0
For link S---E, there should be no LFA for non ECMP dest. For ECMP dest, only link protection LFA exists which is again rejected because destination has ECMP paths
psp - AB, exp - ABC
qsp - CD
C is only PQ node for Dest E
B is the only PQ node for Dest D
    
                    S/-----------\E
                    /             \
                   /               D
                  A               /  
                   \             /   
                    \           /     
                   B \--------+/C

#endif    

    instance_t *instance = get_new_instance();

    node_t *A = create_new_node(instance, "A", AREA1, "192.168.0.1");
    node_t *B = create_new_node(instance, "B", AREA1, "192.168.0.2");
    node_t *C = create_new_node(instance, "C", AREA1, "192.168.0.3");
    node_t *D = create_new_node(instance, "D", AREA1, "192.168.0.4");
    node_t *E = create_new_node(instance, "E", AREA1, "192.168.0.5");
    node_t *S = create_new_node(instance, "S", AREA1, "192.168.0.6");

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("10.1.1.1", 30, LEVEL1), create_new_prefix("10.1.1.2", 30, LEVEL1), LEVEL1)),
                                S, E, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 10, create_new_prefix("20.1.1.1", 30, LEVEL1), create_new_prefix("20.1.1.2", 30, LEVEL1), LEVEL1)),
                                E, D, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/4", "eth0/5", 10, create_new_prefix("30.1.1.1", 30, LEVEL1), create_new_prefix("30.1.1.2", 30, LEVEL1), LEVEL1)),
                                D, C, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/6", "eth0/7", 10, create_new_prefix("40.1.1.1", 30, LEVEL1), create_new_prefix("40.1.1.2", 30, LEVEL1), LEVEL1)),
                                C, B, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/8", "eth0/9", 10, create_new_prefix("50.1.1.1", 30, LEVEL1), create_new_prefix("50.1.1.2", 30, LEVEL1), LEVEL1)),
                                B, A, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/10", "eth0/11", 10, create_new_prefix("60.1.1.1", 30, LEVEL1), create_new_prefix("60.1.1.2", 30, LEVEL1), LEVEL1)),
                                A, S, BIDIRECTIONAL);

    set_instance_root(instance, S);
    return instance;
}


instance_t *
build_ring_topo_7nodes(){

#if 0
For link S---E, there should be no LFA for non ECMP dest. For ECMP dest, only link protection LFA exists which is again rejected because destination has ECMP paths
psp - AB, exp - ABC
qsp - CD
C is only PQ node.
            
               +----------+                              +----------+
               |          |eth0/0                  eth0/1|          |
               |          +------------------------------+          |
               |    R0    |                              |   R1     |
               |          |                              |          |
               +----+-----+                              |          |
                    |0/1                                 +---+------+
                    |                                  eth0/0|
                    |                                        |
                    |                                        |
                    |                                        |
                    |                                        |    
                    |0/0                                     |
               +----+-----+                            eth0/1|
               |          |                            +-----+-------+
               |    R6    |                            |             |
               |          |                            |             |
               |          |                            |      R2     |
               +----+-----+                            |             |
                    |0/1                               |             |
                    |                                  +------+------+
                    |                                  eth0/0 |
                    |                                         |
                    |                                         |
                    |                                         |
                    |                                         |
                    |                                         |eth0/1
                    |0/0                                    +-+------+
              +-----+----+         +-------+                |        |
              |          |      0/0|       |eth0/1    eth0/0|        |
              |   R5     +---------+  R4   +----------------+   R3   |
              |          |0/1      |       |                |        |
              |          |         |       |                |        |
              +----------+         +-------+                +--------+

#endif    

    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "192.168.0.1");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.2");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.3");
    node_t *R3 = create_new_node(instance, "R3", AREA1, "192.168.0.4");
    node_t *R4 = create_new_node(instance, "R4", AREA1, "192.168.0.5");
    node_t *R5 = create_new_node(instance, "R5", AREA1, "192.168.0.6");
    node_t *R6 = create_new_node(instance, "R6", AREA1, "192.168.0.7");

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("10.1.1.1", 30, LEVEL1), create_new_prefix("10.1.1.2", 30, LEVEL1), LEVEL1)),
                                R0, R1, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("20.1.1.1", 30, LEVEL1), create_new_prefix("20.1.1.2", 30, LEVEL1), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("30.1.1.1", 30, LEVEL1), create_new_prefix("30.1.1.2", 30, LEVEL1), LEVEL1)),
                                R2, R3, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("40.1.1.1", 30, LEVEL1), create_new_prefix("40.1.1.2", 30, LEVEL1), LEVEL1)),
                                R3, R4, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("50.1.1.1", 30, LEVEL1), create_new_prefix("50.1.1.2", 30, LEVEL1), LEVEL1)),
                                R4, R5, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("60.1.1.1", 30, LEVEL1), create_new_prefix("60.1.1.2", 30, LEVEL1), LEVEL1)),
                                R5, R6, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("60.1.1.1", 30, LEVEL1), create_new_prefix("60.1.1.2", 30, LEVEL1), LEVEL1)),
                                R6, R0, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
build_cisco_example_topo(){

    /* https://www.cisco.com/c/en/us/support/docs/multiprotocol-label-switching-mpls/mpls/200370-Remote-Loop-Free-Alternate-Path-with-OSP.html*/


#if 0


                         0/0+-------+0/9 50.1.1.1
                   +--------+ R1    +-----+
                   |10.1.1.1|       |     |
                   |        +-------+     |
                   |                      |
                   |                      |
                   |                      |
                   |                      |
                   |                      |
                   |                      |
           10.1.1.2|0/1                   |0/8
               +---+-+                +---+--+              50.1.1.2+-------+
               |R2   +                +      +----------------------+       |
               |     |                | R5   |0/10              0/11| R6    |
               +--+--+                | PN   |                      |       |
          20.1.1.1|0/2                +---+--+                      +-------+
                  |                       |0/7
                  |                       |
                  |                       |
                  |                       |
                  |                       |
                  |                       |
                  |                       |
          20.1.1.2|0/3                    |0/6 50.1.1.3
              +---+--+                +---+---+
              |      |0/4         0/5 |       |
              | R3   +----------------+ R4    |
              |      |30.1.1.1        |       |
              +------+        30.1.1.2+-------+
#endif
              
    instance_t *instance = get_new_instance();

    node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.1");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.2");
    node_t *R3 = create_new_node(instance, "R3", AREA1, "192.168.0.3");
    node_t *R4 = create_new_node(instance, "R4", AREA1, "192.168.0.4");
    node_t *R5 = create_new_node(instance, "R5", AREA1, "192.168.0.5");
    node_t *R6 = create_new_node(instance, "R6", AREA1, "192.168.0.6");


    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("10.1.1.1", 30, LEVEL1), create_new_prefix("10.1.1.2", 30, LEVEL1), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 10, create_new_prefix("20.1.1.1", 30, LEVEL1), create_new_prefix("20.1.1.2", 30, LEVEL1), LEVEL1)),
                                R2, R3, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/4", "eth0/5", 10, create_new_prefix("30.1.1.1", 30, LEVEL1), create_new_prefix("30.1.1.2", 30, LEVEL1), LEVEL1)),
                                R3, R4, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/6", "eth0/7", 10, create_new_prefix("50.1.1.3", 24, LEVEL1), 0, LEVEL1)),
                                R4, R5, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/8", "eth0/9", 10, 0, create_new_prefix("50.1.1.1", 24, LEVEL1), LEVEL1)),
                                R5, R1, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/10", "eth0/11", 10, 0, create_new_prefix("50.1.1.2", 24, LEVEL1), LEVEL1)),
                                R5, R6, BIDIRECTIONAL);

    mark_node_pseudonode(R5, LEVEL1);
    set_instance_root(instance, R1);
    return instance;
}


instance_t *
broadcast_link_protecting_lfa(){

#if 0


                         0/0+-------+0/9 50.1.1.1
                   +--------+   S   +-----+
                   |10.1.1.1|       |     |
                   |        +-------+     |
                   |                      |15
                   |5                     |
                   |                      |
                   |                      |
                   |                      |
                   |                      |
                   |0/1           50.1.1.2|0/8
               +---+-+                +---+--+                               
               | PN  +0/10        0/10+      +                               
               |    -+----------------+ N    |                               
               +--+--+        10.1.1.3|      |                               
                  |0/2                +---+--+                               
                  |              60.1.1.2 |0/7
                  |                       |
                  |                       |
                  |5                      |8
                  |                       |
                  |                       |
                  |                       |
          10.1.1.2|0/3                    |0/6 60.1.1.1
              +---+--+                +---+---+
              |      |0/4    5    0/5 |       |
              | E    +----------------+ D     |
              |      |30.1.1.1        |       |
              +------+        30.1.1.2+-------+
#endif
              
    instance_t *instance = get_new_instance();

    node_t *S = create_new_node(instance, "S", AREA1, "192.168.0.1");
    node_t *PN = create_new_node(instance, "PN", AREA1, ZERO_IP);
    node_t *E = create_new_node(instance, "E", AREA1, "192.168.0.3");
    node_t *D = create_new_node(instance, "D", AREA1, "192.168.0.4");
    node_t *N = create_new_node(instance, "N", AREA1, "192.168.0.5");

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 5, create_new_prefix("10.1.1.1", 30, LEVEL1), 0, LEVEL1)),
                                S, PN, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 5, 0, create_new_prefix("10.1.1.2", 30, LEVEL1), LEVEL1)),
                                PN, E, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/4", "eth0/5", 5, create_new_prefix("30.1.1.1", 30, LEVEL1), create_new_prefix("30.1.1.2", 30, LEVEL1), LEVEL1)),
                                E, D, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/6", "eth0/7", 8, create_new_prefix("60.1.1.1", 30, LEVEL1), create_new_prefix("60.1.1.2", 30, LEVEL1), LEVEL1)),
                                D, N, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/8", "eth0/9", 15, create_new_prefix("50.1.1.2", 30, LEVEL1), create_new_prefix("50.1.1.1", 30, LEVEL1), LEVEL1)),
                                N, S, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/10", "eth0/10", 5, 0, create_new_prefix("10.1.1.3", 30, LEVEL1), LEVEL1)),
                                PN, N, BIDIRECTIONAL);

    mark_node_pseudonode(PN, LEVEL1); 
    set_instance_root(instance, N);                                        
    return instance;                                                        
}                                                                           

instance_t *
lsp_as_backup_topo(){
              
#if 0 
                              +------+
                       5.1.1.1|      |4.1.1.2              5
       +----------------------+  R4  +-------------------------------------+
       |                   0/2|      |0/1                                  |
       |                      +------+                                     |
       |                                                            4.1.1.1|0/2
       |                                                                +--+-----+
       |5                                                               |        |
       |                                                                |  R3    |
       |                                                                +--+-----+
       |                                                                   |0/1
       |                                                                   |3.1.1.2
       |5.1.1.2                                                            |
       |0/1                   +------+                +-------+            |
     +-+---+0/0    2   1.1.1.2| R1   |0/2      2.2.2.2|  R2   |0/2         |(8)
     |     |------------------+      +------5---------+      +------------+
     + R0  |1.1.1.1        0/1+------+2.2.2.1      0/1+---++--+3.1.1.1
     |     |                                              ||
     +++---+                                              ||
      ||                                                  ||
      |+-------------------------R0-R2 RSVP LSP-----------+|
      +------------------------------9--------------------+

#endif

    instance_t *instance = get_new_instance();
    node_t *R0_re = create_new_node(instance, "R0_re", AREA1, "192.168.0.1");
    node_t *R1_re = create_new_node(instance, "R1_re", AREA1, "192.168.0.2");
    node_t *R2_re = create_new_node(instance, "R2_re", AREA1, "192.168.0.3");
    node_t *R3_re = create_new_node(instance, "R3_re", AREA1, "192.168.0.4");
    node_t *R4_re = create_new_node(instance, "R4_re", AREA1, "192.168.0.5");


    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/2", 5, create_new_prefix("5.1.1.2", 30, LEVEL1), create_new_prefix("5.1.1.1", 30, LEVEL1), LEVEL1)),
                                R0_re, R4_re, BIDIRECTIONAL);
    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 2, create_new_prefix("1.1.1.1", 30, LEVEL1), create_new_prefix("1.1.1.2", 30, LEVEL1), LEVEL1)),
                                R0_re, R1_re, BIDIRECTIONAL);
    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/2", 5, create_new_prefix("4.1.1.2", 30, LEVEL1), create_new_prefix("4.1.1.1", 30, LEVEL1), LEVEL1)),
                                R4_re, R3_re, BIDIRECTIONAL);
    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/2", 8, create_new_prefix("3.1.1.2", 30, LEVEL1), create_new_prefix("3.1.1.1", 30, LEVEL1), LEVEL1)),
                                R3_re, R2_re, BIDIRECTIONAL);
    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/2", 5, create_new_prefix("2.2.2.2", 30, LEVEL1), create_new_prefix("2.2.2.1", 30, LEVEL1), LEVEL1)),
                                R2_re, R1_re, BIDIRECTIONAL);
    set_instance_root(instance, R0_re);
    return instance;
}



instance_t *
pseudonode_ecmp_topo(){

#if 0
                                          0/2  +---------+
               +-------------------------------+         |70.1.1.1
               |    5                  60.1.1.2|   D     +-----------------------------------+
               |                               +---------+ 0/3                          5    |
               |                                                                             |
               |                                                                             |
               |                                                                             |
               |                                                                             |
               |                                                                             |
               |                                                                         0/3 |70.1.1.2
       60.1.1.1|0/2                      +-----------+                                  +----+----+
          +----+----+                    |           |             10               0/1 |         |
          |         |50.1.1.1            |    PN     +----------------------------------+   C     |
          |   A     +--------------------+           |                         50.1.1.2 |         |
          |         |0/1      10         |           |                                  +---------+
          +---------+                    +-----------+

#endif

    instance_t *instance = get_new_instance();
    node_t *A  = create_new_node(instance, "A",  AREA1, "192.168.0.1");
    node_t *PN = create_new_node(instance, "PN", AREA1, "192.168.0.1");
    node_t *C  = create_new_node(instance, "C",  AREA1, "192.168.0.3");
    node_t *D  = create_new_node(instance, "D" , AREA1, "192.168.0.2");

    insert_edge_between_2_nodes((create_new_edge(0, "eth0/1", 10, 
                                 0, create_new_prefix("50.1.1.2", 30, LEVEL1),
                                 LEVEL1)),
                                 PN, C, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/2", 5, 
                                 create_new_prefix("60.1.1.1", 30, LEVEL1), create_new_prefix("60.1.1.2", 30, LEVEL1),
                                 LEVEL1)),
                                 A, D, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/1", 0, 10, 
                                 create_new_prefix("50.1.1.1", 30, LEVEL1), 0,
                                 LEVEL1)),
                                 A, PN, BIDIRECTIONAL);
    

    insert_edge_between_2_nodes((create_new_edge("eth0/3", "eth0/3", 5, 
                                 create_new_prefix("70.1.1.1", 30, LEVEL1), create_new_prefix("70.1.1.2", 30, LEVEL1),
                                 LEVEL1)),
                                 D,C,  BIDIRECTIONAL);

    set_instance_root(instance, A);
    mark_node_pseudonode(PN, LEVEL1);
    return instance;
}



instance_t *
overload_router_topo(){

#if 0


                         0/0+-------+0/9 50.1.1.1
                   +--------+ R1    +-----+
                   |10.1.1.1|       |     |
                   |        +-------+     |
                   |                      |
                   |50                    |10
                   |                      |
                   |                      |
                   |                      |
                   |                      |
           10.1.1.2|0/1           50.1.1.2|0/8
               +---+-+                +---+--+              70.1.1.2+-------+
               |R2   +                +      +----------------------+       |
               |     |                | R5   |0/10     10       0/11| R6    |
               +--+--+                |      |70.1.1.1              |       |
          20.1.1.1|0/2                +---+--+                      +-------+
                  |              60.1.1.2 |0/7
                  |                       |
                  |                       |
                  |100                    |10
                  |                       |
                  |                       |
                  |                       |
          20.1.1.2|0/3                    |0/6 60.1.1.1
              +---+--+                +---+---+
              |      |0/4   10    0/5 |       |
              | R3   +----------------+ R4    |
              |      |30.1.1.1        |       |
              +------+        30.1.1.2+-------+
#endif
              
    instance_t *instance = get_new_instance();

    node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.1");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.2");
    node_t *R3 = create_new_node(instance, "R3", AREA1, "192.168.0.3");
    node_t *R4 = create_new_node(instance, "R4", AREA1, "192.168.0.4");
    node_t *R5 = create_new_node(instance, "R5", AREA1, "192.168.0.5");
    node_t *R6 = create_new_node(instance, "R6", AREA1, "192.168.0.6");


    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 50, create_new_prefix("10.1.1.1", 30, LEVEL1), create_new_prefix("10.1.1.2", 30, LEVEL1), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 100, create_new_prefix("20.1.1.1", 30, LEVEL1), create_new_prefix("20.1.1.2", 30, LEVEL1), LEVEL1)),
                                R2, R3, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/4", "eth0/5", 10, create_new_prefix("30.1.1.1", 30, LEVEL1), create_new_prefix("30.1.1.2", 30, LEVEL1), LEVEL1)),
                                R3, R4, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/6", "eth0/7", 10, create_new_prefix("60.1.1.1", 30, LEVEL1), create_new_prefix("60.1.1.2", 30, LEVEL1), LEVEL1)),
                                R4, R5, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/8", "eth0/9", 10, create_new_prefix("50.1.1.2", 30, LEVEL1), create_new_prefix("50.1.1.1", 30, LEVEL1), LEVEL1)),
                                R5, R1, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/10", "eth0/11", 10, create_new_prefix("70.1.1.1", 30, LEVEL1), create_new_prefix("70.1.1.2", 30, LEVEL1), LEVEL1)),
                                R5, R6, BIDIRECTIONAL);

    set_instance_root(instance, R1);                                        
    return instance;                                                        
}                                                                           
                                                                     


instance_t *
lsp_ecmp_topo(){

#if 0

                                                                           +------------+
                                                                           |            |
                        +-------------+ 0/2 - 40.1.1.1        40.1.1.2-0/2 |    C       |
                        |     B       +------------------------------------+            |
                        |             |                 2                  |            |
                        +-----------+-+                                    +-----+------+
                                /   |0/1-30.1.1.1                                |0/1-50.1.1.1
                               /    |                                            |
                              /     |                                            |
                             /      |                                            |
                         LSP/ 5     |                                            |
                           /A-B     |                                            |
                          /         |                                            |
                         /          |                                            |
               +------+ /           |1                                           |1
               |      |/            |                                            |
               |  A   /             |                                            |
               |      |\0/2-20.1.1.1|                                            |
               +------+ \           |                                            |
                         \ 2        |                                            |
                          \         |                                            |
                           \        |                                            |
                            \       |                                            |
                             \    +-+                                            |
                20.1.1.2- 0/1 \   |0/2-30.1.1.2                                  |0/1-50.1.1.1
                           +------++                                         +---+---+
                           |       |                     3                   |       |
                           |   E   +-----------------------------------------+  D    |
                           |       |0/3-60.1.1.1                 60.1.1.2-0/2|       |
                           +-------+                                         +-------+


#endif
              
    instance_t *instance = get_new_instance();

    node_t *A = create_new_node(instance, "A", AREA1, "192.168.0.1");
    node_t *B = create_new_node(instance, "B", AREA1, "192.168.0.2");
    node_t *C = create_new_node(instance, "C", AREA1, "192.168.0.3");
    node_t *D = create_new_node(instance, "D", AREA1, "192.168.0.4");
    node_t *E = create_new_node(instance, "E", AREA1, "192.168.0.5");


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/1", 2, create_new_prefix("20.1.1.1", 30, LEVEL1), create_new_prefix("20.1.1.2", 30, LEVEL1), LEVEL1)),
                                A, E, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/1", 1, create_new_prefix("30.1.1.2", 30, LEVEL1), create_new_prefix("30.1.1.1", 30, LEVEL1), LEVEL1)),
                                E, B, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/2", 2, create_new_prefix("40.1.1.1", 30, LEVEL1), create_new_prefix("40.1.1.2", 30, LEVEL1), LEVEL1)),
                                B, C, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/1", 1, create_new_prefix("50.1.1.1", 30, LEVEL1), create_new_prefix("50.1.1.2", 30, LEVEL1), LEVEL1)),
                                C, D, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 3, create_new_prefix("60.1.1.2", 30, LEVEL1), create_new_prefix("60.1.1.1", 30, LEVEL1), LEVEL1)),
                                D, E, BIDIRECTIONAL);

#if 0
    /* LSP should be inserted after spf runs, not at compile time*/
    config node A lsp A-B metric 5 to 192.168.0.2 level 1
    config node B lsp B-A metric 5 to 192.168.0.1 level 1
    insert_lsp_as_forward_adjacency(A, "A-B", 5, "192.168.0.2", LEVEL1);
    insert_lsp_as_forward_adjacency(B, "B-A", 5, "192.168.0.1", LEVEL1);
#endif
    set_instance_root(instance, A);
    return instance;
}



instance_t *
multi_primary_nxt_hops(){
#if 0
                                                                       +---------+
                                                              11.1.1.2 |         |
                                                  +---------10-------+-+  R2     |
                                                  |11.1.1.1       0/1  |         |
                                                  |0/2                 |         |
                                             +----+---+                +-----+-+-+
           +------+10.1.1.1              0/1 |        |                      |0/2 20.1.1.1  
           |      +---------10---------------+   R1   |                      +10(cost)
           | R0   |0/0               10.1.1.2|        |                      |
           +--+--+                          +-+-+----+                      |0/2 20.1.1.2
              |0/1                        0/3 |  |0/0                   +----++---+
              |30.1.1.1               30.1.1.2|  |12.1.1.1              |         |
              |                               |  |                  0/1 |         |
              +---------------10--------------+  +----------10----------+   R3    |
                                                                12.1.1.2|         |
                                                                        +---------+

#endif
              
    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "192.168.0.1");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.2");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.3");
    node_t *R3 = create_new_node(instance, "R3", AREA1, "192.168.0.4");


    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("10.1.1.1", 30, LEVEL1), create_new_prefix("10.1.1.2", 30, LEVEL1), LEVEL1)),
                                R0, R1, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/3", 10, create_new_prefix("30.1.1.1", 30, LEVEL1), create_new_prefix("30.1.1.2", 30, LEVEL1), LEVEL1)),
                                R0, R1, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/1", 10, create_new_prefix("11.1.1.1", 30, LEVEL1), create_new_prefix("11.1.1.2", 30, LEVEL1), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("12.1.1.1", 30, LEVEL1), create_new_prefix("12.1.1.2", 30, LEVEL1), LEVEL1)),
                                R1, R3, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/2", 10, create_new_prefix("20.1.1.1", 30, LEVEL1), create_new_prefix("20.1.1.2", 30, LEVEL1), LEVEL1)),
                                R2, R3, BIDIRECTIONAL);

    set_instance_root(instance, R0);                                        
    return instance;                                                        
}                                                                           

instance_t *
one_hop_backup(){

#if 0

                                         192.168.0.5
           192.168.0.4                   +------+
           +------+eth0/1      3.1.1.2/30|      |
           |  R3  +----------------------+ R4   |
           |      |3.1.1.1/30      eth0/2|      |
           +--+---+                      +---+--+
    2.1.1.2/30|eth0/10                  eth0/1|4.1.1.1/30
              |                               |
              |                               |
              |                               |
              |                               |
              |                               |
              |                               |
              |                               |
              |                               |
    2.1.1.1/30|eth0/2                         |
         +----+---+                           |
         |  R2    |                           |
         |192.168.|                           |
         |  0.3   |                           |
         +----+---+                           |
    1.1.1.2/30|eth0/1                         |
              |                               |
              |                               |
              |                               |
              |                           4.1.1.2/30|eth0/2
    1.1.1.1/30|eth0/0                       +--+--------+
     +-----+--+---+10.1.1.2/30        eth0/0|  R0       |
     +       R1   +-------------------------+192.168.0.1|
     | 192.168.0.2|eth0/1        10.1.1.1/30+------+----+
     |            |
     +------+-----+

  
#endif

  instance_t *instance = get_new_instance();

  node_t *R0 = create_new_node(instance, "R0", AREA1, "192.168.0.1");
  node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.2");
  node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.3");
  node_t *R3 = create_new_node(instance, "R3", AREA1, "192.168.0.4");
  node_t *R4 = create_new_node(instance, "R4", AREA1, "192.168.0.5");

    
  insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("10.1.1.1", 30, LEVEL1), create_new_prefix("10.1.1.2", 30, LEVEL1), LEVEL1)),
                                R0, R1, BIDIRECTIONAL);

  insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("1.1.1.1", 30, LEVEL1), create_new_prefix("1.1.1.2", 30, LEVEL1), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);

  insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/10", 10, create_new_prefix("2.1.1.1", 30, LEVEL1), create_new_prefix("2.1.1.2", 30, LEVEL1), LEVEL1)),
                                R2, R3, BIDIRECTIONAL);

  insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/2", 10, create_new_prefix("3.1.1.1", 30, LEVEL1), create_new_prefix("3.1.1.2", 30, LEVEL1), LEVEL1)),
                                R3, R4, BIDIRECTIONAL);
  
  insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/2", 50, create_new_prefix("4.1.1.1", 30, LEVEL1), create_new_prefix("4.1.1.2", 30, LEVEL1), LEVEL1)),
                                R4, R0, BIDIRECTIONAL);

 
  set_instance_root(instance, R0);
  return instance;
}


/*Tilfa topologies :*/


instance_t *
tilfa_topo_parallel_links(){

/*
    +--------------+0/0 10.1.1.1        10                           10.1.1.2 0/1+----------------+
    |              +------------------------------------------------------------++                |
    |              |                                                             |                |
    |              |0/1 30.1.1.1        15                           30.1.1.2 0/0|                |
    |    R0        +-------------------------------------------------------------+    R1          |
    |  122.1.1.1   |                                                             |   122.1.1.2    |
    |              |0/2 20.1.1.1        15                           20.1.1.2 0/2|                |
    +              +-------------------------------------------------------------+                |
    |              |                                                             |                |
    +              +0/3 40.1.1.1        15                           40.1.1.2 0/3+                +
    |              |+-------------++---------------------------------------------+                |
    |              |                                                             |                |
    |              |0/4 50.1.1.1        15                           50.1.1.2 0/4|                |
    |              |+-------------+----------------------------------------------+                |
    |              |                                                             |                |
    +--------------+                                                             +----------------+
                                                                                 
                                                                                 
*/
    
    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "122.1.1.1");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "122.1.1.2");

    prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL1);
    prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL1);
    prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL1);
    prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL1);
    prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
    prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);
    prefix_t *prefix_40_1_1_1_24 = create_new_prefix("40.1.1.1", 24, LEVEL1);
    prefix_t *prefix_40_1_1_2_24 = create_new_prefix("40.1.1.2", 24, LEVEL1);
    prefix_t *prefix_50_1_1_1_24 = create_new_prefix("50.1.1.1", 24, LEVEL1);
    prefix_t *prefix_50_1_1_2_24 = create_new_prefix("50.1.1.2", 24, LEVEL1);

    edge_t *R0_R1_edge1 = create_new_edge("eth0/0", "eth0/1", 10,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL1);

    edge_t *R0_R1_edge2 = create_new_edge("eth0/1", "eth0/0", 15,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);

    edge_t *R0_R1_edge3 = create_new_edge("eth0/2", "eth0/2", 15,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL1);
    
    edge_t *R0_R1_edge4 = create_new_edge("eth0/3", "eth0/3", 15,
            prefix_40_1_1_1_24, prefix_40_1_1_2_24, LEVEL1);

    edge_t *R0_R1_edge5 = create_new_edge("eth0/4", "eth0/4", 15,
            prefix_50_1_1_1_24, prefix_50_1_1_2_24, LEVEL1);

    insert_edge_between_2_nodes(R0_R1_edge1, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge2, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge3, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge4, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge5, R0, R1, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
tilfa_topo_one_hop_test(){

/*
    +--------------+0/0 10.1.1.1        10                           10.1.1.2 0/1+----------------+
    |              +------------------------------------------------------------++                |
    |              |                                                             |                |
    |              |0/1 30.1.1.1        15                           30.1.1.2 0/0|                |
    |    R0        +-------------------------------------------------------------+    R1          |
    |  122.1.1.1   |                                                             |   122.1.1.2    |
    |              |0/2 20.1.1.1        15                           20.1.1.2 0/2|                |
    +              +-------------------------------------------------------------+                |
    |              |                                                             |                |
    +              +0/3 40.1.1.1        15                           40.1.1.2 0/3+                +
    |              |+-------------++---------------------------------------------+                |
    |              |                                                             |                |
    |              |0/4 50.1.1.1        15                           50.1.1.2 0/4|                |
    |              |+-------------+----------------------------------------------+                |
    |              |                                                             |                |
    +-------+------+                                                             +------+---------+
            |0/5                                                                        |0/5
            |60.1.1.1                                                                   |70.1.1.2
            |                                                                           |
            |                                                                           |
            |                                                                           |
            |                                                                           |
            |                       +---------+                                         |
            |                       |         |                                         |
            |         7          0/1|  R2     |0/2           8                          |
            +-----------------------+122.1.1.3+-----------------------------------------+
                           60.1.1.2 |         |70.1.1.1
                                    |         |
                                    +---------+

*/
    
    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "122.1.1.1");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "122.1.1.2");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "122.1.1.3");

    prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL1);
    prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL1);
    prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL1);
    prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL1);
    prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
    prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);
    prefix_t *prefix_40_1_1_1_24 = create_new_prefix("40.1.1.1", 24, LEVEL1);
    prefix_t *prefix_40_1_1_2_24 = create_new_prefix("40.1.1.2", 24, LEVEL1);
    prefix_t *prefix_50_1_1_1_24 = create_new_prefix("50.1.1.1", 24, LEVEL1);
    prefix_t *prefix_50_1_1_2_24 = create_new_prefix("50.1.1.2", 24, LEVEL1);
    prefix_t *prefix_60_1_1_1_24 = create_new_prefix("60.1.1.1", 24, LEVEL1);
    prefix_t *prefix_60_1_1_2_24 = create_new_prefix("60.1.1.2", 24, LEVEL1);
    prefix_t *prefix_70_1_1_1_24 = create_new_prefix("70.1.1.1", 24, LEVEL1);
    prefix_t *prefix_70_1_1_2_24 = create_new_prefix("70.1.1.2", 24, LEVEL1);

    edge_t *R0_R1_edge1 = create_new_edge("eth0/0", "eth0/1", 10,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL1);

    edge_t *R0_R1_edge2 = create_new_edge("eth0/1", "eth0/0", 15,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);

    edge_t *R0_R1_edge3 = create_new_edge("eth0/2", "eth0/2", 15,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL1);
    
    edge_t *R0_R1_edge4 = create_new_edge("eth0/3", "eth0/3", 15,
            prefix_40_1_1_1_24, prefix_40_1_1_2_24, LEVEL1);

    edge_t *R0_R1_edge5 = create_new_edge("eth0/4", "eth0/4", 15,
            prefix_50_1_1_1_24, prefix_50_1_1_2_24, LEVEL1);

    edge_t *R0_R2_edge1 = create_new_edge("eth0/5", "eth0/1", 7,
            prefix_60_1_1_1_24, prefix_60_1_1_2_24, LEVEL1);
    
    edge_t *R2_R1_edge1 = create_new_edge("eth0/2", "eth0/5", 8,
            prefix_70_1_1_1_24, prefix_70_1_1_2_24, LEVEL1);

    insert_edge_between_2_nodes(R0_R1_edge1, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge2, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge3, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge4, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R1_edge5, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R2_edge1, R0, R2, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R2_R1_edge1, R2, R1, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
tilfa_topo_p_q_distance_1(){

/*
                                                              +---------+                           +---------+
            +---------+---+                                   |         |                           | R2      |
            |   R0        |10.1.1.1/24                  eth0/0| R1      |eth0/1          20.1.1.2/24|         |
            | 122.1.1.1   +-----------------------------------+122.1.1.2+---------------------------+122.1.1.3|
            |             |eth0/0                  10.1.1.2/24|         |20.1.1.1/24          eth0/2|         |
            |             |                                   |         |                           |         |
            +-+----+----+-+                                   +----+----+                           +---------+
              |eth |eth |eth0/1                                    |eth0/2
              |0/3 |0/2 |70.1.1.1/24                               |30.1.1.1/24
              |90.1|80.1|                                          |
              |    |    |                                          |
              |    |    |                                          |
              |    |    |                                          |
              |    |    |                                          |
              |    |    |                                          |
              |    |    |                                          |
              |    |    |                                          |
              |    |    |                                          |
              |90.2|80.2|                                          |
              |eth |eth |70.1.1.2/24                               |30.1.1.2/24
              |0/5 |0/4 |eth0/3                                    |eth0/0
           +--+----+----+--+eth0/0 40.1.1.1/24      eth0/1 .2/24+--+-------+
           |               +------------------------------------|          |
           | R4            |eth0/1 50.1.1.1/24      eth0/2 .2/24|  R3      |
           | 122.1.1.5     +------------------------------------|122.1.1.4 |
           |               |eth0/2 60.1.1.1/24      eth0/3 .2/24|          |
           |               +------------------------------------+          |
           +---------------+                                    +----------+
*/
    
    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "122.1.1.1");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "122.1.1.2");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "122.1.1.3");
    node_t *R3 = create_new_node(instance, "R3", AREA1, "122.1.1.4");
    node_t *R4 = create_new_node(instance, "R4", AREA1, "122.1.1.5");

    prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL1);
    prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL1);
    prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL1);
    prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL1);
    prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
    prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);
    prefix_t *prefix_40_1_1_1_24 = create_new_prefix("40.1.1.1", 24, LEVEL1);
    prefix_t *prefix_40_1_1_2_24 = create_new_prefix("40.1.1.2", 24, LEVEL1);
    prefix_t *prefix_50_1_1_1_24 = create_new_prefix("50.1.1.1", 24, LEVEL1);
    prefix_t *prefix_50_1_1_2_24 = create_new_prefix("50.1.1.2", 24, LEVEL1);
    prefix_t *prefix_60_1_1_1_24 = create_new_prefix("60.1.1.1", 24, LEVEL1);
    prefix_t *prefix_60_1_1_2_24 = create_new_prefix("60.1.1.2", 24, LEVEL1);
    prefix_t *prefix_70_1_1_1_24 = create_new_prefix("70.1.1.1", 24, LEVEL1);
    prefix_t *prefix_70_1_1_2_24 = create_new_prefix("70.1.1.2", 24, LEVEL1);
    prefix_t *prefix_80_1_1_1_24 = create_new_prefix("80.1.1.1", 24, LEVEL1);
    prefix_t *prefix_80_1_1_2_24 = create_new_prefix("80.1.1.2", 24, LEVEL1);
    prefix_t *prefix_90_1_1_1_24 = create_new_prefix("90.1.1.1", 24, LEVEL1);
    prefix_t *prefix_90_1_1_2_24 = create_new_prefix("90.1.1.2", 24, LEVEL1);

    edge_t *R0_R1_edge1 = create_new_edge("eth0/0", "eth0/0", 10,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL1);

    edge_t *R1_R2_edge1 = create_new_edge("eth0/1", "eth0/2", 10,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL1);

    edge_t *R1_R3_edge1 = create_new_edge("eth0/2", "eth0/0", 10,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);
    
    edge_t *R4_R3_edge1 = create_new_edge("eth0/0", "eth0/1", 10,
            prefix_40_1_1_1_24, prefix_40_1_1_2_24, LEVEL1);

    edge_t *R4_R3_edge2 = create_new_edge("eth0/1", "eth0/2", 10,
            prefix_50_1_1_1_24, prefix_50_1_1_2_24, LEVEL1);

    edge_t *R4_R3_edge3 = create_new_edge("eth0/2", "eth0/3", 10,
            prefix_60_1_1_1_24, prefix_60_1_1_2_24, LEVEL1);
    
    edge_t *R0_R4_edge1 = create_new_edge("eth0/1", "eth0/3", 10,
            prefix_70_1_1_1_24, prefix_70_1_1_2_24, LEVEL1);

    edge_t *R0_R4_edge2 = create_new_edge("eth0/2", "eth0/4", 10,
            prefix_80_1_1_1_24, prefix_80_1_1_2_24, LEVEL1);
    
    edge_t *R0_R4_edge3 = create_new_edge("eth0/3", "eth0/5", 10,
            prefix_90_1_1_1_24, prefix_90_1_1_2_24, LEVEL1);

    insert_edge_between_2_nodes(R0_R1_edge1, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R2_edge1, R1, R2, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R3_edge1, R1, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R4_R3_edge1, R4, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R4_R3_edge2, R4, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R4_R3_edge3, R4, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R4_edge1, R0, R4, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R4_edge2, R0, R4, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R0_R4_edge3, R0, R4, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
tilfa_topo_page_408_node_protection(){

   instance_t *instance = get_new_instance();

   node_t *R11 = create_new_node(instance, "R11", AREA1, "122.1.1.11");
   node_t *R1 =  create_new_node(instance,  "R1", AREA1,  "122.1.1.1");
   node_t *R5 =  create_new_node(instance,  "R5", AREA1,  "122.1.1.5");
   node_t *R4 =  create_new_node(instance,  "R4", AREA1,  "122.1.1.4");
   node_t *R3 =  create_new_node(instance,  "R3", AREA1,  "122.1.1.3");
   node_t *R2 =  create_new_node(instance,  "R2", AREA1,  "122.1.1.2");
   node_t *R6 =  create_new_node(instance,  "R6", AREA1,  "122.1.1.6");
   node_t *R7 =  create_new_node(instance,  "R7", AREA1,  "122.1.1.7");
   node_t *R8 =  create_new_node(instance,  "R8", AREA1,  "122.1.1.8");
   node_t *R9 =  create_new_node(instance,  "R9", AREA1,  "122.1.1.9");
   node_t *R10  = create_new_node(instance, "R10", AREA1, "122.1.1.10");

   prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL1);
   prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL1);
   prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL1);
   prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL1);
   prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
   prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);
   prefix_t *prefix_40_1_1_1_24 = create_new_prefix("40.1.1.1", 24, LEVEL1);
   prefix_t *prefix_40_1_1_2_24 = create_new_prefix("40.1.1.2", 24, LEVEL1);
   prefix_t *prefix_50_1_1_1_24 = create_new_prefix("50.1.1.1", 24, LEVEL1);
   prefix_t *prefix_50_1_1_2_24 = create_new_prefix("50.1.1.2", 24, LEVEL1);
   prefix_t *prefix_60_1_1_1_24 = create_new_prefix("60.1.1.1", 24, LEVEL1);
   prefix_t *prefix_60_1_1_2_24 = create_new_prefix("60.1.1.2", 24, LEVEL1);
   prefix_t *prefix_70_1_1_1_24 = create_new_prefix("70.1.1.1", 24, LEVEL1);
   prefix_t *prefix_70_1_1_2_24 = create_new_prefix("70.1.1.2", 24, LEVEL1);
   prefix_t *prefix_80_1_1_1_24 = create_new_prefix("80.1.1.1", 24, LEVEL1);
   prefix_t *prefix_80_1_1_2_24 = create_new_prefix("80.1.1.2", 24, LEVEL1);
   prefix_t *prefix_90_1_1_1_24 = create_new_prefix("90.1.1.1", 24, LEVEL1);
   prefix_t *prefix_90_1_1_2_24 = create_new_prefix("90.1.1.2", 24, LEVEL1);
   prefix_t *prefix_100_1_1_1_24 = create_new_prefix("100.1.1.1", 24, LEVEL1);
   prefix_t *prefix_100_1_1_2_24 = create_new_prefix("100.1.1.2", 24, LEVEL1);
   prefix_t *prefix_110_1_1_1_24 = create_new_prefix("110.1.1.1", 24, LEVEL1);
   prefix_t *prefix_110_1_1_2_24 = create_new_prefix("110.1.1.2", 24, LEVEL1);
   prefix_t *prefix_120_1_1_1_24 = create_new_prefix("120.1.1.1", 24, LEVEL1);
   prefix_t *prefix_120_1_1_2_24 = create_new_prefix("120.1.1.2", 24, LEVEL1);
   prefix_t *prefix_130_1_1_1_24 = create_new_prefix("130.1.1.1", 24, LEVEL1);
   prefix_t *prefix_130_1_1_2_24 = create_new_prefix("130.1.1.2", 24, LEVEL1);


    edge_t *R11_R1_edge1 = create_new_edge("eth0/2", "eth0/0", 10,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL1);

    edge_t *R1_R2_edge1 = create_new_edge("eth0/1", "eth0/3", 10,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL1);

    edge_t *R1_R5_edge1 = create_new_edge("eth0/2", "eth0/0", 10,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);

    edge_t *R5_R4_edge1 = create_new_edge("eth0/2", "eth0/0", 10,
            prefix_40_1_1_1_24, prefix_40_1_1_2_24, LEVEL1);
    
    edge_t *R4_R3_edge1 = create_new_edge("eth0/1", "eth0/3", 40,
            prefix_50_1_1_1_24, prefix_50_1_1_2_24, LEVEL1);

    edge_t *R3_R2_edge1 = create_new_edge("eth0/0", "eth0/2", 10,
            prefix_60_1_1_1_24, prefix_60_1_1_2_24, LEVEL1);
    
    edge_t *R2_R6_edge1 = create_new_edge("eth0/0", "eth0/2", 10,
            prefix_70_1_1_1_24, prefix_70_1_1_2_24, LEVEL1);

    edge_t *R3_R7_edge1 = create_new_edge("eth0/1", "eth0/3", 10,
            prefix_80_1_1_1_24, prefix_80_1_1_2_24, LEVEL1);
    
    edge_t *R7_R8_edge1 = create_new_edge("eth0/0", "eth0/2", 10,
            prefix_90_1_1_1_24, prefix_90_1_1_2_24, LEVEL1);
    
    edge_t *R8_R9_edge1 = create_new_edge("eth0/0", "eth0/2", 10,
            prefix_100_1_1_1_24, prefix_100_1_1_2_24, LEVEL1);

    edge_t *R9_R10_edge1 = create_new_edge("eth0/0", "eth0/2", 10,
            prefix_110_1_1_1_24, prefix_110_1_1_2_24, LEVEL1);
    
    edge_t *R6_R10_edge1 = create_new_edge("eth0/1", "eth0/3", 10,
            prefix_120_1_1_1_24, prefix_120_1_1_2_24, LEVEL1);

    edge_t *R2_R9_edge1 = create_new_edge("eth0/1", "eth0/3", 10,
            prefix_130_1_1_1_24, prefix_130_1_1_2_24, LEVEL1);

    insert_edge_between_2_nodes(R11_R1_edge1, R11, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R2_edge1, R1, R2, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R5_edge1, R1, R5, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R5_R4_edge1, R5, R4, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R4_R3_edge1, R4, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R3_R2_edge1, R3, R2, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R2_R6_edge1, R2, R6, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R3_R7_edge1, R3, R7, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R7_R8_edge1, R7, R8, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R8_R9_edge1, R8, R9, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R9_R10_edge1, R9, R10, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R6_R10_edge1, R6, R10, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R2_R9_edge1, R2, R9, BIDIRECTIONAL);

    set_instance_root(instance, R1);
    return instance;
}

instance_t *
tilfa_topo_2_adj_segment_example(){

#if 0

/* PLR node : R2, Link protected : eth0/1(connected to R3, Node protection)
 * TILFA path to D from PLR should take 2 Adj Segment : Adj[R7-R8], Adj[R8-R4]
 * output :
 * Protected Resource Name : (R2)eth0/1, Dest Protected : D
 * Protection type : LP : unset   NP : set
 * LEVEL1, n_segment_list = 1
 *         inet3 eth0/2, 60.1.1.2  0(PUSH)  0(PUSH)  <prefix sid D>(PUSH)
 *         mpls0 eth0/2, 60.1.1.2  0(PUSH)  0(PUSH)
 *
 * Topology Reference : Figure 3, Page 16, SR Tilfa IETF RFC
 * */

                                                                                                                            +-------+
                          PLR                                      +--------+                +--------+                     |       |
+-------+-            +-------+             +-----+                +-       |        1       |        |         1           |  D    |
|       |      1      |       |     1       |     |     1          |        +----------------+  R5    +---------------------+       |
|  S    +-------------+  R2   +-------------+ R3  +----------------+  R4    |                |        |                     |       |
|       |             |       |             |     |                |        |                +--------+                     +-------+
+-------+             +--\----+             +-+---\                +---+----+
                          \                   |    \                   |
                           \                  |     \                  |
                            \                 |      \                 |
                             \1k              |       \                |
                              \               |        \               |
                               \              |1        \ 1            |
                                \             |          \             |
                                 \            |           \            |
                                  \           |            \           |1k
                                   \          |             \          |
                                    \         |              \         |
                                    +---------+---+           \        |
                                    |             |            \       |
                                    |    R7       |             \      |
                                    |             |              \     |
                                    +------+---+--+               +----+---+
                                           |   |        1k        |        |
                                           |   +------------------+  R8    |
                                           |                      |        |
                                           |                      +----+---+
                                           |1k                         |    
                                           |                           |
                                           |                           |1
                                           |                           |
                                      +----+-----+                     |
                                      |          |                +----+----+
                                      |          |       1        |         |
                                      |    R9    +----------------+  R10    |
                                      |          |                |         |
                                      +----------+                +---------+

#endif


   instance_t *instance = get_new_instance();

   node_t *S = create_new_node(instance, "S", AREA1, "122.1.1.1");
   node_t *R2 =  create_new_node(instance,  "R2", AREA1,  "122.1.1.2");
   node_t *R3 =  create_new_node(instance,  "R3", AREA1,  "122.1.1.3");
   node_t *R4 =  create_new_node(instance,  "R4", AREA1,  "122.1.1.4");
   node_t *R5 =  create_new_node(instance,  "R5", AREA1,  "122.1.1.5");
   node_t *D =  create_new_node(instance,  "D", AREA1,  "122.1.1.10");
   node_t *R7 =  create_new_node(instance,  "R7", AREA1,  "122.1.1.7");
   node_t *R8 =  create_new_node(instance,  "R8", AREA1,  "122.1.1.8");
   node_t *R9 =  create_new_node(instance,  "R9", AREA1,  "122.1.1.9");
   node_t *R10  = create_new_node(instance, "R10", AREA1, "122.1.1.10");

   prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL1);
   prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL1);
   prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL1);
   prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL1);
   prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
   prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);
   prefix_t *prefix_40_1_1_1_24 = create_new_prefix("40.1.1.1", 24, LEVEL1);
   prefix_t *prefix_40_1_1_2_24 = create_new_prefix("40.1.1.2", 24, LEVEL1);
   prefix_t *prefix_50_1_1_1_24 = create_new_prefix("50.1.1.1", 24, LEVEL1);
   prefix_t *prefix_50_1_1_2_24 = create_new_prefix("50.1.1.2", 24, LEVEL1);
   prefix_t *prefix_60_1_1_1_24 = create_new_prefix("60.1.1.1", 24, LEVEL1);
   prefix_t *prefix_60_1_1_2_24 = create_new_prefix("60.1.1.2", 24, LEVEL1);
   prefix_t *prefix_70_1_1_1_24 = create_new_prefix("70.1.1.1", 24, LEVEL1);
   prefix_t *prefix_70_1_1_2_24 = create_new_prefix("70.1.1.2", 24, LEVEL1);
   prefix_t *prefix_80_1_1_1_24 = create_new_prefix("80.1.1.1", 24, LEVEL1);
   prefix_t *prefix_80_1_1_2_24 = create_new_prefix("80.1.1.2", 24, LEVEL1);
   prefix_t *prefix_90_1_1_1_24 = create_new_prefix("90.1.1.1", 24, LEVEL1);
   prefix_t *prefix_90_1_1_2_24 = create_new_prefix("90.1.1.2", 24, LEVEL1);
   prefix_t *prefix_100_1_1_1_24 = create_new_prefix("100.1.1.1", 24, LEVEL1);
   prefix_t *prefix_100_1_1_2_24 = create_new_prefix("100.1.1.2", 24, LEVEL1);
   prefix_t *prefix_110_1_1_1_24 = create_new_prefix("110.1.1.1", 24, LEVEL1);
   prefix_t *prefix_110_1_1_2_24 = create_new_prefix("110.1.1.2", 24, LEVEL1);
   prefix_t *prefix_120_1_1_1_24 = create_new_prefix("120.1.1.1", 24, LEVEL1);
   prefix_t *prefix_120_1_1_2_24 = create_new_prefix("120.1.1.2", 24, LEVEL1);
   prefix_t *prefix_130_1_1_1_24 = create_new_prefix("130.1.1.1", 24, LEVEL1);
   prefix_t *prefix_130_1_1_2_24 = create_new_prefix("130.1.1.2", 24, LEVEL1);


    edge_t *S_R2_edge = create_new_edge("eth0/1", "eth0/3", 1,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL1);
    
    edge_t *R2_R3_edge = create_new_edge("eth0/1", "eth0/3", 1,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL1);
    
    edge_t *R3_R4_edge = create_new_edge("eth0/1", "eth0/3", 1,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);
    
    edge_t *R4_R5_edge = create_new_edge("eth0/1", "eth0/3", 1,
            prefix_40_1_1_1_24, prefix_40_1_1_2_24, LEVEL1);
    
    edge_t *R5_D_edge = create_new_edge("eth0/1", "eth0/3", 1,
            prefix_50_1_1_1_24, prefix_50_1_1_2_24, LEVEL1);

    edge_t *R2_R7_edge = create_new_edge("eth0/2", "eth0/3", 1000,
            prefix_60_1_1_1_24, prefix_60_1_1_2_24, LEVEL1);

    edge_t *R3_R7_edge = create_new_edge("eth0/3", "eth0/0", 1,
            prefix_70_1_1_1_24, prefix_70_1_1_2_24, LEVEL1);
    
    edge_t *R3_R8_edge = create_new_edge("eth0/1", "eth0/4", 1,
            prefix_80_1_1_1_24, prefix_80_1_1_2_24, LEVEL1);
    
    edge_t *R4_R8_edge = create_new_edge("eth0/1", "eth0/0", 1000,
            prefix_90_1_1_1_24, prefix_90_1_1_2_24, LEVEL1);
    
    edge_t *R7_R8_edge = create_new_edge("eth0/1", "eth0/3", 1000,
            prefix_100_1_1_1_24, prefix_100_1_1_2_24, LEVEL1);
    
    edge_t *R7_R9_edge = create_new_edge("eth0/2", "eth0/0", 1000,
            prefix_110_1_1_1_24, prefix_110_1_1_2_24, LEVEL1);
    
    edge_t *R9_R10_edge = create_new_edge("eth0/1", "eth0/3", 1,
            prefix_120_1_1_1_24, prefix_120_1_1_2_24, LEVEL1);
    
    edge_t *R8_R10_edge = create_new_edge("eth0/2", "eth0/0", 1,
            prefix_130_1_1_1_24, prefix_130_1_1_2_24, LEVEL1);
    
    insert_edge_between_2_nodes(S_R2_edge, S, R2, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R2_R3_edge, R2, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R3_R4_edge, R3, R4, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R4_R5_edge, R4, R5, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R5_D_edge, R5, D, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R2_R7_edge, R2, R7, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R3_R7_edge, R3, R7, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R3_R8_edge, R3, R8, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R7_R8_edge, R7, R8, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R7_R9_edge, R7, R9, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R9_R10_edge, R9, R10, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R8_R10_edge, R8, R10, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R4_R8_edge, R4, R8, BIDIRECTIONAL);

    set_instance_root(instance, R2);
    return instance;
}

/*                                                                      +---------+
 *                                                                      |         |
 *                                                                      |   R2    |
 *      +---------------------------------------------------------------+ 192.1.1.2+---------------------------------------------++-----------+
 *      |                                                               |         |                                                          |
 *      |                                                               +---------+                                                          |
 *      |                                                                                                                                    |
 *      |                                                                                                                                    |
 *      |                                                                                                                                    |
 *      |                                                                                                                                    |
 *      |                                                                +---------+                                                         |
 *      |                                                                |   R5    +---------------+                                         |
 *      |                                       +------------------------+192.1.1.5|               |                                         |
 *      |                                       |                        |         |               |                                         |
 *      |                                       |                        +---------+               |                                         |
 *      |                                       |                                             +----+-----+-----+                        ++---+--+--+
 * +----+---+                               +---+-------+                                     |    R7          |                        |          |
 * |        |                               | R4        |                                     +- 192.1.1.7     +-----------+------------| R3       |
 * |  R1    +-------------------------------+192.168.1.4|                                     |                |                        |192.1.1.3 |
 * |192.1.1.1|                              |           |                                     +----+----+---+--+                        |          |
 * +----+---+                               +---+------++                                          |        |                           +-------+--+
 *      |                                       |                         +--------+               |        |
 *      |                                       |                         |  R6    |               |        |
 *      |                                       |                         |192.1.1.6---------------+        |
 *      |                                       +-------------------------+        |                        |
 *      |                                                                 |        |                        |
 *      |                                                                 +--------+                        |
 *      |                                                                                                   |
 *      |                                                                                                   |
 *      |                                                                                                   |
 *      |                                                                                                   |
 *      |                                                                                                   |   
 *      |                                            +---------+                    +---------+             |   
 *      |                                            |  R8     |                    |  R9     |             |   
 *      |                                            |192.1.1.8|                    |192.1.1.9+---------+---+   
 *      +--------------------------------------------+         +--------------------|         |                 
 *                                                   |         |                    |         |                 
 *                                                   +---------+                    +---------+       
 */
 
instance_t *
tilfa_ecmp_topology(){

    instance_t *instance = get_new_instance();
    
    node_t *R1 = create_new_node(instance,  "R1", AREA1, "192.1.1.1");
    node_t *R2 = create_new_node(instance,  "R2", AREA1, "192.1.1.2");
    node_t *R3 = create_new_node(instance,  "R3", AREA1, "192.1.1.3");
    node_t *R4 = create_new_node(instance,  "R4", AREA1, "192.1.1.4");
    node_t *R5 = create_new_node(instance,  "R5", AREA1, "192.1.1.5");
    node_t *R6 = create_new_node(instance,  "R6", AREA1, "192.1.1.6");
    node_t *R7 = create_new_node(instance,  "R7", AREA1, "192.1.1.7");
    node_t *R8 = create_new_node(instance,  "R8", AREA1, "192.1.1.8");
    node_t *R9 = create_new_node(instance,  "R9", AREA1, "192.1.1.9");


    prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24, LEVEL1);
    prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24, LEVEL1);
    prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24, LEVEL1);
    prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24, LEVEL1);
    prefix_t *prefix_30_1_1_1_24 = create_new_prefix("30.1.1.1", 24, LEVEL1);
    prefix_t *prefix_30_1_1_2_24 = create_new_prefix("30.1.1.2", 24, LEVEL1);
    prefix_t *prefix_40_1_1_1_24 = create_new_prefix("40.1.1.1", 24, LEVEL1);
    prefix_t *prefix_40_1_1_2_24 = create_new_prefix("40.1.1.2", 24, LEVEL1);
    prefix_t *prefix_50_1_1_1_24 = create_new_prefix("50.1.1.1", 24, LEVEL1);
    prefix_t *prefix_50_1_1_2_24 = create_new_prefix("50.1.1.2", 24, LEVEL1);
    prefix_t *prefix_60_1_1_1_24 = create_new_prefix("60.1.1.1", 24, LEVEL1);
    prefix_t *prefix_60_1_1_2_24 = create_new_prefix("60.1.1.2", 24, LEVEL1);
    prefix_t *prefix_70_1_1_1_24 = create_new_prefix("70.1.1.1", 24, LEVEL1);
    prefix_t *prefix_70_1_1_2_24 = create_new_prefix("70.1.1.2", 24, LEVEL1);
    prefix_t *prefix_80_1_1_1_24 = create_new_prefix("80.1.1.1", 24, LEVEL1);
    prefix_t *prefix_80_1_1_2_24 = create_new_prefix("80.1.1.2", 24, LEVEL1);
    prefix_t *prefix_90_1_1_1_24 = create_new_prefix("90.1.1.1", 24, LEVEL1);
    prefix_t *prefix_90_1_1_2_24 = create_new_prefix("90.1.1.2", 24, LEVEL1);
    prefix_t *prefix_100_1_1_1_24 = create_new_prefix("100.1.1.1", 24, LEVEL1);
    prefix_t *prefix_100_1_1_2_24 = create_new_prefix("100.1.1.2", 24, LEVEL1);
    prefix_t *prefix_110_1_1_1_24 = create_new_prefix("110.1.1.1", 24, LEVEL1);
    prefix_t *prefix_110_1_1_2_24 = create_new_prefix("110.1.1.2", 24, LEVEL1);
    
    edge_t *R1_R2_edge = create_new_edge("eth0/1", "eth0/2", 10,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL1);

    edge_t *R2_R3_edge = create_new_edge("eth0/3", "eth0/4", 10,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL1);
    
    edge_t *R1_R4_edge = create_new_edge("eth0/5", "eth0/6", 10,
            prefix_30_1_1_1_24, prefix_30_1_1_2_24, LEVEL1);
    
    edge_t *R1_R8_edge = create_new_edge("eth0/7", "eth0/8", 10,
            prefix_40_1_1_1_24, prefix_40_1_1_2_24, LEVEL1);
    
    edge_t *R4_R5_edge = create_new_edge("eth0/9", "eth0/10", 10,
            prefix_50_1_1_1_24, prefix_50_1_1_2_24, LEVEL1);
    
    edge_t *R4_R6_edge = create_new_edge("eth0/11", "eth0/12", 10,
            prefix_60_1_1_1_24, prefix_60_1_1_2_24, LEVEL1);
    
    edge_t *R5_R7_edge = create_new_edge("eth0/13", "eth0/14", 10,
            prefix_70_1_1_1_24, prefix_70_1_1_2_24, LEVEL1);
    
    edge_t *R6_R7_edge = create_new_edge("eth0/15", "eth0/16", 10,
            prefix_80_1_1_1_24, prefix_80_1_1_2_24, LEVEL1);
    
    edge_t *R7_R3_edge = create_new_edge("eth0/17", "eth0/18", 10,
            prefix_90_1_1_1_24, prefix_90_1_1_2_24, LEVEL1);

    edge_t *R8_R9_edge = create_new_edge("eth0/19", "eth0/20", 10,
            prefix_100_1_1_1_24, prefix_100_1_1_2_24, LEVEL1);
    
    edge_t *R9_R7_edge = create_new_edge("eth0/21", "eth0/22", 10,
            prefix_110_1_1_1_24, prefix_110_1_1_2_24, LEVEL1);
    
    insert_edge_between_2_nodes(R1_R2_edge, R1, R2, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R2_R3_edge, R2, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R4_edge, R1, R4, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R8_edge, R1, R8, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R4_R5_edge, R4, R5, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R4_R6_edge, R4, R6, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R5_R7_edge, R5, R7, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R6_R7_edge, R6, R7, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R7_R3_edge, R7, R3, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R8_R9_edge, R8, R9, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R9_R7_edge, R9, R7, BIDIRECTIONAL);

    set_instance_root(instance, R1);
    return instance;
}

static instance_t *old_instance = NULL;

int
config_dynamic_topology(param_t *param, 
                        ser_buff_t *tlv_buf, 
                        op_mode enable_or_disable){
    
    int cmd_code = -1;
    char *node_name1 = NULL,
         *node_name2 = NULL;
    char *intf_name1 = NULL,
         *intf_name2 = NULL;
    char *ip_address = NULL;
    char mask = 0;
    char *mac = NULL;
    int i = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;

    cmd_code = EXTRACT_CMD_CODE(tlv_buf);

    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name1", strlen("node-name1")) ==0)
            node_name1 = tlv->value;
        else if(strncmp(tlv->leaf_id, "node-name2", strlen("node-name2")) ==0)
            node_name2 = tlv->value;
        else if(strncmp(tlv->leaf_id, "intf-name1", strlen("intf-name1")) ==0)
            intf_name1 = tlv->value;
        else if(strncmp(tlv->leaf_id, "intf-name2", strlen("intf-name2")) ==0)
            intf_name2 = tlv->value;
        else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) ==0)
            ip_address = tlv->value;
        else if(strncmp(tlv->leaf_id, "mac-address", strlen("mac-address")) ==0)
            mac = tlv->value;
        else if(strncmp(tlv->leaf_id, "mask", strlen("mask")) ==0)
            mask = atoi(tlv->value);
        else
            assert(0);
    } TLV_LOOP_END;


    switch(cmd_code){
     
        case CONFIG_TOPO:
        {
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                    if(old_instance)
                        return 0;
                    old_instance = instance;
                    instance = get_new_instance();
                break;
                case CONFIG_DISABLE:    
                    instance = old_instance;
                    old_instance = NULL; /*Memory Leak, sorry!*/
                    break;
                default:
                    assert(0);
            }
        }
        break;

        case TOPO_CREATE_NODE:
        {
           node_t *node = create_new_node(instance, node_name1, AREA1, ZERO_IP) ;
           if(GET_NODE_COUNT_SINGLY_LL(instance->instance_node_list) == 1)
               set_instance_root(instance, node);
        }
        break;

        case TOPO_NODE_ASSIGN_LOOPBACK_IP:
        {
            node_t *node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name1);
            deattach_prefix_on_node(node, node->router_id, 32, LEVEL1); 
            deattach_prefix_on_node(node, node->router_id, 32, LEVEL2);
            memset(node->router_id, 0, PREFIX_LEN);
            memcpy(node->router_id, ip_address, PREFIX_LEN);
            attach_prefix_on_node(node, node->router_id, 32, LEVEL1, 0, 0); 
            attach_prefix_on_node(node, node->router_id, 32, LEVEL2, 0, 0); 
        }
        break;

        case TOPO_NODE_INSERT_LINK:
        {
            node_t *node1 = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name1);
            node_t *node2= (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name2);
            if(!node1){
                printf("Node %s do not exist. Please create this node first\n", node_name1);
                return 0;
            }
            if(!node2){
                printf("Node %s do not exist. Please create this node first\n", node_name2);
                return 0;
            }

            insert_edge_between_2_nodes((create_new_edge(intf_name1, 
                                                         intf_name2, 10,
                                                         0,0, 
                                                         LEVEL1)), 
                                                         node1, node2, 
                                                         BIDIRECTIONAL);
        }
        break;

        case TOPO_NODE_INTF_ASSIGN_IP_ADDRESS:
        {
            node_t *node1 = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name1);
            edge_t *inv_edge = NULL;

            if(!node1){
                printf("Node %s do not exist. Please create this node first\n", node_name1);
                return 0;   
            }

            edge_end = get_interface_from_intf_name(node1, intf_name1);

            if(!edge_end || i == MAX_NODE_INTF_SLOTS){
                printf("Error : Interface %s on node %s not found\n", intf_name1, node1->node_name);
                return 0;
            }

            edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
            inv_edge = edge->inv_edge;

            prefix_t *prefix = edge_end->prefix[LEVEL1];

            if(prefix && strncmp(prefix->prefix, ip_address, PREFIX_LEN) == 0 && 
                    mask == prefix->mask){
                printf("Info : Already configured\n");
                return 0;
            }

            if(prefix){
                delete_prefix_from_prefix_list(node1->local_prefix_list[LEVEL1], 
                        prefix->prefix, prefix->mask);
            }

            if(!prefix){
                prefix = XCALLOC(1, prefix_t);
                BIND_PREFIX(edge->from.prefix[LEVEL1], prefix);
            }
            else{
                memset(prefix, 0, sizeof(prefix_t));
            }

            strncpy(prefix->prefix, ip_address, PREFIX_LEN);
            prefix->prefix[PREFIX_LEN] = '\0';
            prefix->mask = mask;
            prefix->level = LEVEL1;
            prefix->hosting_node = node1;
            set_prefix_property_metric(prefix, DEFAULT_LOCAL_PREFIX_METRIC);

            prefix_t *clone_prefix = create_new_prefix(prefix->prefix, prefix->mask, prefix->level);
            clone_prefix->hosting_node = node1;
            set_prefix_property_metric(clone_prefix, DEFAULT_LOCAL_PREFIX_METRIC);

            assert(add_prefix_to_prefix_list(node1->local_prefix_list[LEVEL1], 
                        clone_prefix, 0));

            edge_end = &inv_edge->to;
            prefix = edge_end->prefix[LEVEL1];
            
            if(!prefix){
                prefix = XCALLOC(1, prefix_t);
                BIND_PREFIX(edge_end->prefix[LEVEL1], prefix);
            }
            else{
                memset(prefix, 0, sizeof(prefix_t));
            }

            memcpy(prefix->prefix, ip_address, PREFIX_LEN);
            prefix->prefix[PREFIX_LEN] = '\0';
            prefix->mask = mask;
            prefix->level = LEVEL1;
            prefix->hosting_node = node1;
            set_prefix_property_metric(prefix, DEFAULT_LOCAL_PREFIX_METRIC);
        }
        break;
        case TOPO_NODE_INTF_ASSIGN_MAC_ADDRESS:
        {
            node_t *node1 = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name1);
            edge_t *inv_edge = NULL;

            if(!node1){
                printf("Node %s do not exist. Please create this node first\n", node_name1);
                return 0;   
            }
            for(i = 0; i < MAX_NODE_INTF_SLOTS; i++){

                edge_end = node1->edges[i];
                if(!edge_end){
                    return 0;
                }
                if(edge_end->dirn == INCOMING) continue;
                if(strncmp(edge_end->intf_name, intf_name1, strlen(edge_end->intf_name)) || 
                        strlen(edge_end->intf_name) != strlen(intf_name1)){
                    continue;
                }
                break;
            }

            if(!edge_end || i == MAX_NODE_INTF_SLOTS){
                printf("Error : Interface %s on node %s not found\n", intf_name1, node1->node_name);
                return 0;
            }

            edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
            inv_edge = edge->inv_edge;
            memset(edge_end->mac_address, 0, MAC_LEN);
            memcpy(edge_end->mac_address, mac, MAC_LEN);
            edge_end = &inv_edge->to;
            memset(edge_end->mac_address, 0, MAC_LEN);
            memcpy(edge_end->mac_address, mac, MAC_LEN);
        }
        break;
        default:
            ;
    } 
    return 0;   
}

void
config_topology_commands(param_t *config_hook){

    static param_t topo;
    init_param(&topo, CMD, "topo", config_dynamic_topology, 0, INVALID, 0, "Topology Configuration");
    libcli_register_param(config_hook, &topo);
    set_param_cmd_code(&topo, CONFIG_TOPO);
    {
        /*config topo node create <node-name1>*/
        {
            static param_t node;
            init_param(&node, CMD, "node" , 0, 0, INVALID, 0, "node");
            libcli_register_param(&topo, &node);
            {
                static param_t create;
                init_param(&create, CMD, "create" , 0, 0, INVALID, 0, "Create Topo entity");
                libcli_register_param(&node, &create);
                {
                    static param_t node_name;
                    init_param(&node_name, LEAF, 0, config_dynamic_topology, 
                            0, STRING, "node-name1", "Node Name"); 
                    libcli_register_param(&create, &node_name);    
                    set_param_cmd_code(&node_name, TOPO_CREATE_NODE);
                }
            }
            /*config topo node <node-name1> loopback <ip-address>*/
            {
                static param_t node_name;
                init_param(&node_name, LEAF, 0, 0, 
                        0, STRING, "node-name1", "Node Name"); 
                libcli_register_param(&node, &node_name);    
                {
                    static param_t lo;
                    init_param(&lo, CMD, "loopback", 0, 0, INVALID, 0, "Loopback ip");
                    libcli_register_param(&node_name, &lo);
                    {
                        static param_t prefix;
                        init_param(&prefix, LEAF, 0, config_dynamic_topology, 0, IPV4, "ip-address", "Ipv4 prefix without mask");
                        libcli_register_param(&lo, &prefix);
                        set_param_cmd_code(&prefix, TOPO_NODE_ASSIGN_LOOPBACK_IP);
                    }    
                }
                /*config topo node <node-name1> from-if <intf-name1> peer <node-name2> to-if <intf-name2>*/
                {
                    static param_t from_if;
                    init_param(&from_if, CMD, "from-if", 0, 0, INVALID, 0, "from interface");
                    libcli_register_param(&node_name, &from_if);
                    {
                        static param_t intf_name1;
                        init_param(&intf_name1, LEAF, 0, 0, 0, STRING, "intf-name1", "interface name ethx/y format");
                        libcli_register_param(&from_if, &intf_name1);    
                        {
                            static param_t peer;
                            init_param(&peer, CMD, "peer", 0, 0, INVALID, 0, "Peer node");
                            libcli_register_param(&intf_name1, &peer);
                            {
                                static param_t node_name;
                                init_param(&node_name, LEAF, 0, 0, 
                                        0, STRING, "node-name2", "Node Name"); 
                                libcli_register_param(&peer, &node_name);    
                                {
                                    static param_t to_if;
                                    init_param(&to_if, CMD, "to-if", 0, 0, INVALID, 0, "other end interface");
                                    libcli_register_param(&node_name, &to_if);
                                    {
                                        static param_t intf_name2;
                                        init_param(&intf_name2, LEAF, 0, config_dynamic_topology, 0, STRING, "intf-name2", "interface name ethx/y format");
                                        libcli_register_param(&to_if, &intf_name2);
                                        set_param_cmd_code(&intf_name2, TOPO_NODE_INSERT_LINK);    
                                    }
                                }
                            }    
                        }    
                    }
                }
                /*config topo node <node-name1> interface <intf-name1> ip <ip-address> <mask>*/
                {
                    static param_t interface;
                    init_param(&interface, CMD, "interface", 0, 0, INVALID, 0, "from interface");
                    libcli_register_param(&node_name, &interface);
                    {
                        static param_t intf_name1;
                        init_param(&intf_name1, LEAF, 0, 0, 0, STRING, "intf-name1", "interface name ethx/y format");
                        libcli_register_param(&interface, &intf_name1); 
                        {
                            static param_t ip;
                            init_param(&ip, CMD, "ip", 0, 0, INVALID, 0, "Interface Ip address");
                            libcli_register_param(&intf_name1, &ip);
                            {
                                static param_t prefix;
                                init_param(&prefix, LEAF, 0, 0, 0, IPV4, "ip-address", "Ipv4 prefix without mask");
                                libcli_register_param(&ip, &prefix);
                                {
                                   static param_t mask;
                                   init_param(&mask,  LEAF, 0, config_dynamic_topology, validate_ipv4_mask, INT, "mask", "mask (0-32)"); 
                                   libcli_register_param(&prefix, &mask);
                                   set_param_cmd_code(&mask, TOPO_NODE_INTF_ASSIGN_IP_ADDRESS); 
                                }
                            }
                        }   
                        {
                            static param_t mac;
                            init_param(&mac, CMD, "mac", 0, 0, INVALID, 0, "Interface MAC address");
                            libcli_register_param(&intf_name1, &mac);
                            {
                                static param_t mac_addr;
                                init_param(&mac_addr, LEAF, 0, config_dynamic_topology, 0, STRING, "mac-address", "Mac adddress");
                                libcli_register_param(&mac, &mac_addr);
                                set_param_cmd_code(&mac_addr, TOPO_NODE_INTF_ASSIGN_MAC_ADDRESS); 
                            }
                        }   
                    }
                }
            }
        }
    }         
}
