/*
 * =====================================================================================
 *
 *       Filename:  unified_nh.c
 *
 *    Description:  Implements the functionality of Unified Nexthops
 *
 *        Version:  1.0
 *        Created:  Friday 09 March 2018 01:03:52  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the SPDComputation distribution (https://github.com/sachinites).
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

#include "unified_nh.h"
#include "instance.h"
#include "stack.h"

#define un_next_hop_type(un_internal_nh_t_ptr)  \
    (un_internal_nh_t_ptr->nh_type)


unsigned int
get_direct_un_next_hop_metric(internal_un_nh_t *nh, LEVEL level){
    
    /*Direct Nexthops could be IPNH Or RSVP Nexthops only*/
    switch(nh->nh_type){
        case IP_NH:
        case RSVPNH:
            return (GET_EGDE_PTR_FROM_FROM_EDGE_END(nh->oif))->metric[level];
        default:
            assert(0);
    }
}

node_t *
un_next_hop_node(internal_un_nh_t *nh){

    return nh->nh_node;
}

char
get_un_next_hop_gateway_pfx_mask(internal_un_nh_t *nh){

    switch(nh->nh_type){
        /*RSVPs dont have nexthop gateway and prefix*/
        case RSVPNH:
            assert(0);
        default:
            return (GET_EGDE_PTR_FROM_FROM_EDGE_END(nh->oif))->to.prefix[nh->level]->mask;
    }
}

char *
get_un_next_hop_gateway_pfx(internal_un_nh_t *nh){

    switch(nh->nh_type){
        /*RSVPs dont have nexthop gateway and prefix*/
        case RSVPNH:
            assert(0);
        default:
            return (GET_EGDE_PTR_FROM_FROM_EDGE_END(nh->oif))->to.prefix[nh->level]->prefix;
    }
}

char *
get_un_next_hop_oif_name(internal_un_nh_t *nh){

    return nh->oif->intf_name;
}

char *
get_un_next_hop_protected_link_name(internal_un_nh_t *nh){

    return nh->protected_link->intf_name;
}

void
set_un_next_hop_gw_pfx(internal_un_nh_t *nh, char *pfx){

    switch(nh->nh_type){
        case IP_NH:
            strncpy(nh->nh.ipnh.gw_prefix, pfx, PREFIX_LEN);
            nh->nh.ipnh.gw_prefix[PREFIX_LEN] = '\0';
            break;
        case SPRNH:
            strncpy(nh->nh.sprnh.gw_prefix, pfx, PREFIX_LEN);
            nh->nh.sprnh.gw_prefix[PREFIX_LEN] = '\0';
            break;
        case LDPNH:
            strncpy(nh->nh.ldpnh.gw_prefix, pfx, PREFIX_LEN);
            nh->nh.ldpnh.gw_prefix[PREFIX_LEN] = '\0';
        default:
            assert(0);
    }
}

boolean
is_un_next_hop_empty(internal_un_nh_t *nh){

    return nh->oif == NULL;
}

/*Fill the nh_type before calling this fn*/
void
init_un_next_hop(internal_un_nh_t *nh, NH_TYPE2 nh_type){

    if(nh_type == SPRNH){
        if(nh->nh.sprnh.mpls_label_stack.stack)
            reset_stack(nh->nh.sprnh.mpls_label_stack.stack);
        else
            nh->nh.sprnh.mpls_label_stack.stack = get_new_stack();
    }
    nh->nh_type = nh_type;
    nh->level = MAX_LEVEL;
    nh->oif = NULL;
    nh->nh_node = 0;
    nh->lfa_type = UNKNOWN_LFA_TYPE;
    nh->protected_link = NULL;
    nh->ref_count = 0;
    nh->flags = 0;
}

void
copy_un_next_hop_t(internal_un_nh_t *src, internal_un_nh_t *dst){

    memcpy(dst, src, sizeof(internal_un_nh_t));
    if(dst->nh_type == SPRNH){
        dst->nh.sprnh.mpls_label_stack.stack = get_new_stack();
        memcpy(dst->nh.sprnh.mpls_label_stack.stack, src->nh.sprnh.mpls_label_stack.stack, sizeof(stack_t));
    }
}

boolean
is_un_nh_t_equal(internal_un_nh_t *nh1, internal_un_nh_t *nh2){

    unsigned int i = 0;
    /*Comparing common properties*/
    if(nh1->nh_type != nh2->nh_type)
        return FALSE;

    if(nh1->level != nh2->level)
        return FALSE;

    if(strncmp(nh1->oif->intf_name, nh2->oif->intf_name, IF_NAME_SIZE))
        return FALSE;

    if(strncmp(nh1->nh_node->node_name, nh2->nh_node->node_name, NODE_NAME_SIZE))
        return FALSE; 

    switch(nh1->nh_type){
        case IP_NH:
            if(strncmp(nh1->nh.ipnh.gw_prefix, nh2->nh.ipnh.gw_prefix, PREFIX_LEN + 1))
                return FALSE;
            return TRUE;
        case RSVPNH:
            if(nh1->nh.rsvpnh.mpls_label != nh2->nh.rsvpnh.mpls_label)
                return FALSE;
            return TRUE;
        case LDPNH:
            if(strncmp(nh1->nh.ldpnh.gw_prefix, nh2->nh.ldpnh.gw_prefix, PREFIX_LEN + 1))
                return FALSE;
            if(strncmp(nh1->nh.ldpnh.rlfa->node_name, nh2->nh.ldpnh.rlfa->node_name, NODE_NAME_SIZE))
                return FALSE;
            if(nh1->nh.ldpnh.mpls_label != nh2->nh.ldpnh.mpls_label)
                return FALSE;
            return TRUE;
        case SPRNH:
            if(strncmp(nh1->nh.sprnh.gw_prefix, nh2->nh.sprnh.gw_prefix, PREFIX_LEN + 1))
                return FALSE;
            if(nh1->nh.sprnh.mpls_label_stack.stack->top != nh2->nh.sprnh.mpls_label_stack.stack->top)
                return FALSE;        
            for(; i < nh1->nh.sprnh.mpls_label_stack.stack->top; i++){
                if((mpls_label_t)(nh1->nh.sprnh.mpls_label_stack.stack->slot[i]) != 
                        (mpls_label_t)(nh2->nh.sprnh.mpls_label_stack.stack->slot[i]))
                    return FALSE;
            }
            return TRUE; 
    }
}

void
free_un_nexthop(internal_un_nh_t *nh){

    if(nh->type == SPRNH){
        if(nh->nh.sprnh.mpls_label_stack.stack)
            free_stack(nh->nh.sprnh.mpls_label_stack.stack);
    }
    free(nh);
}
