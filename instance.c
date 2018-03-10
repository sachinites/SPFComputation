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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instance.h"
#include "spfutil.h"
#include "rttable.h"
#include "rt_mpls.h"
#include "spftrace.h"


extern instance_t *instance;

static void
add_node_to_owning_instance(instance_t *instance, node_t *node){
    singly_ll_add_node_by_val(instance->instance_node_list, (void *)node);
}

node_t *
create_new_node(instance_t *instance, char *node_name, AREA area, char *router_id){
    
    LEVEL level;
    prefix_t *router_id_pfx = NULL;

    assert(node_name);
    node_t * node = calloc(1, sizeof(node_t));
    strncpy(node->node_name, node_name, NODE_NAME_SIZE);
    node->node_name[NODE_NAME_SIZE - 1] = '\0';
    strncpy(node->router_id, router_id, PREFIX_LEN);
    node->router_id[PREFIX_LEN] = '\0';

    node->area = area;
    node->is_node_on_heap = FALSE;

    for(level = LEVEL1; level <= LEVEL2; level++){

        node->node_type[level] = NON_PSEUDONODE;
        node->pn_intf[level] = NULL;

        node->local_prefix_list[level] = init_singly_ll();
        singly_ll_set_comparison_fn(node->local_prefix_list[level] , 
                get_prefix_comparison_fn());
        
        singly_ll_set_order_comparison_fn(node->local_prefix_list[level] , 
                get_prefix_order_comparison_fn());

        router_id_pfx = create_new_prefix(node->router_id, 32, level);
        router_id_pfx->hosting_node = node;
        add_new_prefix_in_list(GET_NODE_PREFIX_LIST(node, level), router_id_pfx, 0);

        node->spf_run_result[level] = init_singly_ll();
        singly_ll_set_comparison_fn(node->spf_run_result[level], spf_run_result_comparison_fn);

        node->spf_info.spf_level_info[level].version = 0;
        node->spf_info.spf_level_info[level].node = node; /*back ptr*/

        node->self_spf_result[level] = init_singly_ll();
        singly_ll_set_comparison_fn(node->self_spf_result[level], 
                        self_spf_run_result_comparison_fn);
    }

    node->spf_info.routes_list = init_singly_ll();/*List of routes calculated, routes are not categorised under Levels*/
    singly_ll_set_comparison_fn(node->spf_info.routes_list, route_search_comparison_fn);

    node->spf_info.priority_routes_list = init_singly_ll();
    singly_ll_set_comparison_fn(node->spf_info.priority_routes_list, route_search_comparison_fn);

    node->spf_info.deferred_routes_list = init_singly_ll();
    singly_ll_set_comparison_fn(node->spf_info.deferred_routes_list, route_search_comparison_fn);

    node->spf_info.rttable = init_rttable("inet.0");
    node->spf_info.mpls_rt_table = init_mpls_rt_table("mpls.0");
    node->attached = 1; /*By default attached bit is enabled*/
    node->traversing_bit = 0;
    node->lsp_distribution_bit = 0;
    node->backup_spf_options = 0;
    node->spring_enabled = FALSE;
    node->srgb = NULL;
    node->is_srms = FALSE;
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

    edge_t *edge = calloc(1, sizeof(edge_t));

    /*ifnames may be specified as NULL in case of edges incoming to PN or 
     * outgoing from PN*/

    if(from_ifname){
        strncpy(edge->from.intf_name, from_ifname, IF_NAME_SIZE);
        edge->from.intf_name[IF_NAME_SIZE - 1] = '\0';
    }
   
    if(to_ifname){
        strncpy(edge->to.intf_name, to_ifname, IF_NAME_SIZE);
        edge->to.intf_name[IF_NAME_SIZE - 1] = '\0';
    }

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
    edge->inv_edge  = NULL;
    edge->etype = UNICAST;
    edge->bandwidth = DEFAULT_LINK_BW;
    return edge;
}

edge_t *
create_new_lsp_adj(char *lsp_name,
               unsigned int metric,
               LEVEL level){

    edge_t *edge = calloc(1, sizeof(edge_t));
    strncpy(edge->from.intf_name, lsp_name, IF_NAME_SIZE);
    edge->from.intf_name[IF_NAME_SIZE - 1] = '\0';

    strncpy(edge->to.intf_name, lsp_name, IF_NAME_SIZE);
    edge->to.intf_name[IF_NAME_SIZE - 1] = '\0';
    
    edge->level     = level;
    
    if(IS_LEVEL_SET(level, LEVEL1)) 
        edge->metric[LEVEL1] = metric;

    if(IS_LEVEL_SET(level, LEVEL2)) 
        edge->metric[LEVEL2] = metric;

    edge->from.dirn = EDGE_END_DIRN_UNKNOWN;
    edge->to.dirn   = EDGE_END_DIRN_UNKNOWN;
    edge->status    = 1;
    edge->inv_edge  = NULL;
    edge->etype = LSP;
    return edge;

}

void
attach_edge_end_prefix_on_node(node_t *node, edge_end_t *edge_end){

    prefix_t *prefix = NULL,
             *clone_prefix = NULL;

    LEVEL level_it, level;
    if(edge_end->dirn != OUTGOING)
        return;

    level = (GET_EGDE_PTR_FROM_FROM_EDGE_END(edge_end))->level;

    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        if(!IS_LEVEL_SET(level_it, level))
            continue;

        prefix = edge_end->prefix[level_it];

        if(!prefix)/* In case of PN*/
            continue;

        if(node_local_prefix_search(node, level_it, prefix->prefix, prefix->mask))
            continue;

        prefix->hosting_node = node;
        clone_prefix = create_new_prefix(prefix->prefix, prefix->mask, LEVEL_UNKNOWN);
        memcpy(clone_prefix, prefix, sizeof(prefix_t));
        clone_prefix->level = level_it;
        add_prefix_to_prefix_list(GET_NODE_PREFIX_LIST(node, level_it), clone_prefix, 0);
    }
}

void
dettach_edge_end_prefix_on_node(node_t *node, edge_end_t *edge_end){

    singly_ll_node_t *prefix_list_node = NULL;
    prefix_t *prefix = NULL;
    LEVEL level_it, level;

    if(edge_end->dirn != OUTGOING)
        return;

    level = (GET_EGDE_PTR_FROM_FROM_EDGE_END(edge_end))->level;

    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){

        if(!IS_LEVEL_SET(level_it, level))
            continue;

        prefix_list_node = singly_ll_get_node_by_data_ptr(GET_NODE_PREFIX_LIST(node, level), 
                edge_end->prefix[level]);
        if(!prefix_list_node)
            continue;

        prefix = (prefix_t *)prefix_list_node->data;
        prefix->ref_count--;
        singly_ll_delete_node(GET_NODE_PREFIX_LIST(node, level), prefix_list_node);
        if(prefix->ref_count == 0){
            free_prefix(prefix);
            prefix = NULL;
        }
    }
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

        /*insert the edge prefixes into node's prefix list*/
        attach_edge_end_prefix_on_node(node, edge_end);       
        break;
    }
}

void
insert_edge_between_2_nodes(edge_t *edge,
                            node_t *from_node,
                            node_t *to_node, DIRECTION dirn){

    edge->from.dirn = OUTGOING;
    insert_interface_into_node(from_node, &edge->from);
    edge->to.dirn = INCOMING;
    insert_interface_into_node(to_node, &edge->to);
    
    edge_t *edge2 = NULL;

    if(dirn == BIDIRECTIONAL){
       
        if(IS_LEVEL_SET(edge->level, LEVEL1)) 
            edge2 = create_new_edge(edge->to.intf_name, edge->from.intf_name, edge->metric[LEVEL1],
                                        edge->to.prefix[LEVEL1], edge->from.prefix[LEVEL1], edge->level);

        else if(IS_LEVEL_SET(edge->level, LEVEL2))
            edge2 = create_new_edge(edge->to.intf_name, edge->from.intf_name, edge->metric[LEVEL2],
                                        edge->to.prefix[LEVEL2], edge->from.prefix[LEVEL2], edge->level);

        edge->inv_edge = edge2;
        edge2->inv_edge = edge;
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
        if(!node->edges[i]) continue;
        
        edge_end = node->edges[i];
        
        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        if(!IS_LEVEL_SET(edge->level, level))
            continue;
     
        if(get_edge_direction(node, edge) == OUTGOING){ 
            edge->metric[level] = 0;
            if(edge_end->prefix[level]){
                singly_ll_delete_node_by_data_ptr(GET_NODE_PREFIX_LIST(node, level), 
                        edge_end->prefix[level]);
                edge_end->prefix[level]->ref_count--;
            }
        }
        UNBIND_PREFIX(edge_end->prefix[level]);
    }
}

int
instance_node_comparison_fn(void *_node, void *input_node_name){

    node_t *node = (node_t *)_node;
    if(strncmp(node->node_name, input_node_name, strlen(input_node_name)) == 0
        && strlen(node->node_name) == strlen(input_node_name))
        return 1;

    return 0;
}

extern void
_spf_display_trace_options(unsigned long long bit_mask);

instance_t *
get_new_instance(){

    instance_t *instance = calloc(1, sizeof(instance_t));
    instance->instance_node_list = init_singly_ll();
    singly_ll_set_comparison_fn(instance->instance_node_list, instance_node_comparison_fn);
    CANDIDATE_TREE_INIT(&instance->ctree);
    instance->traceopts = calloc(1, sizeof(traceoptions));
    init_trace(instance->traceopts);
    register_display_trace_options(instance->traceopts, _spf_display_trace_options);
    enable_spf_trace(instance, SPF_EVENTS_BIT);
    return instance;
}

boolean
is_two_way_nbrship(node_t *node, node_t *node_nbr, LEVEL level){

    edge_t *edge = NULL;
    node_t *temp_nbr_node = NULL,
           *temp_nbr_node2 = NULL;

    ITERATE_NODE_LOGICAL_NBRS_BEGIN(node, temp_nbr_node, edge, level){

        if(temp_nbr_node != node_nbr)
            continue;

        ITERATE_NODE_LOGICAL_NBRS_BEGIN(node_nbr, temp_nbr_node2, edge, level){

            if(temp_nbr_node2 != node)
                continue;

            return TRUE;
        }
        ITERATE_NODE_LOGICAL_NBRS_END;
    }
    ITERATE_NODE_LOGICAL_NBRS_END;
    return FALSE;
}

edge_t *
get_my_pseudonode_nbr(node_t *node, LEVEL level){

    node_t *nbr_node = NULL;
    edge_t *edge = NULL;

    
    ITERATE_NODE_LOGICAL_NBRS_BEGIN(node, nbr_node, edge, level){
        
        if(nbr_node->node_type[level] == PSEUDONODE){
                /*Something terribly wrong with the topo, Two adjacent nodes cannot be 
                 * Pseudonodes*/
                assert(node->node_type[level] != PSEUDONODE);
                return edge;
        }
    }
    ITERATE_NODE_LOGICAL_NBRS_END;
    return NULL;
}

prefix_t *
attach_prefix_on_node(node_t *node,
        char *prefix,
        unsigned char mask,
        LEVEL level,
        unsigned int metric,
        FLAG prefix_flags){

    assert(prefix);
    assert(level == LEVEL1 || level == LEVEL2);

    prefix_t *_prefix = NULL;

    _prefix = create_new_prefix(prefix, mask, level);
    _prefix->metric = metric;
    _prefix->hosting_node = node;
    _prefix->prefix_flags = prefix_flags;

    sprintf(instance->traceopts->b, "Node : %s, prefix attached : %s/%u, prefix metric : %u",
        node->node_name, prefix, mask, metric);
    trace(instance->traceopts, SPF_PREFIX_BIT);

    if(add_prefix_to_prefix_list(GET_NODE_PREFIX_LIST(node, level), _prefix, 0))
        return _prefix;

    free_prefix(_prefix);
    _prefix = NULL;
    return NULL;
}

void
deattach_prefix_on_node(node_t *node,
        char *prefix,
        unsigned char mask,
        LEVEL level){

    assert(prefix);
    assert(level == LEVEL1 || level == LEVEL2);

    common_pfx_key_t key;
    memset(&key, 0, sizeof(common_pfx_key_t));
    strncpy(key.prefix, prefix, strlen(prefix));
    key.mask = mask;
    
    prefix_t *_prefix = singly_ll_search_by_key(GET_NODE_PREFIX_LIST(node, level), &key);
    if(!_prefix)
        return;

    sprintf(instance->traceopts->b, "Node : %s, prefix deattached : %s/%u, prefix metric : %u",
        node->node_name, prefix, mask, _prefix->metric); 
    trace(instance->traceopts, SPF_PREFIX_BIT);
    singly_ll_remove_node_by_dataptr(GET_NODE_PREFIX_LIST(node, level), _prefix);
    free_prefix(_prefix);
    _prefix = NULL;
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

/* Below routine return the OIF from node to node_nbr. In
 * addition, it also returns the remote end prefix if gw_prefix
 * buffer is provided*/

edge_end_t *
get_min_oif(node_t *node, node_t *node_nbr, 
            LEVEL level, char *gw_prefix, nh_type_t nh){

    unsigned int i = 0, 
                 min_metric = INFINITE_METRIC;

    edge_end_t *edge_end     = NULL, 
               *min_edge_oif = NULL,
               *min_pn_oif   = NULL;

    edge_t *edge    = NULL, 
           *edge_it = NULL;

    node_t *PN = NULL;
  
    if(node == node_nbr)
        return NULL;
         
    /*Covering P2P case */ 
    for(; i < MAX_NODE_INTF_SLOTS; i++){
        
        edge_end = node->edges[i];
       
        if(!edge_end || (edge_end->dirn != OUTGOING))   
            continue;
        
        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
        
        if(nh == LSPNH && edge->etype != LSP)
            continue;


        if(nh == IPNH && edge->etype != UNICAST)
            continue;

        if(edge->to.node == node_nbr){
            if(edge->metric[level] < min_metric){
                min_metric = edge->metric[level];
                min_edge_oif = edge_end;
            }
        }
    }

    if(min_edge_oif){
        
        edge = GET_EGDE_PTR_FROM_EDGE_END(min_edge_oif);
        
        if(gw_prefix){
            /*LSP ends do not have edge end prefixes*/
            if(edge->to.prefix[level]){
                strncpy(gw_prefix, edge->to.prefix[level]->prefix, PREFIX_LEN + 1);
                gw_prefix[PREFIX_LEN] = '\0';
            }
            else{
                memset(gw_prefix, 0, PREFIX_LEN);
            }
        }
        return min_edge_oif;
    }

    /* Covering Pseudonode case. If we could not find oif, it means, two nodes are not directly
     * connected to each other, but could be connected through PN in the
     * middle. We need to cover this scenario as well. Also, cover the case
     * of multiple PNs present in between node and node_nbr*/
    
    min_metric = INFINITE_METRIC;
    ITERATE_NODE_LOGICAL_NBRS_BEGIN(node, PN, edge_it, level){

        
        if(nh == LSPNH && edge_it->etype != LSP)
            continue;

        if(nh == IPNH && edge_it->etype != UNICAST)
            continue;

        if(PN->node_type[level] != PSEUDONODE)  
            continue;

        /* Get the OIF from PN to nbr node with remote gw_prefix*/
        min_pn_oif = get_min_oif(PN, node_nbr, level, gw_prefix, nh);
             
        /*Do not count the PN which is not present in same LAN 
         * segment as R0 and R2*/
        if(min_pn_oif == NULL)
            continue;

        /*infact, edge_it->from also represents the same 
         * edge end here, but in production code we dont 
         * have adjacency with PN, hence, get the edge end this way*/

        /* Till here, node ===== PN ====== node_nbr*/

        edge_end = &edge_it->from;
        edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);

        if(edge->metric[level] < min_metric){
            min_metric = edge->metric[level];
            min_edge_oif = edge_end;
        }
    }
    ITERATE_NODE_LOGICAL_NBRS_END;

    return min_edge_oif;
}

edge_end_t *
get_interface_from_intf_name(node_t *node, char *intf_name){

    unsigned int i = 0;
    edge_end_t *interface = NULL;

    for(i = 0; i < MAX_NODE_INTF_SLOTS; i++ ){
        interface = node->edges[i];
        if(interface == NULL){
            printf("Error : slot-no %s do not exist\n", intf_name);
            return NULL;
        }

        if(strncmp(intf_name, interface->intf_name, strlen(interface->intf_name)))
            continue;

        if(interface->dirn != OUTGOING)
            continue;

        return interface;
    }
    return NULL;
}

