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

    nh_type_t nh;
    routes_t *route = calloc(1, sizeof(routes_t));
    ITERATE_NH_TYPE_BEGIN(nh){
        route->primary_nh_list[nh] = init_singly_ll();
        singly_ll_set_comparison_fn(route->primary_nh_list[nh], instance_node_comparison_fn);
    }
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
    nh_type_t nh; 
    singly_ll_node_t* list_node = NULL; 
    route->hosting_node = 0;
    
    ITERATE_NH_TYPE_BEGIN(nh){
        ROUTE_FLUSH_PRIMARY_NH_LIST(route, nh);
        free(route->primary_nh_list[nh]);
        route->primary_nh_list[nh] = 0;
    }

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
        node_t *nxt_hop_node,
        nh_t *nh_template,
        LEVEL level, nh_type_t nh){

    assert(nh_template);
    assert(nxt_hop_node);
    assert(computing_node);

    edge_end_t *oif = NULL;
    char gw_prefix[PREFIX_LEN + 1];
   
    strncpy(nh_template->nh_name, nxt_hop_node->node_name, NODE_NAME_SIZE);
    nh_template->nh_name[NODE_NAME_SIZE - 1] = '\0';


    /*oif can be NULL if computing_node == nxt_hop_node. It will happen
     * for local prefixes */
    oif = get_min_oif(computing_node, nxt_hop_node, level, gw_prefix, nh);
    nh_template->nh_type = nh;

    if(!oif){
        strncpy(nh_template->gwip, "Direct" , PREFIX_LEN + 1);
        nh_template->gwip[PREFIX_LEN] = '\0';
        return;
    }

    strncpy(nh_template->oif, oif->intf_name, IF_NAME_SIZE);
    nh_template->oif[IF_NAME_SIZE -1] = '\0';

    if(nh_template->nh_type == IPNH){
        strncpy(nh_template->gwip, gw_prefix, PREFIX_LEN + 1);
        nh_template->gwip[PREFIX_LEN] = '\0';
    }
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
is_same_next_hop(node_t* computing_node, 
                        node_t *nbr_node,
                        nh_t *nh, LEVEL level, nh_type_t nh_type){

    nh_t nh_temp;
    memset(&nh_temp, 0, sizeof(nh_t));

    prepare_new_nxt_hop_template(computing_node, nbr_node, &nh_temp, level, nh_type);
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
    nh_type_t nh;
    node_t *computing_node = spf_info->spf_level_info[level].node;
    node_t *nbr_node = NULL;

    ITERATE_NH_TYPE_BEGIN(nh){
        
        ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node){
            nbr_node = list_node->data; /*This is nbr node of computing node*/
            if(is_same_next_hop(computing_node, nbr_node, 
                    &rt_entry->primary_nh[nh][i++], level, nh) == FALSE)
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

    nh_type_t nh;
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
       case PRC_RUN:
           ITERATE_NH_TYPE_BEGIN(nh){

               if((strncmp(rt_entry->dest.prefix, route->rt_key.prefix, PREFIX_LEN + 1) == 0)  &&
                       (rt_entry->dest.mask == route->rt_key.mask)                             &&
                       //(rt_entry->cost == route->spf_metric)                                 &&
                       (rt_entry->primary_nh_count[nh] == ROUTE_GET_PR_NH_CNT(route, nh))      && 
                       (route_rib_same_next_hops(spf_info, rt_entry, route, level) == TRUE))
                   return FALSE;
               return TRUE; 
           } ITERATE_NH_TYPE_END;
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
        nh_type_t nh = NH_MAX;

        route_set_key(route, prefix->prefix, prefix->mask); 

        sprintf(LOG, "route : %s/%u being over written for %s", route->rt_key.prefix, 
                    route->rt_key.mask, get_str_level(level)); TRACE();

        assert(IS_LEVEL_SET(route->level, level));
       
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
            /* route->backup_nh_list Not supported yet */
            ROUTE_FLUSH_BACKUP_NH_LIST(route); /*Not supported*/ 
            for(i = 0 ; i < MAX_NXT_HOPS; i++){
                if(result->next_hop[nh][i])
                    ROUTE_ADD_PRIMARY_NH(route->primary_nh_list[nh], result->next_hop[nh][i]);   
                else
                    break;
            }
        } ITERATE_NH_TYPE_END;
}

static void
link_prefix_to_route_prefix_list(routes_t *route, prefix_t *new_prefix,
                                 unsigned int prefix_hosting_node_metric){

    singly_ll_node_t *list_node_prev = NULL,
                     *list_node_next = NULL,
                     *list_new_node = NULL;

    prefix_t *list_prefix = NULL;
    LEVEL level = route->level;
    unsigned int new_prefix_metric = 0;

    prefix_pref_data_t list_prefix_pref = {ROUTE_UNKNOWN_PREFERENCE, 
                                          "ROUTE_UNKNOWN_PREFERENCE"},

                        new_prefix_pref = route_preference(new_prefix->prefix_flags, level);

    sprintf(LOG, "To Route : %s/%u, %s, Appending prefix : %s/%u to Route prefix list",
                 route->rt_key.prefix, route->rt_key.mask, get_str_level(route->level),
                 new_prefix->prefix, new_prefix->mask); TRACE();

    if(is_singly_ll_empty(route->like_prefix_list)){
        singly_ll_add_node_by_val(route->like_prefix_list, new_prefix);
        return;
    }

    list_new_node = singly_ll_init_node(new_prefix);

    ITERATE_LIST_BEGIN(route->like_prefix_list, list_node_next){
        
        list_prefix = (prefix_t *)list_node_next->data;
        list_prefix_pref = route_preference(list_prefix->prefix_flags, level);

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

        if(IS_BIT_SET(new_prefix->prefix_flags, PREFIX_METRIC_TYPE_EXT)) 
            new_prefix_metric = prefix_hosting_node_metric;   
        else
            new_prefix_metric = prefix_hosting_node_metric + new_prefix->metric;

        if(new_prefix->metric >= INFINITE_METRIC)
            new_prefix_metric = INFINITE_METRIC; 
        
        if(new_prefix_metric > list_prefix->metric){
            list_node_prev = list_node_next;
            continue;
        }

        if(new_prefix_metric < list_prefix->metric){
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

    prefix_pref_data_t prefix_pref = {ROUTE_UNKNOWN_PREFERENCE, "ROUTE_UNKNOWN_PREFERENCE"},
                        route_pref = {ROUTE_UNKNOWN_PREFERENCE, "ROUTE_UNKNOWN_PREFERENCE"};
                        


    sprintf(LOG, "Node : %s : result node %s, prefix %s, level %s, prefix metric : %u",
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
             
            for(; i < MAX_NXT_HOPS; i++){
                if(result->next_hop[nh][i]){
                    ROUTE_ADD_PRIMARY_NH(route->primary_nh_list[nh], result->next_hop[nh][i]);   
                    sprintf(LOG, "Node : %s : route : %s/%u Next hop added : %s|%s at %s", 
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask ,
                    result->next_hop[nh][i]->node_name, nh == IPNH ? "IPNH":"LSPNH", get_str_level(level)); TRACE();
                }
                else
                    break;
            }
        }
        /* route->backup_nh_list Not supported yet */

        /*Linkage*/
        route_prefix = calloc(1, sizeof(prefix_t));
        memcpy(route_prefix, prefix, sizeof(prefix_t));
        link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);

        ROUTE_ADD_TO_ROUTE_LIST(spf_info, route);
        route->install_state = RTE_ADDED;
        sprintf(LOG, "Node : %s : route : %s/%u, spf_metric = %u, lsp_metric = %u,  marked RTE_ADDED for level%u",  
            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask, 
            route->spf_metric, route->lsp_metric, route->level); TRACE();
    }
    else{
        sprintf(LOG, "Node : %s : route : %s/%u existing route. route verion : %u," 
                    "spf version : %u, route level : %s, spf level : %s", 
                    GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->prefix, prefix->mask, route->version, 
                    spf_info->spf_level_info[level].version, get_str_level(route->level), get_str_level(level)); TRACE();

        if(route->install_state == RTE_ADDED){
          
/* Comparison Block Start*/

            prefix_pref = route_preference(prefix->prefix_flags, level);
            route_pref  = route_preference(route->flags, route->level);

            if(prefix_pref.pref == ROUTE_UNKNOWN_PREFERENCE){
                sprintf(LOG, "Node : %s : Prefix : %s/%u pref = %s, ignoring prefix",  GET_SPF_INFO_NODE(spf_info, level)->node_name,
                              prefix->prefix, prefix->mask, prefix_pref.pref_str); TRACE();
                return;
            }

            if(route_pref.pref < prefix_pref.pref){
                
                /* if existing route is better*/ 
                sprintf(LOG, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Not overwritten",
                              GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                              route_pref.pref_str, prefix_pref.pref_str); TRACE();
                /*Linkage*/
                route_prefix = calloc(1, sizeof(prefix_t));
                memcpy(route_prefix, prefix, sizeof(prefix_t));
                link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
                return;
            }
            /* If new prefix is better*/
            else if(prefix_pref.pref < route_pref.pref){
                
                sprintf(LOG, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, will be overwritten",
                              GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                              route_pref.pref_str, prefix_pref.pref_str); TRACE();

                overwrite_route(spf_info, route, prefix, result, level);
                /*Linkage*/
                route_prefix = calloc(1, sizeof(prefix_t));
                memcpy(route_prefix, prefix, sizeof(prefix_t));
                link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
                return;
            }
            /* If prefixes are of same preference*/ 
            else{
                /* If route pref = prefix pref, then decide based on metric*/
                sprintf(LOG, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Same preference, Trying based on metric",
                              GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                              route_pref.pref_str, prefix_pref.pref_str); TRACE();
                
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
                    sprintf(LOG, "Node : %s : route : %s/%u Deciding based on External metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask); TRACE(); 

                    if(prefix->metric < route->ext_metric){
                        sprintf(LOG, "Node : %s : prefix external metric ( = %u) is better than routes external metric( = %u), will overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); TRACE();
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(prefix->metric > route->ext_metric){
                        sprintf(LOG, "Node : %s : prefix external metric ( = %u) is no better than routes external metric( = %u), will not overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); TRACE();
                    }
                    else{
                        sprintf(LOG, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask); TRACE();
                                /* Union LFA,s RLFA,s Primary nexthops*/

                        ITERATE_NH_TYPE_BEGIN(nh){
                            UNION_ROUTE_PRIMARY_NEXTHOPS(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    /*Linkage*/
                    route_prefix = calloc(1, sizeof(prefix_t));
                    memcpy(route_prefix, prefix, sizeof(prefix_t));
                    link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
                    return;

                }else{
                    /*Decide pref based on internal metric*/
                    sprintf(LOG, "Node : %s : route : %s/%u Deciding based on Internal metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask); TRACE();
                    if(result->spf_metric + prefix->metric < route->spf_metric){
                        sprintf(LOG, "Node : %s : route : %s/%u is over-written because better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); TRACE();
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(result->spf_metric + prefix->metric == route->spf_metric){
                        sprintf(LOG, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask); TRACE();
                        /* Union LFA,s RLFA,s Primary nexthops*/ 
                        ITERATE_NH_TYPE_BEGIN(nh){
                            UNION_ROUTE_PRIMARY_NEXTHOPS(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    else{
                        sprintf(LOG, "Node : %s : route : %s/%u is not over-written because no better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); TRACE();
                    }
                    /*Linkage*/
                    route_prefix = calloc(1, sizeof(prefix_t));
                    memcpy(route_prefix, prefix, sizeof(prefix_t));
                    link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
                    return;
                }
            }
 /* Comparison Block Ends*/
        }
        
        /* RTE_UPDATED route only*/ 
        if(route->version == spf_info->spf_level_info[level].version){
            
            
/* Comparison Block Start*/

            prefix_pref = route_preference(prefix->prefix_flags, level);
            route_pref = route_preference(route->flags, route->level);

            if(prefix_pref.pref == ROUTE_UNKNOWN_PREFERENCE){
                sprintf(LOG, "Node : %s : Prefix : %s/%u pref = %s, ignoring prefix",  GET_SPF_INFO_NODE(spf_info, level)->node_name,
                              prefix->prefix, prefix->mask, prefix_pref.pref_str); TRACE();
                return;
            }

            /*If prc run, then set route->install_state = RTE_UPDATED else it would stay set to RTE_STALE;
             * */
            if(SPF_RUN_TYPE(GET_SPF_INFO_NODE(spf_info, level), level) == PRC_RUN){
                route->install_state = RTE_UPDATED;
                sprintf(LOG, "Node : %s : PRC route : %s/%u %s STATE set to %s", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask, get_str_level(level), route_intall_status_str(RTE_UPDATED)); TRACE();
            }

            if(route_pref.pref < prefix_pref.pref){
                
                /* if existing route is better*/ 
                sprintf(LOG, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Not overwritten",
                              GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                              route_pref.pref_str, prefix_pref.pref_str); TRACE();
                /*Linkage*/
                route_prefix = calloc(1, sizeof(prefix_t));
                memcpy(route_prefix, prefix, sizeof(prefix_t));
                link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
                return;
            }
            /* If new prefix is better*/
            else if(prefix_pref.pref < route_pref.pref){
                
                sprintf(LOG, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, will be overwritten",
                              GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                              route_pref.pref_str, prefix_pref.pref_str); TRACE();

                overwrite_route(spf_info, route, prefix, result, level);
                /*Linkage*/
                route_prefix = calloc(1, sizeof(prefix_t));
                memcpy(route_prefix, prefix, sizeof(prefix_t));
                link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
                return;
            }
            /* If prefixes are of same preference*/ 
            else{
                /* If route pref = prefix pref, then decide based on metric*/
                sprintf(LOG, "Node : %s : route : %s/%u preference = %s, prefix  pref = %s, Same preference, Trying based on metric",
                              GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask,
                              route_pref.pref_str, prefix_pref.pref_str); TRACE();
                
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
                    sprintf(LOG, "Node : %s : route : %s/%u Deciding based on External metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask); TRACE(); 

                    if(prefix->metric < route->ext_metric){
                        sprintf(LOG, "Node : %s : prefix external metric ( = %u) is better than routes external metric( = %u), will overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); TRACE();
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(prefix->metric > route->ext_metric){
                        sprintf(LOG, "Node : %s : prefix external metric ( = %u) is no better than routes external metric( = %u), will not overwrite",
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, prefix->metric, route->ext_metric); TRACE();
                    }
                    else{
                        sprintf(LOG, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask); TRACE();
                                /* Union LFA,s RLFA,s Primary nexthops*/
                    }
                    /*Linkage*/
                    route_prefix = calloc(1, sizeof(prefix_t));
                    memcpy(route_prefix, prefix, sizeof(prefix_t));
                    link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
                    return;

                }else{
                    /*Decide pref based on internal metric*/
                    sprintf(LOG, "Node : %s : route : %s/%u Deciding based on Internal metric",
                            GET_SPF_INFO_NODE(spf_info, level)->node_name, route->rt_key.prefix, route->rt_key.mask); TRACE();
                    if(result->spf_metric + prefix->metric < route->spf_metric){
                        sprintf(LOG, "Node : %s : route : %s/%u is over-written because better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); TRACE();
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    else if(result->spf_metric + prefix->metric == route->spf_metric){
                        sprintf(LOG, "Node : %s : route : %s/%u hits ecmp case", GET_SPF_INFO_NODE(spf_info, level)->node_name,
                                route->rt_key.prefix, route->rt_key.mask); TRACE();
                          /* Union LFA,s RLFA,s Primary nexthops*/ 
                        ITERATE_NH_TYPE_BEGIN(nh){
                            UNION_ROUTE_PRIMARY_NEXTHOPS(route, result, nh);
                        } ITERATE_NH_TYPE_END;
                    }
                    else{
                        sprintf(LOG, "Node : %s : route : %s/%u is not over-written because no better metric on node %s is found with metric = %u, old route metric = %u", 
                                GET_SPF_INFO_NODE(spf_info, level)->node_name, 
                                route->rt_key.prefix, route->rt_key.mask, result->node->node_name, 
                                result->spf_metric + prefix->metric, route->spf_metric); TRACE();
                    }
                    /*Linkage*/
                    route_prefix = calloc(1, sizeof(prefix_t));
                    memcpy(route_prefix, prefix, sizeof(prefix_t));
                    link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
                    return;

                }
            }
 /* Comparison Block Ends*/
            
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
            
            /*Linkage*/ 
            route_prefix = calloc(1, sizeof(prefix_t));
            memcpy(route_prefix, prefix, sizeof(prefix_t));
            link_prefix_to_route_prefix_list(route, route_prefix, result->spf_metric);
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

    nh_type_t nh;
    singly_ll_node_t *list_node = NULL;

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

    ITERATE_NH_TYPE_BEGIN(nh){
        sprintf(LOG, "route : %s/%u, rt_entry_template->primary_nh_count = %u(%s)", 
                rt_entry_template->dest.prefix, rt_entry_template->dest.mask, 
                rt_entry_template->primary_nh_count[nh], nh == IPNH ? "IPNH" : "LSPNH"); TRACE();
    } ITERATE_NH_TYPE_END ;

    ITERATE_NH_TYPE_BEGIN(nh){

        /*if rt_entry_template->primary_nh_count is zero, it means, it is a local prefix*/
        if(rt_entry_template->primary_nh_count[nh] == 0){
            prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node,
                    spf_info->spf_level_info[level].node,
                    &rt_entry_template->primary_nh[nh][0],
                    level, nh);
        }
        else
        {
            ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node){
                prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node, 
                        list_node->data,
                        &rt_entry_template->primary_nh[nh][i],
                        level, nh);
            } ITERATE_LIST_END;
        }

    } ITERATE_NH_TYPE_END;

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
    singly_ll_node_t *list_node = NULL, 
                     *list_node2 = NULL;

    routes_t *route = NULL;
    rttable_entry_t *rt_entry_template = NULL;
    rttable_entry_t *existing_rt_route = NULL;
    nh_type_t nh;

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

            prepare_new_rt_entry_template(spf_info, rt_entry_template, route,
                    spf_info->spf_level_info[level].version); 

            ITERATE_NH_TYPE_BEGIN(nh){        
                sprintf(LOG, "Template for route : %s/%u, rt_entry_template->primary_nh_count = %u(%s), cost = %u", 
                        rt_entry_template->dest.prefix, rt_entry_template->dest.mask, 
                        rt_entry_template->primary_nh_count[nh], 
                        nh == IPNH ? "IPNH" : "LSPNH", rt_entry_template->cost); TRACE();

                /*if rt_entry_template->primary_nh_count is zero, it means, it is a local prefix or leaked prefix*/
                if(rt_entry_template->primary_nh_count[nh] == 0){

                    prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node,
                            spf_info->spf_level_info[level].node,
                            &rt_entry_template->primary_nh[nh][0],
                            level, nh);
                }
                else{
                    ITERATE_LIST_BEGIN(route->primary_nh_list[nh], list_node2){
                        prepare_new_nxt_hop_template(spf_info->spf_level_info[level].node, 
                                list_node2->data, 
                                &rt_entry_template->primary_nh[nh][i],
                                level, nh);

                    } ITERATE_LIST_END;
                }

            } ITERATE_NH_TYPE_END;
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
