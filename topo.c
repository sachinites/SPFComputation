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

instance_t *
build_linear_topo(){

    /*
     *
     * +------+               +------+                +-------+
     * |      |0/0    10.1.1.2|      |0/2     20.1.1.2|       |
     * |  R0  +------L12------+  R1  +-----L12--------+  R2   |
     * |      |10.1.1.1    0/1|      |20.1.1.1    0/3 |       |
     * +------+               +------+                +-------+
     *                                                    
     * */
    
    instance_t *instance = get_new_instance();

    node_t *R0 = create_new_node(instance, "R0", AREA1, "192.168.0.1");
    node_t *R1 = create_new_node(instance, "R1", AREA1, "192.168.0.2");
    node_t *R2 = create_new_node(instance, "R2", AREA1, "192.168.0.3");

    prefix_t *prefix_10_1_1_1_24 = create_new_prefix("10.1.1.1", 24);
    prefix_t *prefix_10_1_1_2_24 = create_new_prefix("10.1.1.2", 24);
    prefix_t *prefix_20_1_1_1_24 = create_new_prefix("20.1.1.1", 24);
    prefix_t *prefix_20_1_1_2_24 = create_new_prefix("20.1.1.2", 24);

    edge_t *R0_R1_edge = create_new_edge("eth0/0", "eth0/1", 10,
            prefix_10_1_1_1_24, prefix_10_1_1_2_24, LEVEL12);

    edge_t *R1_R2_edge = create_new_edge("eth0/2", "eth0/3", 1,
            prefix_20_1_1_1_24, prefix_20_1_1_2_24, LEVEL12);

    insert_edge_between_2_nodes(R0_R1_edge, R0, R1, BIDIRECTIONAL);
    insert_edge_between_2_nodes(R1_R2_edge, R1, R2, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
build_multi_area_topo(){

#if 0
 
 show spf r l 1 root R0
 show spf r l 1 root R1
 show spf r l 1 root R2
 show spf r l 1 root R3
 show spf r l 1 root R4
 show spf r l 1 root R5
 show spf r l 1 root R6
 show spf r l 2 root R0
 show spf r l 2 root R1
 show spf r l 2 root R2
 show spf r l 2 root R3
 show spf r l 2 root R4
 show spf r l 2 root R5
 show spf r l 2 root R6
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

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/0", 10, create_new_prefix("10.1.1.1", 24), create_new_prefix("10.1.1.2", 24), LEVEL1)),
                                R0, R1, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/0", 10, create_new_prefix("11.1.1.1", 24), create_new_prefix("11.1.1.2", 24), LEVEL1)),
                                R0, R2, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/1", 10, create_new_prefix("12.1.1.1", 24), create_new_prefix("12.1.1.2", 24), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/2", 10, create_new_prefix("14.1.1.1", 24), create_new_prefix("14.1.1.2", 24), LEVEL2)),
                                R0, R3, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/1", "eth0/1", 10, create_new_prefix("15.1.1.1", 24), create_new_prefix("15.1.1.2", 24), LEVEL12)),
                                R3, R4, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/1", 10, create_new_prefix("16.1.1.1", 24), create_new_prefix("16.1.1.2", 24), LEVEL2)),
                                R4, R5, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/0", 10, create_new_prefix("17.1.1.1", 24), create_new_prefix("17.1.1.2", 24), LEVEL1)),
                                R5, R6, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/2", 10, create_new_prefix("20.1.1.1", 24), create_new_prefix("20.1.1.2", 24), LEVEL2)),
                                R2, R5, BIDIRECTIONAL);

    set_instance_root(instance, R0);
    return instance;
}

instance_t *
build_ring_topo(){

#if 0

    
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

    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("10.1.1.1", 30), create_new_prefix("10.1.1.2", 30), LEVEL1)),
                                S, E, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 10, create_new_prefix("20.1.1.1", 30), create_new_prefix("20.1.1.2", 30), LEVEL1)),
                                E, D, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/4", "eth0/5", 10, create_new_prefix("30.1.1.1", 30), create_new_prefix("30.1.1.2", 30), LEVEL1)),
                                D, C, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/6", "eth0/7", 10, create_new_prefix("40.1.1.1", 30), create_new_prefix("40.1.1.2", 30), LEVEL1)),
                                C, B, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/8", "eth0/9", 10, create_new_prefix("50.1.1.1", 30), create_new_prefix("50.1.1.2", 30), LEVEL1)),
                                B, A, BIDIRECTIONAL);
    
    insert_edge_between_2_nodes((create_new_edge("eth0/10", "eth0/11", 10, create_new_prefix("60.1.1.1", 30), create_new_prefix("60.1.1.2", 30), LEVEL1)),
                                A, S, BIDIRECTIONAL);

    set_instance_root(instance, S);
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


    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 10, create_new_prefix("10.1.1.1", 30), create_new_prefix("10.1.1.2", 30), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 10, create_new_prefix("20.1.1.1", 30), create_new_prefix("20.1.1.2", 30), LEVEL1)),
                                R2, R3, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/4", "eth0/5", 10, create_new_prefix("30.1.1.1", 30), create_new_prefix("30.1.1.2", 30), LEVEL1)),
                                R3, R4, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/6", "eth0/7", 10, create_new_prefix("50.1.1.3", 24), 0, LEVEL1)),
                                R4, R5, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/8", "eth0/9", 10, 0, create_new_prefix("50.1.1.1", 24), LEVEL1)),
                                R5, R1, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/10", "eth0/11", 10, 0, create_new_prefix("50.1.1.2", 24), LEVEL1)),
                                R5, R6, BIDIRECTIONAL);

    mark_node_pseudonode(R5, LEVEL1);
    //mark_node_pseudonode(R2, LEVEL1);
    set_instance_root(instance, R1);
    return instance;
}


instance_t *
pseudonode_ecmp_topo(){

#if 0
                                          0/2  +---------+
               +-------------------------------+         |70.1.1.1
               |    5                  60.1.1.2|   D     +-----------------------------------+
               |                               +---------+0/3                                |
               |                                                                             |
               |                                                                             |
               |                                                                             |
               |                                                                             |
               |                                                                             |
               |                                                                         0/3 |70.1.1.2
       60.1.1.1|0/2                      +-----------+                                  +----+----+
          +----+----+                    |           |            0                 0/1 |         |
          |         |50.1.1.1            |    PN     +----------------------------------+         |
          |         +--------------------+           |                         50.1.1.2 |         |
          |         |         10         |           |                                  +---------+
          +---------+                    +-----------+

#endif

    instance_t *instance = get_new_instance();
    node_t *A  = create_new_node(instance, "A",  AREA1, "192.168.0.1");
    node_t *PN = create_new_node(instance, "PN", AREA1, "192.168.0.1");
    node_t *C  = create_new_node(instance, "C",  AREA1, "192.168.0.3");
    node_t *D  = create_new_node(instance, "D" , AREA1, "192.168.0.2");

    insert_edge_between_2_nodes((create_new_edge(0, "eth0/1", 0, 
                                 0, create_new_prefix("50.1.1.2", 30),
                                 LEVEL1)),
                                 PN, C, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/2", 5, 
                                 create_new_prefix("60.1.1.1", 30), create_new_prefix("60.1.1.2", 30),
                                 LEVEL1)),
                                 A, D, BIDIRECTIONAL);

    insert_edge_between_2_nodes((create_new_edge("eth0/1", 0, 10, 
                                 create_new_prefix("50.1.1.1", 30), 0,
                                 LEVEL1)),
                                 A, PN, BIDIRECTIONAL);
    

    insert_edge_between_2_nodes((create_new_edge("eth0/3", "eth0/3", 5, 
                                 create_new_prefix("70.1.1.1", 30), create_new_prefix("70.1.1.2", 30),
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
                   |50                    |
                   |                      |
                   |                      |
                   |                      |
                   |                      |
           10.1.1.2|0/1           50.1.1.2|0/8
               +---+-+                +---+--+              70.1.1.2+-------+
               |R2   +                +      +----------------------+       |
               |     |                | R5   |0/10              0/11| R6    |
               +--+--+                |      |70.1.1.1              |       |
          20.1.1.1|0/2                +---+--+                      +-------+
                  |              60.1.1.2 |0/7
                  |                       |
                  |                       |
                  |100                    |
                  |                       |
                  |                       |
                  |                       |
          20.1.1.2|0/3                    |0/6 60.1.1.1
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


    insert_edge_between_2_nodes((create_new_edge("eth0/0", "eth0/1", 50, create_new_prefix("10.1.1.1", 30), create_new_prefix("10.1.1.2", 30), LEVEL1)),
                                R1, R2, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/2", "eth0/3", 100, create_new_prefix("20.1.1.1", 30), create_new_prefix("20.1.1.2", 30), LEVEL1)),
                                R2, R3, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/4", "eth0/5", 10, create_new_prefix("30.1.1.1", 30), create_new_prefix("30.1.1.2", 30), LEVEL1)),
                                R3, R4, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/6", "eth0/7", 10, create_new_prefix("60.1.1.1", 30), create_new_prefix("60.1.1.2", 30), LEVEL1)),
                                R4, R5, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/8", "eth0/9", 10, create_new_prefix("50.1.1.2", 30), create_new_prefix("50.1.1.1", 30), LEVEL1)),
                                R5, R1, BIDIRECTIONAL);


    insert_edge_between_2_nodes((create_new_edge("eth0/10", "eth0/11", 10, create_new_prefix("70.1.1.1", 30), create_new_prefix("70.1.1.2", 30), LEVEL1)),
                                R5, R6, BIDIRECTIONAL);

    set_instance_root(instance, R1);
    return instance;
}
