/*
 * =====================================================================================
 *
 *       Filename:  instance.c
 *
 *    Description:  The topology construction is done in this file
 *
 *        Version:  1.0
 *        Created:  Wednesday 23 August 2017 02:05:28  IST
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
#include "spfutil.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
add_node_to_owning_instance(instance_t *instance, node_t *node){
    singly_ll_add_node_by_val(instance->instance_node_list, (void *)node);
}

node_t *
create_new_node(instance_t *instance, char *node_name, AREA area){
    
    assert(node_name);
    LEVEL level;
    node_t * node = calloc(1, sizeof(node_t));
    strncpy(node->node_name, node_name, NODE_NAME_SIZE);
    node->node_name[NODE_NAME_SIZE - 1] = '\0';
    node->area = area;

    for(level = LEVEL1; level <= LEVEL2; level++){

        node->node_type[level] = NON_PSEUDONODE;
        node->pn_intf[level] = NULL;

        node->local_prefix_list[level] = init_singly_ll();
        singly_ll_set_comparison_fn(node->local_prefix_list[level] , 
                get_prefix_comparison_fn());

        node->spf_run_result[level] = init_singly_ll();
        node->spf_info.spf_level_info[level].version = 0;
    }

    node->spf_result = NULL;
    node->spf_info.routes = init_singly_ll();/*List of routes calculated, routes are not categorised under Levels*/
    add_node_to_owning_instance(instance, node);
    return node;    
}

edge_t *
create_new_edge(char *from_ifname,
        char *to_ifname,
        unsigned int metric,
        prefix_t *from_prefix,
        prefix_t *to_prefix,
        LEVEL level){/*LEVEL value can be LEVEL12 also*/

    assert(from_ifname);
    assert(to_ifname);

    edge_t *edge = calloc(1, sizeof(edge_t));

    strncpy(edge->from.intf_name, from_ifname, IF_NAME_SIZE);
    edge->from.intf_name[IF_NAME_SIZE - 1] = '\0';
   
    strncpy(edge->to.intf_name, to_ifname, IF_NAME_SIZE);
    edge->to.intf_name[IF_NAME_SIZE - 1] = '\0';

    if(IS_LEVEL_SET(level, LEVEL1)) 
        edge->metric[LEVEL1] = metric;

    if(IS_LEVEL_SET(level, LEVEL2)) 
        edge->metric[LEVEL2] = metric;

    if(IS_LEVEL_SET(level, LEVEL1)){
        BIND_PREFIX(edge->from.prefix[LEVEL1], from_prefix);
        BIND_PREFIX(edge->to.prefix[LEVEL1], to_prefix);
    }
    
    if(IS_LEVEL_SET(level, LEVEL2)){
        BIND_PREFIX(edge->from.prefix[LEVEL2], from_prefix);
        BIND_PREFIX(edge->to.prefix[LEVEL2], to_prefix);
    }

    edge->level     = level;
    edge->from.dirn = EDGE_END_DIRN_UNKNOWN;
    edge->to.dirn   = EDGE_END_DIRN_UNKNOWN;
    edge->status    = 1;
    return edge;
}

static void
insert_interface_into_node(node_t *node, edge_end_t *edge_end){
    
    unsigned int i = 0;
    for(; i < MAX_NODE_INTF_SLOTS; i++){
        if(node->edges[i])
            continue;

        if(i == MAX_NODE_INTF_SLOTS){
            printf("%s() : Error : No slots available in node %s\n", __FUNCTION__, node->node_name);
            return;
        }

        node->edges[i] = edge_end;
        edge_end->node = node;
        break;
    }
}

void
insert_edge_between_2_nodes(edge_t *edge,
                            node_t *from_node,
                            node_t *to_node, DIRECTION dirn){

    insert_interface_into_node(from_node, &edge->from);
    edge->from.dirn = OUTGOING;
    insert_interface_into_node(to_node, &edge->to);
    edge->to.dirn = INCOMING;
    
    edge_t *edge2 = NULL;

    if(dirn == BIDIRECTIONAL){
       
        if(IS_LEVEL_SET(edge->level, LEVEL1)) 
            edge2 = create_new_edge(edge->to.intf_name, edge->from.intf_name, edge->metric[LEVEL1],
                                        edge->to.prefix[LEVEL1], edge->from.prefix[LEVEL1], edge->level);

        else if(IS_LEVEL_SET(edge->level, LEVEL2))
            edge2 = create_new_edge(edge->to.intf_name, edge->from.intf_name, edge->metric[LEVEL2],
                                        edge->to.prefix[LEVEL2], edge->from.prefix[LEVEL2], edge->level);

        insert_edge_between_2_nodes(edge2, to_node, from_node, UNIDIRECTIONAL);
    }
}

void
set_instance_root(instance_t *instance, node_t *root){
    instance->instance_root = root;
}

void
mark_node_pseudonode(node_t *node, LEVEL level){

    unsigned int i = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;

    if(level != LEVEL1 && level != LEVEL2)
        assert(0);

    if(node->node_type[level] == PSEUDONODE)
        return;

    node->node_type[level] = PSEUDONODE;

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        if(!node->edges[i]) continue;;
        
        edge_end = node->edges[i];
        
        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        if(!IS_LEVEL_SET(edge->level, level))
            continue;
      
        UNBIND_PREFIX(edge_end->prefix[level]);

        if(get_edge_direction(node, edge) == OUTGOING)
            edge->metric[level] = 0;
    }
}

static int
instance_node_comparison_fn(void *_node, void *input_node_name){

    node_t *node = (node_t *)_node;
    if(strncmp(node->node_name, input_node_name, strlen(input_node_name)) == 0
        && strlen(node->node_name) == strlen(input_node_name))
        return 1;

    return 0;
}

instance_t *
get_new_instance(){

    instance_t *instance = calloc(1, sizeof(instance_t));
    instance->instance_node_list = init_singly_ll();
    singly_ll_set_comparison_fn(instance->instance_node_list, instance_node_comparison_fn);
    CANDIDATE_TREE_INIT(&instance->ctree);
    return instance;
}

int
is_two_way_nbrship(node_t *node, node_t *node_nbr, LEVEL level){

    edge_t *edge = NULL;
    node_t *temp_nbr_node = NULL,
           *temp_nbr_node2 = NULL;

    ITERATE_NODE_NBRS_BEGIN(node, temp_nbr_node, edge, level){

        if(temp_nbr_node != node_nbr)
            continue;

        ITERATE_NODE_NBRS_BEGIN(node_nbr, temp_nbr_node2, edge, level){

            if(temp_nbr_node2 != node)
                continue;

            return 1;
        }
        ITERATE_NODE_NBRS_END;
    }
    ITERATE_NODE_NBRS_END;
    return 0;
}

/* Fn to traverse the Graph in DFS order. processing_fn_ptr is the
 * ptr to the fn used to perform the required processing on current node
 * while traversing the instance*/

static void
init_instance_traversal(instance_t * instance){

   singly_ll_node_t *list_node = NULL;
   node_t *node = NULL;

   ITERATE_LIST(instance->instance_node_list, list_node){
        node = (node_t *)list_node->data;
        node->traversing_bit = 0;
   }  
}

static void
_traverse_instance(node_t *instance_root, 
               void *(*processing_fn_ptr)(node_t *), LEVEL level){

    if(!instance_root) 
        return;

    if(instance_root->traversing_bit == 1)
        return;

    edge_t *edge = NULL;
    node_t *nbr_node = NULL;

    processing_fn_ptr(instance_root);

    instance_root->traversing_bit = 1;

    ITERATE_NODE_NBRS_BEGIN(instance_root, nbr_node, edge, level){
        _traverse_instance(nbr_node, processing_fn_ptr, level);
    }
    ITERATE_NODE_NBRS_END;
}

/*This is the fn to traverse the instance from root. This fn would
 * simulate the flooding behavior*/
void
traverse_instance(instance_t *instance, 
                void *(*processing_fn_ptr)(node_t *), LEVEL level){

    init_instance_traversal(instance); 
    _traverse_instance(instance->instance_root, processing_fn_ptr, level);
}

edge_t *
get_my_pseudonode_nbr(node_t *node, LEVEL level){

    node_t *nbr_node = NULL;
    edge_t *edge = NULL;

    
    ITERATE_NODE_NBRS_BEGIN(node, nbr_node, edge, level){
        
        if(nbr_node->node_type[level] == PSEUDONODE){
                /*Something terribly wrong with the topo, Two adjacent nodes cannot be 
                 * Pseudonodes*/
                assert(node->node_type[level] != PSEUDONODE);
                return edge;
        }
    }
    ITERATE_NODE_NBRS_END;
    return NULL;
}

void
attach_prefix_on_node(node_t *node,
        char *prefix,
        unsigned char mask,
        LEVEL level,
        unsigned int metric){

    assert(prefix);
    assert(level == LEVEL1 || level == LEVEL2);

    prefix_t *_prefix = NULL;

    _prefix = create_new_prefix(prefix, mask);
    _prefix->metric = metric;
    singly_ll_add_node_by_val(node->local_prefix_list[level], (void *)_prefix);
}

prefix_t *
node_local_prefix_search(node_t *node, LEVEL level, 
                        char *_prefix, char mask){

    common_pfx_key_t key;
    memset(&key, 0, sizeof(common_pfx_key_t));
    strncpy(key.prefix, _prefix, strlen(_prefix));
    key.mask = mask;

    assert(level == LEVEL1 || level == LEVEL2);
    
    ll_t *prefix_list = GET_NODE_PREFIX_LIST(node, level);

    return (prefix_t *)singly_ll_search_by_key(prefix_list, &key);
}
