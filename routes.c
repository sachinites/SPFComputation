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

extern instance_t *instance;
extern void
spf_computation(node_t *spf_root,
                spf_info_t *spf_info,
                LEVEL level, spf_type_t spf_type);
extern int
instance_node_comparison_fn(void *_node, void *input_node_name);

THREAD_NODE_TO_STRUCT(prefix_t,
        like_prefix_thread,
        get_prefix_from_like_prefix_thread);


routes_t *
route_malloc(){

    routes_t *route = calloc(1, sizeof(routes_t));
    route->primary_nh_list = init_singly_ll();
    singly_ll_set_comparison_fn(route->primary_nh_list, instance_node_comparison_fn);
    route->backup_nh_list = init_singly_ll();
    singly_ll_set_comparison_fn(route->backup_nh_list, instance_node_comparison_fn);
    route->like_prefix_list = init_singly_ll();
    singly_ll_set_comparison_fn(route->like_prefix_list, get_prefix_comparison_fn());
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
    
    route->hosting_node = 0;
    
    ROUTE_FLUSH_PRIMARY_NH_LIST(route);
    free(route->primary_nh_list);
    route->primary_nh_list = 0;

    ROUTE_FLUSH_BACKUP_NH_LIST(route);
    free(route->backup_nh_list);
    route->backup_nh_list = 0;

    delete_singly_ll(route->like_prefix_list);
    free(route->like_prefix_list);
    route->like_prefix_list = NULL;
}

static inline routes_t *
search_prefix_in_spf_route_list(spf_info_t *spf_info, 
                                prefix_t *prefix, LEVEL level){

    common_pfx_key_t rt_key;
    apply_mask(prefix->prefix, prefix->mask, rt_key.prefix);
    rt_key.prefix[PREFIX_LEN] = '\0';
    rt_key.mask = prefix->mask;
    routes_t *route = singly_ll_search_by_key(spf_info->routes_list, &rt_key);

    if(!route)
        return NULL;

    if(level == route->level)
        return route;
    
    return NULL;
}

static void 
overwrite_route(spf_info_t *spf_info, routes_t *route, 
                prefix_t *prefix, spf_result_t *result, LEVEL level){

        unsigned int i = 0;
    
        route_set_key(route, prefix->prefix, prefix->mask); 

        sprintf(LOG, "route : %s/%u being over written for level%d", route->rt_key.prefix, 
                    route->rt_key.mask, level); TRACE();

        assert(route->level == level);
       
        route->version = spf_info->spf_level_info[level].version;
        route->flags = prefix->prefix_flags;
        
        route->level = level;
        route->hosting_node = prefix->hosting_node;

        route->spf_metric = result->spf_metric;
        route->lsp_metric = 0; /*Not supported*/

        ROUTE_FLUSH_PRIMARY_NH_LIST(route);

        for(; i < MAX_NXT_HOPS; i++){
            if(result->node->next_hop[level][i])
                ROUTE_ADD_PRIMARY_NH(route, result->node->next_hop[level][i]);   
            else
                break;
        }
        
        /* route->backup_nh_list Not supported yet */
}

static void
route_clone(routes_t *route){


}

static void
update_route(spf_info_t *spf_info,          /*spf_info of computing node*/ 
             spf_result_t *result,          /*result representing some network node*/
             prefix_t *prefix,              /*local prefix hosted on 'result' node*/
             LEVEL level, int linkage,
             unsigned int *rt_added,        
             unsigned int *rt_removed, 
             unsigned int *rt_updated,
             unsigned int *rt_no_change){

    routes_t *route = NULL;
    unsigned int i = 0;
     
    if(prefix->metric > INFINITE_METRIC){
        sprintf(LOG, "prefix : %s/%u discarded because of infinite metric", prefix->prefix, prefix->mask); TRACE();
        return;
    }
    
    route = search_prefix_in_spf_route_list(spf_info, prefix, level);
    
    if(!route){
        sprintf(LOG, "prefix : %s/%u is a New route", 
                prefix->prefix, prefix->mask); TRACE();

        route = route_malloc();
        route_set_key(route, prefix->prefix, prefix->mask); 
       
        route->version = spf_info->spf_level_info[level].version;
        route->flags = prefix->prefix_flags;
        
        route->level = level;
        route->hosting_node = prefix->hosting_node;

        route->spf_metric = result->spf_metric + prefix->metric;
        route->lsp_metric = 0; /*Not supported*/

        for(; i < MAX_NXT_HOPS; i++){
            if(result->node->next_hop[level][i])
                ROUTE_ADD_PRIMARY_NH(route, result->node->next_hop[level][i]);   
            else
                break;
        }
        /* route->backup_nh_list Not supported yet */

        ROUTE_ADD_LIKE_PREFIX_LIST(route, prefix);
        ROUTE_ADD(spf_info, route);

        sprintf(LOG, "route : %s/%u added to main route list for level%u",  
            route->rt_key.prefix, route->rt_key.mask, route->level); TRACE();
        (*rt_added)++;
    }
    else{

        sprintf(LOG, "route : %s/%u existing route. route verion : %u," 
                    "spf version : %u, route level : %d, spf level : %d", 
                    prefix->prefix, prefix->mask, route->version, 
                    spf_info->spf_level_info[level].version, route->level, level); TRACE();

        if(route->install_state == RTE_ADDED){
            if(result->spf_metric + prefix->metric < route->spf_metric){
                sprintf(LOG, "route : %s/%u is over-written", 
                        route->rt_key.prefix, route->rt_key.mask); TRACE();
                overwrite_route(spf_info, route, prefix, result, level);
            }
            ROUTE_ADD_LIKE_PREFIX_LIST(route, prefix);
            return;
        }
        
        /* RTE_UPDATED route only*/ 
        if(route->version == spf_info->spf_level_info[level].version){ 
            if(result->spf_metric + prefix->metric < route->spf_metric){
                sprintf(LOG, "route : %s/%u is over-written", 
                        route->rt_key.prefix, route->rt_key.mask); TRACE();
                overwrite_route(spf_info, route, prefix, result, level);
            }
            ROUTE_ADD_LIKE_PREFIX_LIST(route, prefix);
        }
        else
        {
                /*route is from prev run and exists. This code hits only once per given route*/
                sprintf(LOG, "route : %s/%u, updated route(?)", 
                    route->rt_key.prefix, route->rt_key.mask); TRACE();
                    route->install_state = RTE_UPDATED;
                    route_clone(route);
                    if(result->spf_metric + prefix->metric < route->spf_metric){ 
                        sprintf(LOG, "route : %s/%u is over-written", 
                                route->rt_key.prefix, route->rt_key.mask); TRACE();
                        overwrite_route(spf_info, route, prefix, result, level);
                    }
                    ROUTE_ADD_LIKE_PREFIX_LIST(route, prefix);
        }
    }
}

/*Routine to build the routing table*/

void
start_route_installation(spf_info_t *spf_info,
                         LEVEL level){

    sprintf(LOG, "Entered ... Level : %u", level); TRACE();
    singly_ll_node_t* list_node = NULL;
    routes_t *route = NULL;
    rttable_entry_t *rt_entry_template = NULL;
    unsigned int i = 0;

    ITERATE_LIST(spf_info->routes_list, list_node){
        
        route = list_node->data;

        if(route->install_state == RTE_NO_CHANGE)
            continue;   
             
        rt_entry_template = GET_NEW_RT_ENTRY();

        prepare_new_rt_entry_template(rt_entry_template, 
                        route->rt_key.prefix, route->rt_key.mask);

        /*This route is computed in this spf version run*/
        rt_entry_template->version = spf_info->spf_level_info[level].version; 
        rt_entry_template->cost = route->spf_metric;
        rt_entry_template->primary_nh_count = 
                    next_hop_count(route->hosting_node->next_hop[level]);

        sprintf(LOG, "route : %s/%u, rt_entry_template->primary_nh_count = %u", 
                rt_entry_template->dest.prefix, rt_entry_template->dest.mask, 
                rt_entry_template->primary_nh_count); TRACE();

        for(i = 0; i < rt_entry_template->primary_nh_count; i++){
            prepare_new_nxt_hop_template(spf_info->node, 
                                        route->hosting_node->next_hop[level][i], 
                                        &rt_entry_template->primary_nh[i],
                                        level);
        }
        
        /*Back up Next hop funtionality is not implemented yet*/
        memset(&rt_entry_template->backup_nh, 0, sizeof(nh_t));

        /*rt_entry_template is now ready to be installed in RIB*/

        if(rt_route_install(spf_info->rttable, rt_entry_template) < 0){
            free(rt_entry_template);
            rt_entry_template = NULL;
        }
    }
}

static void
delete_stale_routes(spf_info_t *spf_info, LEVEL level){

   singly_ll_node_t* list_node = NULL;
   routes_t *route = NULL;
   unsigned int i = 0;
   ITERATE_LIST(spf_info->routes_list, list_node){
           
       route = list_node->data;
       if(route->install_state == RTE_STALE && route->level == level){
        sprintf(LOG, "route : %s/%u is STALE and is deleted", route->rt_key.prefix, 
                        route->rt_key.mask); TRACE();
        i++;
        singly_ll_remove_node(spf_info->routes_list, list_node);
        free(list_node);
        free_route(route);
        route = NULL;
       }
   }
   sprintf(LOG, "# of Stale entries Deleted = %u", i); TRACE(); 
}


static void
mark_all_routes_stale(spf_info_t *spf_info, LEVEL level){

   singly_ll_node_t* list_node = NULL;
   routes_t *route = NULL;
   
   ITERATE_LIST(spf_info->routes_list, list_node){
           
       route = list_node->data;
       if(route->level != level)
           continue;
       route->install_state = RTE_STALE;
       delete_singly_ll(route->like_prefix_list);
   }
}

void
build_routing_table(spf_info_t *spf_info,
                    node_t *spf_root, LEVEL level){

    singly_ll_node_t *list_node = NULL,
                     *prefix_list_node = NULL;

    unsigned int rt_added = 0;
    unsigned int rt_removed = 0; 
    unsigned int rt_updated = 0;
    unsigned int rt_no_change = 0;

    prefix_t *prefix = NULL;
    spf_result_t *result = NULL;

    sprintf(LOG, "Entered ... spf_root : %s, Level : %u", spf_root->node_name, level); TRACE();
    
    /*Walk over the SPF result list computed in spf run
     * in the same order. Note that order of this list is :
     * most distant router from spf root is first*/


    ITERATE_LIST(spf_root->spf_run_result[level], list_node){
        
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
        
        ITERATE_LIST(GET_NODE_PREFIX_LIST(result->node, level), prefix_list_node){
        
            prefix = (prefix_t *)prefix_list_node->data;  
            update_route(spf_info, result, prefix, level, TRUE, 
                        &rt_added, &rt_removed, 
                        &rt_updated, &rt_no_change);
        }

        /*Add alias routes here
         * */

    } /*ITERATE_LIST ENDS*/
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
        sprintf(LOG, "L2 spf run, triggering L1 SPF run first before building L2 routes"); TRACE();
        spf_computation(spf_root, spf_info, LEVEL1, FULL_RUN);
    }

    mark_all_routes_stale(spf_info, level);
    build_routing_table(spf_info, spf_root, level);
    delete_stale_routes(spf_info, level);
    start_route_installation(spf_info, level);
}

