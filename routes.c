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
#include "rttable.h"
#include "advert.h"
#include "spftrace.h"

extern instance_t *instance;

extern void
spf_computation(node_t *spf_root,
                spf_info_t *spf_info,
                LEVEL level, spf_type_t spf_type);
extern int
instance_node_comparison_fn(void *_node, void *input_node_name);

void
install_route_in_rib(spf_info_t *spf_info,
        LEVEL level, routes_t *route);

#if 0
THREAD_NODE_TO_STRUCT(prefix_t,
        like_prefix_thread,
        get_prefix_from_like_prefix_thread);
#endif

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

        int_nxt_hop = calloc(1, sizeof(internal_nh_t));
        copy_internal_nh_t(result->next_hop[nh][i], *int_nxt_hop);
        singly_ll_add_node_by_val(route->primary_nh_list[nh], int_nxt_hop);
        sprintf(instance->traceopts->b, "route : %s/%u primary next hop is merged with %s's next hop node %s", 
                     route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                     result->next_hop[nh][i].node->node_name); 
        trace(instance->traceopts, ROUTE_CALCULATION_BIT);
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
    singly_ll_node_t *list_node = NULL;

    /*clean LFAs*/
    if(route->ecmp_dest_count == 2){
            ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node){
                backup = list_node->data;
                if(backup->lfa_type == LINK_PROTECTION_LFA                           ||
                        backup->lfa_type == LINK_PROTECTION_LFA_DOWNSTREAM           ||
                        backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA            ||
                        backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM ||
                        backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA           ||
                        backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA_DOWNSTREAM){
                    singly_ll_remove_node(route->backup_nh_list[nh], list_node);
                    sprintf(instance->traceopts->b, "\t ECMP : LFA backup deleted : %s----%s---->%-s(%s(%s)) protecting link: %s", 
                            backup->oif->intf_name,
                            next_hop_type(*backup) == IPNH ? "IPNH" : "LSPNH",
                            next_hop_type(*backup) == IPNH ? next_hop_gateway_pfx(backup) : "",
                            backup->node->node_name, backup->node->router_id,
                            backup->protected_link->intf_name); 
                    trace(instance->traceopts, ROUTE_CALCULATION_BIT);
                    free(backup);
                    free(list_node);
                }
            } ITERATE_LIST_END;
    }

    for( i = 0; i < MAX_NXT_HOPS ; i++){
        
        backup = &result->node->backup_next_hop[route->level][nh][i];
        if(is_internal_nh_t_empty(*backup)) break;
        /*If route has ECMP, then no need to collect Link protecting LFAs from
         * multiple destinations*/
        if(route->ecmp_dest_count >= 2                                    &&
            (backup->lfa_type == LINK_PROTECTION_LFA                      ||
             backup->lfa_type == LINK_PROTECTION_LFA_DOWNSTREAM           ||
             backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA            ||
             backup->lfa_type == BROADCAST_LINK_PROTECTION_LFA_DOWNSTREAM ||
             backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA           ||
             backup->lfa_type == BROADCAST_LINK_PROTECTION_RLFA_DOWNSTREAM)){
            sprintf(instance->traceopts->b, "\t ECMP : LFA backup dropped : %s----%s---->%-s(%s(%s)) protecting link: %s", 
                    backup->oif->intf_name,
                    next_hop_type(*backup) == IPNH ? "IPNH" : "LSPNH",
                    next_hop_type(*backup) == IPNH ? next_hop_gateway_pfx(backup) : "",
                    backup->node->node_name, backup->node->router_id,
                    backup->protected_link->intf_name); 
            trace(instance->traceopts, ROUTE_CALCULATION_BIT);
            continue;
        }
        int_nxt_hop = calloc(1, sizeof(internal_nh_t));
        copy_internal_nh_t(result->node->backup_next_hop[route->level][nh][i], *int_nxt_hop);
        singly_ll_add_node_by_val(route->backup_nh_list[nh], int_nxt_hop);
        sprintf(instance->traceopts->b, "route : %s/%u backup next hop is merged with %s's next hop node %s", 
                     route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                     result->node->backup_next_hop[route->level][nh][i].node->node_name); 
        trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
    }
    assert(GET_NODE_COUNT_SINGLY_LL(route->backup_nh_list[nh]) <= MAX_NXT_HOPS);
}

void
route_set_key(routes_t *route, char *ipv4_addr, char mask){

    apply_mask(ipv4_addr, mask, route->rt_key.prefix);
    route->rt_key.prefix[PREFIX_LEN] = '\0';
    route->rt_key.mask = mask;
}

void
free_route(routes_t *route){

    if(!route)  return;
    nh_type_t nh; 
    singly_ll_node_t* list_node = NULL; 
    route->hosting_node = 0;
    
    ITERATE_NH_TYPE_BEGIN(nh){
        ROUTE_FLUSH_PRIMARY_NH_LIST(route, nh);
        free(route->primary_nh_list[nh]);
        route->primary_nh_list[nh] = 0;
        ROUTE_FLUSH_BACKUP_NH_LIST(route, nh);
        free(route->backup_nh_list[nh]);
        route->backup_nh_list[nh] = 0;
    } ITERATE_NH_TYPE_END;

    ITERATE_LIST_BEGIN(route->like_prefix_list, list_node){
        
        free(list_node->data);
        list_node->data = NULL;
    } ITERATE_LIST_END;
    
    delete_singly_ll(route->like_prefix_list);
    free(route->like_prefix_list);
    route->like_prefix_list = NULL;
    free(route);
}

/* Store only prefix related info in rttable_entry_t*/
void
prepare_new_rt_entry_template(spf_info_t *spf_info, rttable_entry_t *rt_entry_template,
        routes_t *route, unsigned int version){

    nh_type_t nh;
    strncpy(rt_entry_template->dest.prefix, route->rt_key.prefix, PREFIX_LEN + 1);
    rt_entry_template->dest.prefix[PREFIX_LEN] = '\0';
    rt_entry_template->dest.mask = route->rt_key.mask;
    rt_entry_template->version = version;
    rt_entry_template->cost = route->spf_metric;
    rt_entry_template->level = route->level;
    rt_entry_template->flags = route->flags;

    ITERATE_NH_TYPE_BEGIN(nh){
        rt_entry_template->primary_nh_count[nh] =
            GET_NODE_COUNT_SINGLY_LL(route->primary_nh_list[nh]);
    } ITERATE_NH_TYPE_END;
}

void
prepare_new_nxt_hop_template(node_t *computing_node,
        internal_nh_t *nxt_hop_node,
        nh_t *nh_template,
        LEVEL level, nh_type_t nh){

    assert(nh_template);
    assert(computing_node);

    edge_end_t *oif = NULL;

    if(nxt_hop_node){
        if(nxt_hop_node->node){
            /*If nxt_hop_node is IPNH Or FA*/
            strncpy(nh_template->nh_name, nxt_hop_node->node->node_name, NODE_NAME_SIZE);
        }
        else{
            /*RLFA case*/
            strncpy(nh_template->nh_name, nxt_hop_node->rlfa->node_name, NODE_NAME_SIZE);
        }
    }
    else{
        /*if nxt_hop_node is NULL*/
        strncpy(nh_template->nh_name, computing_node->node_name, NODE_NAME_SIZE);
    }
    
    nh_template->nh_name[NODE_NAME_SIZE - 1] = '\0';
    if(nxt_hop_node && nxt_hop_node->protected_link){
        strncpy(nh_template->protected_link, nxt_hop_node->protected_link->intf_name, IF_NAME_SIZE-1);
        nh_template->protected_link[IF_NAME_SIZE-1] = '\0';
    }

    /*Get the router id if nxt_hop_node is Forward Adjacency Or LDP Nexthop*/
    if(nxt_hop_node){
        if(nxt_hop_node->node && nh == LSPNH){
            /*Forward Adjacency*/
            strncpy(nh_template->router_id, nxt_hop_node->node->router_id, PREFIX_LEN);
        }
        else if(!nxt_hop_node->node &&
                    nxt_hop_node->rlfa){
            /*LDP next hop*/
            strncpy(nh_template->router_id, nxt_hop_node->rlfa->router_id, PREFIX_LEN);
        }
    }
    /*oif can be NULL if computing_node == nxt_hop_node. It will happen
     * for local prefixes */
    oif = nxt_hop_node ? nxt_hop_node->oif : NULL;
    nh_template->nh_type = nh;

    if(!oif){
        strncpy(nh_template->gwip, "Direct" , PREFIX_LEN + 1);
        nh_template->gwip[PREFIX_LEN] = '\0';
        return;
    }

    strncpy(nh_template->oif, oif ? oif->intf_name : "NA" , IF_NAME_SIZE);
    nh_template->oif[IF_NAME_SIZE -1] = '\0';

    if(nh_template->nh_type == IPNH){
        strncpy(nh_template->gwip, nxt_hop_node ? nxt_hop_node->gw_prefix : "0.0.0.0", PREFIX_LEN);
        nh_template->gwip[PREFIX_LEN] = '\0';
    }

    if(!nxt_hop_node) return;

    if(nxt_hop_node->proxy_nbr){
        strncpy(nh_template->proxy_nbr_name, nxt_hop_node->proxy_nbr->node_name, NODE_NAME_SIZE);
        nh_template->proxy_nbr_name[NODE_NAME_SIZE - 1] = '\0';
    }
#if 1
    if(nxt_hop_node->rlfa){
        strncpy(nh_template->rlfa_name, nxt_hop_node->rlfa->node_name, NODE_NAME_SIZE);
        nh_template->rlfa_name[NODE_NAME_SIZE - 1] = '\0';
    }
#endif
    nh_template->ldplabel = nxt_hop_node->ldplabel;
}

routes_t *
search_route_in_spf_route_list(spf_info_t *spf_info, 
                                prefix_t *prefix, LEVEL level/*Unused*/){

    routes_t *route = NULL;
    singly_ll_node_t* list_node = NULL;
    char prefix_with_mask[PREFIX_LEN + 1];
    apply_mask(prefix->prefix, prefix->mask, prefix_with_mask);
    prefix_with_mask[PREFIX_LEN] = '\0';
    ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){
        route = list_node->data;
        if(strncmp(route->rt_key.prefix, prefix_with_mask, PREFIX_LEN) == 0 &&
                (route->rt_key.mask == prefix->mask))
            return route;    
    }ITERATE_LIST_END;
    return NULL;
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

static boolean
is_same_next_hop(internal_nh_t *nxt_hop, nh_t *nh){

    /*Compare the fields of internal nexthop with what is installed in routing table*/

    nh_type_t nh_type;
   
    /*If routing table entry do not have any backups, but applcation
     * route has new backups*/ 
    if(nxt_hop && strlen(nh->oif) == 0)
        return FALSE;

    if(strncmp(next_hop_oif_name((*nxt_hop)), nh->oif, IF_NAME_SIZE))
        return FALSE;
    
    /*For RLFA nexthops: nxt_hop->node is NULL*/
    if(!nxt_hop->node){
        if(strncmp(nxt_hop->proxy_nbr->node_name, nh->proxy_nbr_name, NODE_NAME_SIZE))
            return FALSE;
        if(strncmp(nxt_hop->rlfa->node_name, nh->rlfa_name, NODE_NAME_SIZE))
            return FALSE;
    }

    if(nxt_hop->node){
        if(strncmp(nxt_hop->node->node_name, nh->nh_name, NODE_NAME_SIZE))
            return FALSE;
    }
    
    nh_type = next_hop_type((*nxt_hop));

    if(nh_type != nh->nh_type)
        return FALSE;

    if(nh_type == LSPNH)
        return TRUE;
        
    if(strncmp(nxt_hop->gw_prefix, nh->gwip, PREFIX_LEN))
        return FALSE;

    /*Add more fields to compare*/
    if(nxt_hop->proxy_nbr){
        if(strncmp(nxt_hop->proxy_nbr->node_name, nh->proxy_nbr_name, NODE_NAME_SIZE))
            return FALSE;
    }

    if(nxt_hop->rlfa){
        if(strncmp(nxt_hop->rlfa->node_name, nh->rlfa_name, NODE_NAME_SIZE))
            return FALSE;
    }
    return TRUE;
}

static boolean
route_rib_same_next_hops(rttable_entry_t *rt_entry, /*We need to compare the nexthop array of rt_entry and route*/
        routes_t *route){

    unsigned int i = 0;
    singly_ll_node_t* list_node = NULL;
    nh_type_t nh;
    internal_nh_t *nxt_hop = NULL;

    ITERATE_NH_TYPE_BEGIN(nh){
        ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node){
            nxt_hop = list_node->data; /*This is nbr node of computing node*/
            if(is_same_next_hop(nxt_hop, &rt_entry->primary_nh[nh][i++]) == FALSE)
                return FALSE;
        }ITERATE_LIST_END;
    }ITERATE_NH_TYPE_END;

    i = 0;
    ITERATE_NH_TYPE_BEGIN(nh){
        ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node){
            nxt_hop = list_node->data; /*This is nbr node of computing node*/
            if(is_same_next_hop(nxt_hop, &rt_entry->backup_nh[i++]) == FALSE)
                return FALSE;
        }ITERATE_LIST_END;
    }ITERATE_NH_TYPE_END;
    return TRUE;
}

static boolean
is_changed_route(spf_info_t *spf_info, 
                 rttable_entry_t *rt_entry, /*route from RIB*/
                 routes_t *route,           /*route from our SPF*/
                 spf_type_t spf_type, LEVEL level){

   assert(route->install_state == RTE_UPDATED);
   switch(spf_type){
       case FULL_RUN:
       case PRC_RUN:
               if((strncmp(rt_entry->dest.prefix, route->rt_key.prefix, PREFIX_LEN + 1) == 0)   &&
                       (rt_entry->dest.mask == route->rt_key.mask)                              &&
                       (rt_entry->primary_nh_count[IPNH] == ROUTE_GET_PR_NH_CNT(route, IPNH))   && 
                       (rt_entry->primary_nh_count[LSPNH] == ROUTE_GET_PR_NH_CNT(route, LSPNH)) &&
                       (rt_entry->backup_nh_count == GET_NODE_COUNT_SINGLY_LL(route->backup_nh_list[IPNH])
                                + GET_NODE_COUNT_SINGLY_LL(route->backup_nh_list[LSPNH]))       && 
                       (route_rib_same_next_hops(rt_entry, route) == TRUE))
                   return FALSE;
               return TRUE; 
           break;
       case FORWARD_RUN:
           break;
       default:
           ; 
   }
   return TRUE;
}

static unsigned int 
delete_stale_routes(spf_info_t *spf_info, LEVEL level){

   singly_ll_node_t* list_node = NULL;
   routes_t *route = NULL;
   unsigned int i = 0;

   sprintf(instance->traceopts->b, "Deleting Stale Routes"); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
   ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){
           
       route = list_node->data;
       if(route->install_state == RTE_STALE && IS_LEVEL_SET(route->level, level)){
        sprintf(instance->traceopts->b, "route : %s/%u is STALE for Level%d, deleted", route->rt_key.prefix, 
                        route->rt_key.mask, level); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
        i++;
        ROUTE_DEL_FROM_ROUTE_LIST(spf_info, route);
        free_route(route);
        route = NULL;
       }
   }ITERATE_LIST_END;

   return i;
}


static void
mark_all_routes_stale_except_direct_routes(spf_info_t *spf_info, LEVEL level){

   singly_ll_node_t* list_node = NULL,
                     *list_node1 = NULL;
   routes_t *route = NULL;
   prefix_t *prefix = NULL;
   
   ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){
           
       route = list_node->data;
       
       if(!IS_LEVEL_SET(route->level, level))
           continue;
      
       /*Do not stale direct or leaked routes*/ 
       if(route->hosting_node != GET_SPF_INFO_NODE(spf_info, level))
           route->install_state = RTE_STALE; /**/
       else
           route->install_state = RTE_NO_CHANGE;

           /*Delete all prefixes except the local prefixes*/
           ITERATE_LIST_BEGIN(route->like_prefix_list, list_node1){
             /*This is a bug. Deletion of list nodes while traversing the list
              * will leave holes in the list. Hence memory leak*/ 
             prefix = list_node1->data;
             if(prefix->hosting_node != GET_SPF_INFO_NODE(spf_info, level)){
                free(list_node1->data);
                free(list_node1);
             }
           } ITERATE_LIST_END;
           //delete_singly_ll(route->like_prefix_list);
   }ITERATE_LIST_END;
}


void
mark_all_routes_stale(spf_info_t *spf_info, LEVEL level){

    singly_ll_node_t* list_node = NULL,
        *list_node1 = NULL;
    routes_t *route = NULL;

    ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){

        route = list_node->data;
        if(!IS_LEVEL_SET(route->level, level))
            continue;

        route->install_state = RTE_STALE;
        ITERATE_LIST_BEGIN(route->like_prefix_list, list_node1){

            free(list_node1->data);
        } ITERATE_LIST_END;

        delete_singly_ll(route->like_prefix_list);
    }ITERATE_LIST_END;
}

/* This routine delete all routes from protocol as well as RIB
 * except DIRECT routes*/
void
delete_all_routes_except_direct_from_rib(node_t *node, LEVEL level){

    singly_ll_node_t* list_node = NULL, *list_node1 = NULL;
    routes_t *route = NULL;

    mark_all_routes_stale_except_direct_routes(&node->spf_info, level);

    ITERATE_LIST_BEGIN(node->spf_info.routes_list, list_node){

        route = list_node->data;
        if(route->install_state != RTE_STALE || 
            route->level != level)
            continue;
        
        install_route_in_rib(&node->spf_info, level, route);
        
        ITERATE_LIST_BEGIN(route->like_prefix_list, list_node1){

            free(list_node1->data);
        } ITERATE_LIST_END;

        delete_singly_ll(route->like_prefix_list);

    }ITERATE_LIST_END; 

    delete_stale_routes(&node->spf_info, level);    
}

void
delete_all_routes_from_rib(node_t *node, LEVEL level){

    singly_ll_node_t* list_node = NULL;
    routes_t *route = NULL;

    mark_all_routes_stale(&node->spf_info, level);

    ITERATE_LIST_BEGIN(node->spf_info.routes_list, list_node){

        route = list_node->data;
        if(route->install_state != RTE_STALE || 
            route->level != level)
            continue;
        
        install_route_in_rib(&node->spf_info, level, route);

    }ITERATE_LIST_END; 

    delete_stale_routes(&node->spf_info, level);    
}


void
delete_all_application_routes(node_t *node, LEVEL level){

    singly_ll_node_t* list_node = NULL;
    routes_t *route = NULL;

    ITERATE_LIST_BEGIN(node->spf_info.routes_list, list_node){

        route = list_node->data;
        if(route->level != level)
            continue;

        ROUTE_DEL_FROM_ROUTE_LIST((&node->spf_info), route); 
        free_route(route);
        route = NULL;
    }ITERATE_LIST_END; 
}

static void 
overwrite_route(spf_info_t *spf_info, routes_t *route, 
                prefix_t *prefix, spf_result_t *result, LEVEL level){

        unsigned int i = 0;
        nh_type_t nh = NH_MAX;
        internal_nh_t *int_nxt_hop = NULL;
        singly_ll_node_t* list_node = NULL;

        /*Delete the older prefix list. We dont know what is the use
         * of maintingin this list. We feel we should renew this list every
         * time the route is overwritten with better prefix. We will revisit 
         * once we find the use of this list.*/
        ITERATE_LIST_BEGIN(route->like_prefix_list, list_node){
            free(list_node->data);
        } ITERATE_LIST_END;
        delete_singly_ll(route->like_prefix_list);
        route->ecmp_dest_count = 1;
        route_set_key(route, prefix->prefix, prefix->mask); 

        sprintf(instance->traceopts->b, "route : %s/%u being over written for %s", route->rt_key.prefix, 
                    route->rt_key.mask, get_str_level(level)); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

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

        ITERATE_NH_TYPE_BEGIN(nh){

            ROUTE_FLUSH_PRIMARY_NH_LIST(route, nh);
            ROUTE_FLUSH_BACKUP_NH_LIST(route, nh);

            for(i = 0 ; i < MAX_NXT_HOPS; i++){
                if(!is_internal_nh_t_empty(result->next_hop[nh][i])){
                    int_nxt_hop = calloc(1, sizeof(internal_nh_t));
                    copy_internal_nh_t(result->next_hop[nh][i], *int_nxt_hop);
                    ROUTE_ADD_NH(route->primary_nh_list[nh], int_nxt_hop);   
                    sprintf(instance->traceopts->b, "route : %s/%u primary next hop is merged with %s's next hop node %s", 
                            route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                            result->next_hop[nh][i].node->node_name); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                }
                else
                    break;
            }
            for(i = 0 ; i < MAX_NXT_HOPS; i++){
                if(!is_internal_nh_t_empty((result->node->backup_next_hop[level][nh][i]))){
                    int_nxt_hop = calloc(1, sizeof(internal_nh_t));
                    copy_internal_nh_t((result->node->backup_next_hop[level][nh][i]), *int_nxt_hop);
                    ROUTE_ADD_NH(route->backup_nh_list[nh], int_nxt_hop);   
                    sprintf(instance->traceopts->b, "route : %s/%u backup next hop is merged with %s's backup next hop node %s", 
                            route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                            result->node->backup_next_hop[level][nh][i].node->node_name); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                }
                else
                    break;
            }
        } ITERATE_NH_TYPE_END;
}

static void
link_prefix_to_route(routes_t *route, prefix_t *new_prefix,
                                 unsigned int prefix_hosting_node_metric, spf_info_t *spf_info){

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

    sprintf(instance->traceopts->b, "To Route : %s/%u, %s, Appending prefix : %s/%u to Route prefix list",
                 route->rt_key.prefix, route->rt_key.mask, get_str_level(route->level),
                 new_prefix->prefix, new_prefix->mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

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
             LEVEL level, boolean linkage){

    routes_t *route = NULL;
    unsigned int i = 0;
    prefix_t *route_prefix = NULL;
    nh_type_t nh = NH_MAX;
    internal_nh_t *int_nxt_hop = NULL;

    prefix_pref_data_t prefix_pref = {ROUTE_UNKNOWN_PREFERENCE, "ROUTE_UNKNOWN_PREFERENCE"},
                       route_pref = {ROUTE_UNKNOWN_PREFERENCE, "ROUTE_UNKNOWN_PREFERENCE"};



    sprintf(instance->traceopts->b, "Node : %s : result node %s, prefix %s, level %s, prefix metric : %u",
            GET_SPF_INFO_NODE(spf_info, level)->node_name, result->node->node_name, 
            prefix->prefix, get_str_level(level), prefix->metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

    if(prefix->metric == INFINITE_METRIC){
        sprintf(instance->traceopts->b, "prefix : %s/%u discarded because of infinite metric", prefix->prefix, prefix->mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
        return;
    }

    route = search_route_in_spf_route_list(spf_info, prefix, level);

    if(!route){
        sprintf(instance->traceopts->b, "prefix : %s/%u is a New route (malloc'd) in %s, hosting_node %s", 
                prefix->prefix, prefix->mask, get_str_level(level), prefix->hosting_node->node_name); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

        route = route_malloc();
        route_set_key(route, prefix->prefix, prefix->mask); 
        route->ecmp_dest_count = 1;
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
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Next hop added : %s|%s at %s", 
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask ,
                            result->next_hop[nh][i].node->node_name, nh == IPNH ? "IPNH":"LSPNH", get_str_level(level)); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                }
                else
                    break;
            }
            for(i = 0 ; i < MAX_NXT_HOPS; i++){
                if(!is_internal_nh_t_empty((result->node->backup_next_hop[level][nh][i]))){
                    int_nxt_hop = calloc(1, sizeof(internal_nh_t));
                    copy_internal_nh_t((result->node->backup_next_hop[level][nh][i]), *int_nxt_hop);
                    ROUTE_ADD_NH(route->backup_nh_list[nh], int_nxt_hop);   
                    sprintf(instance->traceopts->b, "route : %s/%u backup next hop is copied with with %s's next hop node %s", 
                            route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                            result->node->backup_next_hop[level][nh][i].node->node_name); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                }
                else
                    break;
            }
        }ITERATE_NH_TYPE_END;
        /* route->backup_nh_list Not supported yet */

        /*Linkage*/
        if(linkage){
            route_prefix = calloc(1, sizeof(prefix_t));
            memcpy(route_prefix, prefix, sizeof(prefix_t));
            link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
        }

        ROUTE_ADD_TO_ROUTE_LIST(spf_info, route);
        route->install_state = RTE_ADDED;
        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u, spf_metric = %u, lsp_metric = %u,  marked RTE_ADDED for level%u",  
                GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask, 
                route->spf_metric, route->lsp_metric, route->level); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
    }
    else{
        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u existing route. route verion : %u," 
                "spf version : %u, route level : %s, spf level : %s", 
                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->prefix, prefix->mask, route->version, 
                spf_info->spf_level_info[level].version, get_str_level(route->level), get_str_level(level)); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

        if(route->install_state == RTE_ADDED){

            /* Comparison Block Start*/

            prefix_pref = route_preference(prefix->prefix_flags, prefix->level);
            route_pref  = route_preference(route->flags, route->level);

            if(prefix_pref.pref == ROUTE_UNKNOWN_PREFERENCE){
                sprintf(instance->traceopts->b, "Node : %s : Prefix : %s/%u pref = %s, ignoring prefix",  GET_SPF_INFO_NODE(spf_info, level)->node_name,
                        prefix->prefix, prefix->mask, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                return;
            }

            if(route_pref.pref < prefix_pref.pref){

                /* if existing route is better*/ 
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Not overwritten",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                /*Linkage*/
                if(linkage){
                    route_prefix = calloc(1, sizeof(prefix_t));
                    memcpy(route_prefix, prefix, sizeof(prefix_t));
                    link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
                }
                return;
            }
            /* If new prefix is better*/
            else if(prefix_pref.pref < route_pref.pref){

                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, will be overwritten",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

                overwrite_route(spf_info, route, prefix, result, level);
                /*Linkage*/
                if(linkage){
                    route_prefix = calloc(1, sizeof(prefix_t));
                    memcpy(route_prefix, prefix, sizeof(prefix_t));
                    link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
                }
                return;
            }
            /* If prefixes are of same preference*/ 
            else{
                /* If route pref = prefix pref, then decide based on metric*/
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Same preference, Trying based on metric",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

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
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Deciding based on External metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);; 

                    if(prefix->metric < route->ext_metric){
                        sprintf(instance->traceopts->b, "Node : %s : prefix external metric ( = %u) is better than routes external metric( = %u), will overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(prefix->metric > route->ext_metric){
                        sprintf(instance->traceopts->b, "Node : %s : prefix external metric ( = %u) is no better than routes external metric( = %u), will not overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                    }
                    else{
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                        /* Union LFA,s RLFA,s Primary nexthops*/
                        route->ecmp_dest_count++;
                        ITERATE_NH_TYPE_BEGIN(nh){
                            merge_route_primary_nexthops(route, result, nh);
                            merge_route_backup_nexthops(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    /*Linkage*/
                    if(linkage){
                        route_prefix = calloc(1, sizeof(prefix_t));
                        memcpy(route_prefix, prefix, sizeof(prefix_t));
                        link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
                    }
                    return;

                }else{
                    /*Decide pref based on internal metric*/
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Deciding based on Internal metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                    if(result->spf_metric + prefix->metric < route->spf_metric){
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u is over-written because better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(result->spf_metric + prefix->metric == route->spf_metric){
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                        /* Union LFA,s RLFA,s Primary nexthops*/ 
                        route->ecmp_dest_count++;
                        ITERATE_NH_TYPE_BEGIN(nh){
                            merge_route_primary_nexthops(route, result, nh);
                            merge_route_backup_nexthops(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    else{
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u is not over-written because no better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                    }
                    /*Linkage*/
                    if(linkage){
                        route_prefix = calloc(1, sizeof(prefix_t));
                        memcpy(route_prefix, prefix, sizeof(prefix_t));
                        link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
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
                sprintf(instance->traceopts->b, "Node : %s : Prefix : %s/%u pref = %s, ignoring prefix",  GET_SPF_INFO_NODE(spf_info, level)->node_name,
                        prefix->prefix, prefix->mask, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                return;
            }
            
            if(route_pref.pref < prefix_pref.pref){

                /* if existing route is better*/ 
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Not overwritten",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                /*Linkage*/
                if(linkage){
                    route_prefix = calloc(1, sizeof(prefix_t));
                    memcpy(route_prefix, prefix, sizeof(prefix_t));
                    link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
                }
                return;
            }
            /* If new prefix is better*/
            else if(prefix_pref.pref < route_pref.pref){

                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, will be overwritten",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

                overwrite_route(spf_info, route, prefix, result, level);
                /*Linkage*/
                if(linkage){
                    route_prefix = calloc(1, sizeof(prefix_t));
                    memcpy(route_prefix, prefix, sizeof(prefix_t));
                    link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
                }
                return;
            }
            /* If prefixes are of same preference*/ 
            else{
                /* If route pref = prefix pref, then decide based on metric*/
                sprintf(instance->traceopts->b, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Same preference, Trying based on metric",
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                        route_pref.pref_str, prefix_pref.pref_str); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

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
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Deciding based on External metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);; 

                    if(prefix->metric < route->ext_metric){
                        sprintf(instance->traceopts->b, "Node : %s : prefix external metric ( = %u) is better than routes external metric( = %u), will overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(prefix->metric > route->ext_metric){
                        sprintf(instance->traceopts->b, "Node : %s : prefix external metric ( = %u) is no better than routes external metric( = %u), will not overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                    }
                    else{
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                        /* Union LFA,s RLFA,s Primary nexthops*/
                        route->ecmp_dest_count++;
                        ITERATE_NH_TYPE_BEGIN(nh){
                            merge_route_primary_nexthops(route, result, nh);
                            merge_route_backup_nexthops(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    /*Linkage*/
                    if(linkage){
                        route_prefix = calloc(1, sizeof(prefix_t));
                        memcpy(route_prefix, prefix, sizeof(prefix_t));
                        link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
                    }
                    return;

                }else{
                    /*Decide pref based on internal metric*/
                    sprintf(instance->traceopts->b, "Node : %s : route : %s/%u Deciding based on Internal metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                    if(result->spf_metric + prefix->metric < route->spf_metric){
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u is over-written because better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(result->spf_metric + prefix->metric == route->spf_metric){
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                        /* Union LFA,s RLFA,s Primary nexthops*/ 
                        route->ecmp_dest_count++;
                        ITERATE_NH_TYPE_BEGIN(nh){
                            merge_route_primary_nexthops(route, result, nh);
                            merge_route_backup_nexthops(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    else{
                        sprintf(instance->traceopts->b, "Node : %s : route : %s/%u is not over-written because no better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
                    }
                    /*Linkage*/
                    if(linkage){
                        route_prefix = calloc(1, sizeof(prefix_t));
                        memcpy(route_prefix, prefix, sizeof(prefix_t));
                        link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
                    }
                    return;
                }
            }
            /* Comparison Block Ends*/

        }
        else/*This else should not hit for prc run*/
        {
            /*prefix is from prev run and exists. This code hits only once per given route*/
            sprintf(instance->traceopts->b, "route : %s/%u, updated route(?)", 
                    route->rt_key.prefix, route->rt_key.mask); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;
            route->install_state = RTE_UPDATED;

            sprintf(instance->traceopts->b, "Node : %s : route : %s/%u, old spf_metric = %u, new spf metric = %u, marked RTE_UPDATED for level%u",  
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask, 
                    route->spf_metric, result->spf_metric + prefix->metric, route->level); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

            sprintf(instance->traceopts->b, "Node : %s : route : %s/%u %s is mandatorily over-written because of version mismatch",
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask, get_str_level(level)); trace(instance->traceopts, ROUTE_CALCULATION_BIT);;

            overwrite_route(spf_info, route, prefix, result, level);
            route->version = spf_info->spf_level_info[level].version;

            /*Linkage*/ 
            if(linkage){
                route_prefix = calloc(1, sizeof(prefix_t));
                memcpy(route_prefix, prefix, sizeof(prefix_t));
                link_prefix_to_route(route, route_prefix, result->spf_metric, spf_info);
            }
        }
    }
}

/* Routine to add one single route to RIB*/
/*This routine is not in use*/
void
install_route_in_rib(spf_info_t *spf_info, 
        LEVEL level, routes_t *route){

    rttable_entry_t *rt_entry_template = NULL;
    unsigned int i = 0,
                 rt_added = 0,
                 rt_removed = 0, 
                 rt_updated = 0, 
                 rt_no_change = 0;

    nh_type_t nh;
    singly_ll_node_t *list_node = NULL;

    assert(route->level == level);
    assert(route->spf_metric != INFINITE_METRIC);

    if(route->install_state == RTE_STALE)
        goto STALE;

    rt_entry_template = GET_NEW_RT_ENTRY();

    prepare_new_rt_entry_template(spf_info, rt_entry_template, route,
            spf_info->spf_level_info[level].version); 

    ITERATE_NH_TYPE_BEGIN(nh){
        sprintf(instance->traceopts->b, "route : %s/%u, rt_entry_template->primary_nh_count = %u(%s)", 
                rt_entry_template->dest.prefix, rt_entry_template->dest.mask, 
                rt_entry_template->primary_nh_count[nh], nh == IPNH ? "IPNH" : "LSPNH"); 
        trace(instance->traceopts, ROUTE_INSTALLATION_BIT);
    } ITERATE_NH_TYPE_END ;

    ITERATE_NH_TYPE_BEGIN(nh){

        /*if rt_entry_template->primary_nh_count is zero, it means, it is a local prefix*/
        if(rt_entry_template->primary_nh_count[nh] == 0){
            prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node,
                    NULL,
                    &rt_entry_template->primary_nh[nh][0],
                    level, nh);
        }
        else
        {
            i = 0;
            ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node){
                prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node, 
                        list_node->data,
                        &rt_entry_template->primary_nh[nh][i],
                        level, nh);
                i++;
            } ITERATE_LIST_END;
        }

    } ITERATE_NH_TYPE_END;

    /*Back up Next hop funtionality is not implemented yet*/
    memset(&rt_entry_template->backup_nh, 0, sizeof(nh_t));

    /*rt_entry_template is now ready to be installed in RIB*/

    STALE:
    sprintf(instance->traceopts->b, "RIB modification : route : %s/%u, Level%u, Action : %s", 
            route->rt_key.prefix, route->rt_key.mask, 
            route->level, route_intall_status_str(route->install_state)); 
    trace(instance->traceopts, ROUTE_INSTALLATION_BIT);

    switch(route->install_state){

        case RTE_STALE:
            if(rt_route_delete(spf_info->rttable, route->rt_key.prefix, route->rt_key.mask) < 0){
                printf("%s() : Error : Could not delete route : %s/%u, Level%u\n", __FUNCTION__, 
                        route->rt_key.prefix, route->rt_key.mask, route->level);
                break;
            }
            rt_removed++;
            break;
        case RTE_UPDATED:
            assert(0);
        case RTE_ADDED:
            if(rt_route_install(spf_info->rttable, rt_entry_template) < 0){
                printf("%s() : Error : Could not Add route : %s/%u, Level%u\n", __FUNCTION__,
                        route->rt_key.prefix, route->rt_key.mask, route->level);
                free(rt_entry_template);
                rt_entry_template = NULL;
                break;
            }
            rt_added++;
            break;
        case RTE_CHANGED:
            rt_route_update(spf_info->rttable, rt_entry_template);
            free(rt_entry_template);
            rt_entry_template = NULL;
            rt_updated++;
            break;
        case RTE_NO_CHANGE:
            assert(0); /*You cant reach here for unchanged routes*/
            break;
        default:
            assert(0);
    }

    sprintf(instance->traceopts->b, "Result for Route : %s/%u, L%d : #Added:%u, #Deleted:%u, #Updated:%u, #Unchanged:%u",
            route->rt_key.prefix, route->rt_key.mask, route->level, 
            rt_added, rt_removed, rt_updated, rt_no_change); 
    trace(instance->traceopts, ROUTE_INSTALLATION_BIT);
    printf("Node %s : Result for Route : %s/%u, L%d : #Added:%u, #Deleted:%u, #Updated:%u, #Unchanged:%u\n",
            spf_info->spf_level_info[level].node->node_name, route->rt_key.prefix, route->rt_key.mask, route->level, 
            rt_added, rt_removed, rt_updated, rt_no_change);

}

#if 0
static boolean
is_route_eligible_for_backups(routes_t *route){

    return TRUE;
}
#endif
/*Routine to build the routing table*/

void
start_route_installation(spf_info_t *spf_info,
                         LEVEL level){

    sprintf(instance->traceopts->b, "Entered ... Level : %u", level);
    trace(instance->traceopts, ROUTE_INSTALLATION_BIT);
    singly_ll_node_t *list_node = NULL, 
                     *list_node2 = NULL;

    routes_t *route = NULL;
    rttable_entry_t *rt_entry_template = NULL;
    rttable_entry_t *existing_rt_route = NULL;
    nh_type_t nh;
    internal_nh_t *backup = NULL;

    unsigned int i = 0,
                 rt_added = 0,
                 rt_removed = 0, 
                 rt_updated = 0, 
                 rt_no_change = 0;

    ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){
        
        route = list_node->data;

        if(route->level != level)
            continue;

        if(route->install_state != RTE_NO_CHANGE &&
            route->install_state != RTE_STALE){
             
            rt_entry_template = GET_NEW_RT_ENTRY();
            prepare_new_rt_entry_template(spf_info, rt_entry_template, route,
                    spf_info->spf_level_info[level].version); 

            ITERATE_NH_TYPE_BEGIN(nh){        
                /*if rt_entry_template->primary_nh_count is zero, it means, it is a local prefix or leaked prefix*/
                if(rt_entry_template->primary_nh_count[nh] == 0){

                    prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node,
                            NULL,/*No next hop*/
                            &rt_entry_template->primary_nh[nh][0],
                            level, nh);
                }
                else{
                    i = 0;
                    ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node2){
                        prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node, 
                                list_node2->data, 
                                &rt_entry_template->primary_nh[nh][i],
                                level, nh);
                        i++;
                    } ITERATE_LIST_END;
                }
            } ITERATE_NH_TYPE_END;
          
//            if(is_route_eligible_for_backups(route)){ 
#if 0
                backup = backup_selection_policy(route);
                if(backup){
                    prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node,
                            backup, &rt_entry_template->backup_nh, level, backup->nh_type);
                    rt_entry_template->backup_nh_count++;
                }
#endif
#if 1
                /*Install All backups instead of best one*/
                rt_entry_template->backup_nh_count = 0;
                ITERATE_NH_TYPE_BEGIN(nh){
                    ITERATE_LIST_BEGIN(route->backup_nh_list[nh], list_node2){
                        backup = list_node2->data;
                        prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node,
                                backup, &rt_entry_template->backup_nh[rt_entry_template->backup_nh_count], 
                                level, backup->nh_type);
                        rt_entry_template->backup_nh_count++;
                    } ITERATE_LIST_END;
                } ITERATE_NH_TYPE_END;
#endif
//            }
            ITERATE_NH_TYPE_BEGIN(nh){
                sprintf(instance->traceopts->b, "rt_entry Template for route : %s/%u, rt_entry_template->primary_nh_count = %u(%s), cost = %u", 
                        rt_entry_template->dest.prefix, rt_entry_template->dest.mask, 
                        rt_entry_template->primary_nh_count[nh], 
                        nh == IPNH ? "IPNH" : "LSPNH", rt_entry_template->cost); 
                trace(instance->traceopts, ROUTE_INSTALLATION_BIT);
            } ITERATE_NH_TYPE_END;
            sprintf(instance->traceopts->b, "rt_entry Template for route : %s/%u, rt_entry_template->backup_nh_count = %u",
                    rt_entry_template->dest.prefix, rt_entry_template->dest.mask, rt_entry_template->backup_nh_count); 
            trace(instance->traceopts, ROUTE_INSTALLATION_BIT);

            /*rt_entry_template is now ready to be installed in RIB*/
        }
        
        sprintf(instance->traceopts->b, "RIB modification : route : %s/%u, Level%u, metric = %u, Action : %s", 
                route->rt_key.prefix, route->rt_key.mask, 
                route->level, route->spf_metric, route_intall_status_str(route->install_state)); 
        trace(instance->traceopts, ROUTE_INSTALLATION_BIT);

        switch(route->install_state){
            
            case RTE_STALE:
                if(rt_route_delete(spf_info->rttable, route->rt_key.prefix, route->rt_key.mask) < 0){
                    printf("%s() : Error : Could not delete route : %s/%u, Level%u\n", __FUNCTION__, 
                                    route->rt_key.prefix, route->rt_key.mask, route->level);
                    break;
                }
                rt_removed++;
                break;
            case RTE_UPDATED:
                assert(0);
            case RTE_ADDED:
                    if(rt_route_install(spf_info->rttable, rt_entry_template) < 0){
                        printf("%s() : Error : Could not Add route : %s/%u, Level%u\n", __FUNCTION__,
                                route->rt_key.prefix, route->rt_key.mask, route->level);
                        free(rt_entry_template);
                        rt_entry_template = NULL;
                        break;
                    }
                    rt_added++;
                break;
            case RTE_CHANGED:
                rt_route_update(spf_info->rttable, rt_entry_template);
                free(rt_entry_template);
                rt_entry_template = NULL;
                rt_updated++;
                break;
            case RTE_NO_CHANGE:
                    rt_no_change++;
                    /*Update the version number in RIB*/
                    existing_rt_route = rt_route_lookup(spf_info->rttable,
                        route->rt_key.prefix, route->rt_key.mask);
                    assert(existing_rt_route);
                    
                    if(existing_rt_route->level != route->level)
                        continue;
                    
                    existing_rt_route->version = spf_info->spf_level_info[level].version;
                break;
            default:
                assert(0);
        }
    }ITERATE_LIST_END;
    
    delete_stale_routes(spf_info, level);
    sprintf(instance->traceopts->b, "SPF Stats : L%d, Node : %s : #Added:%u, #Deleted:%u, #Updated:%u, #Unchanged:%u",
            level, spf_info->spf_level_info[level].node->node_name, rt_added, rt_removed, rt_updated, rt_no_change); 
    trace(instance->traceopts, ROUTE_INSTALLATION_BIT);
    printf("SPF Stats : L%d, Node : %s : #Added:%u, #Deleted:%u, #Updated:%u, #Unchanged:%u\n",
            level, spf_info->spf_level_info[level].node->node_name, rt_added, rt_removed, rt_updated, rt_no_change);
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

    rttable_entry_t *rt_entry = NULL;
    
    sprintf(instance->traceopts->b, "Entered ... spf_root : %s, Level : %s", spf_root->node_name, get_str_level(level));
    trace(instance->traceopts, ROUTE_INSTALLATION_BIT);
    
    mark_all_routes_stale(spf_info, level);

    /*Walk over the SPF result list computed in spf run
     * in the same order. Note that order of this list is :
     * most distant router from spf root is first*/


    ITERATE_LIST_BEGIN(spf_root->spf_run_result[level], list_node){

        result = (spf_result_t *)list_node->data;
        sprintf(instance->traceopts->b, "Node %s : processing result of %s, at level %s", 
            spf_root->node_name, result->node->node_name, get_str_level(level)); 
        trace(instance->traceopts, ROUTE_INSTALLATION_BIT);

        /*Iterate over all the prefixes of result->node for level 'level'*/


        if(level == LEVEL1                                               &&  /* If current spf run is Level1*/
                !IS_BIT_SET(spf_root->instance_flags, IGNOREATTACHED)    &&  /* If computing router is programmed to detect the L1L2 routers*/
                result->node->spf_info.spff_multi_area                   &&  /* if the router being inspected is L1L2 router*/
                !spf_root->spf_info.spff_multi_area){                        /* if the computing router is L1-only router*/

            L1L2_result = result;                                    /* Record the L1L2 router result*/
            sprintf(instance->traceopts->b, "Node %s : L1L2_result recorded - %s", 
                            spf_root->node_name, L1L2_result->node->node_name); 
            trace(instance->traceopts, ROUTE_INSTALLATION_BIT); 

            prefix_t default_prefix;
            memset(&default_prefix, 0, sizeof(prefix_t)); 
            UNSET_BIT(default_prefix.prefix_flags, PREFIX_DOWNBIT_FLAG);
            UNSET_BIT(default_prefix.prefix_flags, PREFIX_EXTERNABIT_FLAG);
            UNSET_BIT(default_prefix.prefix_flags, PREFIX_METRIC_TYPE_EXT);
            default_prefix.hosting_node = L1L2_result->node;
            default_prefix.metric = 0;
            default_prefix.mask = 0;
            default_prefix.level = LEVEL1;
            update_route(spf_info, L1L2_result, &default_prefix, LEVEL1, FALSE);
        }


        ITERATE_LIST_BEGIN(GET_NODE_PREFIX_LIST(result->node, level), prefix_list_node){

            prefix = (prefix_t *)prefix_list_node->data;  
            update_route(spf_info, result, prefix, level, TRUE);
        }ITERATE_LIST_END;

    } ITERATE_LIST_END;

    /*Remove backups which do not protect any primary next hops oif from route's backup list*/


    /*Iterate over all UPDATED routes and figured out which one needs to be updated
     * in RIB*/
    ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){

        route = list_node->data;
        //remove_not_protecting_backups(route);
        if(route->install_state != RTE_UPDATED)
            continue;

        if(!IS_LEVEL_SET(route->level, level))
            continue;

        rt_entry = rt_route_lookup(spf_info->rttable, route->rt_key.prefix, route->rt_key.mask);
        assert(rt_entry); /*This entry MUST exist in RIB*/

        if(is_changed_route(spf_info, rt_entry, route, FULL_RUN, level) == TRUE)
            route->install_state = RTE_CHANGED;
        else
            route->install_state = RTE_NO_CHANGE;
        sprintf(instance->traceopts->b, "Route : %s/%u is Marked as %s at %s", 
                route->rt_key.prefix, route->rt_key.mask, 
                route_intall_status_str(route->install_state),
                get_str_level(level)); 
        trace(instance->traceopts, ROUTE_CALCULATION_BIT); 
    }ITERATE_LIST_END;
}

void
spf_backup_postprocessing(node_t *spf_root, LEVEL level){

}

void
spf_postprocessing(spf_info_t *spf_info, /* routes are stored globally*/
                   node_t *spf_root,     /* computing node which stores the result (list) of spf run*/
                   LEVEL level){         /*Level of spf run*/

    
    /*-----------------------------------------------------------------------------
     *  If this is L2 run, then set my spf_info_t->spff_multi_area bit, and schedule
     *  SPF L1 run to ensure L1 routes are uptodate before updating L2 routes
     *-----------------------------------------------------------------------------*/
    sprintf(instance->traceopts->b, "Entered ... ");
    trace(instance->traceopts, ROUTE_CALCULATION_BIT);
       
    if(level == LEVEL2 && spf_info->spf_level_info[LEVEL1].version){
        /*If at least 1 SPF L1 run has been triggered*/
        
        spf_determine_multi_area_attachment(spf_info, spf_root);  
        /*Schedule level 1 spf run, just to make sure L1 routes are up
         *      * to date before building L2 routes*/
        //sprintf(instance->traceopts->b, "L2 spf run, triggering L1 SPF run first before building L2 routes"); TRACE();
        //spf_computation(spf_root, spf_info, LEVEL1, FULL_RUN);
    }

    build_routing_table(spf_info, spf_root, level);
    start_route_installation(spf_info, level);
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
