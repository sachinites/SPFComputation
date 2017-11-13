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
#include "logging.h"
#include "bitsop.h"
#include "rttable.h"
#include "advert.h"

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

    routes_t *route = calloc(1, sizeof(routes_t));
    route->primary_nh_list = init_singly_ll();
    singly_ll_set_comparison_fn(route->primary_nh_list, instance_node_comparison_fn);
    route->backup_nh_list = init_singly_ll();
    singly_ll_set_comparison_fn(route->backup_nh_list, instance_node_comparison_fn);
    route->like_prefix_list = init_singly_ll();
    singly_ll_set_comparison_fn(route->like_prefix_list, get_prefix_comparison_fn());
    singly_ll_set_order_comparison_fn(route->like_prefix_list, get_prefix_order_comparison_fn());
    route->install_state = RTE_ADDED;
    return route;
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
    
    singly_ll_node_t* list_node = NULL; 
    route->hosting_node = 0;
    
    ROUTE_FLUSH_PRIMARY_NH_LIST(route);
    free(route->primary_nh_list);
    route->primary_nh_list = 0;

    ROUTE_FLUSH_BACKUP_NH_LIST(route);
    free(route->backup_nh_list);
    route->backup_nh_list = 0;


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
spf_result_t *
prepare_new_rt_entry_template(spf_info_t *spf_info, rttable_entry_t *rt_entry_template,
        routes_t *route, unsigned int version){

    self_spf_result_t *self_res = NULL;
    strncpy(rt_entry_template->dest.prefix, route->rt_key.prefix, PREFIX_LEN + 1);
    rt_entry_template->dest.prefix[PREFIX_LEN] = '\0';
    rt_entry_template->dest.mask = route->rt_key.mask;
    rt_entry_template->version = version;
    rt_entry_template->cost = route->spf_metric;
    rt_entry_template->level = route->level;

    /*Find the spf result to reach route->hosting_node when spf_root is current node.*/
    self_res = singly_ll_search_by_key(route->hosting_node->self_spf_result[route->level], 
                            GET_SPF_INFO_NODE(spf_info, route->level));
    assert(self_res);

    rt_entry_template->primary_nh_count =
        next_hop_count(&self_res->res->next_hop[0]);

    return self_res->res; /* To be used for next hop template preparation*/
}

void
prepare_new_nxt_hop_template(node_t *computing_node,
        node_t *nxt_hop_node,
        nh_t *nh_template,
        LEVEL level){

    assert(nh_template);
    assert(nxt_hop_node);
    assert(computing_node);

    edge_end_t *oif = NULL;
    char gw_prefix[PREFIX_LEN + 1];

    nh_template->nh_type = IPNH; /*We have not implemented yet LSP NH functionality*/

    strncpy(nh_template->nh_name, nxt_hop_node->node_name, NODE_NAME_SIZE);
    nh_template->nh_name[NODE_NAME_SIZE - 1] = '\0';

    /*oif can be NULL if computing_node == nxt_hop_node. It will happen
     * for local prefixes */
    oif = get_min_oif(computing_node, nxt_hop_node, level, gw_prefix);
    if(!oif){
        strncpy(nh_template->gwip, "Direct" , PREFIX_LEN + 1);
        nh_template->gwip[PREFIX_LEN] = '\0';
        return;
    }

    strncpy(nh_template->oif, oif->intf_name, IF_NAME_SIZE);
    nh_template->oif[IF_NAME_SIZE -1] = '\0';
    strncpy(nh_template->gwip, gw_prefix, PREFIX_LEN + 1);
    nh_template->gwip[PREFIX_LEN] = '\0';
}

routes_t *
search_route_in_spf_route_list(spf_info_t *spf_info, 
                                prefix_t *prefix, LEVEL level){

    routes_t *route = NULL;
    singly_ll_node_t* list_node = NULL;
    char prefix_with_mask[PREFIX_LEN + 1];
    apply_mask(prefix->prefix, prefix->mask, prefix_with_mask);
    prefix_with_mask[PREFIX_LEN] = '\0';
    ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){
        route = list_node->data;
        if(strncmp(route->rt_key.prefix, prefix_with_mask, PREFIX_LEN) == 0 &&
                (route->rt_key.mask == prefix->mask) && route->level == level)
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
is_same_next_hop(node_t* computing_node, 
                        node_t *nbr_node,
                        nh_t *nh, LEVEL level){

    nh_t nh_temp;
    memset(&nh_temp, 0, sizeof(nh_t));

    prepare_new_nxt_hop_template(computing_node, nbr_node, &nh_temp, level);
    if(memcmp(nh, &nh_temp, sizeof(nh_t)) == 0)
        return TRUE;
    return FALSE;
}

static boolean
route_rib_same_next_hops(spf_info_t *spf_info, 
        rttable_entry_t *rt_entry, /*We need to compare the nexthop array of rt_entry and route*/
        routes_t *route, 
        LEVEL level){

    unsigned int i = 0;
    singly_ll_node_t* list_node = NULL;
    node_t *computing_node = spf_info->spf_level_info[level].node;
    node_t *nbr_node = NULL;

    ITERATE_LIST_BEGIN(route->primary_nh_list, list_node){
        nbr_node = list_node->data; /*This is nbr node of computing node*/
        if(is_same_next_hop(computing_node, nbr_node, &rt_entry->primary_nh[i], level) == FALSE)
            return FALSE;
    }ITERATE_LIST_END;
    return TRUE;
}

static boolean
is_changed_route(spf_info_t *spf_info, 
                 rttable_entry_t *rt_entry, /*route from RIB*/
                 routes_t *route,           /*route from our SPF*/
                 spf_type_t spf_type, LEVEL level){

    /* If spf_type == FULL_RUN, we will not consider back_up nexthop field to
     * evaluate whether the route we have has changed against what is installed in RIB because
     * the backup we have now are stale backups (from previous spf runs). If the 
     * route is changed we will  install the changed route in RIB without backup 
     * because the old  backup will be stale and do not guarantee microloop 
     * free alternate path. It is better to have no backup than to have unpromising backup.
     *
     * If spf_type == SKELETON_RUN, it means, we are in second phase, that we are 
     * done with computing our main routes and now comuting backups only. Whatever 
     * backup we have at this point, we will install backups only against the corresponging
     * route. 
     */
   
   /* We should only feed route with RTE_UPDATED status in this fn, because these are the
    * routes which were present in prev spf runs, and in current spf runs as well, and we
    * are not sure yet whether they are same or something changed*/

   assert(route->install_state == RTE_UPDATED);
   
   switch(spf_type){

       case FULL_RUN:
           if((strncmp(rt_entry->dest.prefix, route->rt_key.prefix, PREFIX_LEN + 1) == 0)  &&
                   (rt_entry->dest.mask == route->rt_key.mask)                             &&
                   (rt_entry->cost == route->spf_metric)                                   &&
                   (rt_entry->primary_nh_count == ROUTE_GET_PR_NH_CNT(route))              && 
                   (route_rib_same_next_hops(spf_info, rt_entry, route, level) == TRUE))
               return FALSE;
           return TRUE; 
           break;
       case SKELETON_RUN:
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

   sprintf(LOG, "Deleting Stale Routes"); TRACE();
   ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){
           
       route = list_node->data;
       if(route->install_state == RTE_STALE && IS_LEVEL_SET(route->level, level)){
        sprintf(LOG, "route : %s/%u is STALE for Level%d, deleted", route->rt_key.prefix, 
                        route->rt_key.mask, level); TRACE();
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
    
        route_set_key(route, prefix->prefix, prefix->mask); 

        sprintf(LOG, "route : %s/%u being over written for %s", route->rt_key.prefix, 
                    route->rt_key.mask, get_str_level(level)); TRACE();

        assert(IS_LEVEL_SET(route->level, level));
       
        route->version = spf_info->spf_level_info[level].version;
        route->flags = prefix->prefix_flags;
       
        SET_LEVEL(route->level, level);
        route->hosting_node = prefix->hosting_node;

        route->spf_metric = result->spf_metric + prefix->metric;
        route->lsp_metric = 0; /*Not supported*/

        ROUTE_FLUSH_PRIMARY_NH_LIST(route);
        ROUTE_FLUSH_BACKUP_NH_LIST(route); 

        for(; i < MAX_NXT_HOPS; i++){
            if(result->next_hop[i])
                ROUTE_ADD_PRIMARY_NH(route, result->next_hop[i]);   
            else
                break;
        }
        /* route->backup_nh_list Not supported yet */
}

static void
update_route(spf_info_t *spf_info,          /*spf_info of computing node*/ 
             spf_result_t *result,          /*result representing some network node*/
             prefix_t *prefix,              /*local prefix hosted on 'result' node*/
             LEVEL level, boolean linkage){

    routes_t *route = NULL;
    unsigned int i = 0;
    prefix_t *route_prefix = NULL;

    sprintf(LOG, "Node %s, result node %s, prefix %s, level %s, prefix metric : %u",
             GET_SPF_INFO_NODE(spf_info, level)->node_name, result->node->node_name, 
             prefix->prefix, get_str_level(level), prefix->metric); TRACE();
    
    if(prefix->metric == INFINITE_METRIC){
        sprintf(LOG, "prefix : %s/%u discarded because of infinite metric", prefix->prefix, prefix->mask); TRACE();
        return;
    }
    
    route = search_route_in_spf_route_list(spf_info, prefix, level);

    if(!route){
        sprintf(LOG, "prefix : %s/%u is a New route (malloc'd) in %s, hosting_node %s", 
                prefix->prefix, prefix->mask, get_str_level(level), prefix->hosting_node->node_name); TRACE();

        route = route_malloc();
        route_set_key(route, prefix->prefix, prefix->mask); 
       
        route->version = spf_info->spf_level_info[level].version;
        route->flags = prefix->prefix_flags;
        
        route->level = level;
        route->hosting_node = prefix->hosting_node;
        
        /* Update route metric */
        route->spf_metric =  result->spf_metric + prefix->metric;
        route->lsp_metric = 0; /*Not supported*/
        //prefix->metric = route->spf_metric;

        for(; i < MAX_NXT_HOPS; i++){
            if(result->next_hop[i])
                ROUTE_ADD_PRIMARY_NH(route, result->next_hop[i]);   
            else
                break;
        }
        /* route->backup_nh_list Not supported yet */

        route_prefix = calloc(1, sizeof(prefix_t));
        memcpy(route_prefix, prefix, sizeof(prefix_t));
        ROUTE_ADD_LIKE_PREFIX_LIST(route, route_prefix, result->spf_metric);
        route_prefix->metric = route->spf_metric;
        ROUTE_ADD_TO_ROUTE_LIST(spf_info, route);
        route->install_state = RTE_ADDED;
        sprintf(LOG, "Node : %s : route : %s/%u, spf_metric = %u,  marked RTE_ADDED for level%u",  
            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask, 
            route->spf_metric, route->level); TRACE();
    }
    else{
        sprintf(LOG, "Node : %s : route : %s/%u existing route. route verion : %u," 
                    "spf version : %u, route level : %s, spf level : %s", 
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->prefix, prefix->mask, route->version, 
                    spf_info->spf_level_info[level].version, get_str_level(route->level), get_str_level(level)); TRACE();

        if(route->install_state == RTE_ADDED){
            if(result->spf_metric + prefix->metric < route->spf_metric){
                sprintf(LOG, "Node : %s : route : %s/%u is over-written because better metric on node %s is found with metric = %u", 
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                        route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                        result->spf_metric + prefix->metric); TRACE();
                overwrite_route(spf_info, route, prefix, result, level);
            }
            else if(result->spf_metric + prefix->metric == route->spf_metric){
                 sprintf(LOG, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                        route->rt_key.prefix, route->rt_key.mask); TRACE();
            }
            route_prefix = calloc(1, sizeof(prefix_t));
            memcpy(route_prefix, prefix, sizeof(prefix_t));
            ROUTE_ADD_LIKE_PREFIX_LIST(route, route_prefix, result->spf_metric);
            route_prefix->metric = route->spf_metric;
            return;
        }
        
        /* RTE_UPDATED route only*/ 
        if(route->version == spf_info->spf_level_info[level].version){ 
            if(result->spf_metric + prefix->metric < route->spf_metric){
                sprintf(LOG, "Node : %s : route : %s/%u is over-written because better metric on node %s is found with metric = %u", 
                        GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                        route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                        result->spf_metric + prefix->metric); TRACE();
                overwrite_route(spf_info, route, prefix, result, level);
#if 1
                /* if prc run then route->install_state = RTE_CHANGED*/
                
                if(SPF_RUN_TYPE(GET_SPF_INFO_NODE(spf_info, level), level) == PRC_RUN){
                    route->install_state = RTE_CHANGED;
                    sprintf(LOG, "Node : %s : PRC route : %s/%u %s STATE set to %s", (GET_SPF_INFO_NODE(spf_info, level))->node_name,
                            route->rt_key.prefix, route->rt_key.mask, get_str_level(level), route_intall_status_str(RTE_CHANGED)); TRACE();
                }
#endif
            }
            route_prefix = calloc(1, sizeof(prefix_t));
            memcpy(route_prefix, prefix, sizeof(prefix_t));
            ROUTE_ADD_LIKE_PREFIX_LIST(route, route_prefix, result->spf_metric);
            route_prefix->metric = route->spf_metric;

#if 1
            /*If prc run, then set route->install_state = RTE_UPDATED;
             * */
            if(SPF_RUN_TYPE(GET_SPF_INFO_NODE(spf_info, level), level) == PRC_RUN){
                route->install_state = RTE_UPDATED;
                sprintf(LOG, "Node : %s : PRC route : %s/%u %s STATE set to %s", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask, get_str_level(level), route_intall_status_str(RTE_UPDATED)); TRACE();
            }
#endif
        }
        else/*This else should not hit for prc run*/
        {
            /*route is from prev run and exists. This code hits only once per given route*/
            sprintf(LOG, "route : %s/%u, updated route(?)", 
                    route->rt_key.prefix, route->rt_key.mask); TRACE();
            route->install_state = RTE_UPDATED;

            sprintf(LOG, "Node : %s : route : %s/%u, old spf_metric = %u, new spf metric = %u, marked RTE_UPDATED for level%u",  
                GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask, 
                route->spf_metric, result->spf_metric + prefix->metric, route->level); TRACE();
            
            sprintf(LOG, "Node : %s : route : %s/%u %s is mandatorily over-written because of version mismatch",
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask, get_str_level(level)); TRACE();

            overwrite_route(spf_info, route, prefix, result, level);
            route->version = spf_info->spf_level_info[level].version; 
            route_prefix = calloc(1, sizeof(prefix_t));
            memcpy(route_prefix, prefix, sizeof(prefix_t));
            ROUTE_ADD_LIKE_PREFIX_LIST(route, route_prefix, result->spf_metric);
            route_prefix->metric = route->spf_metric;
        }
    }
}

/* Routine to add one single route to RIB*/

void
install_route_in_rib(spf_info_t *spf_info, 
        LEVEL level, routes_t *route){

    rttable_entry_t *rt_entry_template = NULL;
    unsigned int i = 0,
                 rt_added = 0,
                 rt_removed = 0, 
                 rt_updated = 0, 
                 rt_no_change = 0;

    rttable_entry_t *existing_rt_route = NULL;

    assert(route->level == level);
    assert(route->spf_metric != INFINITE_METRIC);

    /* Note : For unchanged routes, we need to send notification to RIB 
     * to simply remove the LFA/RLFA for unchanged routes present at this point, 
     * these are stale LFA/RLFA and can cause Microloops*/

    if(route->install_state == RTE_NO_CHANGE){

        rt_no_change++;
        if(!is_singly_ll_empty(route->backup_nh_list))
            rt_route_remove_backup_nh(spf_info->rttable, 
                    route->rt_key.prefix, route->rt_key.mask);
        return;  
    }

    if(route->install_state == RTE_STALE)
        goto STALE;

    rt_entry_template = GET_NEW_RT_ENTRY();

    prepare_new_rt_entry_template(spf_info, rt_entry_template, route,
            spf_info->spf_level_info[level].version); 

    sprintf(LOG, "route : %s/%u, rt_entry_template->primary_nh_count = %u", 
            rt_entry_template->dest.prefix, rt_entry_template->dest.mask, 
            rt_entry_template->primary_nh_count); TRACE();

    self_spf_result_t *self_res = NULL;
    
    /*Find the spf result to reach route->hosting_node when spf_root is current node.*/
    self_res = singly_ll_search_by_key(route->hosting_node->self_spf_result[route->level],
            GET_SPF_INFO_NODE(spf_info, route->level));

    assert(self_res);

    for(i = 0; i < rt_entry_template->primary_nh_count; i++){
        prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node, 
                self_res->res->next_hop[i], 
                &rt_entry_template->primary_nh[i],
                level);
    }

    /*if rt_entry_template->primary_nh_count is zero, it means, it is a local prefix*/
    if(rt_entry_template->primary_nh_count == 0)
        prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node,
                spf_info->spf_level_info[level].node,
                &rt_entry_template->primary_nh[i],
                level);

    /*Back up Next hop funtionality is not implemented yet*/
    memset(&rt_entry_template->backup_nh, 0, sizeof(nh_t));

    /*rt_entry_template is now ready to be installed in RIB*/

    STALE:
    sprintf(LOG, "RIB modification : route : %s/%u, Level%u, Action : %s", 
            route->rt_key.prefix, route->rt_key.mask, 
            route->level, route_intall_status_str(route->install_state)); TRACE();

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
        /*It might be the case that the route already installed in RIB belong to other level, 
         * we need to check if the route we are installing has better cost or not*/
        {
            existing_rt_route = rt_route_lookup(spf_info->rttable, 
                                route->rt_key.prefix, route->rt_key.mask);
            if(existing_rt_route){
                if(existing_rt_route->level != route->level){
                    if(existing_rt_route->cost > rt_entry_template->cost){
                        sprintf(LOG, "Node %s: RIB Route %s/%u, %s, cost = %d updated with route %s/%u, %s, cost = %d",
                                    GET_SPF_INFO_NODE(spf_info, level)->node_name, existing_rt_route->dest.prefix, existing_rt_route->dest.mask,
                                    get_str_level(existing_rt_route->level), existing_rt_route->cost,
                                    rt_entry_template->dest.prefix, rt_entry_template->dest.mask, get_str_level(rt_entry_template->level),
                                    rt_entry_template->cost); TRACE();
                        rt_route_update(spf_info->rttable, rt_entry_template);
                        free(rt_entry_template);
                        rt_entry_template = NULL;
                        rt_updated++;
                        break;
                    }
                    else{
                        rt_no_change++;
                        break;
                    }
                }
                else
                    assert(0); /* Impossible case*/
            }
            if(rt_route_install(spf_info->rttable, rt_entry_template) < 0){
                printf("%s() : Error : Could not Add route : %s/%u, Level%u\n", __FUNCTION__,
                        route->rt_key.prefix, route->rt_key.mask, route->level);
                free(rt_entry_template);
                rt_entry_template = NULL;
                break;
            }
            rt_added++;
        }
            break;
        case RTE_CHANGED:
            existing_rt_route = rt_route_lookup(spf_info->rttable, 
                                route->rt_key.prefix, route->rt_key.mask);
            if(existing_rt_route){
                if(existing_rt_route->level != route->level){
                    if(existing_rt_route->cost > rt_entry_template->cost){
                        sprintf(LOG, "Node %s: RIB Route %s/%u, %s, cost = %d updated with route %s/%u, %s, cost = %d",
                                    GET_SPF_INFO_NODE(spf_info, level)->node_name, existing_rt_route->dest.prefix, existing_rt_route->dest.mask,
                                    get_str_level(existing_rt_route->level), existing_rt_route->cost,
                                    rt_entry_template->dest.prefix, rt_entry_template->dest.mask, get_str_level(rt_entry_template->level),
                                    rt_entry_template->cost); TRACE();
                        rt_route_update(spf_info->rttable, rt_entry_template);
                        free(rt_entry_template);
                        rt_entry_template = NULL;
                        rt_updated++;
                        break;
                    }
                    else{
                        rt_no_change++;
                        break;
                    }
                }
#if 0
                else
                    assert(0); /* Impossible case*/
#endif
            }
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

    //delete_stale_routes(spf_info, level);
    sprintf(LOG, "Result for Route : %s/%u, L%d : #Added:%u, #Deleted:%u, #Updated:%u, #Unchanged:%u",
            route->rt_key.prefix, route->rt_key.mask, route->level, 
            rt_added, rt_removed, rt_updated, rt_no_change); TRACE();
    printf("Node %s : Result for Route : %s/%u, L%d : #Added:%u, #Deleted:%u, #Updated:%u, #Unchanged:%u\n",
            spf_info->spf_level_info[level].node->node_name, route->rt_key.prefix, route->rt_key.mask, route->level, 
            rt_added, rt_removed, rt_updated, rt_no_change);

}



/*Routine to build the routing table*/

void
start_route_installation(spf_info_t *spf_info,
                         LEVEL level){

    sprintf(LOG, "Entered ... Level : %u", level); TRACE();
    singly_ll_node_t* list_node = NULL;
    routes_t *route = NULL;
    rttable_entry_t *rt_entry_template = NULL;
    rttable_entry_t *existing_rt_route = NULL;
    spf_result_t *spf_res = NULL;

    unsigned int i = 0,
                 rt_added = 0,
                 rt_removed = 0, 
                 rt_updated = 0, 
                 rt_no_change = 0;

    ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){
        
        route = list_node->data;

        if(route->level != level)
            continue;

        if(route->install_state != RTE_NO_CHANGE ||
            route->install_state != RTE_STALE){
             
            rt_entry_template = GET_NEW_RT_ENTRY();

            spf_res = prepare_new_rt_entry_template(spf_info, rt_entry_template, route,
                    spf_info->spf_level_info[level].version); 

            sprintf(LOG, "Template for route : %s/%u, rt_entry_template->primary_nh_count = %u, cost = %u", 
                    rt_entry_template->dest.prefix, rt_entry_template->dest.mask, 
                    rt_entry_template->primary_nh_count, rt_entry_template->cost); TRACE();

            /* use the spf_res return by above fn here to save a search*/
            for(i = 0; i < rt_entry_template->primary_nh_count; i++){
                prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node, 
                        spf_res->next_hop[i], 
                        &rt_entry_template->primary_nh[i],
                        level);
            }

            /*if rt_entry_template->primary_nh_count is zero, it means, it is a local prefix or leaked prefix*/
            if(rt_entry_template->primary_nh_count == 0)
                prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node,
                        spf_info->spf_level_info[level].node,
                        &rt_entry_template->primary_nh[0],
                        level);

            /*Back up Next hop funtionality is not implemented yet*/
            memset(&rt_entry_template->backup_nh, 0, sizeof(nh_t));

            /*rt_entry_template is now ready to be installed in RIB*/
        }
        sprintf(LOG, "RIB modification : route : %s/%u, Level%u, metric = %u, Action : %s", 
                route->rt_key.prefix, route->rt_key.mask, 
                route->level, route->spf_metric, route_intall_status_str(route->install_state)); TRACE();

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
                /*It might be the case that the route already installed in RIB belong to other level, 
                 * we need to check if the route we are installing has better cost or not*/
                {
                    existing_rt_route = rt_route_lookup(spf_info->rttable, 
                            route->rt_key.prefix, route->rt_key.mask);
                    if(existing_rt_route){
                        if(existing_rt_route->level != route->level){
                            if(existing_rt_route->cost > rt_entry_template->cost){
                                sprintf(LOG, "Node %s: RIB Route %s/%u, %s, cost = %d updated with route %s/%u, %s, cost = %d",
                                        GET_SPF_INFO_NODE(spf_info, level)->node_name, existing_rt_route->dest.prefix, existing_rt_route->dest.mask,
                                        get_str_level(existing_rt_route->level), existing_rt_route->cost,
                                        rt_entry_template->dest.prefix, rt_entry_template->dest.mask, get_str_level(rt_entry_template->level),
                                        rt_entry_template->cost); TRACE();
                                rt_route_update(spf_info->rttable, rt_entry_template);
                                free(rt_entry_template);
                                rt_entry_template = NULL;
                                rt_updated++;
                                break;
                            }
                            else{
                                rt_no_change++;
                                break;    
                            }
                        }
                    }
                    if(rt_route_install(spf_info->rttable, rt_entry_template) < 0){
                        printf("%s() : Error : Could not Add route : %s/%u, Level%u\n", __FUNCTION__,
                                route->rt_key.prefix, route->rt_key.mask, route->level);
                        free(rt_entry_template);
                        rt_entry_template = NULL;
                        break;
                    }
                    rt_added++;
                }
                break;
            case RTE_CHANGED:
                existing_rt_route = rt_route_lookup(spf_info->rttable, 
                        route->rt_key.prefix, route->rt_key.mask);
                if(existing_rt_route){
                    if(existing_rt_route->level != route->level){
                        if(existing_rt_route->cost > rt_entry_template->cost){
                            sprintf(LOG, "Node %s: RIB Route %s/%u, %s, cost = %d updated with route %s/%u, %s, cost = %d",
                                    GET_SPF_INFO_NODE(spf_info, level)->node_name, existing_rt_route->dest.prefix, existing_rt_route->dest.mask,
                                    get_str_level(existing_rt_route->level), existing_rt_route->cost,
                                    rt_entry_template->dest.prefix, rt_entry_template->dest.mask, get_str_level(rt_entry_template->level),
                                    rt_entry_template->cost); TRACE();
                            rt_route_update(spf_info->rttable, rt_entry_template);
                            free(rt_entry_template);
                            rt_entry_template = NULL;
                            rt_updated++;
                            break;
                        }
                        else{
                            rt_no_change++;
                            break;
                        }
                    }
                }
                rt_route_update(spf_info->rttable, rt_entry_template);
                free(rt_entry_template);
                rt_entry_template = NULL;
                rt_updated++;
                break;
            case RTE_NO_CHANGE:
                
                /* Note : For unchanged routes, we need to send notification to RIB 
                 * to simply remove the LFA/RLFA for unchanged routes present at this point,
                 * these are stale LFA/RLFA and can cause Microloops*/

                    rt_no_change++;
                    if(!is_singly_ll_empty(route->backup_nh_list))
                        rt_route_remove_backup_nh(spf_info->rttable, 
                                route->rt_key.prefix, route->rt_key.mask);
                    /*Update thge version no in RIB*/
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
    sprintf(LOG, "SPF Stats : L%d, Node : %s : #Added:%u, #Deleted:%u, #Updated:%u, #Unchanged:%u",
            level, spf_info->spf_level_info[level].node->node_name, rt_added, rt_removed, rt_updated, rt_no_change); TRACE();
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
    spf_result_t *result = NULL;
    rttable_entry_t *rt_entry = NULL;

    sprintf(LOG, "Entered ... spf_root : %s, Level : %s", spf_root->node_name, get_str_level(level)); TRACE();
    
    mark_all_routes_stale(spf_info, level);

    /*Walk over the SPF result list computed in spf run
     * in the same order. Note that order of this list is :
     * most distant router from spf root is first*/


    ITERATE_LIST_BEGIN(spf_root->spf_run_result[level], list_node){
        
         result = (spf_result_t *)list_node->data;
       
        /*If computing router is a pure L1 router, and if computing
         * router is connected to L1L2 router with in its own area, then
         * computing router should install the default gw in RIB*/
         
         if(level == LEVEL1                                              &&       /*The current run us LEVEL1*/
                 !IS_BIT_SET(spf_root->instance_flags, IGNOREATTACHED)   &&       /*By default, router should process Attached Bit*/
                 spf_info->spff_multi_area == 0                          &&       /*computing router L1-only router. For L1-only router this bit is never set*/
                 result->node != NULL                                    &&       /*first fragment of the node whose result is being processed exist*/
                 result->node->attached){                                         /*Router being inspected is L1L2 router*/

             /* Installation of Default route in L1-only router RIB*/

             common_pfx_key_t common_pfx;
             memset(&common_pfx, 0, sizeof(common_pfx_key_t));
             if(node_local_prefix_search(result->node, level, common_pfx.prefix, common_pfx.mask) == NULL){

                 /* Default prefix 0.0.0.0/0 is not advertised by 'node' which is L1L2 router*/
                 sprintf(LOG, "Default prefix 0.0.0.0/0 not found in L1L2 node %s prefix db for %s", 
                         result->node->node_name, get_str_level(level));  TRACE();
#if 0
                 prefix_t *prefix = calloc(1, sizeof(prefix_t));
                 fill_prefix(prefix, &common_pfx, 0, FALSE);
                 update_route(spf_info, result, prefix, level, FALSE);
#endif
             }
         }

        /*Iterate over all the prefixes of result->node for level 'level'*/
        
        prefix = NULL;    
        ITERATE_LIST_BEGIN(GET_NODE_PREFIX_LIST(result->node, level), prefix_list_node){
        
            prefix = (prefix_t *)prefix_list_node->data;  
            update_route(spf_info, result, prefix, level, TRUE);
        }ITERATE_LIST_END;
    } ITERATE_LIST_END;


    /*Iterate over all UPDATED routes and figured out which one needs to be updated
     * in RIB*/
    ITERATE_LIST_BEGIN(spf_info->routes_list, list_node){

        route = list_node->data;
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
    }ITERATE_LIST_END;
}

void
spf_postprocessing(spf_info_t *spf_info, /* routes are stored globally*/
                   node_t *spf_root,     /* computing node which stores the result (list) of spf run*/
                   LEVEL level){         /*Level of spf run*/

    
    /*-----------------------------------------------------------------------------
     *  If this is L2 run, then set my spf_info_t->spff_multi_area bit, and schedule
     *  SPF L1 run to ensure L1 routes are uptodate before updating L2 routes
     *-----------------------------------------------------------------------------*/
    sprintf(LOG, "Entered ... "); TRACE();
       
    if(level == LEVEL2 && spf_info->spf_level_info[LEVEL1].version){
        /*If at least 1 SPF L1 run has been triggered*/
        
        spf_determine_multi_area_attachment(spf_info, spf_root);  
        /*Schedule level 1 spf run, just to make sure L1 routes are up
         *      * to date before building L2 routes*/
        //sprintf(LOG, "L2 spf run, triggering L1 SPF run first before building L2 routes"); TRACE();
        //spf_computation(spf_root, spf_info, LEVEL1, FULL_RUN);
    }

    build_routing_table(spf_info, spf_root, level);
    start_route_installation(spf_info, level);
}


/* Single Route add/delete routine. This routine takes case
 * for adding a local prefix and remote prefix routes*/

void
add_route(node_t *lsp_reciever, 
        node_t *lsp_generator,
        LEVEL info_dist_level,
        char *prefix, char mask,
        unsigned int metric,
        FLAG prefix_flags,
        node_t *hosting_node){ /*Hosting node of the prefix, in case is the prefix is leaked, this is the node on which leak was done*/

    unsigned int i = 0; 
    spf_result_t *result = NULL;
    routes_t *route = NULL;
    spf_info_t *spf_info = &lsp_reciever->spf_info;
    prefix_t *_prefix = NULL;
    FLAG is_prefix_added = 0;

    sprintf(LOG, "node %s, prefix add recvd : %s/%u, L%d, metric = %u", 
            lsp_reciever->node_name, prefix, mask, info_dist_level, metric); TRACE();

    if(hosting_node == lsp_reciever){ /*The node is adding a new local prefix*/

        _prefix = node_local_prefix_search(lsp_reciever, info_dist_level, prefix, mask);

        if(!_prefix){
            sprintf(LOG, "Node : %s : prefix %s/%u added to local prefix list at %s",
                    lsp_reciever->node_name, prefix, mask, get_str_level(info_dist_level)); TRACE();

            _prefix = attach_prefix_on_node(lsp_reciever, prefix, mask, info_dist_level, metric, prefix_flags);
        }
    }
    else{

        _prefix = create_new_prefix(prefix, mask);
        init_prefix_key(_prefix, prefix, mask);
        _prefix->hosting_node = hosting_node;
        _prefix->metric = metric;
        _prefix->prefix_flags = prefix_flags;
    }

    route = search_route_in_spf_route_list(spf_info, _prefix, info_dist_level);

    if(!route){
        sprintf(LOG, "node %s, route %s/%u %s not found Routing tree", 
                lsp_reciever->node_name, prefix, mask, get_str_level(info_dist_level)); TRACE(); 

        if(spf_info->spf_level_info[info_dist_level].version == 0){
            sprintf(LOG, "node %s, SPF run at %s has not been run", lsp_reciever->node_name, get_str_level(info_dist_level));
            TRACE();
            spf_computation(lsp_reciever, spf_info, info_dist_level, FULL_RUN);
            return;
        }

        /* Dont create route DS for prefixes with INFINITE_METRIC locally or remotely, 
         * but continue to flood in network*/
        if(_prefix->metric == INFINITE_METRIC){

            sprintf(LOG, "node %s, route %s/%u %s has infinite metric, route creation aborted",
                    lsp_reciever->node_name, _prefix->prefix, _prefix->mask, get_str_level(info_dist_level));
            /* Free the prefix if it is a remote node. 
             * It simply means, No router in the network computes path to any prefix with infinite metric*/
            if(lsp_reciever != lsp_generator){
                free(_prefix);
                _prefix = NULL;   
            }
            return;
        }
        route = route_malloc();
        route_set_key(route, prefix, mask); 

        /*Apply latest spf version we have*/
        route->version = spf_info->spf_level_info[info_dist_level].version;
        route->flags = prefix_flags; 

        route->level = info_dist_level;
        route->hosting_node = hosting_node; /*This route was originally advertised by this node or leaked by this node*/ 

        result = (spf_result_t *)singly_ll_search_by_key(lsp_reciever->spf_run_result[info_dist_level], 
                hosting_node);

        assert(result);

        sprintf(LOG, "prefix hosting node (%s) distance from LSP receiver(%s) = %u at L%d", 
                lsp_generator->node_name, lsp_reciever->node_name, result->spf_metric, info_dist_level);  TRACE();

        route->spf_metric = result->spf_metric + metric;
        route->lsp_metric = 0; /*Not supported*/

        for(; i < MAX_NXT_HOPS; i++){
            if(result->next_hop[i])
                ROUTE_ADD_PRIMARY_NH(route, result->next_hop[i]);   
            else
                break;
        }

        assert(ROUTE_ADD_LIKE_PREFIX_LIST(route, _prefix, result->spf_metric ));
        _prefix->metric = route->spf_metric;

        ROUTE_ADD_TO_ROUTE_LIST(spf_info, route);

        sprintf(LOG, "At node %s, route : %s/%u added to main route list for level%u, metric : %u",  
                lsp_reciever->node_name, route->rt_key.prefix, route->rt_key.mask, route->level, route->spf_metric); TRACE();

        install_route_in_rib(spf_info, info_dist_level, route);        
    }
    /*We have recieved a prefix whose route already exists, it can be local or remote node wrt to prefix*/
    else{
        sprintf(LOG, "At Node %s, route %s/%u, %s found in Routing tree with cost = %u", 
                lsp_reciever->node_name, route->rt_key.prefix, route->rt_key.mask, 
                get_str_level(info_dist_level), route->spf_metric); TRACE(); 

        /* We wil check if the existing route is better than leaked route, then no action.
         * If leaked route is better than existing route, then overwrite the existing route 
         * with leaked route information and update the RIB.
         *
         * Scenario : In build_multi_area_topo(), Say node R0 distribute its Local L2 prefix 
         * 192.168.0.0 in L2 domain. Node R2 will compute the cost to 192.168.0.0 as 40. Now, 
         * Node R2 leaks the route 192.168.0.0 in L1 domain. Node R0, will form LEVEL1 route 
         * to 192.168.0.0 with cost 50 and destination as R2(next-hop = R2). At this point, Node
         * R0 has two routes for 192.168.0.0 - LEVEL2 route with cost 0, and LEVEL1 route with cost 50.
         * However, we are saved because when nodes have two distinct level routes for same prefix,
         * we install only the better one of the two.
         * Now if node R0 leak the prefix 192.168.0.0 to LEVEL1, (This code block hits) it must overwrite the LEVEL1 route
         * since, 192.168.0.0 is a local prefix, and new LEVEL1 route for this prefix would be surely
         * better than what LEVEL1 route exists for prefix 192.168.0.0. Also, node R2 will recieve TLV128 for
         * this prefix leak, and node R2 would replair its bad LEVEL1 route to 192.168.0.0. with cost 50 with cost 10
         * to R0 node.
         * Thus, wrong prefix leaking could do wonders !! So, it is administrative mis-configuration if the admin leak
         * the prefix unappropriately.*/

        spf_result_t *hosting_node_result = NULL;
        prefix_t *best_prefix = ROUTE_GET_BEST_PREFIX(route);
        assert(best_prefix);
        prefix_t *leaked_prefix = NULL;
        hosting_node_result = singly_ll_search_by_key(lsp_reciever->spf_run_result[info_dist_level], hosting_node);
        assert(hosting_node_result);

        is_prefix_added = ROUTE_ADD_LIKE_PREFIX_LIST(route, _prefix, hosting_node_result->spf_metric);

        if(is_prefix_added){

            sprintf(LOG, "Node %s, prefix %s/%u with hosting node %s added to route prefix list at %s", lsp_reciever->node_name, 
                            prefix, mask, hosting_node->node_name, get_str_level(info_dist_level)); TRACE();

            /*Prefix has been added, it could be the first prefix*/
            if(ROUTE_GET_BEST_PREFIX(route) == _prefix){

                sprintf(LOG, "Node %s, prefix %s/%u added is best prefix at %s",  lsp_reciever->node_name, 
                            prefix, mask, get_str_level(info_dist_level)); TRACE(); 

                overwrite_route(spf_info, route, _prefix, hosting_node_result, info_dist_level);
                sprintf(LOG, "Node %s, prefix %s/%u %s old metric : %u, updated new metric : %u", lsp_reciever->node_name,
                        prefix, mask, get_str_level(info_dist_level), _prefix->metric, _prefix->metric + hosting_node_result->spf_metric); TRACE();
                _prefix->metric = route->spf_metric;
                route->install_state = RTE_CHANGED;
                install_route_in_rib(spf_info, info_dist_level, route); 
            }
            else{
                sprintf(LOG, "Node %s, prefix %s/%u added is Inferior prefix at %s",  lsp_reciever->node_name,
                        prefix, mask, get_str_level(info_dist_level)); TRACE();
                sprintf(LOG, "Node %s, prefix %s/%u %s old metric : %u, updated new metric : %u", lsp_reciever->node_name,
                        prefix, mask, get_str_level(info_dist_level), _prefix->metric, _prefix->metric + hosting_node_result->spf_metric); TRACE();
                _prefix->metric += hosting_node_result->spf_metric;
            }

            /*Or may be inferior prefix, even if it is inferior we should
             * promote the leaked prefix to leaked level*/

            LEVEL other_level = get_other_level(info_dist_level);
            leaked_prefix =  node_local_prefix_search(lsp_reciever, other_level, prefix, mask);
            if(!leaked_prefix) {
                sprintf(LOG, "Node %s, prefix %s/%u added is not leaked to level %s. Processing done", lsp_reciever->node_name,
                        prefix, mask, get_str_level(other_level)); TRACE();   
                return;
            }
            if(!IS_BIT_SET(leaked_prefix->prefix_flags, PREFIX_DOWNBIT_FLAG)) {
                sprintf(LOG, "Node %s, prefix %s/%u added is present in level %s, but leak flag not set. Processing done", 
                    lsp_reciever->node_name, prefix, mask, get_str_level(other_level)); TRACE();   
                return;
            }
            /*So we need to promote the prefix to other level, other level will update
             * accordingly*/
            sprintf(LOG, "Node %s, prefix %s/%u being promoted to leaked level %s", 
                        lsp_reciever->node_name, prefix, mask, get_str_level(other_level)); TRACE();

            leaked_prefix = create_new_prefix(prefix, mask);
            leaked_prefix->metric = route->spf_metric;
            leaked_prefix->prefix_flags = _prefix->prefix_flags;
            leaked_prefix->hosting_node = lsp_reciever;
            SET_BIT(leaked_prefix->prefix_flags, PREFIX_DOWNBIT_FLAG);
            SET_BIT(leaked_prefix->prefix_flags, PREFIX_EXTERNABITL_FLAG);
            is_prefix_added = add_prefix_to_prefix_list(GET_NODE_PREFIX_LIST(lsp_reciever, other_level), 
                    leaked_prefix, 0);

            if(is_prefix_added){

                sprintf(LOG, "Node %s, prefix %s/%u with hosting node %s added to node prefix list at leaked level %s", 
                    lsp_reciever->node_name, prefix, mask, leaked_prefix->hosting_node->node_name, get_str_level(other_level)); TRACE();
                    
                route = search_route_in_spf_route_list(spf_info, leaked_prefix, other_level); 
                assert(route);

                if(ROUTE_GET_BEST_PREFIX(route) == leaked_prefix){
                    /*The prefix added is the best prefix, we should update the route*/

                    sprintf(LOG, "Node %s, prefix %s/%u added is best prefix at %s",  lsp_reciever->node_name, 
                            prefix, mask, get_str_level(other_level)); TRACE(); 

                    hosting_node_result = singly_ll_search_by_key(lsp_reciever->spf_run_result[info_dist_level], 
                            lsp_reciever);

                    overwrite_route(spf_info, route, leaked_prefix, hosting_node_result, other_level);
                    //leaked_prefix->metric = route->spf_metric;
                    route->install_state = RTE_CHANGED;
                    install_route_in_rib(spf_info, other_level, route);
                }
                /*Advertise this prefix*/
                 sprintf(LOG, "Node %s, re-distributing leaked prefix %s/%u at level %s", lsp_reciever->node_name,
                    prefix, mask, get_str_level(other_level)); TRACE();

                tlv128_ip_reach_t ad_msg;
                memset(&ad_msg, 0, sizeof(tlv128_ip_reach_t));
                dist_info_hdr_t dist_info_hdr;
                memset(&dist_info_hdr, 0, sizeof(dist_info_hdr_t));
                ad_msg.prefix = leaked_prefix->prefix;
                ad_msg.mask = leaked_prefix->mask;
                ad_msg.metric = route->spf_metric;
                ad_msg.prefix_flags = leaked_prefix->prefix_flags;
                ad_msg.hosting_node = leaked_prefix->hosting_node;
                dist_info_hdr.lsp_generator = lsp_reciever;
                dist_info_hdr.info_dist_level = other_level;
                dist_info_hdr.add_or_remove = AD_CONFIG_ADDED;
                dist_info_hdr.advert_id = TLV128;
                dist_info_hdr.info_data = (char *)&ad_msg;
                //generate_lsp(instance, lsp_reciever, prefix_distribution_routine, &dist_info_hdr);
                generate_lsp(instance, lsp_reciever, lsp_distribution_routine, &dist_info_hdr);
            }
            else{
                sprintf(LOG, "Node %s, clone prefix %s/%u is already present in leaked level %s. Processing done",
                    lsp_reciever->node_name, prefix, mask, get_str_level(other_level)); TRACE(); 
                free(leaked_prefix);
                leaked_prefix = NULL;
            }
        }
        else{
            sprintf(LOG, "Node %s, clone prefix %s/%u is already present in level %s. Processing done",
                    lsp_reciever->node_name, prefix, mask, get_str_level(info_dist_level)); TRACE(); 
            free(leaked_prefix);
            leaked_prefix = NULL;
        }
    }
}


void
delete_route(node_t *lsp_reciever,
        node_t *lsp_generator,
        LEVEL info_dist_level,
        char *prefix, char mask,
        FLAG prefix_flags,
        unsigned int metric,
        node_t *hosting_node){

}
