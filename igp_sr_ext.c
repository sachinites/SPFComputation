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
#include "rt_mpls.h"
#include "bitsop.h"
#include "spftrace.h"

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
    
    if(prefix->hosting_node->is_srms)
        mapping_entry_out->prf = IGP_DEFAULT_SID_SRMS_PFX_PREFERENCE_VALUE;
    else
        mapping_entry_out->prf = IGP_DEFAULT_SID_PFX_PREFERENCE_VALUE;

    char subnet[PREFIX_LEN + 1];
    unsigned int binary_prefix = 0;
    
    memset(subnet, 0, PREFIX_LEN + 1);
    apply_mask(prefix->prefix, prefix->mask, subnet);
    inet_pton(AF_INET, subnet, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    mapping_entry_out->pi          = binary_prefix;
    mapping_entry_out->pe          = binary_prefix;
    mapping_entry_out->pfx_len     = prefix->mask;
    mapping_entry_out->si          = PREFIX_SID_VALUE(prefix);
    mapping_entry_out->se          = PREFIX_SID_VALUE(prefix);
    mapping_entry_out->range_value = 1;
    mapping_entry_out->algorithm   = prefix->prefix_sid->algorithm;
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

            if(nbr_node->traversing_bit ||
                nbr_node->spring_enabled == FALSE){
                ITERATE_NODE_PHYSICAL_NBRS_CONTINUE(curr_node, nbr_node, pn_node, level);
            }

            ITERATE_LIST_BEGIN(nbr_node->local_prefix_list[level], list_node){
                prefix = list_node->data;
                if(prefix->prefix_sid){
                    MARK_PREFIX_SR_ACTIVE(prefix);
                    singly_ll_add_node_by_val(global_pfx_lst, list_node->data);
                }
            } ITERATE_LIST_END;
            
            nbr_node->traversing_bit = 1;
            enqueue(q, nbr_node);
        } ITERATE_NODE_PHYSICAL_NBRS_END(curr_node, nbr_node, pn_node, level);
    }

    /*Add self list*/
    ITERATE_LIST_BEGIN(node->local_prefix_list[level], list_node){
        prefix = list_node->data;
        if(prefix->prefix_sid){
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
    srgb->range = SRGB_DEF_UPPER_BOUND - SRGB_DEF_LOWER_BOUND + 1;
    srgb->first_sid.type = SID_SUBTLV_TYPE;
    srgb->first_sid.length = 0;
    srgb->first_sid.type = SID_SUBTLV_TYPE;
    srgb->first_sid.sid = SRGB_DEF_LOWER_BOUND;
    if(!srgb->index_array)
        srgb->index_array = calloc(1, srgb->range);
    else
        memset(srgb->index_array, 0, srgb->range);
}

mpls_label_t
get_available_srgb_label(srgb_t *srgb){

   unsigned int i = 0;
   for(; i < srgb->range; i++){
    if(*(srgb->index_array + i))
        continue;
    break;    
   }
   if(i == srgb->range)
       return 0;
   return srgb->first_sid.sid + i;
}

void
mark_srgb_mpls_label_in_use(srgb_t *srgb, mpls_label_t label){

    if(!(label >= SRGB_DEF_LOWER_BOUND && label <= SRGB_DEF_UPPER_BOUND)){
        printf("Error : Invalid label specified\n");
        return;
    }

    unsigned int index = label - srgb->first_sid.sid;
    assert(*(srgb->index_array + index) < 255);
    (*(srgb->index_array + index))++;
}


void
mark_srgb_mpls_label_not_in_use(srgb_t *srgb, mpls_label_t label){

    if(!(label >= SRGB_DEF_LOWER_BOUND && label <= SRGB_DEF_UPPER_BOUND)){
        printf("Error : Invalid label specified\n");
        return;
    }

    unsigned int index = label - srgb->first_sid.sid;
    if(*(srgb->index_array + index) > 0)
        (*(srgb->index_array + index))--;
}

boolean
is_mpls_label_in_use(srgb_t *srgb, mpls_label_t label){

    if(!(label >= SRGB_DEF_LOWER_BOUND && label <= SRGB_DEF_UPPER_BOUND)){
        printf("Error : Invalid label specified\n");
        return TRUE;
    }

    unsigned int index = label - srgb->first_sid.sid;
    return *(srgb->index_array + index) >= 1;
}

/*This fn blinkdly copies all IPV4 route members to mpls route.
 * You need to override the fields in the called appropriately*/

static mpls_rt_entry_t *
prepare_mpls_entry_template_from_ipv4_route(routes_t *route){

   singly_ll_node_t *list_node = NULL;
   internal_nh_t *ipv4_prim_nh = NULL;
   mpls_rt_nh_t *mpls_prim_nh = NULL;
   unsigned int i = 0, 
                prim_nh_count = 0;
   
   prim_nh_count = GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[IPNH]);
   mpls_rt_entry_t *mpls_rt_entry = NULL;

   /*Add atleast 1 mpls next hop so that we can add POP operation
    * when labelled packet is recieved for local mpls SIDs*/
   if(prim_nh_count) 
       mpls_rt_entry = calloc(1, sizeof(mpls_rt_entry_t) + \
               (prim_nh_count * sizeof(mpls_rt_nh_t)));
   else
       mpls_rt_entry = calloc(1, sizeof(mpls_rt_entry_t) + \
               sizeof(mpls_rt_nh_t));

   prefix_t *prefix  = ROUTE_GET_BEST_PREFIX(route);

   mpls_rt_entry->incoming_label = PREFIX_SID_VALUE(prefix);
   mpls_rt_entry->version = route->version;
   mpls_rt_entry->prim_nh_count = prim_nh_count;
    
   ITERATE_LIST_BEGIN(route->primary_nh_list[IPNH], list_node){
        
        ipv4_prim_nh = list_node->data;
        mpls_prim_nh = &mpls_rt_entry->mpls_nh[i];
        mpls_prim_nh->outgoing_label = mpls_rt_entry->incoming_label;
        strncpy(mpls_prim_nh->gwip, ipv4_prim_nh->gw_prefix, PREFIX_LEN);
        mpls_prim_nh->gwip[PREFIX_LEN] = '\0';
        strncpy(mpls_prim_nh->oif, ipv4_prim_nh->oif->intf_name, IF_NAME_SIZE);
        mpls_prim_nh->oif[IF_NAME_SIZE]= '\0';
        mpls_prim_nh->stack_op = SWAP;
        i++;
   } ITERATE_LIST_END;

   if(prim_nh_count == 0){
        mpls_prim_nh = &mpls_rt_entry->mpls_nh[0];
        mpls_prim_nh->outgoing_label = 0;
        strncpy(mpls_prim_nh->gwip, "0.0.0.0" , PREFIX_LEN);
        mpls_prim_nh->gwip[PREFIX_LEN] = '\0'; 
        strncpy(mpls_prim_nh->oif, "Nil", IF_NAME_SIZE); 
        mpls_prim_nh->oif[IF_NAME_SIZE]= '\0';
        mpls_prim_nh->stack_op = POP;
        mpls_rt_entry->prim_nh_count = 1;
   }

   return mpls_rt_entry;
}

void
sr_install_local_prefix_mpls_fib_entry(node_t *node, routes_t *route){

    /*route should be local route*/
    assert(route->hosting_node == node);

    prefix_t *prefix  = ROUTE_GET_BEST_PREFIX(route);

    if(!prefix->prefix_sid){
        printf("Error : No prefix SID assigned\n");
        return;
    }

    if(!IS_PREFIX_SR_ACTIVE(prefix)){
        sprintf(instance->traceopts->b, "Node : %s : Local route %s/%u was not installed. Reason : Conflicted prefix", 
                node->node_name, route->rt_key.prefix, route->rt_key.mask);
        trace(instance->traceopts, MPLS_ROUTE_INSTALLATION_BIT);
        return;
    }
    
    /* If Router recieves unlabelled pkt because nbr has performed PHP, 
     * hence no need to insall mpls entry*/

    if(!IS_BIT_SET(prefix->prefix_sid->flags, NO_PHP_P_FLAG)){
        sprintf(instance->traceopts->b, "Node : %s : Local route %s/%u was not installed. Reason : PHP enabled", 
                node->node_name, route->rt_key.prefix, route->rt_key.mask);
        trace(instance->traceopts, MPLS_ROUTE_INSTALLATION_BIT);
        return;
    }

    /*If current router do not want nbrs to perform PHP, then current router
     * should implment POP and OIF - NULL in mpls table*/
    mpls_rt_entry_t *mpls_rt_entry = prepare_mpls_entry_template_from_ipv4_route(route);
    
    /*sanity checks*/
    assert(!mpls_rt_entry->prim_nh_count);

    /*Current router would recieve this prefix with label, install the
     * entry to pop the label*/
    mpls_rt_entry->mpls_nh[0].stack_op = POP;
    mpls_rt_entry->mpls_nh[0].outgoing_label = 0;
    sprintf(instance->traceopts->b, "Node : %s : Local route %s/%u Installed, Opn : POP",
                node->node_name, route->rt_key.prefix, route->rt_key.mask);
    trace(instance->traceopts, MPLS_ROUTE_INSTALLATION_BIT);
    install_mpls_forwarding_entry(GET_MPLS_TABLE_HANDLE(node), mpls_rt_entry);
}

void
sr_install_remote_prefix_mpls_fib_entry(node_t *node, routes_t *route){

   /*route should be for remote destination*/
    assert(route->hosting_node != node); 

    unsigned int i = 0;
    singly_ll_node_t *list_node = NULL;
    internal_nh_t *ipv4_prim_nh = NULL;
    mpls_rt_nh_t *mpls_nh = NULL;
    prefix_t *prefix  = ROUTE_GET_BEST_PREFIX(route);

    if(!prefix->prefix_sid){
        printf("Error : No prefix SID assigned\n");
        return;
    }

    if(!IS_PREFIX_SR_ACTIVE(prefix)){
        sprintf(instance->traceopts->b, "Node : %s : Remote route %s/%u was not installed. Reason : Conflicted prefix", 
                node->node_name, route->rt_key.prefix, route->rt_key.mask);
        trace(instance->traceopts, MPLS_ROUTE_INSTALLATION_BIT);
        return;
    }
  
    mpls_rt_entry_t *mpls_rt_entry = prepare_mpls_entry_template_from_ipv4_route(route);

    if(!IS_BIT_SET(prefix->prefix_sid->flags, NO_PHP_P_FLAG)){
        /*I am suppose to perform PHP. I will do PHP Only when the 
         * primary next of this route is a destination*/
        /*@override*/ 
        ITERATE_LIST_BEGIN(route->primary_nh_list[IPNH], list_node){
            
            ipv4_prim_nh = list_node->data;
            if(ipv4_prim_nh->node != prefix->hosting_node){
                i++;
                continue;
            }
            
            mpls_nh = &mpls_rt_entry->mpls_nh[i];        
            mpls_nh->stack_op = POP;
            i++;
        } ITERATE_LIST_END;
    }
    else{
        /*I dont have to do any PHP, nothing to do*/
    }

    sprintf(instance->traceopts->b, "Node : %s : Remote route %s/%u Installed",
                node->node_name, route->rt_key.prefix, route->rt_key.mask);
    trace(instance->traceopts, MPLS_ROUTE_INSTALLATION_BIT);
    install_mpls_forwarding_entry(GET_MPLS_TABLE_HANDLE(node), mpls_rt_entry);
}

void
igp_install_mpls_static_route(node_t *node, char *str_prefix, char mask){

    prefix_t prefix, *prefix2 = NULL;

    init_prefix_key(&prefix, str_prefix, mask);
    routes_t *route = search_route_in_spf_route_list(&(node->spf_info), &prefix, LEVEL1/*unused*/);
    if(!route){
        printf("Info : No Unicast Route exist\n");
        return;
    }
    
    prefix2 = ROUTE_GET_BEST_PREFIX(route);
    
    if(!IS_PREFIX_SR_ACTIVE(prefix2)){
        printf("Warning : Conflicted prefix\n");
    }

    if(route->hosting_node == node)
        sr_install_local_prefix_mpls_fib_entry(node, route);           
    else
        sr_install_remote_prefix_mpls_fib_entry(node, route);
}

void
igp_uninstall_mpls_static_route(node_t *node, char *str_prefix, char mask){

    
    prefix_t prefix, *prefix2 = NULL;

    init_prefix_key(&prefix, str_prefix, mask);
    routes_t *route = search_route_in_spf_route_list(&node->spf_info, &prefix, LEVEL1/*unused*/);
    if(!route){
        printf("Info : No Unicast Route exist\n");
        return;
    }
    
    prefix2 = ROUTE_GET_BEST_PREFIX(route);
    mpls_label_t label = PREFIX_SID_VALUE(prefix2);
    delete_mpls_forwarding_entry(GET_MPLS_TABLE_HANDLE(node), label);
}

