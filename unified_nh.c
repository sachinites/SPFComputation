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
#include "spftrace.h"

extern instance_t *instance;

unsigned int
get_direct_un_next_hop_metric(internal_un_nh_t *nh){
    return nh->root_metric;
}

node_t *
get_un_next_hop_node(internal_un_nh_t *nh){
    return nh->nh_node;
}

char *
get_un_next_hop_gateway_pfx(internal_un_nh_t *nh){
    return nh->gw_prefix;
}

char *
get_un_next_hop_oif_name(internal_un_nh_t *nh){
    return nh->oif;
}

char *
get_un_next_hop_protected_link_name(internal_un_nh_t *nh){
    return nh->protected_link->intf_name;
}

void
set_un_next_hop_gw_pfx(internal_un_nh_t *nh, char *pfx){
    strncpy(nh->gw_prefix, pfx, PREFIX_LEN);
    nh->gw_prefix[PREFIX_LEN] = '\0';
}

boolean
is_un_next_hop_empty(internal_un_nh_t *nh){
    return nh->oif == NULL;
}

/*Fill the nh_type before calling this fn*/
void
init_un_next_hop(internal_un_nh_t *nh){
    memset(nh, 0 , sizeof(internal_un_nh_t));
}

void
copy_un_next_hop_t(internal_un_nh_t *src, internal_un_nh_t *dst){
    memcpy(dst, src, sizeof(internal_un_nh_t));
}

boolean
is_un_nh_t_equal(internal_un_nh_t *nh1, internal_un_nh_t *nh2){
    return memcmp(nh1, nh2, sizeof(internal_un_nh_t)) == 0;
}

void
free_un_nexthop(internal_un_nh_t *nh){
    free(nh);
}

internal_un_nh_t *
malloc_un_nexthop(){
    return calloc(1, sizeof(internal_un_nh_t));
}

void
free_rt_un_entry(rt_un_entry_t *rt_un_entry){

    singly_ll_node_t *list_node = NULL;
    internal_un_nh_t *nxt_hop = NULL;

    ITERATE_LIST_BEGIN(rt_un_entry->primary_nh_list, list_node){
        nxt_hop = list_node->data;
        free_un_nexthop(nxt_hop);    
    } ITERATE_LIST_END;
    delete_singly_ll(rt_un_entry->primary_nh_list);
    free(rt_un_entry->primary_nh_list);
    
    ITERATE_LIST_BEGIN(rt_un_entry->backup_nh_list, list_node){
        nxt_hop = list_node->data;
        free_un_nexthop(nxt_hop);    
    } ITERATE_LIST_END;

    delete_singly_ll(rt_un_entry->backup_nh_list);
    free(rt_un_entry->backup_nh_list);
    remove_glthread(&rt_un_entry->glthread);
    free(rt_un_entry);
}

/*Rib functions*/

static boolean
inet_0_rt_un_route_install(rt_un_table_t *rib, rt_un_entry_t *rt_un_entry){
    
    sprintf(instance->traceopts->b, "RIB : %s : Added route %s/%d to Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);
    /*Refresh time before adding an enntry*/
    time(&rt_un_entry->last_refresh_time);
    glthread_add_next(&rib->head, &rt_un_entry->glthread);
    rib->count++;
    return TRUE;
}

static rt_un_entry_t *
inet_0_rt_un_route_lookup(rt_un_table_t *rib, rt_key_t *rt_key){
     
    glthread_t *curr = NULL;
    rt_un_entry_t *rt_un_entry = NULL;
    
    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){
        
       rt_un_entry = glthread_to_rt_un_entry(curr);
       if(UN_RTENTRY_PFX_MATCH(rt_un_entry, rt_key))
           return rt_un_entry;    
    } ITERATE_GLTHREAD_END(&rib->head, curr);

    return NULL;
}

static boolean
inet_0_rt_un_route_update(rt_un_table_t *rib, rt_un_entry_t *rt_un_entry){

    rt_un_entry_t *rt_un_entry1 =
        rib->rt_un_route_lookup(rib, &rt_un_entry->rt_key);

    if(!rt_un_entry1){
        printf("%s() : RIB : %s : Warning route for %s/%d not found in routing table\n",
                __FUNCTION__, rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), 
                RT_ENTRY_MASK(&rt_un_entry->rt_key));
        return FALSE;
    }

    sprintf(instance->traceopts->b, "RIB : %s : Updated route %s/%d to Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), 
            RT_ENTRY_MASK(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);

    rib->rt_un_route_delete(rib, &rt_un_entry1->rt_key);
    rib->rt_un_route_install(rib, rt_un_entry);

    rt_un_entry->last_refresh_time = time(NULL);
    return TRUE;

}

static boolean
inet_0_rt_un_route_delete(rt_un_table_t *rib, rt_key_t *rt_key){

    glthread_t *curr = NULL;

    rt_un_entry_t *temp = NULL,
                  *rt_un_entry = rib->rt_un_route_lookup(rib, rt_key);

    if(!rt_un_entry){
        printf("%s() : Warning route for %s/%d not found in routing table\n", 
            __FUNCTION__, RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key));
        return FALSE;
    }

    sprintf(instance->traceopts->b, "RIB : %s : Deleted route %s/%d from Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){
        temp = glthread_to_rt_un_entry(curr);
        if(UN_RTENTRY_PFX_MATCH(temp, rt_key)){
            free_rt_un_entry(temp);
            rib->count--;
            return TRUE;
        }
    }ITERATE_GLTHREAD_END(&rib->head, curr);
    return FALSE;
}

static boolean
inet_3_rt_un_route_install(rt_un_table_t *rib, rt_un_entry_t *rt_un_entry){
    
    sprintf(instance->traceopts->b, "RIB : %s : Added route %s/%d to Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);
    /*Refresh time before adding an enntry*/
    time(&rt_un_entry->last_refresh_time);
    glthread_add_next(&rib->head, &rt_un_entry->glthread);
    rib->count++;
    return TRUE;
}

static rt_un_entry_t *
inet_3_rt_un_route_lookup(rt_un_table_t *rib, rt_key_t *rt_key){
     
    glthread_t *curr = NULL;
    rt_un_entry_t *rt_un_entry = NULL;
    
    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){
        
       rt_un_entry = glthread_to_rt_un_entry(curr);
       if(UN_RTENTRY_PFX_MATCH(rt_un_entry, rt_key))
           return rt_un_entry;    
    } ITERATE_GLTHREAD_END(&rib->head, curr);

    return NULL;
}

static boolean
inet_3_rt_un_route_update(rt_un_table_t *rib, rt_un_entry_t *rt_un_entry){

    rt_un_entry_t *rt_un_entry1 =
        rib->rt_un_route_lookup(rib, &rt_un_entry->rt_key);

    if(!rt_un_entry1){
        printf("%s() : RIB : %s : Warning route for %s/%d not found in routing table\n",
                __FUNCTION__, rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), 
                RT_ENTRY_MASK(&rt_un_entry->rt_key));
        return FALSE;
    }

    sprintf(instance->traceopts->b, "RIB : %s : Updated route %s/%d to Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), 
            RT_ENTRY_MASK(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);

    rib->rt_un_route_delete(rib, &rt_un_entry1->rt_key);
    rib->rt_un_route_install(rib, rt_un_entry);

    rt_un_entry->last_refresh_time = time(NULL);
    return TRUE;

}

static boolean
inet_3_rt_un_route_delete(rt_un_table_t *rib, rt_key_t *rt_key){

    glthread_t *curr = NULL;

    rt_un_entry_t *temp = NULL,
                  *rt_un_entry = rib->rt_un_route_lookup(rib, rt_key);

    if(!rt_un_entry){
        printf("%s() : Warning route for %s/%d not found in routing table\n", 
            __FUNCTION__, RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key));
        return FALSE;
    }

    sprintf(instance->traceopts->b, "RIB : %s : Deleted route %s/%d from Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){
        temp = glthread_to_rt_un_entry(curr);
        if(UN_RTENTRY_PFX_MATCH(temp, rt_key)){
            free_rt_un_entry(temp);
            rib->count--;
            return TRUE;
        }
    }ITERATE_GLTHREAD_END(&rib->head, curr);
    return FALSE;
}


static boolean
mpls_0_rt_un_route_install(rt_un_table_t *rib, rt_un_entry_t *rt_un_entry){
    
    sprintf(instance->traceopts->b, "RIB : %s : Added route %s/%d(%u) to Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key),
            RT_ENTRY_LABEL(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);
    /*Refresh time before adding an enntry*/
    time(&rt_un_entry->last_refresh_time);
    glthread_add_next(&rib->head, &rt_un_entry->glthread);
    rib->count++;
    return TRUE;
}

static rt_un_entry_t *
mpls_0_rt_un_route_lookup(rt_un_table_t *rib, rt_key_t *rt_key){
     
    glthread_t *curr = NULL;
    rt_un_entry_t *rt_un_entry = NULL;
    
    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){
        
       rt_un_entry = glthread_to_rt_un_entry(curr);
       if(UN_RTENTRY_LABEL_MATCH(rt_un_entry, rt_key))
           return rt_un_entry;    
    } ITERATE_GLTHREAD_END(&rib->head, curr);

    return NULL;
}

static boolean
mpls_0_rt_un_route_update(rt_un_table_t *rib, rt_un_entry_t *rt_un_entry){

    rt_un_entry_t *rt_un_entry1 =
        rib->rt_un_route_lookup(rib, &rt_un_entry->rt_key);

    if(!rt_un_entry1){
        printf("%s() : RIB : %s : Warning route for %s/%d(%u) not found in routing table\n",
                __FUNCTION__, rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), 
                RT_ENTRY_MASK(&rt_un_entry->rt_key), RT_ENTRY_LABEL(&rt_un_entry->rt_key));
        return FALSE;
    }

    sprintf(instance->traceopts->b, "RIB : %s : Updated route %s/%d(%u) to Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), 
            RT_ENTRY_MASK(&rt_un_entry->rt_key), RT_ENTRY_LABEL(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);

    rib->rt_un_route_delete(rib, &rt_un_entry1->rt_key);
    rib->rt_un_route_install(rib, rt_un_entry);

    rt_un_entry->last_refresh_time = time(NULL);
    return TRUE;

}

static boolean
mpls_0_rt_un_route_delete(rt_un_table_t *rib, rt_key_t *rt_key){

    glthread_t *curr = NULL;

    rt_un_entry_t *temp = NULL,
                  *rt_un_entry = rib->rt_un_route_lookup(rib, rt_key);

    if(!rt_un_entry){
        printf("%s() : Warning route for %s/%d(%u) not found in routing table\n", 
            __FUNCTION__, RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key),
            RT_ENTRY_LABEL(&rt_un_entry->rt_key));
        return FALSE;
    }

    sprintf(instance->traceopts->b, "RIB : %s : Deleted route %s/%d(%u) from Routing table",
            rib->rib_name, RT_ENTRY_PFX(&rt_un_entry->rt_key), 
            RT_ENTRY_MASK(&rt_un_entry->rt_key), RT_ENTRY_LABEL(&rt_un_entry->rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){
        temp = glthread_to_rt_un_entry(curr);
        if(UN_RTENTRY_LABEL_MATCH(temp, rt_key)){
            free_rt_un_entry(temp);
            rib->count--;
            return TRUE;
        }
    }ITERATE_GLTHREAD_END(&rib->head, curr);
    return FALSE;
}

rt_un_table_t *
init_rib(rib_type_t rib_type){

    rt_un_table_t * rib = calloc(1, sizeof(rt_un_table_t));
    rib->count = 0;
    init_glthread(&rib->head);

    switch (rib_type){
        case INET_0:
            rib->rib_name = "INET.0";
            rib->rt_un_route_install = inet_0_rt_un_route_install;
            rib->rt_un_route_lookup  = inet_0_rt_un_route_lookup;
            rib->rt_un_route_update  = inet_0_rt_un_route_update;
            rib->rt_un_route_delete  = inet_0_rt_un_route_delete;
        case INET_3:
            rib->rib_name = "INET.3";
            rib->rt_un_route_install = inet_3_rt_un_route_install;
            rib->rt_un_route_lookup  = inet_3_rt_un_route_lookup;
            rib->rt_un_route_update  = inet_3_rt_un_route_update;
            rib->rt_un_route_delete  = inet_3_rt_un_route_delete;
        case MPLS_0:
            rib->rib_name = "MPLS.0";
            rib->rt_un_route_install = mpls_0_rt_un_route_install;
            rib->rt_un_route_lookup  = mpls_0_rt_un_route_lookup;
            rib->rt_un_route_update  = mpls_0_rt_un_route_update;
            rib->rt_un_route_delete  = mpls_0_rt_un_route_delete;
        default:
            assert(0);
    }
}
