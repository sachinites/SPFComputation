/*
 * =====================================================================================
 *
 *       Filename:  igp_sr_ext.c
 *
 *    Description:  Implementation of SR extnsion for IGPs
 *
 *        Version:  1.0
 *        Created:  Friday 02 March 2018 07:56:04  IST
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

#include "igp_sr_ext.h"
#include "LinkedListApi.h"
#include "instance.h"
#include "advert.h"
#include "Queue.h"
#include "spfutil.h"
#include <arpa/inet.h>
#include "routes.h"
#include "bitsop.h"
#include "spftrace.h"
#include "sr_tlv_api.h"
#include "bitarr.h"
#include "no_warn.h"
#include "LinuxMemoryManager/uapi_mm.h"

extern instance_t *instance;

/*SRGB operations*/

/*Opimize . ..*/
void
init_srgb_defaults(srgb_t *srgb){
   
    srgb->type = SR_CAPABILITY_SRGB_SUBTLV_TYPE;
    srgb->length = 0;
    srgb->flags = 0;
    srgb->range = SRGB_DEFAULT_RANGE;
    srgb->first_sid.type = SID_SUBTLV_TYPE;
    srgb->first_sid.length = 0;
    srgb->first_sid.sid = SRGB_FIRST_DEFAULT_SID;
    init_bit_array(&srgb->index_array, srgb->range);
}

mpls_label_t
get_available_srgb_label(srgb_t *srgb){

   unsigned int index = 0;
   index = get_next_available_bit(SRGB_INDEX_ARRAY(srgb));
   if(index == 0xFFFFFFFF)
       return index;
   return srgb->first_sid.sid + index;
}

void
mark_srgb_index_in_use(srgb_t *srgb, unsigned int index){

    assert(index >= 0 && index < srgb->range);
    set_bit(SRGB_INDEX_ARRAY(srgb), index);
}


void
mark_srgb_index_not_in_use(srgb_t *srgb, unsigned int index){

    assert(index >= 0 && index < srgb->range);
    unset_bit(SRGB_INDEX_ARRAY(srgb) , index);
}

boolean
is_srgb_index_in_use(srgb_t *srgb, unsigned int index){

    assert(index >= 0 && index < srgb->range);
    return is_bit_set(SRGB_INDEX_ARRAY(srgb), index) == 0 ?
        FALSE:TRUE;
}

mpls_label_t 
get_label_from_srgb_index(srgb_t *srgb, unsigned int index){

    assert(index >= 0 && index < srgb->range);
    return srgb->first_sid.sid + index;
}


/* Test if the route has SPRING path also. If the best prefix
 * prefix originator of the route could not pass conflict-resolution
 * test, then route is said to have no spring path, otherwise it will
 * lead to traffic black-holing
 * */

boolean
IS_ROUTE_SPRING_CAPABLE(routes_t *route) {

    prefix_t *prefix = ROUTE_GET_BEST_PREFIX(route);
    if(prefix->psid_thread_ptr && IS_PREFIX_SR_ACTIVE(prefix))
        return TRUE;
    return FALSE;
}

boolean
is_node_spring_enabled(node_t *node, LEVEL level){

    return node->spring_enabled;
/*
    prefix_t *router_id = node_local_prefix_search(node, level, node->router_id, 32);
    assert(router_id);

    if(router_id->psid_thread_ptr &&
            IS_PREFIX_SR_ACTIVE(router_id)){
        return TRUE;
    }
    return FALSE;
*/
}

void
spring_disable_cleanup(node_t *node){
    
    glthread_t *curr = NULL;
    LEVEL level_it;
    prefix_sid_subtlv_t *prefix_sid = NULL;
    
    free((SRGB_INDEX_ARRAY(node->srgb))->array);
    XFREE(node->srgb);
    node->use_spring_backups = FALSE;

    /*Break the association between prefixes and prefix SIDs*/
    for(level_it = LEVEL1 ; level_it <= LEVEL2 ; level_it++){
        ITERATE_GLTHREAD_BEGIN(&node->prefix_sids_thread_lst[level_it], curr){
            prefix_sid = glthread_to_prefix_sid(curr);
            free_prefix_sid(prefix_sid->prefix);
        } ITERATE_GLTHREAD_END(&node->prefix_sids_thread_lst[level_it], curr);
    }
}

prefix_sid_subtlv_t *
prefix_sid_search(node_t *node, LEVEL level, unsigned int prefix_sid_val){

    assert(node->spring_enabled);
    glthread_t *curr = NULL;
    prefix_sid_subtlv_t *prefix_sid = NULL;

    ITERATE_GLTHREAD_BEGIN(&node->prefix_sids_thread_lst[level], curr){
        prefix_sid = glthread_to_prefix_sid(curr);
        if(prefix_sid && prefix_sid->sid.sid == prefix_sid_val)
            return prefix_sid;
    }ITERATE_GLTHREAD_END(&node->prefix_sids_thread_lst[level], curr);
    return NULL;
}


prefix_sid_subtlv_t *
get_node_segment_prefix_sid(node_t *node, LEVEL level){

    if(node->spring_enabled == FALSE)
        return NULL;

    prefix_sid_subtlv_t *prefix_sid = NULL;
    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(&node->prefix_sids_thread_lst[level], curr){
       
        prefix_sid = glthread_to_prefix_sid(curr);
        if(strncmp(prefix_sid->prefix->prefix, node->router_id, PREFIX_LEN)) 
            continue;
        return prefix_sid;
    } ITERATE_GLTHREAD_END(&node->prefix_sids_thread_lst[level], curr);

    return NULL;
}

/*There should not be such thing : Convert RSVP tunnel to
 * SR tunnels*/
static void
springify_rsvp_nexthop(node_t *spf_root, 
        internal_nh_t *nxthop, 
        routes_t *route, 
        unsigned int prefix_sid_index){
}

static void
springify_rlfa_nexthop(node_t *spf_root, 
        internal_nh_t *nxthop, 
        routes_t *route, 
        unsigned int prefix_sid_index){

    prefix_sid_subtlv_t *rlfa_node_prefix_sid = NULL;
    mpls_label_t mpls_label = 0;
    
    if(!is_node_spring_enabled(nxthop->proxy_nbr, route->level)) {
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s, LDP proxy nexthop %s(%s) cannot be springified. SPRING not enabled",
                spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                get_str_level(route->level), next_hop_oif_name(*nxthop), nxthop->node->node_name);
        trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
        return;
    }

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s, springifying LDP backup nexthops %s(%s), RLFA : %s",
            spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
            get_str_level(route->level), next_hop_oif_name(*nxthop), nxthop->node->node_name,
            nxthop->rlfa->node_name);
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif

    /* PLR should send the traffic to Destination via RLFA. There are two options to
     * perform this via SR-tunnels:
     * a. Either SR-tunnel the traffic from PLR -- RLFA --- Destination
     * b. Or SR-tunnel the traffic from PLR -- RLFA, and then traffic is sent via IGP path to Dest as IPV4 traffic. 
     * We will chooe option 'a' by default. We may provide knob to switch between these two behaviors.
     * Operation to perform :
     1. lookup prefix_sid_index in nxthop->rlfa->srgb and PUSH
     2. lookup RLFA router id node segment index in nxthop->node->srgb, and perform SWAP 
     */

    /*Op 2*/ /*Remember this operation is being done on PLR and RLFA needs to be installed in inet.3 and mpls.0  table
     only as LDP tunnel or SR tunnel*/
    rlfa_node_prefix_sid = get_node_segment_prefix_sid(nxthop->rlfa, route->level);
    mpls_label = get_label_from_srgb_index(nxthop->proxy_nbr->srgb, rlfa_node_prefix_sid->sid.sid);
    nxthop->mpls_label_out[0] = mpls_label;
    nxthop->stack_op[0] = PUSH;

    /*Op 1*/
    /*If RLFA is also the Destination, then this operation is not required*/
    if(!is_node_best_prefix_originator(nxthop->rlfa, route)){
        
        mpls_label = get_label_from_srgb_index(nxthop->rlfa->srgb, prefix_sid_index); 
        nxthop->mpls_label_out[1] = mpls_label;
        nxthop->stack_op[1] = PUSH;

#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Node : %s : After Springification : route %s/%u at %s InLabel : %u\n\tStack : %s:%u\t%s:%u, oif : %s, gw : %s, nexthop : %s", 
                spf_root->node_name, route->rt_key.u.prefix.prefix,
                route->rt_key.u.prefix.mask, get_str_level(route->level), route->rt_key.u.label,
                get_str_stackops(nxthop->stack_op[1]) , nxthop->mpls_label_out[1],
                get_str_stackops(nxthop->stack_op[0]) , nxthop->mpls_label_out[0], next_hop_oif_name(*nxthop),
                next_hop_gateway_pfx(nxthop), nxthop->proxy_nbr->node_name);
        trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
        return;
    }

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : After Springification : route %s/%u at %s InLabel : %u\n\tStack : %s:%u, oif : %s, gw : %s, nexthop : %s", 
        spf_root->node_name, route->rt_key.u.prefix.prefix,
        route->rt_key.u.prefix.mask, get_str_level(route->level), route->rt_key.u.label,
        get_str_stackops(nxthop->stack_op[0]) , nxthop->mpls_label_out[0], next_hop_oif_name(*nxthop),
        next_hop_gateway_pfx(nxthop), nxthop->proxy_nbr->node_name);
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
}

static void
springify_ipv4_nexthop(node_t *spf_root, 
        internal_nh_t *nxthop, 
        routes_t *route, 
        unsigned int prefix_sid_index){

    MPLS_STACK_OP stack_op = STACK_OPS_UNKNOWN;
    prefix_sid_subtlv_t *prefix_sid = NULL;
    unsigned int outgoing_label = 0;
   
    if(!is_node_spring_enabled(nxthop->node, route->level)) {
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s, IPV4 nexthop %s(%s) cannot be springified. SPRING not enabled",
                spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                get_str_level(route->level), next_hop_oif_name(*nxthop), nxthop->node->node_name);
        trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
        return;
    }

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s, springifying IPV4 nexthop %s(%s)",
            spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
            get_str_level(route->level), next_hop_oif_name(*nxthop), nxthop->node->node_name);
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif

    /*caluclate the SPRING Nexthop related information first*/
    if(is_node_best_prefix_originator(nxthop->node, route)){
        /*Check if node advertised PHP service*/
        prefix_sid = prefix_sid_search(nxthop->node, route->level, prefix_sid_index);
        assert(prefix_sid);
        if(IS_BIT_SET(prefix_sid->flags, NO_PHP_P_FLAG) || 
            IS_BIT_SET(prefix_sid->flags, RE_ADVERTISEMENT_R_FLAG)){
            stack_op = SWAP;   
            outgoing_label = get_label_from_srgb_index(nxthop->node->srgb, prefix_sid_index);
            assert(outgoing_label);
        }
        else{
            stack_op = POP;
            outgoing_label = 0;
        }
    }
    else{
        stack_op = SWAP;
        outgoing_label = get_label_from_srgb_index(nxthop->node->srgb, prefix_sid_index);
        assert(outgoing_label);
    }
    /*Now compare the SPring related information*/
    /*populate the SPRING related information only*/
    nxthop->mpls_label_out[0] = outgoing_label;
    nxthop->stack_op[0] = stack_op;
    

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : After Springification : route %s/%u at %s InLabel : %u, OutLabel : %u," 
            " Stack Op : %s, oif : %s, gw : %s, nexthop : %s", spf_root->node_name, route->rt_key.u.prefix.prefix, 
            route->rt_key.u.prefix.mask, get_str_level(route->level), route->rt_key.u.label, 
            nxthop->mpls_label_out[0], get_str_stackops(nxthop->stack_op[0]), next_hop_oif_name(*nxthop),
            next_hop_gateway_pfx(nxthop), nxthop->node->node_name);
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
}


void
springify_unicast_route(node_t *spf_root, routes_t *route, 
                        unsigned int dst_prefix_sid){

    mpls_label_t incoming_label = 0,
                 old_incoming_label = 0;

    singly_ll_node_t *list_node = NULL;
    internal_nh_t *nxthop = NULL;

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : Springifying route %s/%u at %s", 
        spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
        get_str_level(route->level));
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif

    /*Now Do primary next hops*/
    ITERATE_LIST_BEGIN(route->primary_nh_list[IPNH], list_node){
        nxthop = list_node->data;
        springify_ipv4_nexthop(spf_root, nxthop, route, dst_prefix_sid);        
    } ITERATE_LIST_END;
    
    ITERATE_LIST_BEGIN(route->primary_nh_list[LSPNH], list_node){
        nxthop = list_node->data;
        if(is_internal_backup_nexthop_rsvp(nxthop))
            springify_rsvp_nexthop(spf_root, nxthop, route, dst_prefix_sid);
        else
            springify_rlfa_nexthop(spf_root, nxthop, route, dst_prefix_sid);
    } ITERATE_LIST_END;

    /*Now do backups*/
    ITERATE_LIST_BEGIN(route->backup_nh_list[IPNH], list_node){
        nxthop = list_node->data;
        springify_ipv4_nexthop(spf_root, nxthop, route, dst_prefix_sid);        
    } ITERATE_LIST_END;
    
    ITERATE_LIST_BEGIN(route->backup_nh_list[LSPNH], list_node){
        nxthop = list_node->data;
        if(is_internal_backup_nexthop_rsvp(nxthop))
            springify_rsvp_nexthop(spf_root, nxthop, route, dst_prefix_sid);
        else
            springify_rlfa_nexthop(spf_root, nxthop, route, dst_prefix_sid);
    } ITERATE_LIST_END;
}

void
PRINT_ONE_LINER_SPRING_NXT_HOP(internal_nh_t *nh){

    if(nh->protected_link == NULL){
        printf("\t%s----%s---->%-s(%s(%s))\n", nh->oif->intf_name,
                "SPRING",
                next_hop_gateway_pfx(nh),
                nh->node->node_name,
                nh->node->router_id);
    }
    else{
        printf("\t%s----%s---->%-s(%s(%s)) protecting : %s -- %s\n", nh->oif->intf_name,
                "SPRING",
                next_hop_gateway_pfx(nh),
                nh->node ? nh->node->node_name : nh->proxy_nbr->node_name,
                nh->node ? nh->node->router_id : nh->proxy_nbr->router_id,
                nh->protected_link->intf_name,
                get_str_lfa_type(nh->lfa_type));
    }
    /*Print spring stack here*/
    int i = MPLS_STACK_OP_LIMIT_MAX -1;
    printf("\t\t");

    for(; i >= 0; i--){
        if(nh->mpls_label_out[i] == 0 &&
                nh->stack_op[i] == STACK_OPS_UNKNOWN)
            continue;
        if(nh->stack_op[i] == POP)
            printf("%s  ", get_str_stackops(nh->stack_op[i]));
        else
            printf("%s:%u  ", get_str_stackops(nh->stack_op[i]), nh->mpls_label_out[i]);
    }
    printf("\n");
}

