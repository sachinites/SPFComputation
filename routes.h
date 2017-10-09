/*
 * =====================================================================================
 *
 *       Filename:  routes.h
 *
 *    Description:  This file declares the Data structure for routing table construction
 *
 *        Version:  1.0
 *        Created:  Monday 04 September 2017 12:06:27  IST
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

#ifndef __ROUTES__
#define __ROUTES__

#include "instance.h"

/*Routine to build the routing table*/
typedef enum RTE_INSTALL_STATUS{

    RTE_STALE,
    RTE_ADDED,
    RTE_UPDATED, /*Updated route means, route has been re-calculated in subsequent spf run, but may or may not be changed*/
    RTE_CHANGED,
    RTE_NO_CHANGE
} route_intall_status; 

typedef struct routes_{

    common_pfx_key_t rt_key;
    int version;
    int flags;
    LEVEL level;
    node_t *hosting_node;
    unsigned int spf_metric;
    unsigned int lsp_metric; /*meaningful if this LSP route*/

    /* NH lists*/
    ll_t *primary_nh_list;/*Taking it as a list to accomodate ECMP*/
    ll_t *backup_nh_list; /*List of node_t pointers*/

    /*same subnet prefix lists*/
    ll_t *like_prefix_list; 
    route_intall_status install_state; 

} routes_t;

routes_t *route_malloc();

void
route_set_key(routes_t *route, char *ipv4_addr, char mask);

void
free_route(routes_t *route);

#define ROUTE_ADD_PRIMARY_NH(routeptr, nodeptr)     \
    singly_ll_add_node_by_val(routeptr->primary_nh_list, nodeptr)

#define ROUTE_FLUSH_PRIMARY_NH_LIST(routeptr)       \
    delete_singly_ll(routeptr->primary_nh_list)

#define ROUTE_ADD_BACKUP_NH(routeptr, nodeptr)      \
    singly_ll_add_node_by_val(routeptr->backup_nh_list, nodeptr)

#define ROUTE_FLUSH_BACKUP_NH_LIST(routeptr)   \
    delete_singly_ll(routeptr->backup_nh_list)

#define ROUTE_ADD_LIKE_PREFIX_LIST(routeptr, prefixptr) \
    singly_ll_add_node_by_val(routeptr->like_prefix_list, prefixptr)

#define ROUTE_ADD_TO_ROUTE_LIST(spfinfo_ptr, routeptr)    \
    singly_ll_add_node_by_val(spfinfo_ptr->routes_list, routeptr);   \
    singly_ll_add_node_by_val(spfinfo_ptr->priority_routes_list, routeptr)

#define ROUTE_DEL_FROM_ROUTE_LIST(spfinfo_ptr, routeptr)    \
    singly_ll_delete_node_by_data_ptr(spfinfo_ptr->routes_list, routeptr);  \
    singly_ll_delete_node_by_data_ptr(spfinfo_ptr->priority_routes_list, routeptr)

#define ROUTE_GET_PR_NH_CNT(routeptr)   \
    GET_NODE_COUNT_SINGLY_LL(routeptr->primary_nh_list)


void
spf_postprocessing(spf_info_t *spf_info,      /* routes are stored globally*/
                   node_t *spf_root,          /* computing node which stores the result of spf run*/
                   LEVEL level);              /*Level of spf run*/

void
build_routing_table(spf_info_t *spf_info,
                    node_t *spf_root, LEVEL level);

void
start_route_installation(spf_info_t *spf_info, 
                         LEVEL level);

int
route_search_comparison_fn(void * route, void *key);

typedef struct rttable_entry_ rttable_entry_t;

void
prepare_new_rt_entry_template(rttable_entry_t *rt_entry_template,
                               routes_t *route, unsigned int version);

typedef struct nh_t_ nh_t;

void
prepare_new_nxt_hop_template(node_t *computing_node, /*Computing node running SPF*/
        node_t *nxt_hop_node,   /*Nbr node*/
        nh_t *nh_template,
        LEVEL level);           /*SPF run level*/

void
add_route(node_t *lsp_reciever, 
        node_t *lsp_generator,
        LEVEL info_dist_level,
        char *prefix, char mask,
        LEVEL level, unsigned int metric);

void
delete_route(node_t *lsp_reciever,
        node_t *lsp_generator,
        LEVEL info_dist_level,
        char *prefix, char mask,
        LEVEL level, unsigned int metric);


#endif /* __ROUTES__ */
