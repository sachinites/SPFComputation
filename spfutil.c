/*
 * =====================================================================================
 *
 *       Filename:  spfutil.c
 *
 *    Description:  Implementation of spfutil.h
 *
 *        Version:  1.0
 *        Created:  Sunday 27 August 2017 12:49:32  IST
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

#include <arpa/inet.h>
#include "spfutil.h"
#include "bitsop.h"
#include "Queue.h"
#include "advert.h"
#include "spftrace.h"

extern instance_t *instance;

boolean
is_all_nh_list_empty2(node_t *node, LEVEL level){

    nh_type_t nh;

    ITERATE_NH_TYPE_BEGIN(nh){

        if(!is_nh_list_empty2(&node->next_hop[level][nh][0]))
            return FALSE;

    } ITERATE_NH_TYPE_END;

    return TRUE;
}

void
empty_nh_list(node_t *node, LEVEL level, nh_type_t nh){

    unsigned int i = 0;
    for(i=0; i < MAX_NXT_HOPS; i++){
        init_internal_nh_t(node->next_hop[level][nh][i]);    
    }
} 

void
copy_nh_list2(internal_nh_t *src_direct_nh_list, internal_nh_t *dst_nh_list){
    
    unsigned int i = 0;

    for(; i < MAX_NXT_HOPS; i++){
        init_internal_nh_t(dst_nh_list[i]);
    }
    
    if(is_nh_list_empty2(src_direct_nh_list))
        return;

    for(i = 0; i < MAX_NXT_HOPS; i++){
        if(is_internal_nh_t_empty(src_direct_nh_list[i]))
            return;
        copy_internal_nh_t(src_direct_nh_list[i], dst_nh_list[i]);
        set_next_hop_gw_pfx((dst_nh_list[i]), src_direct_nh_list[i].gw_prefix);
    }
}

boolean 
is_present2(internal_nh_t *list, internal_nh_t *nh){

    unsigned int i = 0;
    for(; i < MAX_NXT_HOPS; i++){
        if(!is_nh_list_empty2(&list[i])){
            if(is_internal_nh_t_equal(list[i], (*nh)))
                return TRUE;
            continue;
        }
        return FALSE;
    }
    return FALSE;
}

void
union_nh_list2(internal_nh_t *src_nh_list, internal_nh_t *dst_nh_list){

    unsigned int i = 0, j = 0;

    for(; i < MAX_NXT_HOPS; i++){
        if(!is_nh_list_empty2(&dst_nh_list[i]))
            continue;
        break;
    }

    if(i == MAX_NXT_HOPS)
        return;

    for(; i < MAX_NXT_HOPS; i++){

        if(!is_nh_list_empty2(&src_nh_list[j])){
            if(!is_present2(&dst_nh_list[0], &src_nh_list[j])){
                copy_internal_nh_t(src_nh_list[j], dst_nh_list[i]);
            }
            j++;
            continue;
        }
        break;
    }
}


void
union_direct_nh_list2(internal_nh_t *src_direct_nh_list, internal_nh_t *dst_nh_list){

    unsigned int nh_count = 0;

    if(is_nh_list_empty2(src_direct_nh_list))
        return;

    nh_count = get_nh_count(&dst_nh_list[0]);
    if(nh_count == MAX_NXT_HOPS)
        return; 
    
    union_nh_list2(&src_direct_nh_list[0], &dst_nh_list[0]);
}

boolean
is_nh_list_empty2(internal_nh_t *nh_list){

    return (nh_list->node == NULL && nh_list->oif == NULL);
}

boolean
is_empty_internal_nh(internal_nh_t *nh){
    return is_nh_list_empty2(nh);
}

char *
get_str_level(LEVEL level){

    switch(level){
        case LEVEL1:
            return "LEVEL1";
        case LEVEL2:
            return "LEVEL2";
        default:
            if(level == (LEVEL1 | LEVEL2))
                return "LEVEL12";
            else
                return "LEVEL_UNKNOWN";
    }
}

char*
get_str_node_area(AREA area){

    switch(area){
        case AREA1:
            return "AREA1";
        case AREA2:
            return "AREA2";
        case AREA3:
            return "AREA3";
        case AREA4:
            return "AREA4";
        case AREA5:
            return "AREA5";
        case AREA6:
            return "AREA6";
        default:
            return "AREA_UNKNOWN";
    }
}

/* set spf_info->spff_multi_area bit, if computing node is
 * L2 Attached. This fn is called assuming current spf run is 
 * L2 */
void
spf_determine_multi_area_attachment(spf_info_t *spf_info,
                                    node_t *spf_root){

    singly_ll_node_t *list_node = NULL;
    spf_result_t *res = NULL;
    AREA myarea = spf_root->area;
    
    spf_info->spff_multi_area = 0;
       
    ITERATE_LIST_BEGIN(spf_root->spf_run_result[LEVEL2], list_node){
        
        res = (spf_result_t *)list_node->data;
        if(res->node->area != myarea && 
                is_two_way_nbrship(res->node, spf_root, LEVEL2)){
            spf_info->spff_multi_area = 1;
            sprintf(instance->traceopts->b, "spf_root : %s is L2 Attached with remote Area node : %s", 
                            spf_root->node_name, res->node->node_name); 
            trace(instance->traceopts, ROUTE_CALCULATION_BIT);
            break;   
        }
    }ITERATE_LIST_END;

    if(spf_info->spff_multi_area == 0){
        sprintf(instance->traceopts->b, "spf_root : %s is not L2 Attached with remote Area", spf_root->node_name);
        trace(instance->traceopts, ROUTE_CALCULATION_BIT);
    }
}

void
apply_mask(char *prefix, char mask, char *str_prefix){

    unsigned int binary_prefix = 0, i = 0;
    inet_pton(AF_INET, prefix, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    for(; i < (32 - mask); i++)
        UNSET_BIT(binary_prefix, i);
    binary_prefix = htonl(binary_prefix);
    inet_ntop(AF_INET, &binary_prefix, str_prefix, PREFIX_LEN + 1);
    str_prefix[PREFIX_LEN] = '\0';
}

void
apply_mask2(char *prefix, char mask, char *str_prefix){

    apply_mask(prefix, mask, str_prefix);
    char *mask_ptr = str_prefix + strlen(str_prefix);
    str_prefix[PREFIX_LEN] = 48;
    sprintf(mask_ptr, "/%u", mask);
    str_prefix[PREFIX_LEN_WITH_MASK] = '\0';
} 
/* Iterate over all 'level' reachable routers in the network from
 *  * ingress_lsr and get the node_t * of router whose router_id is tail_end_ip*/

node_t *
get_system_id_from_router_id(node_t *ingress_lsr,
        char *tail_end_ip, LEVEL level){


    node_t  *curr_node = NULL,
            *nbr_node = NULL,
            *pn_node = NULL;

    edge_t *edge1 = NULL,  /*Edge connecting curr node with PN*/
           *edge2 = NULL; /*Edge connecting PN to its nbr*/

    assert(level != LEVEL12);

    Queue_t *q = initQ();
    init_instance_traversal(instance);

    ingress_lsr->traversing_bit = 1;

    enqueue(q, ingress_lsr);

    while(!is_queue_empty(q)){
        
        curr_node = deque(q);

        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(curr_node, nbr_node, pn_node, edge1,
                edge2, level){

            if(nbr_node->traversing_bit){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(curr_node, nbr_node, pn_node, level);
            }

            if(strncmp(nbr_node->router_id, tail_end_ip, PREFIX_LEN) == 0){
                free(q);
                q = NULL;
                return nbr_node;
            }
            nbr_node->traversing_bit = 1;
            enqueue(q, nbr_node);
        }
        ITERATE_NODE_PHYSICAL_NBRS_END(curr_node, nbr_node, pn_node, level);
    }
    assert(is_queue_empty(q));
    free(q);
    q = NULL;
    return NULL;
}

unsigned int
get_nh_count(internal_nh_t *nh_list){

    unsigned int i = 0;
    for(; i < MAX_NXT_HOPS; i++){
        if(!is_nh_list_empty2(&nh_list[i]))
            continue;
        break;
    }
    return i;
}

void
print_nh_list2(internal_nh_t *nh_list){

    unsigned int i = 0;
    
    sprintf(instance->traceopts->b, "printing next hop list"); 
    trace(instance->traceopts, DIJKSTRA_BIT);
    for(; i < MAX_NXT_HOPS; i++){
        if(is_nh_list_empty2(&nh_list[i])) return;
        sprintf(instance->traceopts->b, "oif = %s, NH =  %s , Level = %s, gw_prefix = %s", 
            nh_list[i].oif->intf_name, nh_list[i].node->node_name, get_str_level(nh_list[i].level), nh_list[i].gw_prefix);
        trace(instance->traceopts, DIJKSTRA_BIT);
    }
}

boolean
is_broadcast_link(edge_t *edge, LEVEL level){

    assert(level == LEVEL1 || level == LEVEL2);
    node_t *node = edge->to.node;
    if(node->node_type[level] == PSEUDONODE)
        return TRUE;
    return FALSE;
}

boolean
is_broadcast_member_node(node_t *S, edge_t *interface, node_t *D, LEVEL level){

    assert(S!=D);
    
    if(!is_broadcast_link(interface, level)){
        return D == interface->to.node;
    }

    node_t *PN = interface->to.node,
           *nbr_node = NULL;

    edge_t *edge = NULL;
    ITERATE_NODE_LOGICAL_NBRS_BEGIN(PN, nbr_node, edge, level){
        
        if(nbr_node == S)
            continue;

        if(nbr_node == D)
            return TRUE;

    } ITERATE_NODE_LOGICAL_NBRS_END;
    return FALSE;
}

void
add_to_nh_list(internal_nh_t *nh_list, internal_nh_t *nh){

    unsigned int nh_count = get_nh_count(nh_list);
    if(nh_count == MAX_NXT_HOPS)
        return;
    memcpy(&nh_list[nh_count], nh, sizeof(internal_nh_t));
}

char* 
hrs_min_sec_format(unsigned int seconds){

    static char time_f[16];
    unsigned int hrs = 0, 
                 min =0, sec = 0;

    if(seconds > 3600){
        min = seconds/60;
        sec = seconds%60;
        hrs = min/60;
        min = min%60;
    }
    else{
        min = seconds/60;
        sec = seconds%60;
    }
    memset(time_f, 0, sizeof(time_f));
    sprintf(time_f, "%u::%u::%u", hrs, min, sec);
    return time_f;
}
