/*
 * =====================================================================================
 *
 *       Filename:  routes.c
 *
 *    Description:  This file implements the construction of routing table building
 *
 *        Version:  1.0
 *        Created:  Monday 04 September 2017 12:05:40  IST
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

#include "spfutil.h"
#include "routes.h"
#include "bitsop.h"
#include "advert.h"
#include "spftrace.h"
#include "igp_sr_ext.h"
#include "sr_tlv_api.h"
#include "no_warn.h"

extern instance_t *instance;

extern void
spf_computation(node_t *spf_root,
                spf_info_t *spf_info,
                LEVEL level, spf_type_t spf_type);
extern int
instance_node_comparison_fn(void *_node, void *input_node_name);

static void
enhanced_start_route_installation(spf_info_t *spf_info,
                         LEVEL level, rtttype_t rtttype);

static boolean
is_destination_has_multiple_primary_nxthops(spf_result_t *D_res){

    if((is_internal_nh_t_empty(D_res->next_hop[IPNH][0]) &&
                !is_internal_nh_t_empty(D_res->next_hop[LSPNH][0]) &&
                is_internal_nh_t_empty(D_res->next_hop[LSPNH][1])) 
            ||
            (is_internal_nh_t_empty(D_res->next_hop[LSPNH][0]) &&
             !is_internal_nh_t_empty(D_res->next_hop[IPNH][0]) &&
             is_internal_nh_t_empty(D_res->next_hop[IPNH][1]))){

        return FALSE;
    }
    return TRUE;
}

routes_t *
route_malloc(){

    nh_type_t nh;
    routes_t *route = calloc(1, sizeof(routes_t));
    ITERATE_NH_TYPE_BEGIN(nh){
        route->primary_nh_list[nh] = init_singly_ll();
        singly_ll_set_comparison_fn(route->primary_nh_list[nh], instance_node_comparison_fn);
        route->backup_nh_list[nh] = init_singly_ll();
        singly_ll_set_comparison_fn(route->backup_nh_list[nh], instance_node_comparison_fn);
    }ITERATE_NH_TYPE_END;
    route->like_prefix_list = init_singly_ll();
    singly_ll_set_comparison_fn(route->like_prefix_list, get_prefix_comparison_fn());
    singly_ll_set_order_comparison_fn(route->like_prefix_list, get_prefix_order_comparison_fn());
    route->install_state = RTE_ADDED;
    return route;
}

static void
merge_route_primary_nexthops(routes_t *route, spf_result_t *result, nh_type_t nh){

    unsigned int i = 0;
    internal_nh_t *int_nxt_hop = NULL;
     
    for( ; i < MAX_NXT_HOPS ; i++){

        if(is_internal_nh_t_empty(result->next_hop[nh][i]))
            break;

        if(is_internal_nh_exist(route->primary_nh_list[nh], &result->next_hop[nh][i]))
            continue;
        int_nxt_hop = calloc(1, sizeof(internal_nh_t));
        copy_internal_nh_t(result->next_hop[nh][i], *int_nxt_hop);
        singly_ll_add_node_by_val(route->primary_nh_list[nh], int_nxt_hop);
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "route : %s/%u primary next hop is merged with %s's next hop node %s", 
                     route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                     result->next_hop[nh][i].node->node_name); 
        trace(instance->traceopts, ROUTE_CALCULATION_BIT);
#endif
    }

    assert(GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[nh]) <= MAX_NXT_HOPS);
}

static void
merge_route_backup_nexthops(routes_t *route, 
                            spf_result_t *result, 
                            nh_type_t nh){

    unsigned int i = 0;
    internal_nh_t *int_nxt_hop = NULL,
                  *backup = NULL;

    boolean dont_collect_onlylink_protecting_backups =
        is_destination_has_multiple_primary_nxthops(result);

    for( i = 0; i < MAX_NXT_HOPS ; i++){
        
        backup = &result->node->backup_next_hop[route->level][nh][i];
        if(is_internal_nh_t_empty(*backup)) break;
        if(is_internal_nh_exist(route->backup_nh_list[nh], backup))
            continue;

        if(dont_collect_onlylink_protecting_backups){
            if(backup->lfa_type == LINK_PROTECTION_LFA                           ||
                    backup->lfa_type == LINK_PROTECTION_LFA_DOWNSTREAM           ||
                    backup->lfa_type == LINK_PROTECTION_RLFA                     ||
                    backup->lfa_type == LINK_PROTECTION_RLFA_DOWNSTREAM          ||
                    backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA            ||
                    backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM ||
                    backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA           ||
                    backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA_DOWNSTREAM){
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "\t ECMP : only link-protecting backup dropped : %s----%s---->%-s(%s(%s)) protecting link: %s", 
                            backup->oif->intf_name,
                            next_hop_type(*backup) == IPNH ? "IPNH" : "LSPNH",
                            next_hop_type(*backup) == IPNH ? next_hop_gateway_pfx(backup) : "",
                            backup->node ? backup->node->node_name : backup->rlfa->node_name,
                            backup->node ? backup->node->router_id : backup->rlfa->router_id, 
                            backup->protected_link->intf_name); 
                    trace(instance->traceopts, ROUTE_CALCULATION_BIT);
#endif
                continue;
            }
        }

        int_nxt_hop = calloc(1, sizeof(internal_nh_t));
        copy_internal_nh_t(result->node->backup_next_hop[route->level][nh][i], *int_nxt_hop);
        singly_ll_add_node_by_val(route->backup_nh_list[nh], int_nxt_hop);
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "route : %s/%u backup next hop is merged with %s's next hop node %s", 
                     route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                     result->node->backup_next_hop[route->level][nh][i].node->node_name); 
        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
    }
    assert(GET_NODE_COUNT_SINGLY_LL(route->backup_nh_list[nh]) <= MAX_NXT_HOPS);
}

void
route_set_key(routes_t *route, char *ipv4_addr, char mask){

    apply_mask(ipv4_addr, mask, route->rt_key.u.prefix.prefix);
    route->rt_key.u.prefix.prefix[PREFIX_LEN] = '\0';
    route->rt_key.u.prefix.mask = mask;
}

void
free_route(routes_t *route){

    if(!route)  return;
    nh_type_t nh; 
    route->hosting_node = 0;
    
    ITERATE_NH_TYPE_BEGIN(nh){
        ROUTE_FLUSH_PRIMARY_NH_LIST(route, nh);
        free(route->primary_nh_list[nh]);
        route->primary_nh_list[nh] = 0;
        ROUTE_FLUSH_BACKUP_NH_LIST(route, nh);
        free(route->backup_nh_list[nh]);
        route->backup_nh_list[nh] = 0;
    } ITERATE_NH_TYPE_END;
    
    delete_singly_ll(route->like_prefix_list);
    free(route->like_prefix_list);
    route->like_prefix_list = NULL;
    free(route);
}


routes_t *
search_route_in_spf_route_list(spf_info_t *spf_info, 
                                prefix_t *prefix,
                                rtttype_t rt_type){

    routes_t *route = NULL;
    singly_ll_node_t* list_node = NULL;
    char prefix_with_mask[PREFIX_LEN + 1];
    apply_mask(prefix->prefix, prefix->mask, prefix_with_mask);
    prefix_with_mask[PREFIX_LEN] = '\0';
    ITERATE_LIST_BEGIN(spf_info->routes_list[rt_type], list_node){
        route = list_node->data;
        if(strncmp(route->rt_key.u.prefix.prefix, prefix_with_mask, PREFIX_LEN) == 0 &&
                (route->rt_key.u.prefix.mask == prefix->mask))
            return route;    
    }ITERATE_LIST_END;
    return NULL;
}

/*Search internal route using longest prefix
 *  * match*/
routes_t *
search_route_in_spf_route_list_by_lpm(spf_info_t *spf_info,
                                char *prefix, rtttype_t rt_type){

    routes_t *route = NULL,
             *default_route = NULL,
             *lpm_route = NULL;

    char longest_mask = 0;
    singly_ll_node_t* list_node = NULL;

    ITERATE_LIST_BEGIN(spf_info->routes_list[rt_type], list_node){

        route = list_node->data;
        if(strncmp("0.0.0.0", route->rt_key.u.prefix.prefix, strlen("0.0.0.0")) == 0 &&
                route->rt_key.u.prefix.mask == 0){
            default_route = route;
        }
        else if(strncmp(prefix, route->rt_key.u.prefix.prefix, PREFIX_LEN) == 0){
            if( route->rt_key.u.prefix.mask > longest_mask){
                longest_mask = route->rt_key.u.prefix.mask;
                lpm_route = route;   
            }
        }
    } ITERATE_LIST_END;
    return lpm_route ? lpm_route : default_route;
}


char * 
route_intall_status_str(route_intall_status install_status){

    switch(install_status){
        case RTE_STALE:
            return "RTE_STALE";
        case RTE_ADDED:
            return "RTE_ADDED";
        case RTE_UPDATED:
            return "RTE_UPDATED";
        case RTE_CHANGED:
            return "RTE_CHANGED";
        case RTE_NO_CHANGE:
            return "RTE_NO_CHANGE";
        default:
            assert(0);
    }
}

static unsigned int 
delete_stale_routes(spf_info_t *spf_info, LEVEL level, rtttype_t rt_type){

   singly_ll_node_t* list_node = NULL;
   routes_t *route = NULL;
   unsigned int i = 0;

#ifdef __ENABLE_TRACE__   
   sprintf(instance->traceopts->b, "Deleting Stale Routes"); 
   trace(instance->traceopts, ROUTE_CALCULATION_BIT);
#endif

   ITERATE_LIST_BEGIN(spf_info->routes_list[rt_type], list_node){
           
       route = list_node->data;
       if(route->install_state == RTE_STALE && IS_LEVEL_SET(route->level, level)){
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "route : %s/%u is STALE for Level%d, deleted", route->rt_key.u.prefix.prefix, 
                        route->rt_key.u.prefix.mask, level); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
        i++;
        ROUTE_DEL_FROM_ROUTE_LIST(spf_info, route, rt_type);
        free_route(route);
        route = NULL;
       }
   }ITERATE_LIST_END;

   return i;
}

void
mark_all_routes_stale(spf_info_t *spf_info, LEVEL level, rtttype_t topology){

    singly_ll_node_t* list_node = NULL;
    routes_t *route = NULL;

    ITERATE_LIST_BEGIN(spf_info->routes_list[topology], list_node){

        route = list_node->data;
        if(!IS_LEVEL_SET(route->level, level))
            continue;

        route->install_state = RTE_STALE;
#if 0
        ITERATE_LIST_BEGIN(route->like_prefix_list, list_node1){
            free_prefix(list_node1->data);
        } ITERATE_LIST_END;
#endif

        delete_singly_ll(route->like_prefix_list);
    }ITERATE_LIST_END;
}

static void 
overwrite_route(spf_info_t *spf_info, routes_t *route, 
                prefix_t *prefix, spf_result_t *result, LEVEL level){

        unsigned int i = 0;
        nh_type_t nh = NH_MAX;
        internal_nh_t *int_nxt_hop = NULL,
                      *backup = NULL;

        delete_singly_ll(route->like_prefix_list);
        route_set_key(route, prefix->prefix, prefix->mask); 

#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "route : %s/%u being over written for %s", route->rt_key.u.prefix.prefix, 
                    route->rt_key.u.prefix.mask, get_str_level(level)); 
        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

        route->version = spf_info->spf_level_info[level].version;
        route->flags = prefix->prefix_flags;
       
        route->level = level;
        route->hosting_node = prefix->hosting_node;

        if(!IS_BIT_SET(prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)){
            /* If the prefix metric type is internal*/
            route->spf_metric =  result->spf_metric + prefix->metric;
            route->lsp_metric =  result->spf_metric + prefix->metric;; 
        }else{
            /* If the metric type is external, we only compute the hosting node distance as spf metric*/
            route->spf_metric =  result->spf_metric;
            route->lsp_metric =  result->spf_metric;
            route->ext_metric =  prefix->metric;
        }

        /*Check if Destination node has More than one primary next hops, then we need not copy
         * only-link protecting backups*/
        boolean dont_collect_onlylink_protecting_backups = 
            is_destination_has_multiple_primary_nxthops(result);
        
        ITERATE_NH_TYPE_BEGIN(nh){

            ROUTE_FLUSH_PRIMARY_NH_LIST(route, nh);
            ROUTE_FLUSH_BACKUP_NH_LIST(route, nh);

            for(i = 0 ; i < MAX_NXT_HOPS; i++){
                if(!is_internal_nh_t_empty(result->next_hop[nh][i])){
                    int_nxt_hop = calloc(1, sizeof(internal_nh_t));
                    copy_internal_nh_t(result->next_hop[nh][i], *int_nxt_hop);
                    ROUTE_ADD_NH(route->primary_nh_list[nh], int_nxt_hop);   
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "route : %s/%u primary next hop is merged with %s's next hop node %s", 
                            route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                            result->next_hop[nh][i].node->node_name); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                }
                else
                    break;
            }
            for(i = 0 ; i < MAX_NXT_HOPS; i++){
                if(!is_internal_nh_t_empty((result->node->backup_next_hop[level][nh][i]))){
                    backup = &result->node->backup_next_hop[level][nh][i];        
                    if(dont_collect_onlylink_protecting_backups){
                        if(backup->lfa_type == LINK_PROTECTION_LFA                           ||
                                backup->lfa_type == LINK_PROTECTION_LFA_DOWNSTREAM           ||
                                backup->lfa_type == LINK_PROTECTION_RLFA                     ||
                                backup->lfa_type == LINK_PROTECTION_RLFA_DOWNSTREAM          ||
                                backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA            ||
                                backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM ||
                                backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA           ||
                                backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA_DOWNSTREAM){
#ifdef __ENABLE_TRACE__                            
                            sprintf(instance->traceopts->b, "\t ECMP : only link-protecting backup dropped : %s----%s---->%-s(%s(%s)) protecting link: %s", 
                                    backup->oif->intf_name,
                                    next_hop_type(*backup) == IPNH ? "IPNH" : "LSPNH",
                                    next_hop_type(*backup) == IPNH ? next_hop_gateway_pfx(backup) : "",
                                    backup->node ? backup->node->node_name : backup->rlfa->node_name,
                                    backup->node ? backup->node->router_id : backup->rlfa->router_id, 
                                    backup->protected_link->intf_name); 
                            trace(instance->traceopts, ROUTE_CALCULATION_BIT);
#endif
                            continue;
                        }
                    }

                    int_nxt_hop = calloc(1, sizeof(internal_nh_t));
                    copy_internal_nh_t((result->node->backup_next_hop[level][nh][i]), *int_nxt_hop);
                    ROUTE_ADD_NH(route->backup_nh_list[nh], int_nxt_hop);   
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "route : %s/%u backup next hop is merged with %s's backup next hop node %s", 
                            route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                            result->node->backup_next_hop[level][nh][i].node->node_name); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                }
                else
                    break;
            }
        } ITERATE_NH_TYPE_END;
}

static void
link_prefix_to_route(routes_t *route, prefix_t *new_prefix,
                     unsigned int prefix_hosting_node_metric, 
                     spf_info_t *spf_info){

    singly_ll_node_t *list_node_prev = NULL,
                     *list_node_next = NULL,
                     *list_new_node = NULL;

    prefix_t *list_prefix = NULL;
    unsigned int new_prefix_metric = 0,
                 list_prefix_metric = 0;

    spf_result_t *res = NULL;
    
    prefix_pref_data_t list_prefix_pref = {ROUTE_UNKNOWN_PREFERENCE, 
                                          "ROUTE_UNKNOWN_PREFERENCE"},

                        new_prefix_pref = route_preference(new_prefix->prefix_flags, new_prefix->level);

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "To Route : %s/%u, %s, Appending prefix : %s/%u to Route prefix list",
                 route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, get_str_level(route->level),
                 new_prefix->prefix, new_prefix->mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

    if(is_singly_ll_empty(route->like_prefix_list)){
        singly_ll_add_node_by_val(route->like_prefix_list, new_prefix);
        return;
    }

    list_new_node = singly_ll_init_node(new_prefix);

    ITERATE_LIST_BEGIN(route->like_prefix_list, list_node_next){
        
        list_prefix = (prefix_t *)list_node_next->data;
        list_prefix_pref = route_preference(list_prefix->prefix_flags, list_prefix->level);

        if(new_prefix_pref.pref > list_prefix_pref.pref){
            list_node_prev = list_node_next;
            continue;
        }

        if(new_prefix_pref.pref < list_prefix_pref.pref){
            list_new_node->next =  list_node_next;
            if(!list_node_prev)
                route->like_prefix_list->head = list_new_node; 
            else   
                list_node_prev->next = list_new_node;
            INC_NODE_COUNT_SINGLY_LL(route->like_prefix_list);
            return;
        }

        /* Now tie based on metric*/
            
        res = GET_SPF_RESULT(spf_info, list_prefix->hosting_node, list_prefix->level);

        assert(res);

        if(IS_BIT_SET(new_prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)) 
            new_prefix_metric = prefix_hosting_node_metric;   
        else
            new_prefix_metric = prefix_hosting_node_metric + new_prefix->metric;


        if(IS_BIT_SET(list_prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)) 
            list_prefix_metric = res->spf_metric;
        else
            list_prefix_metric = res->spf_metric + list_prefix->metric;

        if(new_prefix->metric >= INFINITE_METRIC)
            new_prefix_metric = INFINITE_METRIC; 
       
        /*Check with production code, comparison looks unfair to me*/ 
        if(new_prefix_metric > list_prefix_metric){
            list_node_prev = list_node_next;
            continue;
        }

        if(new_prefix_metric < list_prefix_metric){
            list_new_node->next =  list_node_next;
            if(!list_node_prev)
                route->like_prefix_list->head = list_new_node; 
            else
                list_node_prev->next = list_new_node; 
            INC_NODE_COUNT_SINGLY_LL(route->like_prefix_list);
            return;
        }

        /* We are here because preference and metrics are same.
         * now tie based on Router ID (We dont support this comparison, so
         * simply insert the prefix here and exit)*/

        list_new_node->next =  list_node_next;
        if(!list_node_prev)
            route->like_prefix_list->head = list_new_node;
        else
            list_node_prev->next = list_new_node; 
        INC_NODE_COUNT_SINGLY_LL(route->like_prefix_list);
        return;

    }ITERATE_LIST_END;

    /* Add at the end of list*/
    list_new_node->next =  list_node_next;
    if(!list_node_prev)
        route->like_prefix_list->head = list_new_node; 
    else 
        list_node_prev->next = list_new_node;
    INC_NODE_COUNT_SINGLY_LL(route->like_prefix_list);
}

static void
update_route(spf_info_t *spf_info,          /*spf_info of computing node*/ 
             spf_result_t *result,          /*result representing some network node*/
             prefix_t *prefix,              /*local prefix hosted on 'result' node*/
             LEVEL level,  rtttype_t rt_type,
             boolean linkage){

    routes_t *route = NULL;
    unsigned int i = 0;
    nh_type_t nh = NH_MAX;
    internal_nh_t *int_nxt_hop = NULL;

    prefix_pref_data_t prefix_pref = {ROUTE_UNKNOWN_PREFERENCE, "ROUTE_UNKNOWN_PREFERENCE"},
                       route_pref = {ROUTE_UNKNOWN_PREFERENCE, "ROUTE_UNKNOWN_PREFERENCE"};



#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : result node %s, topo = %s, prefix %s, level %s, prefix metric : %u",
            GET_SPF_INFO_NODE(spf_info, level)->node_name, result->node->node_name, get_topology_name(rt_type),
            prefix->prefix, get_str_level(level), prefix->metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

    if(prefix->metric == INFINITE_METRIC){
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "prefix : %s/%u discarded because of infinite metric", 
        prefix->prefix, prefix->mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
        return;
    }

    route = search_route_in_spf_route_list(spf_info, prefix, rt_type);

    if(!route){
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "prefix : %s/%u is a New route (malloc'd) in %s, hosting_node %s", 
                prefix->prefix, prefix->mask, get_str_level(level), prefix->hosting_node->node_name); 
        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

        route = route_malloc();
        route_set_key(route, prefix->prefix, prefix->mask); 
        route->version = spf_info->spf_level_info[level].version;

        /*Copy the prefix flags to route flags. flags include :
         * PREFIX_DOWNBIT_FLAG
         * PREFIX_EXTERNABIT_FLAG
         * PREFIX_METRIC_TYPE_EXT */

        route->flags = prefix->prefix_flags;
        route->level = level;
        route->hosting_node = prefix->hosting_node;

        /* Update route metric. Metric is to be filled depending on the prefix
         * metric type is external or internal*/

        if(!IS_BIT_SET(prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)){
            /* If the prefix metric type is internal*/
            route->spf_metric =  result->spf_metric + prefix->metric;
            route->lsp_metric = result->lsp_metric + prefix->metric; 
        }else{
            /* If the metric type is external, we only compute the hosting node distance as spf metric*/
            route->spf_metric =  result->spf_metric;
            route->lsp_metric = result->lsp_metric; 
            route->ext_metric = prefix->metric;
        }

        ITERATE_NH_TYPE_BEGIN(nh){ 

            for(i = 0; i < MAX_NXT_HOPS; i++){
                if(!is_internal_nh_t_empty(result->next_hop[nh][i])){
                    int_nxt_hop = calloc(1, sizeof(internal_nh_t));
                    copy_internal_nh_t(result->next_hop[nh][i], *int_nxt_hop);
                    ROUTE_ADD_NH(route->primary_nh_list[nh], int_nxt_hop);   
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Next hop added : %s|%s at %s", 
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask ,
                            result->next_hop[nh][i].node->node_name, nh == IPNH ? "IPNH":"LSPNH", get_str_level(level)); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                }
                else
                    break;
            }
            for(i = 0 ; i < MAX_NXT_HOPS; i++){
                if(!is_internal_nh_t_empty((result->node->backup_next_hop[level][nh][i]))){
                    int_nxt_hop = calloc(1, sizeof(internal_nh_t));
                    copy_internal_nh_t((result->node->backup_next_hop[level][nh][i]), *int_nxt_hop);
                    ROUTE_ADD_NH(route->backup_nh_list[nh], int_nxt_hop);   
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "route : %s/%u backup next hop is copied with with %s's next hop node %s", 
                            route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                            result->node->backup_next_hop[level][nh][i].node->node_name); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                }
                else
                    break;
            }
        }ITERATE_NH_TYPE_END;
        /* route->backup_nh_list Not supported yet */

        /*Linkage*/
        if(linkage){
            link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
        }

        ROUTE_ADD_TO_ROUTE_LIST(spf_info, route, rt_type);
        route->install_state = RTE_ADDED;
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u, spf_metric = %u, lsp_metric = %u,  marked RTE_ADDED for level%u",  
                GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
                route->spf_metric, route->lsp_metric, route->level); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
    }
    else{
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u existing route. route verion : %u," 
                "spf version : %u, route level : %s, spf level : %s", 
                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->prefix, prefix->mask, route->version, 
                spf_info->spf_level_info[level].version, get_str_level(route->level), get_str_level(level)); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

        if(route->install_state == RTE_ADDED){

            /* Comparison Block Start*/

            prefix_pref = route_preference(prefix->prefix_flags, prefix->level);
            route_pref  = route_preference(route->flags, route->level);

            if(prefix_pref.pref == ROUTE_UNKNOWN_PREFERENCE){
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : Prefix : %s/%u pref = %s, ignoring prefix",  GET_SPF_INFO_NODE(spf_info, level)->node_name,
                        prefix->prefix, prefix->mask, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                return;
            }

            if(route_pref.pref < prefix_pref.pref){

                /* if existing route is better*/ 
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Not overwritten",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                /*Linkage*/
                if(linkage){
                    link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
                }
                return;
            }
            /* If new prefix is better*/
            else if(prefix_pref.pref < route_pref.pref){

#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, will be overwritten",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

                overwrite_route(spf_info, route, prefix, result, level);
                /*Linkage*/
                if(linkage){
                    link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
                }
                return;
            }
            /* If prefixes are of same preference*/ 
            else{
                /* If route pref = prefix pref, then decide based on metric*/
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Same preference, Trying based on metric",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

                /* If the prefix and route are of same pref, both will have internal metric Or both will have external metric*/
                if(IS_BIT_SET(route->flags, PREFIX_METRIC_TYPE_EXT) && 
                   !IS_BIT_SET(prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)){
                    assert(0);
                }
                if(!IS_BIT_SET(route->flags, PREFIX_METRIC_TYPE_EXT) && 
                    IS_BIT_SET(prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)){
                    assert(0);
                }

                if(IS_BIT_SET(prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)){
                    /*Decide pref based on external metric*/
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Deciding based on External metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); 
                    trace(instance->traceopts, ROUTE_CALCULATION_BIT);; 
#endif

                    if(prefix->metric < route->ext_metric){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : prefix external metric ( = %u) is better than routes external metric( = %u), will overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); 
                        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(prefix->metric > route->ext_metric){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : prefix external metric ( = %u) is no better than routes external metric( = %u), will not overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); 
                        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                    }
                    else{
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                        /* Union LFA,s RLFA,s Primary nexthops*/
                        ITERATE_NH_TYPE_BEGIN(nh){
                            merge_route_primary_nexthops(route, result, nh);
                            merge_route_backup_nexthops(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    /*Linkage*/
                    if(linkage){
                        link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
                    }
                    return;

                }else{
                    /*Decide pref based on internal metric*/
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Deciding based on Internal metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); 
                    trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                    if(result->spf_metric + prefix->metric < route->spf_metric){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u is over-written because better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); 
                        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(result->spf_metric + prefix->metric == route->spf_metric){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); 
                        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                        /* Union LFA,s RLFA,s Primary nexthops*/ 
                        ITERATE_NH_TYPE_BEGIN(nh){
                            merge_route_primary_nexthops(route, result, nh);
                            merge_route_backup_nexthops(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    else{
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u is not over-written because no better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                    }
                    /*Linkage*/
                    if(linkage){
                        link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
                    }
                    return;
                }
            }
            /* Comparison Block Ends*/
        }

        /* RTE_UPDATED route only*/ 
        if(route->version == spf_info->spf_level_info[level].version){

            /* Comparison Block Start*/

            prefix_pref = route_preference(prefix->prefix_flags, prefix->level);
            route_pref = route_preference(route->flags, route->level);

            if(prefix_pref.pref == ROUTE_UNKNOWN_PREFERENCE){
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : Prefix : %s/%u pref = %s, ignoring prefix",  GET_SPF_INFO_NODE(spf_info, level)->node_name,
                        prefix->prefix, prefix->mask, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                return;
            }
            
            if(route_pref.pref < prefix_pref.pref){

                /* if existing route is better*/ 
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Not overwritten",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                /*Linkage*/
                if(linkage){
                    link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
                }
                return;
            }
            /* If new prefix is better*/
            else if(prefix_pref.pref < route_pref.pref){

#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, will be overwritten",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

                overwrite_route(spf_info, route, prefix, result, level);
                /*Linkage*/
                if(linkage){
                    link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
                }
                return;
            }
            /* If prefixes are of same preference*/ 
            else{
                /* If route pref = prefix pref, then decide based on metric*/
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Same preference, Trying based on metric",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

                /* If the prefix and route are of same pref, both will have internal metric Or both will have external metric*/
                if(IS_BIT_SET(route->flags, PREFIX_METRIC_TYPE_EXT) && 
                  !IS_BIT_SET(prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)){
                    assert(0);
                }
                if(!IS_BIT_SET(route->flags, PREFIX_METRIC_TYPE_EXT) && 
                    IS_BIT_SET(prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)){
                    assert(0);
                }

                if(IS_BIT_SET(prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)){
                    /*Decide pref based on external metric*/
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Deciding based on External metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); 
                    trace(instance->traceopts, ROUTE_CALCULATION_BIT);; 
#endif

                    if(prefix->metric < route->ext_metric){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : prefix external metric ( = %u) is better than routes external metric( = %u), will overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); 
                        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(prefix->metric > route->ext_metric){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : prefix external metric ( = %u) is no better than routes external metric( = %u), will not overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); 
                        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                    }
                    else{
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                        /* Union LFA,s RLFA,s Primary nexthops*/
                        ITERATE_NH_TYPE_BEGIN(nh){
                            merge_route_primary_nexthops(route, result, nh);
                            merge_route_backup_nexthops(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    /*Linkage*/
                    if(linkage){
                        link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
                    }
                    return;

                }else{
                    /*Decide pref based on internal metric*/
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Deciding based on Internal metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); 
                    trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                    if(result->spf_metric + prefix->metric < route->spf_metric){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u is over-written because better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(result->spf_metric + prefix->metric == route->spf_metric){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                        /* Union LFA,s RLFA,s Primary nexthops*/ 
                        ITERATE_NH_TYPE_BEGIN(nh){
                            merge_route_primary_nexthops(route, result, nh);
                            merge_route_backup_nexthops(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    else{
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u is not over-written because no better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
                    }
                    /*Linkage*/
                    if(linkage){
                        link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
                    }
                    return;
                }
            }
            /* Comparison Block Ends*/

        }
        else/*This else should not hit for prc run*/
        {
            /*prefix is from prev run and exists. This code hits only once per given route*/
#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "route : %s/%u, updated route(?)", 
                    route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif
            route->install_state = RTE_UPDATED;

#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "Node : %s : route : %s/%u, old spf_metric = %u, new spf metric = %u, marked RTE_UPDATED for level%u",  
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
                    route->spf_metric, result->spf_metric + prefix->metric, route->level); 
            trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "Node : %s : route : %s/%u %s is mandatorily over-written because of version mismatch",
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, get_str_level(level)); 
            trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
#endif

            overwrite_route(spf_info, route, prefix, result, level);
            route->version = spf_info->spf_level_info[level].version;

            /*Linkage*/ 
            if(linkage){
                link_prefix_to_route(route, prefix, result->spf_metric, spf_info);
            }
        }
    }
}

/* Efficient fn is introduced to decide if the route needs any backups at all. A route
 * Does not need only-Link protecting backups if only it has more than 1 primary nexthops.
 * We delete all only-link protecting backups at the stage when route was being built while
 * iterating over ECMP destinations nodes. This is done in merge_route_backup_nexthops() fn.
 * After the process of accumulating primary nexthops from all ECMP destinations node is complete, we need
 * to analyse the nature of primary nexthops of the route. Here 'nature' means how primary nexthops
 * are related to each other. We need to find out basically whether the route has independant
 * set of primary nexthops. A set of nexthops of a route is said to be independant if and only if
 * there exist atleast 1 nexthop in the set which could relay the traffic to atleast one ECMP destination
 * of route without bypassing any other primary nexthop node in the set. The below function returns True
 * if this condn is satified for the route, else false. If this fn return is TRUE, we can safely remove all
 * backup nexthops of all types from the route's backup nexthop lists. This function is written for routes 
 * and will be used in phase2 to weed out the routes which do not need any backups. Function 
 * is_independant_primary_next_hop_list(node_t *node, LEVEL level) serves the same purpose but for Destination
 * and in phase 1 of backup route calculation*/
/* This is very Expensive function, need optimization, will revisit . . . */

/*This fn should be used in phase 1 in backup route calculation*/
boolean
is_independant_primary_next_hop_list_for_nodes(node_t *S, node_t *dst_node, LEVEL level){

    internal_nh_t *prim_next_hop1 = NULL,
                  *prim_next_hop2 = NULL;
    nh_type_t nh,nh1;
    unsigned int dist_prim_nh1_to_D = 0,
                 dist_prim_nh2_to_D = 0,
                 dist_prim_nh1_to_prim_nh2 = 0,
                 i = 0, j = 0;

    /*This fn assumes that all requires SPF runs has been triggered*/
    //Compute_PHYSICAL_Neighbor_SPFs(S, level);

    spf_result_t *D_res = GET_SPF_RESULT((&S->spf_info), dst_node, level); 
    D_res->backup_requirement[level] = BACKUPS_REQUIRED;

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : Testing for Independant primary nexthops at %s for Dest %s",
                    S->node_name, get_str_level(level), dst_node->node_name);
    trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
#endif
    
    ITERATE_NH_TYPE_BEGIN(nh){
        for(i = 0; i < MAX_NXT_HOPS; i++){
            prim_next_hop1 = &D_res->next_hop[nh][i];
            if(is_nh_list_empty2(prim_next_hop1))
                break;
            dist_prim_nh1_to_D = DIST_X_Y(prim_next_hop1->node, dst_node, level);

            ITERATE_NH_TYPE_BEGIN(nh1){
                for(j = 0; j < MAX_NXT_HOPS; j++){
                    prim_next_hop2 = &D_res->next_hop[nh1][j];
                    if(is_nh_list_empty2(prim_next_hop2))
                        break;
                    if(prim_next_hop1 == prim_next_hop2) continue;
                    dist_prim_nh2_to_D = DIST_X_Y(prim_next_hop2->node, dst_node, level);
                    dist_prim_nh1_to_prim_nh2 = DIST_X_Y(prim_next_hop1->node, prim_next_hop2->node, level);

                    if(dist_prim_nh1_to_D < dist_prim_nh1_to_prim_nh2 + dist_prim_nh2_to_D){
                        D_res->backup_requirement[level] = NO_BACKUP_REQUIRED;
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "Node : %s : Dest %s has independent Primary nexthops at %s",
                            S->node_name, dst_node->node_name, get_str_level(level));
                        trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
#endif
                        return TRUE;
                    }
                }
            } ITERATE_NH_TYPE_END;
        }
    }
#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : Dest %s do not have independent Primary nexthops at %s",
            S->node_name, dst_node->node_name, get_str_level(level));
    trace(instance->traceopts, BACKUP_COMPUTATION_BIT);
#endif
    return FALSE;
}

/*This fn should be used in phase 2 in backup route calculation*/
boolean
is_independant_primary_next_hop_list(routes_t *route){

    singly_ll_node_t *list_node = NULL,
                     *list_node2 = NULL,
                     *list_node3 = NULL;

    prefix_t *prefix = NULL;
    node_t *ecmp_dest_node = NULL,
           *primary_nh1 = NULL,
           *primary_nh2 = NULL;
    internal_nh_t *next_hop = NULL;
    nh_type_t nh, nh1;
    LEVEL level = route->level;
    unsigned int dist_prim_nh1_to_D = 0,
                 dist_prim_nh2_to_D = 0,
                 dist_prim_nh1_to_prim_nh2 = 0;

    prefix_t *first_best_prefix = ROUTE_GET_BEST_PREFIX(route);
    assert(first_best_prefix);

    ITERATE_LIST_BEGIN(route->like_prefix_list, list_node){
        prefix = list_node->data;
        ecmp_dest_node = prefix->hosting_node;

        ITERATE_NH_TYPE_BEGIN(nh){
            ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node2){
                next_hop = list_node2->data;
                primary_nh1 = next_hop->node;
                dist_prim_nh1_to_D = DIST_X_Y(primary_nh1, ecmp_dest_node, level);
                ITERATE_NH_TYPE_BEGIN(nh1){
                    ITERATE_LIST_BEGIN(route->primary_nh_list[nh1], list_node3){
                        next_hop = list_node3->data;
                        primary_nh2 = next_hop->node;
                        if(primary_nh1 == primary_nh2)
                            continue;
                        dist_prim_nh2_to_D = DIST_X_Y(primary_nh2, ecmp_dest_node, level);
                        /*if primary_nh1 can relay traffic to ecmp_dest_node without 
                         * passing through primary_nh2, then return TRUE*/
                        dist_prim_nh1_to_prim_nh2 = DIST_X_Y(primary_nh1, primary_nh2, level);
                        if(dist_prim_nh1_to_D < dist_prim_nh1_to_prim_nh2 + dist_prim_nh2_to_D){
                            return TRUE;
                        }
                    } ITERATE_LIST_END;
                } ITERATE_NH_TYPE_END;
            } ITERATE_LIST_END;
        } ITERATE_NH_TYPE_END;
    } ITERATE_LIST_END;
    return FALSE;
}

static void
refine_route_backups(routes_t *route){

    nh_type_t nh;
    singly_ll_node_t *list_node1 = NULL;

    if(IS_DEFAULT_ROUTE(route))
        return;

    LEVEL level = route->level;
    if(is_independant_primary_next_hop_list(route)){
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "route %s/%u at %s has independant "
                "Primary Nexthops, All backup nexthops deleted", 
                route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, get_str_level(level));
        trace(instance->traceopts, ROUTE_CALCULATION_BIT);   
#endif
        ITERATE_NH_TYPE_BEGIN(nh){
            ROUTE_FLUSH_BACKUP_NH_LIST(route, nh);
        } ITERATE_NH_TYPE_END;
    }else{
        /*If route has more than one primary nexthops, cleanup all only-link protecting
         * backups*/
        unsigned int count = 0;
        singly_ll_node_t *prev_list_node = NULL;
        internal_nh_t *backup = NULL;

        ITERATE_NH_TYPE_BEGIN(nh){
            count += GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[nh]);
        } ITERATE_NH_TYPE_END;

        if(count > 1){
            ITERATE_NH_TYPE_BEGIN(nh){
                ITERATE_LIST_BEGIN2(route->backup_nh_list[nh], list_node1, prev_list_node){
                    backup = list_node1->data;
                    if(backup->lfa_type == LINK_PROTECTION_LFA                           ||
                            backup->lfa_type == LINK_PROTECTION_LFA_DOWNSTREAM           ||
                            backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA            ||
                            backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM ||
                            backup->lfa_type == LINK_PROTECTION_RLFA                     ||
                            backup->lfa_type == LINK_PROTECTION_RLFA_DOWNSTREAM          ||
                            backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA           ||
                            backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA_DOWNSTREAM){
#ifdef __ENABLE_TRACE__                        
                        sprintf(instance->traceopts->b, "\t ECMP : only link-protecting backup deleted : %s----%s---->%-s(%s(%s)) protecting link: %s", 
                                backup->oif->intf_name,
                                next_hop_type(*backup) == IPNH ? "IPNH" : "LSPNH",
                                next_hop_type(*backup) == IPNH ? next_hop_gateway_pfx(backup) : "",
                                backup->node ? backup->node->node_name : backup->rlfa->node_name,
                                backup->node ? backup->node->router_id : backup->rlfa->router_id, 
                                backup->protected_link->intf_name); 
                        trace(instance->traceopts, ROUTE_CALCULATION_BIT);
#endif
                        free(backup);
                        ITERATIVE_LIST_NODE_DELETE2(route->backup_nh_list[nh], list_node1, prev_list_node);
                    }
                } ITERATE_LIST_END2(route->backup_nh_list[nh], list_node1, prev_list_node);
            } ITERATE_NH_TYPE_END;
        }
    }
}

void
build_routing_table(spf_info_t *spf_info,
                    node_t *spf_root, LEVEL level){

    singly_ll_node_t *list_node = NULL,
                     *prefix_list_node = NULL;

    routes_t *route = NULL;
    prefix_t *prefix = NULL;
    spf_result_t *result = NULL,
                 *L1L2_result = NULL;

#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Entered ... spf_root : %s, Level : %s", spf_root->node_name, get_str_level(level));
    trace(instance->traceopts, ROUTE_INSTALLATION_BIT);
#endif
    
    mark_all_routes_stale(spf_info, level, UNICAST_T);

    /*Walk over the SPF result list computed in spf run
     * in the same order. Note that order of this list is :
     * most distant router from spf root is first*/


    ITERATE_LIST_BEGIN(spf_root->spf_run_result[level], list_node){

        result = (spf_result_t *)list_node->data;
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Node %s : processing result of %s, at level %s", 
            spf_root->node_name, result->node->node_name, get_str_level(level)); 
        trace(instance->traceopts, ROUTE_INSTALLATION_BIT);
#endif

        /*Iterate over all the prefixes of result->node for level 'level'*/


        if(level == LEVEL1                                               &&  /* If current spf run is Level1*/
                !IS_BIT_SET(spf_root->instance_flags, IGNOREATTACHED)    &&  /* If computing router is programmed to detect the L1L2 routers*/
                result->node->spf_info.spff_multi_area                   &&  /* if the router being inspected is L1L2 router*/
                !spf_root->spf_info.spff_multi_area){                        /* if the computing router is L1-only router*/

            L1L2_result = result;                                    /* Record the L1L2 router result*/
#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "Node %s : L1L2_result recorded - %s", 
                            spf_root->node_name, L1L2_result->node->node_name); 
            trace(instance->traceopts, ROUTE_INSTALLATION_BIT); 
#endif

            prefix_t default_prefix;
            memset(&default_prefix, 0, sizeof(prefix_t)); 
            UNSET_BIT(default_prefix.prefix_flags, PREFIX_DOWNBIT_FLAG);
            UNSET_BIT(default_prefix.prefix_flags, PREFIX_EXTERNABIT_FLAG);
            UNSET_BIT(default_prefix.prefix_flags, PREFIX_METRIC_TYPE_EXT);
            default_prefix.hosting_node = L1L2_result->node;
            default_prefix.metric = 0;
            default_prefix.mask = 0;
            default_prefix.level = LEVEL1;
            update_route(spf_info, L1L2_result, &default_prefix, LEVEL1, UNICAST_T, FALSE);
        }


        ITERATE_LIST_BEGIN(GET_NODE_PREFIX_LIST(result->node, level), prefix_list_node){

            prefix = (prefix_t *)prefix_list_node->data;  
            update_route(spf_info, result, prefix, level, UNICAST_T, TRUE);
        }ITERATE_LIST_END;

    } ITERATE_LIST_END;

    /*Iterate over all UPDATED routes and figured out which one needs to be updated
     * in RIB*/
    ITERATE_LIST_BEGIN(spf_info->routes_list[UNICAST_T], list_node){

        route = list_node->data;
        
        if(route->level != level)
            continue;

        if(route->install_state != RTE_STALE)
            refine_route_backups(route);

    } ITERATE_LIST_END;
}

void
spf_postprocessing(spf_info_t *spf_info, /* routes are stored globally*/
                   node_t *spf_root,     /* computing node which stores the result (list) of spf run*/
                   LEVEL level){         /*Level of spf run*/

    
    /*-----------------------------------------------------------------------------
     *  If this is L2 run, then set my spf_info_t->spff_multi_area bit, and schedule
     *  SPF L1 run to ensure L1 routes are uptodate before updating L2 routes
     *-----------------------------------------------------------------------------*/
#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Entered ... ");
    trace(instance->traceopts, ROUTE_CALCULATION_BIT);
#endif
       
    if(level == LEVEL2 && spf_info->spf_level_info[LEVEL1].version){
        /*If at least 1 SPF L1 run has been triggered*/
        
        spf_determine_multi_area_attachment(spf_info, spf_root);  
        /*Schedule level 1 spf run, just to make sure L1 routes are up
         *      * to date before building L2 routes*/
#ifdef __ENABLE_TRACE__        //
        //sprintf(instance->traceopts->b, "L2 spf run, triggering L1 SPF run first before building L2 routes"); TRACE();
        //spf_computation(spf_root, spf_info, LEVEL1, FULL_RUN);
    }

    build_routing_table(spf_info, spf_root, level);
    //start_route_installation(spf_info, level);
    delete_stale_routes(spf_info, level, UNICAST_T);
    if(is_node_spring_enabled(spf_root, level)){
        //start_spring_routes_installation(spf_info, level);
        update_node_segment_routes_for_remote(spf_info, level);
        delete_stale_routes(spf_info, level, SPRING_T);
    }
  
    /*Flush all Ribs before route installation*/ 
    flush_rib(spf_info->rib[INET_0], level);
    flush_rib(spf_info->rib[INET_3], level);
    flush_rib(spf_info->rib[MPLS_0], level);

    enhanced_start_route_installation(spf_info, level, UNICAST_T);
    if(is_node_spring_enabled(spf_root, level)){
        enhanced_start_route_installation(spf_info, level, SPRING_T);   
    }
}

internal_nh_t *
backup_selection_policy(routes_t *route){

   nh_type_t nh;
   singly_ll_node_t *list_node = NULL;
   internal_nh_t *backup = NULL;

   ITERATE_NH_TYPE_BEGIN(nh){
        ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node){
            backup = (internal_nh_t *)(list_node->data);         
            if(is_internal_nh_t_empty(*backup))
                continue;
            return backup;
        } ITERATE_LIST_END;     
   } ITERATE_NH_TYPE_END;
   return NULL;
}


void
show_internal_routing_tree(node_t *node, char *prefix, char mask, rtttype_t rt_type){

        singly_ll_node_t *list_node = NULL;
        routes_t *route = NULL;
        char subnet[PREFIX_LEN_WITH_MASK + 1];
        nh_type_t nh;
        unsigned int j = 0,
                     total_nx_hops = 0;

        printf("Internal Routes : %s\n", rt_type == UNICAST_T ? "Unicast" : "Spring");
        printf("Destination           Version        Metric       Level   Gateway            Nxt-Hop                     OIF           protection    Backup Score\n");
        printf("--------------------------------------------------------------------------------------------------------------------------------------------------\n");

        ITERATE_LIST_BEGIN(node->spf_info.routes_list[rt_type], list_node){

            route = list_node->data;

            /*filter*/
            if(prefix){
                if(!(strncmp(prefix, route->rt_key.u.prefix.prefix, PREFIX_LEN) == 0 &&
                            mask == route->rt_key.u.prefix.mask))
                    continue;
            }
            memset(subnet, 0, PREFIX_LEN_WITH_MASK + 1);
            sprintf(subnet, "%s/%d", route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask);

            /*handling local prefixes*/

            if(GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[IPNH]) == 0 &&
                    GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[LSPNH]) == 0){

                sprintf(subnet, "%s/%d", route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask);
                printf("%-20s      %-4d        %-3d (%-3s)     %-2d    %-15s    %-s|%-8s   %-12s      %-16s\n",
                        subnet, route->version, route->spf_metric,
                        IS_BIT_SET(route->flags, PREFIX_METRIC_TYPE_EXT) ? "EXT" : "INT",
                        route->level,
                        "Direct", "",
                        "--", "", "");
                if(prefix)
                    return;
                else
                    continue;
            }

            printf("%-20s      %-4d        %-3d (%-3s)     %-2d    ",
                    subnet, route->version, route->spf_metric,
                    IS_BIT_SET(route->flags, PREFIX_METRIC_TYPE_EXT) ? "EXT" : "INT",
                    route->level);

            ITERATE_NH_TYPE_BEGIN(nh){
                total_nx_hops += GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[nh]);
            } ITERATE_NH_TYPE_END;

            singly_ll_node_t *list_node = NULL;
            internal_nh_t *nexthop = NULL;

            ITERATE_NH_TYPE_BEGIN(nh){
                ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node){
                    nexthop = list_node->data;
                    printf("%-15s    %-s|%-22s   %-26s\n",
                            nh == IPNH ? next_hop_gateway_pfx(nexthop) : "--",
                            nexthop->node->node_name,
                            next_hop_type(*nexthop) == IPNH ? "IPNH" : "LSPNH",
                            next_hop_oif_name(*nexthop));
                    if(j < total_nx_hops -1)
                        printf("%-20s      %-4s        %-3s  %-3s      %-2s    ", "","","","","");
                    j++;
                } ITERATE_LIST_END;
            } ITERATE_NH_TYPE_END;

            /*print the back up here*/
            ITERATE_NH_TYPE_BEGIN(nh){
                ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node){
                    nexthop = list_node->data;
                    printf("%-20s      %-4s        %-3s  %-3s      %-2s    ", "","","","","");
                    nh = next_hop_type(*nexthop);
                    /*print the back as per its type*/
                    switch(nh){
                        case IPNH:
                            printf("%-15s    %s|%-22s   %-12s    %-10s       %-5u\n",
                                    next_hop_gateway_pfx(nexthop),
                                    nexthop->node->node_name,
                                    "IPNH",
                                    next_hop_oif_name(*nexthop),
                                    backup_next_hop_protection_name(*nexthop), 5000);
                            break;
                        case LSPNH:
                            printf("%-15s    %s->%s|%-17s   %-12s    %-10s       %-5u\n",
                                    "",
                                    is_internal_backup_nexthop_rsvp(nexthop) ? "RSVP" : \
                                               rt_type == UNICAST_T ? "LDP" : "SPRING",
                                    is_internal_backup_nexthop_rsvp(nexthop) ?  nexthop->node->node_name :
                                    nexthop->rlfa->node_name,
                                    nexthop->rlfa->router_id,
                                    next_hop_oif_name(*nexthop),
                                    backup_next_hop_protection_name(*nexthop), 6000);
                            break;
                        default:
                            assert(0);
                    }
                } ITERATE_LIST_END;
            } ITERATE_NH_TYPE_END;
                if(prefix)
                    return;
        }ITERATE_LIST_END;
}


/*SR related APIs*/
/*It is essentially a routing table building routine for SR node/prefix segment routes*/
void
update_node_segment_routes_for_remote(spf_info_t *spf_info, LEVEL level){

    singly_ll_node_t *list_node = NULL;
    spf_result_t *result = NULL;
    glthread_t *curr = NULL;
    prefix_sid_subtlv_t *prefix_sid = NULL;
    routes_t *route = NULL;
    node_t *spf_root = GET_SPF_INFO_NODE(spf_info, level),
           *D_res = NULL;

    sprintf(instance->traceopts->b, "Entered ... spf_root : %s, Level : %s", 
#endif
        spf_root->node_name, get_str_level(level));
    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);

    mark_all_routes_stale(spf_info, level, SPRING_T);

    ITERATE_LIST_BEGIN(spf_root->spf_run_result[level], list_node){
        result = list_node->data;
        D_res = result->node;
        
        if(!is_node_spring_enabled(D_res, level)){
#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "Node : %s : skipping Dest %s at %s, not SPRING enabled",
                spf_root->node_name, D_res->node_name, get_str_level(level));
            trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
            continue;
        }
        
        /*Iterate over all prefix SIDs advertised by this node*/
        ITERATE_GLTHREAD_BEGIN(&D_res->prefix_sids_thread_lst[level], curr){
            
            prefix_sid = glthread_to_prefix_sid(curr);
            assert(prefix_sid->prefix);
            if(!IS_PREFIX_SR_ACTIVE(prefix_sid->prefix)){
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : skipping prefix %s/%u, hosting node : %s at %s, conflicting prefix",
                    spf_root->node_name, STR_PREFIX(prefix_sid->prefix), PREFIX_MASK(prefix_sid->prefix), 
                    D_res->node_name, get_str_level(level)); 
                trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
                continue;
            }
            
            update_route(spf_info, result, prefix_sid->prefix, level, SPRING_T, TRUE);

        } ITERATE_GLTHREAD_END(&D_res->prefix_sids_thread_lst[level], curr);
    } ITERATE_LIST_END;
    
    /*Refine work for backups - delete backups for ECMP routes*/
    ITERATE_LIST_BEGIN(spf_info->routes_list[SPRING_T], list_node){
        route = list_node->data;
        if(route->level != level)
            continue;

        refine_route_backups(route);

        if(route->install_state != RTE_UPDATED)
            continue;
    } ITERATE_LIST_END;

    ITERATE_LIST_BEGIN(spf_info->routes_list[SPRING_T], list_node){
        route = list_node->data;
        if(route->install_state == RTE_STALE)
            continue;
        springify_unicast_route(spf_root, list_node->data);
    } ITERATE_LIST_END;
}

static void
ldpify_rlfa_nexthop(internal_nh_t *nxthop,
        char *prefix, char mask){

    mpls_label_t mpls_ldp_label = 0;
    if(!nxthop->proxy_nbr->ldp_config.is_enabled)
        return;

    mpls_ldp_label = get_ldp_label_binding(nxthop->proxy_nbr, prefix, mask);
    if(!mpls_ldp_label){
        printf("Error : Could not get ldp label for prefix %s/%u from node %s",
                prefix, mask, nxthop->proxy_nbr->node_name);
        return;
    }
    nxthop->mpls_label_out[0] = mpls_ldp_label;
    nxthop->stack_op[0] = PUSH;
}


static void
enhanced_start_route_installation_unicast(spf_info_t *spf_info, LEVEL level){

    /*Unicast (IGPs) protocols installs the routes in inet.0 and inet.3 tables
     * only. Flush both the tables first*/

    singly_ll_node_t *list_node = NULL,
    *list_node2 = NULL;

    routes_t *route = NULL;
    nh_type_t nh;
    internal_nh_t *nxthop = NULL;
    internal_un_nh_t *un_nxthop = NULL;
    boolean rc = FALSE;
    rt_key_t rt_key;
    boolean is_local_route = FALSE;

    ITERATE_LIST_BEGIN(spf_info->routes_list[UNICAST_T], list_node){

        route = list_node->data;
        if(route->level != level) continue;

        assert(route->install_state != RTE_STALE);

        memset(&rt_key, 0, sizeof(rt_key_t));
        strncpy(RT_ENTRY_PFX(&rt_key), route->rt_key.u.prefix.prefix, PREFIX_LEN);
        RT_ENTRY_MASK(&rt_key) = route->rt_key.u.prefix.mask;

        /*Handle local routes*/
        is_local_route = is_route_local(route);

        if(is_local_route){
            inet_0_rt_un_route_install_nexthop(spf_info->rib[INET_0], &rt_key, level, NULL);
            inet_3_rt_un_route_install_nexthop(spf_info->rib[INET_3], &rt_key, level, NULL);
            continue;
        }

        /*Install primary nexthop first. Primary nexthops are inet.0 routes Or RSVP routes (inet.3)*/
        ITERATE_NH_TYPE_BEGIN(nh){
            ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node2){
                nxthop = list_node2->data;
                rc = FALSE;
                if(nh == IPNH){
                    un_nxthop = inet_0_unifiy_nexthop(nxthop, IGP_PROTO);                
                    rc = inet_0_rt_un_route_install_nexthop(spf_info->rib[INET_0], &rt_key, level, un_nxthop);
                    if(rc == FALSE){
                        free_un_nexthop(un_nxthop);
                    }
                }
                else{ /*It is RSVP LSP nexthop, which needs to be installed in inet.3 table*/
                    if(is_node_best_prefix_originator(nxthop->node, route)){
                        /* RSVP nexthop should not be installed in inet.3 table. Instead it should
                         * be installed in inet.0 table. We will
                         * revisit this when we shall support RSVP nexthops properly*/
                    }
                    else{
                        un_nxthop = inet_3_unifiy_nexthop(nxthop, IGP_PROTO, IPV4_LDP_NH, route);
                        rc = inet_3_rt_un_route_install_nexthop(spf_info->rib[INET_3], &rt_key, level, un_nxthop);
                        if(rc == FALSE){
                            free_un_nexthop(un_nxthop);
                        }
                    }
                }
            } ITERATE_LIST_END;
        } ITERATE_NH_TYPE_END;


        /*Install backup nexthop now. Backup nexthops are inet.0 routes Or RSVP/LDP routes (inet.3)*/
        ITERATE_NH_TYPE_BEGIN(nh){
            ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node2){
                nxthop = list_node2->data;
                rc = FALSE;
                if(nh == IPNH){
                    un_nxthop = inet_0_unifiy_nexthop(nxthop, IGP_PROTO);                
                    rc = inet_0_rt_un_route_install_nexthop(spf_info->rib[INET_0], &rt_key, level, un_nxthop);
                    if(rc == FALSE){
                        free_un_nexthop(un_nxthop);
                    }
                }
                else{ /*backup is either RSVP or LDP nexthop*/
                    if(is_internal_backup_nexthop_rsvp(nxthop)) {
                        /*ToDo*/

                    }else{
                        /*LDP backup nexthop(RLFAs)*/
                        prefix_t *prefix = ROUTE_GET_BEST_PREFIX(route);
                        ldpify_rlfa_nexthop(nxthop, prefix->prefix, prefix->mask);
                        /*Could not get LDP label, skip installation of this LDP nexthop*/
                        if(IS_INTERNAL_NH_MPLS_STACK_EMPTY(nxthop))
                            continue;
                        un_nxthop = inet_3_unifiy_nexthop(nxthop, IGP_PROTO, IPV4_LDP_NH, route);
                        if(IS_BIT_SET(un_nxthop->flags, IPV4_LDP_NH))
                            rc = inet_3_rt_un_route_install_nexthop(spf_info->rib[INET_3], &rt_key, level, un_nxthop);
                        else if(IS_BIT_SET(un_nxthop->flags, IPV4_NH))
                            rc = inet_0_rt_un_route_install_nexthop(spf_info->rib[INET_0], &rt_key, level, un_nxthop);
                        if(rc == FALSE){
                            free_un_nexthop(un_nxthop);
                        }
                    }
                }
            } ITERATE_LIST_END;
        } ITERATE_NH_TYPE_END;
    } ITERATE_LIST_END;
}

static boolean
is_route_have_backup_protection(routes_t *route, edge_end_t *protected_link){

    edge_t *edge = GET_EGDE_PTR_FROM_FROM_EDGE_END(protected_link);

    if(!IS_LEVEL_SET(edge->level, route->level))
        return FALSE;

    nh_type_t nh;
    singly_ll_node_t *list_node = NULL;
    internal_nh_t *nxthop = NULL;

    ITERATE_NH_TYPE_BEGIN(nh){
        ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node){
            nxthop = list_node->data;
            if(nxthop->protected_link == protected_link)
                return TRUE;  
        } ITERATE_LIST_END;
    } ITERATE_NH_TYPE_END;
    return FALSE;
}

static void
enhanced_start_route_installation_spring(spf_info_t *spf_info, LEVEL level){

    /* (L-IGP) protocol installs the routes in inet.3 and mpls.0 tables
     * only. Flush both the tables first*/

    singly_ll_node_t *list_node = NULL,
                     *list_node2 = NULL;

    routes_t *route = NULL;
    nh_type_t nh;
    internal_nh_t *nxthop = NULL;
    internal_un_nh_t *un_nxthop = NULL;
    boolean rc = FALSE;
    rt_key_t rt_key;

    ITERATE_LIST_BEGIN(spf_info->routes_list[SPRING_T], list_node){
        
        route = list_node->data;
        if(route->level != level) continue;
        
        assert(route->install_state != RTE_STALE);

        /*First install primary routes in inet.3 table*/
        memset(&rt_key, 0, sizeof(rt_key_t));
        strncpy(RT_ENTRY_PFX(&rt_key), route->rt_key.u.prefix.prefix, PREFIX_LEN);
        RT_ENTRY_MASK(&rt_key) = route->rt_key.u.prefix.mask;
       
        /*Install springified IPV4 routes in inet.3 table. RSVP LSP Nexthops 
         * should not be springified in the first place*/ 
        ITERATE_LIST_BEGIN(route->primary_nh_list[IPNH], list_node2){
            nxthop = list_node2->data;
            if(!IS_INTERNAL_NH_SPRINGIFIED(nxthop)){
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "node : %s : route %s/%u, at %s nexthop (%s)%s not installed, not spring capable", 
                GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
                get_str_level(level), next_hop_oif_name(*nxthop), nxthop->node ? nxthop->node->node_name :
                nxthop->proxy_nbr->node_name);
                trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
                continue;
            }
            rc = FALSE;
            un_nxthop = inet_3_unifiy_nexthop(nxthop, L_IGP_PROTO, IPV4_SPRING_NH, route);
            if(IS_BIT_SET(un_nxthop->flags, IPV4_SPRING_NH))
                rc = inet_3_rt_un_route_install_nexthop(spf_info->rib[INET_3], &rt_key, level, un_nxthop);
            else if(IS_BIT_SET(un_nxthop->flags, IPV4_NH))
                rc = inet_0_rt_un_route_install_nexthop(spf_info->rib[INET_0], &rt_key, level, un_nxthop);
            if(rc == FALSE){
                free_un_nexthop(un_nxthop);
            }
        } ITERATE_LIST_END;

        /* RSVP nexthop Should have been installed in inet.3 table in 
         * enhanced_start_route_installation_unicast(). So no need to
         * do it again during spring route installation*/

        /*Spring Backups. Install ipv4 springified backups in inet.3 table*/
        ITERATE_LIST_BEGIN(route->backup_nh_list[IPNH], list_node2){
            nxthop = list_node2->data;
            if(!IS_INTERNAL_NH_SPRINGIFIED(nxthop)){
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "node : %s : route %s/%u, at %s backup nexthop (%s)%s not installed not spring capable", 
                GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
                get_str_level(level), next_hop_oif_name(*nxthop), nxthop->node ? nxthop->node->node_name :
                nxthop->proxy_nbr->node_name);
                trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
                continue;
            }
            rc = FALSE;
            un_nxthop = inet_3_unifiy_nexthop(nxthop, L_IGP_PROTO, IPV4_SPRING_NH, route);
            if(IS_BIT_SET(un_nxthop->flags, IPV4_SPRING_NH))
                rc = inet_3_rt_un_route_install_nexthop(spf_info->rib[INET_3], &rt_key, level, un_nxthop);
            else if(IS_BIT_SET(un_nxthop->flags, IPV4_NH))
                rc = inet_0_rt_un_route_install_nexthop(spf_info->rib[INET_0], &rt_key, level, un_nxthop);
            if(rc == FALSE){
                free_un_nexthop(un_nxthop);
            }
        } ITERATE_LIST_END;
        
        /*Nw do LSP backups - which could be RSVP backups Or LDP(RLFA) backups*/
        ITERATE_LIST_BEGIN(route->backup_nh_list[LSPNH], list_node2){
            nxthop = list_node2->data;
            if(is_internal_backup_nexthop_rsvp(nxthop))
                continue; /*ToDo : Support RSVP later . . . */
            if(!IS_INTERNAL_NH_SPRINGIFIED(nxthop) || !is_node_spring_enabled(nxthop->rlfa, level)){
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "node : %s : route %s/%u, at %s backup nexthop (%s)%s not installed not spring capable", 
                GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
                get_str_level(level), next_hop_oif_name(*nxthop), nxthop->node ? nxthop->node->node_name :
                nxthop->proxy_nbr->node_name);
                trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
                continue;
            }
            rc = FALSE;
            /*springified RLFA nexthops*/
            un_nxthop = inet_3_unifiy_nexthop(nxthop, L_IGP_PROTO, IPV4_SPRING_NH, route);
            if(IS_BIT_SET(un_nxthop->flags, IPV4_SPRING_NH))
                rc = inet_3_rt_un_route_install_nexthop(spf_info->rib[INET_3], &rt_key, level, un_nxthop);
            else if(IS_BIT_SET(un_nxthop->flags, IPV4_NH))
                rc = inet_0_rt_un_route_install_nexthop(spf_info->rib[INET_0], &rt_key, level, un_nxthop);
            if(rc == FALSE){
                free_un_nexthop(un_nxthop);
            }
        } ITERATE_LIST_END;


        /*Now install all primary/backups routes in mpls_0 table*/
        RT_ENTRY_LABEL(&rt_key) = route->rt_key.u.label; 

        ITERATE_NH_TYPE_BEGIN(nh){
            ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node2){
                nxthop = list_node2->data;
                if(!IS_INTERNAL_NH_SPRINGIFIED(nxthop)){
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "node : %s : route %s/%u, at %s primarynexthop (%s)%s not installed, not spring capable", 
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
                            get_str_level(level), next_hop_oif_name(*nxthop), nxthop->node ? nxthop->node->node_name :
                            nxthop->proxy_nbr->node_name);
                    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
                    continue;
                }
                rc = FALSE;
                un_nxthop = mpls_0_unifiy_nexthop(nxthop, L_IGP_PROTO);
                rc = mpls_0_rt_un_route_install_nexthop(spf_info->rib[MPLS_0], &rt_key, level, un_nxthop);
                if(rc == FALSE){
                    free_un_nexthop(un_nxthop);
                }
            } ITERATE_LIST_END;
        } ITERATE_NH_TYPE_END;

        ITERATE_NH_TYPE_BEGIN(nh){
            ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node2){
                nxthop = list_node2->data;
                if(is_internal_backup_nexthop_rsvp(nxthop))
                    continue;
                if(!IS_INTERNAL_NH_SPRINGIFIED(nxthop) || (nxthop->rlfa && !is_node_spring_enabled(nxthop->rlfa, level))){
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "node : %s : route %s/%u, at %s backup nexthop (%s)%s not installed, not spring capable", 
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask, 
                            get_str_level(level), next_hop_oif_name(*nxthop), nxthop->node ? nxthop->node->node_name :
                            nxthop->proxy_nbr->node_name);
                    trace(instance->traceopts, SPRING_ROUTE_CAL_BIT);
#endif
                    continue;
                }
                rc = FALSE;
                un_nxthop = mpls_0_unifiy_nexthop(nxthop, L_IGP_PROTO);
                if(nh == LSPNH){
                    /*In case if RLFA is also a destination, then mpls label stack depth would only be 1.
                     * The below stack modificiation need not done*/
                    if(un_nxthop->nh.mpls0_nh.mpls_label_out[1] && 
                            un_nxthop->nh.mpls0_nh.stack_op[1] != STACK_OPS_UNKNOWN){
                        /*Means, RLFA is a transient router to destination, and RLFA itself is not
                         * a destination*/
                        un_nxthop->nh.mpls0_nh.stack_op[1] = SWAP;
                    }
                    else{
                        /*if RLFA it self is a destination*/
                        un_nxthop->nh.mpls0_nh.stack_op[0] = SWAP;
                    }
                }
                rc = mpls_0_rt_un_route_install_nexthop(spf_info->rib[MPLS_0], &rt_key, level, un_nxthop);
                if(rc == FALSE){
                    free_un_nexthop(un_nxthop);
                }
            } ITERATE_LIST_END;
        } ITERATE_NH_TYPE_END;
    } ITERATE_LIST_END;
}

static void
enhanced_start_route_installation(spf_info_t *spf_info,
                         LEVEL level, rtttype_t rtttype){

    
    switch(rtttype){
        case UNICAST_T:
            enhanced_start_route_installation_unicast(spf_info, level);
            break;
        case SPRING_T:
            enhanced_start_route_installation_spring(spf_info, level);
            break;
         default:
            assert(0);
    }
}

void 
flush_routes(node_t *node){

}

