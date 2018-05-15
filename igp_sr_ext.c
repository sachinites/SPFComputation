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

extern instance_t *instance;

boolean
is_identical_mapping_entries(sr_mapping_entry_t *mapping_entry1,
    sr_mapping_entry_t *mapping_entry2){

    if(mapping_entry1->prf == mapping_entry2->prf &&
        mapping_entry1->pi == mapping_entry2->pi &&
        mapping_entry1->pe == mapping_entry2->pe &&
        mapping_entry1->pfx_len == mapping_entry2->pfx_len &&
        mapping_entry1->si == mapping_entry2->si &&
        mapping_entry1->se == mapping_entry2->se &&
        mapping_entry1->range_value == mapping_entry2->range_value &&
        mapping_entry1->algorithm == mapping_entry2->algorithm &&
        mapping_entry1->topology == mapping_entry2->topology)
            return TRUE;
     return FALSE;
}

void
construct_prefix_mapping_entry(prefix_t *prefix, 
            sr_mapping_entry_t *mapping_entry_out){
    
    if(prefix->hosting_node->am_i_mapping_server)
        mapping_entry_out->prf = IGP_DEFAULT_SID_SRMS_PFX_PREFERENCE_VALUE;
    else
        mapping_entry_out->prf = IGP_DEFAULT_SID_PFX_PREFERENCE_VALUE;

    char subnet[PREFIX_LEN + 1];
    unsigned int binary_prefix = 0;
    prefix_sid_subtlv_t *prefix_sid = get_prefix_sid(prefix); 
     
    memset(subnet, 0, PREFIX_LEN + 1);
    apply_mask(prefix->prefix, prefix->mask, subnet);
    inet_pton(AF_INET, subnet, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    mapping_entry_out->pi          = binary_prefix;
    mapping_entry_out->pe          = binary_prefix;
    mapping_entry_out->pfx_len     = prefix->mask;
    mapping_entry_out->max_pfx_len = 32;
    mapping_entry_out->si          = prefix_sid->sid.sid;
    mapping_entry_out->se          = prefix_sid->sid.sid;
    mapping_entry_out->range_value = 1;
    mapping_entry_out->algorithm   = prefix_sid->algorithm;
    mapping_entry_out->topology    = 0;
}


boolean
is_prefixes_conflicting(sr_mapping_entry_t *pfx_mapping_entry1,
    sr_mapping_entry_t *pfx_mapping_entry2){

    /*topology check*/
    if(pfx_mapping_entry1->topology != pfx_mapping_entry2->topology)
        return FALSE;

    /*algorithm check*/
    if(pfx_mapping_entry1->algorithm != pfx_mapping_entry2->algorithm)
        return FALSE;

    /*address family*/
    if(pfx_mapping_entry1->max_pfx_len != pfx_mapping_entry2->max_pfx_len)
        return FALSE;

    /*prefix length*/
    if(pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len)
        return FALSE;

    /*simplifying by assuming Range is always 1*/
    /*prefix*/
    if(pfx_mapping_entry1->pi != pfx_mapping_entry2->pi)
        return FALSE;

    /*SID*/
    if(pfx_mapping_entry1->si != pfx_mapping_entry2->si)
        return TRUE;

    return FALSE;
}

boolean
is_prefixes_sid_conflicting(sr_mapping_entry_t *pfx_mapping_entry1,
    sr_mapping_entry_t *pfx_mapping_entry2){

    if(is_identical_mapping_entries(pfx_mapping_entry1, pfx_mapping_entry2))
        return FALSE;

    /* If the two prefixes have same SIDs but they different in some way, 
     * then it is SID conflict*/
    if((pfx_mapping_entry1->si == pfx_mapping_entry2->si)                   &&
        (pfx_mapping_entry1->algorithm != pfx_mapping_entry2->algorithm     ||
         pfx_mapping_entry1->pi != pfx_mapping_entry2->pi                   ||
         pfx_mapping_entry1->topology != pfx_mapping_entry2->topology       ||
         pfx_mapping_entry1->max_pfx_len != pfx_mapping_entry2->max_pfx_len ||
         pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len))
        return TRUE;

    return FALSE;
}

static void
free_global_prefix_list(ll_t *global_prefix_list){

    delete_singly_ll(global_prefix_list);
    free(global_prefix_list);
}

static int 
global_prefix_list_comparison_fn(void *data1, void *data2){

    singly_ll_node_t *list_node1 = data1,
                     *list_node2 = data2;

    prefix_t *prefix1 = list_node1->data;
    prefix_t *prefix2 = list_node2->data;

    unsigned int binary_prefix1 = 0, 
                 binary_prefix2 = 0;

    char subnet[PREFIX_LEN + 1];
    memset(subnet, 0, PREFIX_LEN + 1);
    
    apply_mask(prefix1->prefix, prefix1->mask, subnet);
    inet_pton(AF_INET, subnet, &binary_prefix1);
    binary_prefix1 = htonl(binary_prefix1);
    
    memset(subnet, 0, PREFIX_LEN + 1);
    apply_mask(prefix2->prefix, prefix2->mask, subnet);
    inet_pton(AF_INET, subnet, &binary_prefix2);
    binary_prefix2 = htonl(binary_prefix2);

    if(binary_prefix1 < binary_prefix2)
        return -1;
    if(binary_prefix1 > binary_prefix2)
        return 1;
    return 0; 
}

/* Generate a global prefix list. This list contains all prefixes advertised by
 * all nodes in a given level*/

static ll_t *
build_global_prefix_list(node_t *node, LEVEL level){

    node_t *curr_node = NULL,
    *nbr_node = NULL, 
    *pn_node = NULL;

    edge_t *edge1 = NULL,
           *edge2 = NULL;

    singly_ll_node_t *list_node = NULL;
    prefix_t *prefix = NULL;

    init_instance_traversal(instance);
    Queue_t *q = initQ();
    node->traversing_bit = 1;
    
    enqueue(q, node);

    ll_t *global_pfx_lst = init_singly_ll();
    singly_ll_set_comparison_fn(global_pfx_lst, get_prefix_comparison_fn());

    while(!is_queue_empty(q)){
        curr_node = deque(q);
        ITERATE_NODE_PHYSICAL_NBRS_BEGIN(curr_node, nbr_node, pn_node, edge1,
                edge2, level){

            if(nbr_node->traversing_bit){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(curr_node, nbr_node, pn_node, level);
            }

            if(nbr_node->spring_enabled == FALSE){
                goto NEXT_NODE;
            }

            ITERATE_LIST_BEGIN(nbr_node->local_prefix_list[level], list_node){
                prefix = list_node->data;
                if(prefix->psid_thread_ptr){
                    MARK_PREFIX_SR_ACTIVE(prefix);
                    singly_ll_add_node_by_val(global_pfx_lst, list_node->data);
                }
            } ITERATE_LIST_END;
     
     NEXT_NODE:
            nbr_node->traversing_bit = 1;
            enqueue(q, nbr_node);
        } ITERATE_NODE_PHYSICAL_NBRS_END(curr_node, nbr_node, pn_node, level);
    }

    /*Add self list*/
    if(node->spring_enabled == FALSE)
        return global_pfx_lst;

    ITERATE_LIST_BEGIN(node->local_prefix_list[level], list_node){
        prefix = list_node->data;
        if(prefix->psid_thread_ptr){
            MARK_PREFIX_SR_ACTIVE(prefix);
            singly_ll_add_node_by_val(global_pfx_lst, list_node->data);
        }
    } ITERATE_LIST_END;

    return global_pfx_lst;
}


void
resolve_prefix_sid_conflict(prefix_t *prefix1, sr_mapping_entry_t *pfx_mapping_entry1,
        prefix_t *prefix2, sr_mapping_entry_t *pfx_mapping_entry2){

    assert(IS_PREFIX_SR_ACTIVE(prefix1) && 
            IS_PREFIX_SR_ACTIVE(prefix2));

    if(pfx_mapping_entry1->prf != pfx_mapping_entry2->prf){
        
        if(pfx_mapping_entry1->prf > pfx_mapping_entry2->prf)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
         return;
    }
        
    
    if(pfx_mapping_entry1->range_value != pfx_mapping_entry2->range_value){

        if(pfx_mapping_entry1->range_value < pfx_mapping_entry2->range_value)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }


    if(pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len){

        if(pfx_mapping_entry1->pfx_len < pfx_mapping_entry2->pfx_len)
            MARK_PREFIX_SR_INACTIVE(prefix1);
        else
            MARK_PREFIX_SR_INACTIVE(prefix2);
        return;
    }

    if(pfx_mapping_entry1->pi != pfx_mapping_entry2->pi){

        if(pfx_mapping_entry1->pi < pfx_mapping_entry2->pi)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }

    if(pfx_mapping_entry1->algorithm != pfx_mapping_entry2->algorithm){

        if(pfx_mapping_entry1->algorithm < pfx_mapping_entry2->algorithm)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }

#if 0
    if(pfx_mapping_entry1->si != pfx_mapping_entry2->si){

        if(pfx_mapping_entry1->si < pfx_mapping_entry2->si)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }
#endif

    if(pfx_mapping_entry1->topology != pfx_mapping_entry2->topology){
        MARK_PREFIX_SR_INACTIVE(prefix1);
        MARK_PREFIX_SR_INACTIVE(prefix2);
        return;
    }
    else{
        return;
    }
    assert(0);
}

void
resolve_prefix_conflict(prefix_t *prefix1, sr_mapping_entry_t *pfx_mapping_entry1,
        prefix_t *prefix2, sr_mapping_entry_t *pfx_mapping_entry2){

    assert(IS_PREFIX_SR_ACTIVE(prefix1) && 
            IS_PREFIX_SR_ACTIVE(prefix2));

    if(pfx_mapping_entry1->prf != pfx_mapping_entry2->prf){
        
        if(pfx_mapping_entry1->prf > pfx_mapping_entry2->prf)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
         return;
    }
        
    
    if(pfx_mapping_entry1->range_value != pfx_mapping_entry2->range_value){

        if(pfx_mapping_entry1->range_value < pfx_mapping_entry2->range_value)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }


    if(pfx_mapping_entry1->pfx_len != pfx_mapping_entry2->pfx_len){

        if(pfx_mapping_entry1->pfx_len < pfx_mapping_entry2->pfx_len)
            MARK_PREFIX_SR_INACTIVE(prefix1);
        else
            MARK_PREFIX_SR_INACTIVE(prefix2);
        return;
    }
#if 0
    if(pfx_mapping_entry1->pi != pfx_mapping_entry2->pi){

        if(pfx_mapping_entry1->pi < pfx_mapping_entry2->pi)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }
#endif
    if(pfx_mapping_entry1->si != pfx_mapping_entry2->si){

        if(pfx_mapping_entry1->si < pfx_mapping_entry2->si)
            MARK_PREFIX_SR_INACTIVE(prefix2);
        else
            MARK_PREFIX_SR_INACTIVE(prefix1);
        return;
    }
    assert(0);
}

ll_t *
prefix_sid_conflict_resolution(ll_t *global_prefix_list){

    singly_ll_node_t *list_node1 = NULL,
                     *list_node2 = NULL;

    sr_mapping_entry_t mapping_entry1, 
                       mapping_entry2;
                     
    prefix_t *prefix1 = NULL,
             *prefix2 = NULL;

    ITERATE_LIST_BEGIN(global_prefix_list, list_node1){

        prefix1 = list_node1->data;
        if(!IS_PREFIX_SR_ACTIVE(prefix1)) continue;

        construct_prefix_mapping_entry(prefix1, &mapping_entry1); 
        for(list_node2 = list_node1->next; list_node2; list_node2 = list_node2->next){
            
            prefix2 = list_node2->data;
            if(!IS_PREFIX_SR_ACTIVE(prefix2)) continue;
            
             construct_prefix_mapping_entry(prefix2, &mapping_entry2); 
             if(is_prefixes_sid_conflicting(&mapping_entry1, &mapping_entry2) == FALSE)
                 continue;

             resolve_prefix_sid_conflict(prefix1, &mapping_entry1, 
                    prefix2, &mapping_entry2);
        
             if(!IS_PREFIX_SR_ACTIVE(prefix1))
                 break;
        } 
    } ITERATE_LIST_END;

    /*Clean up the weed out ones from the global list*/ 
    ITERATE_LIST_BEGIN2(global_prefix_list, list_node1, list_node2){

        prefix1 = list_node1->data;
        if(!IS_PREFIX_SR_ACTIVE(prefix1)){
            ITERATIVE_LIST_NODE_DELETE2(global_prefix_list, list_node1, list_node2);
        }
    } ITERATE_LIST_END2(global_prefix_list, list_node1, list_node2);

    return global_prefix_list;    
}
/*This is the highest level function which takes responsibility 
 * to resolve prefix conflicts */

ll_t *
prefix_conflict_resolution(node_t *node, LEVEL level){

   singly_ll_node_t *list_node1 = NULL,
                    *list_node2 = NULL;
                    
   sr_mapping_entry_t mapping_entry1, 
                      mapping_entry2;
                     
   prefix_t *prefix1 = NULL,
            *prefix2 = NULL;


   ll_t *global_prefix_list = build_global_prefix_list(node, level);
                    
   ITERATE_LIST_BEGIN(global_prefix_list, list_node1){
       
       prefix1 = list_node1->data;
       if(!IS_PREFIX_SR_ACTIVE(prefix1)) 
           continue;

       construct_prefix_mapping_entry(prefix1, &mapping_entry1);

       for(list_node2 = list_node1->next; list_node2; list_node2 = list_node2->next){
        
         prefix2 = list_node2->data;
         if(!IS_PREFIX_SR_ACTIVE(prefix2))
             continue;

         construct_prefix_mapping_entry(prefix2,  &mapping_entry2);

         if(is_prefixes_conflicting(&mapping_entry1, &mapping_entry2) == FALSE)
             continue;
        /*Now resolve the conflict between prefix1 and prefix 2*/
         resolve_prefix_conflict(prefix1, &mapping_entry1, prefix2, &mapping_entry2);
 
         if(!IS_PREFIX_SR_ACTIVE(prefix1))
             break;
         
         assert((IS_PREFIX_SR_ACTIVE(prefix1) && !IS_PREFIX_SR_ACTIVE(prefix2)) || 
            (!IS_PREFIX_SR_ACTIVE(prefix1) &&  IS_PREFIX_SR_ACTIVE(prefix2)));
       }
   } ITERATE_LIST_END;

   /*Now cleanup the prefixes which are filtered out and will not be considered
    * for SID conflict resolution phase*/

    ITERATE_LIST_BEGIN2(global_prefix_list, list_node1, list_node2){

        prefix1 = list_node1->data;
        if(!IS_PREFIX_SR_ACTIVE(prefix1)){
            ITERATIVE_LIST_NODE_DELETE2(global_prefix_list, list_node1, list_node2);
        }
    } ITERATE_LIST_END2(global_prefix_list, list_node1, list_node2);
   
   return global_prefix_list;
}

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
    free(node->srgb);
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

/*This fn returns the best prefix which is also
 * SR Active prefix. In case of ECMP, all best prefixes will
 * have same usable SIDs due to conflict resolution test*/
static prefix_t *
get_best_sr_active_route_prefix(routes_t *route){

    singly_ll_node_t *list_node = NULL;
    prefix_t *prefix = NULL;
    
    prefix_pref_data_t route_pref = route_preference(route->flags, route->level);
    prefix_pref_data_t prefix_pref;

    ITERATE_LIST_BEGIN(route->like_prefix_list, list_node){
        prefix = list_node->data;   
        assert(IS_PREFIX_SR_ACTIVE(prefix)); 
        prefix_pref = route_preference(prefix->prefix_flags, route->level);
        if(route_pref.pref != prefix_pref.pref)
            break;
        return prefix;
    }ITERATE_LIST_END;
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
        sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s, LDP proxy nexthop %s(%s) cannot be springified. SPRING not enabled",
                spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                get_str_level(route->level), next_hop_oif_name(*nxthop), nxthop->node->node_name);
        trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
        return;
    }

    sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s, springifying LDP backup nexthops %s(%s), RLFA : %s",
            spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
            get_str_level(route->level), next_hop_oif_name(*nxthop), nxthop->node->node_name,
            nxthop->rlfa->node_name);
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);

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

        sprintf(instance->traceopts->b, "Node : %s : After Springification : route %s/%u at %s InLabel : %u\n\tStack : %s:%u\t%s:%u, oif : %s, gw : %s, nexthop : %s", 
                spf_root->node_name, route->rt_key.u.prefix.prefix,
                route->rt_key.u.prefix.mask, get_str_level(route->level), route->rt_key.u.label,
                get_str_stackops(nxthop->stack_op[1]) , nxthop->mpls_label_out[1],
                get_str_stackops(nxthop->stack_op[0]) , nxthop->mpls_label_out[0], next_hop_oif_name(*nxthop),
                next_hop_gateway_pfx(nxthop), nxthop->proxy_nbr->node_name);
        trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
        return;
    }

    sprintf(instance->traceopts->b, "Node : %s : After Springification : route %s/%u at %s InLabel : %u\n\tStack : %s:%u, oif : %s, gw : %s, nexthop : %s", 
        spf_root->node_name, route->rt_key.u.prefix.prefix,
        route->rt_key.u.prefix.mask, get_str_level(route->level), route->rt_key.u.label,
        get_str_stackops(nxthop->stack_op[0]) , nxthop->mpls_label_out[0], next_hop_oif_name(*nxthop),
        next_hop_gateway_pfx(nxthop), nxthop->proxy_nbr->node_name);
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
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
        sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s, IPV4 nexthop %s(%s) cannot be springified. SPRING not enabled",
                spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                get_str_level(route->level), next_hop_oif_name(*nxthop), nxthop->node->node_name);
        trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
        return;
    }

    sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s, springifying IPV4 nexthop %s(%s)",
            spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
            get_str_level(route->level), next_hop_oif_name(*nxthop), nxthop->node->node_name);
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);

    /*caluclate the SPRING Nexthop related information first*/
    if(is_node_best_prefix_originator(nxthop->node, route)){
        /*Check if node advertised PHP service*/
        prefix_sid = prefix_sid_search(nxthop->node, route->level, prefix_sid_index);
        assert(prefix_sid);
        if(IS_BIT_SET(prefix_sid->flags, NO_PHP_P_FLAG)){
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
    

    sprintf(instance->traceopts->b, "Node : %s : After Springification : route %s/%u at %s InLabel : %u, OutLabel : %u," 
            " Stack Op : %s, oif : %s, gw : %s, nexthop : %s", spf_root->node_name, route->rt_key.u.prefix.prefix, 
            route->rt_key.u.prefix.mask, get_str_level(route->level), route->rt_key.u.label, 
            nxthop->mpls_label_out[0], get_str_stackops(nxthop->stack_op[0]), next_hop_oif_name(*nxthop),
            next_hop_gateway_pfx(nxthop), nxthop->node->node_name);
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
}


void
springify_unicast_route(node_t *spf_root, routes_t *route){

    mpls_label_t incoming_label = 0,
                 old_incoming_label = 0;

    singly_ll_node_t *list_node = NULL;
    internal_nh_t *nxthop = NULL;
    unsigned int  dst_prefix_sid = 0;

    sprintf(instance->traceopts->b, "Node : %s : Springifying route %s/%u at %s", 
        spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
        get_str_level(route->level));
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);

    prefix_t *prefix = get_best_sr_active_route_prefix(route);/*This prefix is one of the best prefix in case of ECMP*/
    dst_prefix_sid = PREFIX_SID_INDEX(prefix);

    incoming_label = get_label_from_srgb_index(spf_root->srgb, dst_prefix_sid);
    old_incoming_label = route->rt_key.u.label;

    if(route->rt_key.u.label != incoming_label){
        route->rt_key.u.label = incoming_label;
        if(route->install_state != RTE_ADDED)
            route->install_state = RTE_CHANGED;
        sprintf(instance->traceopts->b, "Node : %s : route %s/%u at %s Incoming label updated %u -> %u, route status = %s",
                spf_root->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
                get_str_level(route->level), old_incoming_label, incoming_label, 
                route_intall_status_str(route->install_state));
        trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
    }

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

