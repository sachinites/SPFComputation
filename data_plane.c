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

#include "data_plane.h"
#include "instance.h"
#include "spftrace.h"
#include "spfcomputation.h"
#include "routes.h"
#include "ldp.h"
#include "spfutil.h"
#include "stack.h"

extern instance_t *instance;
void
init_pfe();

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
    return nh->oif->intf_name;
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
    
    glthread_t glthread;
    memcpy(&glthread, &dst->glthread, sizeof(glthread_t));
    memcpy(dst, src, sizeof(internal_un_nh_t));
    memcpy(&dst->glthread, &glthread, sizeof(glthread_t));
}

boolean
is_un_nh_t_clones(internal_un_nh_t *nh1, internal_un_nh_t *nh2){

    if(nh1->protocol != nh2->protocol)
        return FALSE;

    if(strncmp(nh1->oif->intf_name, nh2->oif->intf_name, IF_NAME_SIZE))
        return FALSE;

    if(nh1->nh_node != nh2->nh_node)
        return FALSE;

    if(strncmp(nh1->gw_prefix, nh2->gw_prefix, PREFIX_LEN))
        return FALSE;

    if(memcmp(&nh1->nh, &nh2->nh, sizeof(nh1->nh)))
        return FALSE;

    if(nh1->flags != nh2->flags)
        return FALSE;

    if(nh1->protected_link != nh1->protected_link)
        return FALSE;

    if(nh1->lfa_type != nh2->lfa_type)
        return FALSE;

    if(nh1->root_metric != nh2->root_metric)
        return FALSE;

    if(nh1->dest_metric != nh2->dest_metric)
        return FALSE;

    return TRUE;
}

static boolean
is_inet_0_un_nh_t_equal(internal_un_nh_t *nh1, internal_un_nh_t *nh2){

    if(nh1->oif != nh2->oif)
        return FALSE;

    if(strncmp(nh1->gw_prefix, nh2->gw_prefix, PREFIX_LEN))
        return FALSE;

    return TRUE;
}


static boolean
is_inet_3_un_nh_t_equal(internal_un_nh_t *nh1, internal_un_nh_t *nh2){

    if(nh1->oif != nh2->oif)
        return FALSE;

    if(strncmp(nh1->gw_prefix, nh2->gw_prefix, PREFIX_LEN))
        return FALSE;

    if(memcmp(&nh1->nh.inet3_nh, &nh2->nh.inet3_nh, sizeof(internal_un_nh_t)))
        return FALSE;

    return TRUE;
}


static boolean
is_mpls_0_un_nh_t_equal(internal_un_nh_t *nh1, internal_un_nh_t *nh2){

    if(nh1->oif != nh2->oif)
        return FALSE;

    if(strncmp(nh1->gw_prefix, nh2->gw_prefix, PREFIX_LEN))
        return FALSE;

    if(memcmp(&nh1->nh.mpls0_nh, &nh2->nh.mpls0_nh, sizeof(internal_un_nh_t)))
        return FALSE;

    return TRUE;
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

    internal_un_nh_t *nxt_hop = NULL;
    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr){
        nxt_hop = glthread_to_unified_nh(curr);
        remove_glthread(curr);
        free_un_nexthop(nxt_hop);    
    } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr);
    
    remove_glthread(&rt_un_entry->glthread);
    free(rt_un_entry);
}

internal_un_nh_t *
lookup_clone_next_hop(rt_un_table_t *rib, 
                       rt_un_entry_t *rt_un_entry, 
                       internal_un_nh_t *nexthop){

    internal_un_nh_t *nxt_hop = NULL;
    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr){

        nxt_hop = glthread_to_unified_nh(curr);
        if(rib->rt_un_nh_t_equal(nxt_hop, nexthop))
            return nxt_hop;
    } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr);
    return NULL;
}


/*Rib functions*/
boolean
inet_0_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, LEVEL level, 
                            internal_un_nh_t *nexthop){
   
    glthread_t *curr = NULL;
    internal_un_nh_t *nxt_hop = NULL;
     
    sprintf(instance->traceopts->b, "RIB : %s : Adding route %s/%d to Routing table",
            rib->rib_name, RT_ENTRY_PFX(rt_key), RT_ENTRY_MASK(rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);
    
    rt_un_entry_t *rt_un_entry = rib->rt_un_route_lookup(rib, rt_key);
    internal_un_nh_t *existing_nh = NULL;

    if(!rt_un_entry){
        rt_un_entry = calloc(1, sizeof(rt_un_entry_t));
        memcpy(&rt_un_entry->rt_key, rt_key, sizeof(rt_key_t));
        time(&rt_un_entry->last_refresh_time);
        rt_un_entry->level = level;
        glthread_add_next(&rib->head, &rt_un_entry->glthread);
        rib->count++;
    }

    if(rt_un_entry->level != level){
        /*replace the route with incoming version*/
        rt_un_entry->flags = 0;
        rt_un_entry->level = level;
        time(&rt_un_entry->last_refresh_time);
        
        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr){
            nxt_hop = glthread_to_unified_nh(curr);
            remove_glthread(curr);
            free_un_nexthop(nxt_hop);
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr);
        
        init_glthread(&rt_un_entry->nh_list_head);
    }

    if(!nexthop){
        sprintf(instance->traceopts->b, "RIB : %s : local route %s/%d added to Routing table",
            rib->rib_name, RT_ENTRY_PFX(rt_key), RT_ENTRY_MASK(rt_key));
        return TRUE;
    }
    
    /*Refresh time before adding an enntry*/
    time(&nexthop->last_refresh_time);

    existing_nh = lookup_clone_next_hop(rib, rt_un_entry, nexthop);
    
    if(existing_nh){
        sprintf(instance->traceopts->b, "Warning : RIB : %s : Nexthop (%s) --> (%s)%s already exists in %s/%d route",
            rib->rib_name, existing_nh->oif->intf_name, existing_nh->gw_prefix, existing_nh->nh_node->node_name,
            RT_ENTRY_PFX(rt_key), RT_ENTRY_MASK(rt_key));
        trace(instance->traceopts, ROUTING_TABLE_BIT);
        return FALSE;
    }

    init_glthread(&nexthop->glthread);
    if(IS_BIT_SET(nexthop->flags, PRIMARY_NH))
        glthread_add_next(&rt_un_entry->nh_list_head, &nexthop->glthread);
    else
        glthread_add_last(&rt_un_entry->nh_list_head, &nexthop->glthread);
    
    return TRUE;
}

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


boolean
inet_3_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, LEVEL level,
                            internal_un_nh_t *nexthop){
   
    glthread_t *curr = NULL;
    internal_un_nh_t *nxt_hop = NULL;
     
    sprintf(instance->traceopts->b, "RIB : %s : Adding route %s/%d to Routing table",
            rib->rib_name, RT_ENTRY_PFX(rt_key), RT_ENTRY_MASK(rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);

    rt_un_entry_t *rt_un_entry = rib->rt_un_route_lookup(rib, rt_key);
    internal_un_nh_t *existing_nh = NULL;

    if(!rt_un_entry){
        rt_un_entry = calloc(1, sizeof(rt_un_entry_t));
        memcpy(&rt_un_entry->rt_key, rt_key, sizeof(rt_key_t));
        time(&rt_un_entry->last_refresh_time);
        rt_un_entry->level = level;
        glthread_add_next(&rib->head, &rt_un_entry->glthread);
        rib->count++;
    }
    
    if(rt_un_entry->level != level){
        /*replace the route with incoming version*/
        rt_un_entry->flags = 0;
        rt_un_entry->level = level;
        time(&rt_un_entry->last_refresh_time);
        
        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr){
            nxt_hop = glthread_to_unified_nh(curr);
            remove_glthread(curr);
            free_un_nexthop(nxt_hop);
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr);
        
        init_glthread(&rt_un_entry->nh_list_head);
    }

    if(!nexthop){
        sprintf(instance->traceopts->b, "RIB : %s : local route %s/%d added to Routing table",
            rib->rib_name, RT_ENTRY_PFX(rt_key), RT_ENTRY_MASK(rt_key));
        return TRUE;
    }

    /*Refresh time before adding an enntry*/
    time(&nexthop->last_refresh_time);
    existing_nh = lookup_clone_next_hop(rib, rt_un_entry, nexthop);

    if(existing_nh){
        sprintf(instance->traceopts->b, "Warning : RIB : %s : Nexthop (%s) --> (%s)%s already exists in %s/%d route",
            rib->rib_name, existing_nh->oif->intf_name, existing_nh->gw_prefix, existing_nh->nh_node->node_name,
            RT_ENTRY_PFX(rt_key), RT_ENTRY_MASK(rt_key));
        trace(instance->traceopts, ROUTING_TABLE_BIT);
        return FALSE;
    }

    init_glthread(&nexthop->glthread);
    if(IS_BIT_SET(nexthop->flags, PRIMARY_NH))
        glthread_add_next(&rt_un_entry->nh_list_head, &nexthop->glthread);
    else
        glthread_add_last(&rt_un_entry->nh_list_head, &nexthop->glthread);
    
    return TRUE;
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


boolean
mpls_0_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, LEVEL level,
                            internal_un_nh_t *nexthop){
    
    glthread_t *curr = NULL;
    internal_un_nh_t *nxt_hop = NULL;

    sprintf(instance->traceopts->b, "RIB : %s : Adding route %s/%d to Routing table",
            rib->rib_name, RT_ENTRY_PFX(rt_key), RT_ENTRY_MASK(rt_key));
    trace(instance->traceopts, ROUTING_TABLE_BIT);
    /*Refresh time before adding an enntry*/
    time(&nexthop->last_refresh_time);

    rt_un_entry_t *rt_un_entry = rib->rt_un_route_lookup(rib, rt_key);
    internal_un_nh_t *existing_nh = NULL;

    if(!rt_un_entry){
        rt_un_entry = calloc(1, sizeof(rt_un_entry_t));
        memcpy(&rt_un_entry->rt_key, rt_key, sizeof(rt_key_t));
        time(&rt_un_entry->last_refresh_time);
        rt_un_entry->level = level;
        glthread_add_next(&rib->head, &rt_un_entry->glthread);
        rib->count++;
    }

    if(rt_un_entry->level != level){
        /*replace the route with incoming version*/
        rt_un_entry->flags = 0;
        rt_un_entry->level = level;
        time(&rt_un_entry->last_refresh_time);
        
        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr){
            nxt_hop = glthread_to_unified_nh(curr);
            remove_glthread(curr);
            free_un_nexthop(nxt_hop);
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr);
        
        init_glthread(&rt_un_entry->nh_list_head);
    }

    existing_nh = lookup_clone_next_hop(rib, rt_un_entry, nexthop);
    if(existing_nh){
        sprintf(instance->traceopts->b, "Warning : RIB : %s : Nexthop (%s) --> (%s)%s already exists in %s/%d route",
            rib->rib_name, existing_nh->oif->intf_name, existing_nh->gw_prefix, existing_nh->nh_node->node_name,
            RT_ENTRY_PFX(rt_key), RT_ENTRY_MASK(rt_key));
        trace(instance->traceopts, ROUTING_TABLE_BIT);
        return FALSE;
    }

    init_glthread(&nexthop->glthread);
    
    if(IS_BIT_SET(nexthop->flags, PRIMARY_NH))
        glthread_add_next(&rt_un_entry->nh_list_head, &nexthop->glthread);
    else
        glthread_add_last(&rt_un_entry->nh_list_head, &nexthop->glthread);
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

internal_un_nh_t *
inet_0_unifiy_nexthop(internal_nh_t *nexthop, PROTOCOL proto){

    internal_un_nh_t *un_nh = malloc_un_nexthop();
    un_nh->oif = nexthop->oif;
    un_nh->protocol = proto;
    un_nh->nh_node = nexthop->node;
    strncpy(un_nh->gw_prefix, nexthop->gw_prefix, PREFIX_LEN);
    un_nh->gw_prefix[PREFIX_LEN] = '\0';
    un_nh->protected_link = nexthop->protected_link;
    if(!un_nh->protected_link)
        SET_BIT(un_nh->flags, PRIMARY_NH);
    SET_BIT(un_nh->flags, IPV4_NH);
    un_nh->ref_count = 0;
    un_nh->lfa_type = nexthop->lfa_type;
    un_nh->root_metric = nexthop->root_metric;
    un_nh->dest_metric = nexthop->dest_metric;
    time(&un_nh->last_refresh_time);
    init_glthread(&un_nh->glthread);
    return un_nh;
}

/*This fn converts the RSVP|LDP|SPRING nexthop on ingress router
 * into inet.3 route format*/
internal_un_nh_t *
inet_3_unifiy_nexthop(internal_nh_t *nexthop, PROTOCOL proto, 
                      unsigned int nxthop_type,
                      routes_t *route){

    /*In case of IPV4 and RSVP nxthops, next hop is nexthop->node, whereas in case
     * of LDP (RLFA) backups, nexthop is nexthop->proxy_nbr*/
    node_t *nexthop_node = nexthop->node ?  nexthop->node : nexthop->proxy_nbr;
    internal_un_nh_t *un_nh = inet_0_unifiy_nexthop(nexthop, proto);
    UNSET_BIT(un_nh->flags, IPV4_NH);
    switch(proto){
        case IGP_PROTO:
            if(is_internal_backup_nexthop_rsvp(nexthop))
                SET_BIT(un_nh->flags, IPV4_RSVP_NH);
            else
                SET_BIT(un_nh->flags, IPV4_LDP_NH);
            break;
        case LDP_PROTO:
            SET_BIT(un_nh->flags, IPV4_LDP_NH);
            break;
        case L_IGP_PROTO:
            SET_BIT(un_nh->flags, IPV4_SPRING_NH);
            break;
        case RSVP_PROTO: 
            SET_BIT(un_nh->flags, IPV4_RSVP_NH);
            break;
         default:
            assert(0);
    }

    switch(nxthop_type){
        case IPV4_RSVP_NH:
            break; /*Later*/
        case IPV4_LDP_NH: /*This functionality should go in ldpify_rlfa_nexthop()*/
        {
            if(is_node_best_prefix_originator(nexthop_node, route)){
                /* No need to label the packet when src and dest are 
                 * direct nbrs - that is there is not transient nxthop*/
                SET_BIT(un_nh->flags, IPV4_NH);
                UNSET_BIT(un_nh->flags, IPV4_LDP_NH);
                return un_nh;
            }
            SET_BIT(un_nh->flags, IPV4_LDP_NH);
            un_nh->nh.inet3_nh.mpls_label_out[0] = nexthop->mpls_label_out[0];
            un_nh->nh.inet3_nh.stack_op[0] = nexthop->stack_op[0];
            return un_nh;
        }
        break;
        case IPV4_SPRING_NH:
        {
            if(is_node_best_prefix_originator(nexthop_node, route)){
                /* No need to label the packet when src and dest are 
                 * direct nbrs - that is there is not transient nxthop*/
                SET_BIT(un_nh->flags, IPV4_NH);
                UNSET_BIT(un_nh->flags, IPV4_SPRING_NH);
                return un_nh;
            }
            SET_BIT(un_nh->flags, IPV4_SPRING_NH);
            if(!is_node_spring_enabled(nexthop_node, route->level)){
                /*If nexthop node is not spring enabled, we cannot obtain 
                 * outgoing SR label for it. Try for LDP label instead.
                 * This feature is called SRoLDP*/
                break;
            }
            
            /*copy stack*/
            int i = MPLS_STACK_OP_LIMIT_MAX -1;
            for(; i >= 0; i--){
                un_nh->nh.mpls0_nh.mpls_label_out[i] = nexthop->mpls_label_out[i];
                un_nh->nh.mpls0_nh.stack_op[i] = nexthop->stack_op[i];
            }
            /*Change top Label stack operation to PUSH*/
            unsigned int top_index = get_stack_top_index(un_nh);
            un_nh->nh.mpls0_nh.stack_op[top_index] = PUSH;
            return un_nh;
        }
        break;
        default:
            ;
    }
    return un_nh;
}


internal_un_nh_t *
mpls_0_unifiy_nexthop(internal_nh_t *nexthop, PROTOCOL proto){

    internal_un_nh_t *un_nh = inet_0_unifiy_nexthop(nexthop, proto);
    UNSET_BIT(un_nh->flags, IPV4_NH);
    switch(proto){
        case IGP_PROTO:
            assert(0); /*IGP do not install transient route*/
        case LDP_PROTO:
            SET_BIT(un_nh->flags, LDP_TRANSIT_NH);
            break;
        case L_IGP_PROTO:
            SET_BIT(un_nh->flags, SPRING_TRANSIT_NH);
            break;
        case RSVP_PROTO: 
            SET_BIT(un_nh->flags, RSVP_TRANSIT_NH);
         default:
            assert(0);
    }

    /*Fill the MPLS label stack*/

    unsigned int i = 0;
    
    for(; i < MPLS_STACK_OP_LIMIT_MAX; i++){
        un_nh->nh.mpls0_nh.mpls_label_out[i] = nexthop->mpls_label_out[i];
        un_nh->nh.mpls0_nh.stack_op[i] = nexthop->stack_op[i];
    }

    return un_nh;
}


rt_un_table_t *
init_rib(rib_type_t rib_type){

    rt_un_table_t * rib = calloc(1, sizeof(rt_un_table_t));
    rib->count = 0;
    init_glthread(&rib->head);

    switch (rib_type){
        case INET_0:
            rib->rib_name = "INET.0";
            rib->rt_un_route_install_nexthop = inet_0_rt_un_route_install_nexthop;
            rib->rt_un_route_install = inet_0_rt_un_route_install;
            rib->rt_un_route_lookup  = inet_0_rt_un_route_lookup;
            rib->rt_un_route_update  = inet_0_rt_un_route_update;
            rib->rt_un_route_delete  = inet_0_rt_un_route_delete;
            rib->rt_un_nh_t_equal = is_inet_0_un_nh_t_equal;
            break;
        case INET_3:
            rib->rib_name = "INET.3";
            rib->rt_un_route_install_nexthop = inet_3_rt_un_route_install_nexthop;
            rib->rt_un_route_install = inet_3_rt_un_route_install;
            rib->rt_un_route_lookup  = inet_3_rt_un_route_lookup;
            rib->rt_un_route_update  = inet_3_rt_un_route_update;
            rib->rt_un_route_delete  = inet_3_rt_un_route_delete;
            rib->rt_un_nh_t_equal = is_inet_3_un_nh_t_equal;
            break;
        case MPLS_0:
            rib->rib_name = "MPLS.0";
            rib->rt_un_route_install_nexthop = mpls_0_rt_un_route_install_nexthop;
            rib->rt_un_route_install = mpls_0_rt_un_route_install;
            rib->rt_un_route_lookup  = mpls_0_rt_un_route_lookup;
            rib->rt_un_route_update  = mpls_0_rt_un_route_update;
            rib->rt_un_route_delete  = mpls_0_rt_un_route_delete;
            rib->rt_un_nh_t_equal = is_mpls_0_un_nh_t_equal;
            break;
        default:
            assert(0);
    }
    
    return rib;
}

void
flush_rib(rt_un_table_t *rib, LEVEL level){
   
    unsigned int count = 0; 
    glthread_t *curr = NULL;
    rt_un_entry_t *rt_un_entry = NULL;

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){

        rt_un_entry = glthread_to_rt_un_entry(curr);
        if(rt_un_entry->level != level)
            continue;
        free_rt_un_entry(rt_un_entry);
        count++;
    } ITERATE_GLTHREAD_END(&rib->head, curr);
    rib->count -= count;
}

void
inet_0_display(rt_un_table_t *rib, char *prefix, char mask){

    glthread_t *curr = NULL, *curr1 = NULL;
    rt_un_entry_t *rt_un_entry = NULL;
    internal_un_nh_t *nexthop = NULL;

    printf("%s  count : %u\n\n", rib->rib_name, rib->count);
    if(prefix){
        rt_key_t rt_key;
        memset(&rt_key, 0, sizeof(rt_key_t));
        strncpy(RT_ENTRY_PFX(&rt_key), prefix, PREFIX_LEN);
        RT_ENTRY_MASK(&rt_key) = mask;

        rt_un_entry = rib->rt_un_route_lookup(rib, &rt_key);
        if(!rt_un_entry){
            printf("Do not exist\n");
            return;
        }
        printf("%s/%u(L%u)\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), 
            RT_ENTRY_MASK(&rt_un_entry->rt_key), rt_un_entry->level);

        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr1){
            nexthop = glthread_to_unified_nh(curr1);
            printf("\t%s %-16s %s   %s %s\n", protocol_name(nexthop->protocol), 
                    nexthop->oif->intf_name, nexthop->gw_prefix,
                    IS_BIT_SET(nexthop->flags, PRIMARY_NH) ? "PRIMARY": "BACKUP",
                    get_str_nexthop_type(nexthop->flags));
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr1);
        return;
    }

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){

        rt_un_entry = glthread_to_rt_un_entry(curr);
        printf("%s/%u(L%u)\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key),
            rt_un_entry->level);

        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr1){
            nexthop = glthread_to_unified_nh(curr1);
            printf("\t%s %-16s %s   %s %s\n", protocol_name(nexthop->protocol), 
                    nexthop->oif->intf_name, nexthop->gw_prefix,
                    IS_BIT_SET(nexthop->flags, PRIMARY_NH) ? "PRIMARY": "BACKUP",
                    get_str_nexthop_type(nexthop->flags));
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr1);
    } ITERATE_GLTHREAD_END(&rib->head, curr);
}


void
inet_3_display(rt_un_table_t *rib, char *prefix, char mask){

    glthread_t *curr = NULL, *curr1 = NULL;
    rt_un_entry_t *rt_un_entry = NULL;
    internal_un_nh_t *nexthop = NULL;
    int i = 0;

    printf("%s  count : %u\n\n", rib->rib_name, rib->count);
    if(prefix){
        rt_key_t rt_key;
        memset(&rt_key, 0, sizeof(rt_key_t));
        strncpy(RT_ENTRY_PFX(&rt_key), prefix, PREFIX_LEN);
        RT_ENTRY_MASK(&rt_key) = mask;
        rt_un_entry = rib->rt_un_route_lookup(rib, &rt_key);
        if(!rt_un_entry){
            printf("Do not exist\n");
            return;
        }
        printf("%s/%u(L%u)\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key),
                rt_un_entry->level);

        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr1){
            nexthop = glthread_to_unified_nh(curr1);
            printf("\t%s %-16s %s   %s %s\n", protocol_name(nexthop->protocol), 
                    nexthop->oif->intf_name, nexthop->gw_prefix,
                    IS_BIT_SET(nexthop->flags, PRIMARY_NH) ? "PRIMARY": "BACKUP",
                    get_str_nexthop_type(nexthop->flags));

            /*Print spring stack here*/
            i = MPLS_STACK_OP_LIMIT_MAX -1;
            printf("\t\t");

            for(; i >= 0; i--){
                if(nexthop->nh.inet3_nh.mpls_label_out[i] == 0 &&
                        nexthop->nh.inet3_nh.stack_op[i] == STACK_OPS_UNKNOWN)
                    continue;
                if(nexthop->nh.inet3_nh.stack_op[i] == POP)
                    printf("%s  ", get_str_stackops(nexthop->nh.inet3_nh.stack_op[i]));
                else
                    printf("%s:%u  ", get_str_stackops(nexthop->nh.inet3_nh.stack_op[i]),   \
                            nexthop->nh.inet3_nh.mpls_label_out[i]);
            }
            printf("\n");
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr1);
        return;
    }

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){

        rt_un_entry = glthread_to_rt_un_entry(curr);
        
        printf("%s/%u(L%u)\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key),
                rt_un_entry->level);

        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr1){
            nexthop = glthread_to_unified_nh(curr1);
            printf("\t%s %-16s %s   %s %s\n", protocol_name(nexthop->protocol), 
                    nexthop->oif->intf_name, nexthop->gw_prefix,
                    IS_BIT_SET(nexthop->flags, PRIMARY_NH) ? "PRIMARY": "BACKUP",
                    get_str_nexthop_type(nexthop->flags));

            /*Print spring stack here*/
            i = MPLS_STACK_OP_LIMIT_MAX -1;
            printf("\t\t");

            for(; i >= 0; i--){
                if(nexthop->nh.inet3_nh.mpls_label_out[i] == 0 &&
                        nexthop->nh.inet3_nh.stack_op[i] == STACK_OPS_UNKNOWN)
                    continue;
                if(nexthop->nh.inet3_nh.stack_op[i] == POP)
                    printf("%s  ", get_str_stackops(nexthop->nh.inet3_nh.stack_op[i]));
                else
                    printf("%s:%u  ", get_str_stackops(nexthop->nh.inet3_nh.stack_op[i]),   \
                            nexthop->nh.inet3_nh.mpls_label_out[i]);
            }
            printf("\n");
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr1);
    } ITERATE_GLTHREAD_END(&rib->head, curr);
}

void
mpls_0_display(rt_un_table_t *rib, mpls_label_t in_label){

    glthread_t *curr = NULL, *curr1 = NULL;
    rt_un_entry_t *rt_un_entry = NULL;
    internal_un_nh_t *nexthop = NULL;
    int i = 0;

    printf("%s  count : %u\n\n", rib->rib_name, rib->count);
    if(in_label){
        rt_key_t rt_key;
        memset(&rt_key, 0, sizeof(rt_key_t));
        RT_ENTRY_LABEL(&rt_key) = in_label;
        rt_un_entry = rib->rt_un_route_lookup(rib, &rt_key);
        if(!rt_un_entry){
            printf("Do not exist\n");
            return;
        }
    
        printf("%s/%u(L%u), Inlabel : %u\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), 
            RT_ENTRY_MASK(&rt_un_entry->rt_key), rt_un_entry->level, 
            RT_ENTRY_LABEL(&rt_un_entry->rt_key));

        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr1){
            nexthop = glthread_to_unified_nh(curr1);
            printf("\tInLabel : %u, %s %-16s %s   %s %s\n", in_label, 
                    protocol_name(nexthop->protocol), 
                    nexthop->oif->intf_name, nexthop->gw_prefix,
                    IS_BIT_SET(nexthop->flags, PRIMARY_NH) ? "PRIMARY": "BACKUP",
                    get_str_nexthop_type(nexthop->flags));

            /*Print spring stack here*/
            i = MPLS_STACK_OP_LIMIT_MAX -1;
            printf("\t\t");

            for(; i >= 0; i--){
                if(nexthop->nh.inet3_nh.mpls_label_out[i] == 0 &&
                        nexthop->nh.inet3_nh.stack_op[i] == STACK_OPS_UNKNOWN)
                    continue;
                if(nexthop->nh.inet3_nh.stack_op[i] == POP)
                    printf("%s  ", get_str_stackops(nexthop->nh.inet3_nh.stack_op[i]));
                else
                    printf("%s:%u  ", get_str_stackops(nexthop->nh.inet3_nh.stack_op[i]),   \
                            nexthop->nh.inet3_nh.mpls_label_out[i]);
            }
            printf("\n");
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr1);
        return;
    }

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){

        rt_un_entry = glthread_to_rt_un_entry(curr);

        printf("%s/%u(L%u), Inlabel : %u\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), 
            RT_ENTRY_MASK(&rt_un_entry->rt_key), rt_un_entry->level,
            RT_ENTRY_LABEL(&rt_un_entry->rt_key));

        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr1){
            nexthop = glthread_to_unified_nh(curr1);
            printf("\t%s %-16s %s   %s %s\n", 
                    protocol_name(nexthop->protocol), 
                    nexthop->oif->intf_name, nexthop->gw_prefix,
                    IS_BIT_SET(nexthop->flags, PRIMARY_NH) ? "PRIMARY": "BACKUP",
                    get_str_nexthop_type(nexthop->flags));

            /*Print spring stack here*/
            i = MPLS_STACK_OP_LIMIT_MAX -1;
            printf("\t\t");

            for(; i >= 0; i--){
                if(nexthop->nh.inet3_nh.mpls_label_out[i] == 0 &&
                        nexthop->nh.inet3_nh.stack_op[i] == STACK_OPS_UNKNOWN)
                    continue;
                if(nexthop->nh.inet3_nh.stack_op[i] == POP)
                    printf("%s  ", get_str_stackops(nexthop->nh.inet3_nh.stack_op[i]));
                else
                    printf("%s:%u  ", get_str_stackops(nexthop->nh.inet3_nh.stack_op[i]),   \
                            nexthop->nh.inet3_nh.mpls_label_out[i]);
            }
            printf("\n");
        } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr1);
    } ITERATE_GLTHREAD_END(&rib->head, curr);
}

int
get_stack_top_index(internal_un_nh_t *nexthop){

    int i = MPLS_STACK_OP_LIMIT_MAX -1;
    for(; i >= 0; i--){
        if(nexthop->nh.inet3_nh.mpls_label_out[i] == 0 &&
            nexthop->nh.inet3_nh.stack_op[i] == STACK_OPS_UNKNOWN)
            continue;
        return i;   
    }
    return -1;
}



/*Applicable for inet.0 and inet.3 tables*/

rt_un_entry_t *
get_longest_prefix_match2(rt_un_table_t *rib, char *prefix){

    rt_un_entry_t *rt_un_entry = NULL,
                  *lpm_rt_un_entry = NULL,
                  *rt_default_un_entry = NULL;

    glthread_t *curr = NULL;
    char subnet[PREFIX_LEN + 1];
    char longest_mask = 0;

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){
        
        rt_un_entry = glthread_to_rt_un_entry(curr);
        memset(subnet, 0, PREFIX_LEN + 1);
        apply_mask(prefix, RT_ENTRY_MASK(&rt_un_entry->rt_key), subnet);
        if(strncmp("0.0.0.0", RT_ENTRY_PFX(&rt_un_entry->rt_key), strlen("0.0.0.0")) == 0 &&
                RT_ENTRY_MASK(&rt_un_entry->rt_key) == 0){
            rt_default_un_entry = rt_un_entry;
        }
        else if(strncmp(subnet, RT_ENTRY_PFX(&rt_un_entry->rt_key), strlen(subnet)) == 0){
            if( RT_ENTRY_MASK(&rt_un_entry->rt_key) > longest_mask){
                longest_mask = RT_ENTRY_MASK(&rt_un_entry->rt_key);
                lpm_rt_un_entry = rt_un_entry;
            }
        }
    }ITERATE_GLTHREAD_END(&rib->head, curr);
    return lpm_rt_un_entry ? lpm_rt_un_entry : rt_default_un_entry;
}

static void
execute_nexthop_mpls_label_stack_on_packet(internal_un_nh_t *nxthop, 
        mpls_label_stack_t *mpls_label_stack){

    /*Execute the Nexthop mpls label stack on incoming packet which could be
     * labelled or unlabelled*/
    int i = MPLS_STACK_OP_LIMIT_MAX -1;

    for(; i >= 0; i--){
        MPLS_STACK_OP stack_op = nxthop->nh.mpls0_nh.stack_op[i];
        mpls_label_t imposing_label =  nxthop->nh.mpls0_nh.mpls_label_out[i];
        
        if(stack_op == STACK_OPS_UNKNOWN &&
                imposing_label == 0){
            continue;
        }

        switch(stack_op){
            case PUSH:
                PUSH_MPLS_LABEL(mpls_label_stack, imposing_label);
                break;
            case POP:
                assert(!IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack));
                POP_MPLS_LABEL(mpls_label_stack);
                break;
            case SWAP:
                assert(!IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack));
                SWAP_MPLS_LABEL(mpls_label_stack, imposing_label);
                break;
            default:
                assert(0);
        }
    }
}


#define ITERATE_PREF_ROUTE_ORDER_BEGIN(_pref_order)   \
    for(_pref_order = ROUTE_TYPE_MIN + 1; _pref_order < ROUTE_TYPE_MAX; _pref_order++)

#define ITERATE_PREF_ROUTE_ORDER_END(_pref_order)

#define FIRST_PREF_ORDER     (ROUTE_TYPE_MIN + 1)

static inline trace_route_pref_order_t
get_next_preferred_route_type(trace_route_pref_order_t curr_pref){
    if(curr_pref == ROUTE_TYPE_MAX)
        assert(0);
    return curr_pref + 1;
}

typedef enum{
    TRACE_COMPLETE,
    TRACE_INCOMPLETE
} trace_rc_t;

typedef mpls_label_stack_t * (*pfe_engine)(node_t *node, char *dst_prefix, 
                        trace_rc_t *trace_rc, node_t **next_node);

mpls_label_stack_t *
ipv4_pfe_engine(node_t *node, char *dst_prefix, trace_rc_t *trace_rc, node_t **next_node){

    unsigned int i = 1;

    do{
        rt_un_entry_t * rt_un_entry = 
            get_longest_prefix_match2(node->spf_info.rib[INET_0], dst_prefix);

        if(!rt_un_entry){
            *trace_rc = TRACE_INCOMPLETE;
            *next_node = node;
            return NULL;   
        }

        internal_un_nh_t *prim_nexthop = NULL; 

        prim_nexthop =  GET_FIRST_NH(rt_un_entry, IPV4_NH, PRIMARY_NH);

        if(!prim_nexthop){
            *trace_rc = TRACE_COMPLETE;
            *next_node = node;
            return NULL;
        }

        printf("%u. %s(%s)--IPNH-->(%s)%s\n", i++, node->node_name, get_un_next_hop_oif_name(prim_nexthop),
                get_un_next_hop_gateway_pfx(prim_nexthop), get_un_next_hop_node(prim_nexthop)->node_name);

        node = get_un_next_hop_node(prim_nexthop);
    }while(1);

    return NULL;
}

mpls_label_stack_t *
ldp_pfe_engine(node_t *node, char *dst_prefix, trace_rc_t *trace_rc, node_t **next_node){

    unsigned int i = 1;

    rt_un_entry_t * rt_un_entry = 
        get_longest_prefix_match2(node->spf_info.rib[INET_3], dst_prefix);

    if(!rt_un_entry){
        *trace_rc = TRACE_INCOMPLETE;
        *next_node = node;
        return NULL;   
    }

    internal_un_nh_t *prim_nexthop = NULL; 

    prim_nexthop =  GET_FIRST_NH(rt_un_entry, IPV4_LDP_NH, PRIMARY_NH);

    if(!prim_nexthop){
        *trace_rc = TRACE_INCOMPLETE;
        *next_node = node;
        return NULL;
    }

    printf("%u. %s(%s)--LDP_TUNN-IN->(%s)%s\n", i++, node->node_name, get_un_next_hop_oif_name(prim_nexthop),
            get_un_next_hop_gateway_pfx(prim_nexthop), get_un_next_hop_node(prim_nexthop)->node_name);

    mpls_label_stack_t *mpls_label_stack = get_new_mpls_label_stack();
    execute_nexthop_mpls_label_stack_on_packet(prim_nexthop, mpls_label_stack);

    *next_node = get_un_next_hop_node(prim_nexthop);
    *trace_rc = TRACE_INCOMPLETE;
    return mpls_label_stack;
}

mpls_label_stack_t *
sr_pfe_engine(node_t *node, char *dst_prefix, trace_rc_t *trace_rc, node_t **next_node){
    
    unsigned int i = 1;

    rt_un_entry_t * rt_un_entry = 
        get_longest_prefix_match2(node->spf_info.rib[INET_3], dst_prefix);

    if(!rt_un_entry){
        *trace_rc = TRACE_INCOMPLETE;
        *next_node = node;
        return NULL;   
    }

    internal_un_nh_t *prim_nexthop = NULL; 

    prim_nexthop =  GET_FIRST_NH(rt_un_entry, IPV4_SPRING_NH, PRIMARY_NH);

    if(!prim_nexthop){
        *trace_rc = TRACE_INCOMPLETE;
        *next_node = node;
        return NULL;
    }

    printf("%u. %s(%s)--SPR_TUNN_IN->(%s)%s\n", i++, node->node_name, get_un_next_hop_oif_name(prim_nexthop),
            get_un_next_hop_gateway_pfx(prim_nexthop), get_un_next_hop_node(prim_nexthop)->node_name);

    mpls_label_stack_t *mpls_label_stack = get_new_mpls_label_stack();
    execute_nexthop_mpls_label_stack_on_packet(prim_nexthop, mpls_label_stack);

    *next_node = get_un_next_hop_node(prim_nexthop);
    *trace_rc = TRACE_INCOMPLETE;
    return mpls_label_stack;
}

mpls_label_stack_t *
rsvp_pfe_engine(node_t *node, char *dst_prefix, trace_rc_t *trace_rc, node_t **next_node){
    
    unsigned int i = 1;

    rt_un_entry_t * rt_un_entry = 
        get_longest_prefix_match2(node->spf_info.rib[INET_3], dst_prefix);

    if(!rt_un_entry){
        *trace_rc = TRACE_INCOMPLETE;
        *next_node = node;
        return NULL;   
    }

    internal_un_nh_t *prim_nexthop = NULL; 

    prim_nexthop =  GET_FIRST_NH(rt_un_entry, IPV4_RSVP_NH, PRIMARY_NH);

    if(!prim_nexthop){
        *trace_rc = TRACE_INCOMPLETE;
        *next_node = node;
        return NULL;
    }

    printf("%u. %s(%s)--RSVP_TUNN-IN->(%s)%s\n", i++, node->node_name, get_un_next_hop_oif_name(prim_nexthop),
            get_un_next_hop_gateway_pfx(prim_nexthop), get_un_next_hop_node(prim_nexthop)->node_name);

    mpls_label_stack_t *mpls_label_stack = get_new_mpls_label_stack();
    execute_nexthop_mpls_label_stack_on_packet(prim_nexthop, mpls_label_stack);

    *next_node = get_un_next_hop_node(prim_nexthop);
    *trace_rc = TRACE_INCOMPLETE;
    return mpls_label_stack;
}

/*ToDo : Currently the nexthop applies only top most label stack operation
 * on incoming packet label stack. Ideally, Entire mpls label stack of nexthop
 * should be applied on incoming mpls label. We will improve*/

void
transient_mpls_pfe_engine(node_t *node, mpls_label_stack_t *mpls_label_stack, 
                          node_t **next_node){

    unsigned int i = 1;
    rt_un_entry_t *rt_un_entry = NULL;
    rt_key_t rt_key;

    assert(!IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack));
    *next_node = node;

    do{
        rt_key.u.label = GET_MPLS_LABEL_STACK_TOP(mpls_label_stack);
        rt_un_entry = mpls_0_rt_un_route_lookup(node->spf_info.rib[MPLS_0], &rt_key);

        if(!rt_un_entry){
            printf("Node %s : MPLS Route not found\n", node->node_name);
            return;
        }

        /* In mpls table, Flag to identify nexthop type really do not matter becase
         * for a given incoming label - there will be unique nexthop types*/
        internal_un_nh_t *prim_nh = GET_FIRST_NH(rt_un_entry, 0xFF, PRIMARY_NH);
        assert(prim_nh); /*MPLS table has to have a primary nexthop, even for local labels*/

        MPLS_STACK_OP stack_op = get_internal_un_nh_stack_top_operation(prim_nh);
        mpls_label_t outgoing_mpls_label = get_internal_un_nh_stack_top_label(prim_nh);

        assert(stack_op != STACK_OPS_UNKNOWN);
        
        execute_nexthop_mpls_label_stack_on_packet(prim_nh, mpls_label_stack);

        switch(stack_op){
            case SWAP:
                printf("%u. %s(%s)---swap[%u,%u]--->(%s)%s", i++, node->node_name, 
                    get_un_next_hop_oif_name(prim_nh), rt_key.u.label, outgoing_mpls_label, 
                    get_un_next_hop_gateway_pfx(prim_nh),
                    get_un_next_hop_node(prim_nh)->node_name);
                break;
            case NEXT:
                printf("%u. %s(%s)---push[%u,%u]--->(%s)%s", i++, node->node_name, 
                    get_un_next_hop_oif_name(prim_nh), rt_key.u.label, outgoing_mpls_label, 
                    get_un_next_hop_gateway_pfx(prim_nh),
                    get_un_next_hop_node(prim_nh)->node_name);
                break;
            case POP:
                if(prim_nh->oif){
                    printf("%u. %s(%s)---PHP[%u,%u]--->(%s)%s", i++, node->node_name, 
                            get_un_next_hop_oif_name(prim_nh), rt_key.u.label, 
                            IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack) ?  0  : \
                            GET_MPLS_LABEL_STACK_TOP(mpls_label_stack),
                            get_un_next_hop_gateway_pfx(prim_nh),
                            get_un_next_hop_node(prim_nh)->node_name);
                }
                else{
                    printf("%u. %s---pop[%u,%u]--->consume", i++, node->node_name, 
                            rt_key.u.label, 
                            IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack) ?  0  : \
                            GET_MPLS_LABEL_STACK_TOP(mpls_label_stack));
                }
                break;
            default:
                ;
        }
        
        printf("\n");
        if(IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack)){
            /*Two cases here : 
             * Either PHP is performed by PHP router
             *                  Or
             * Tunnel Terminate here at egress LSR*/
            if(prim_nh->oif)
                *next_node = get_un_next_hop_node(prim_nh);  /*Case 1 : it is PHP*/
            else if(!prim_nh->oif)
                *next_node = node;                           /* case 2 : It is ultimate destination*/
            return;
        }

        /*We are here because MPLs label stack is not empty*/
        node = get_un_next_hop_node(prim_nh);
        *next_node = node;
    } while(1);
    return;
}

static pfe_engine pfe[ROUTE_TYPE_MAX];

void
init_pfe(){

    pfe[IP_ROUTE] = ipv4_pfe_engine;
    pfe[SPRING_ROUTE] = sr_pfe_engine;
    pfe[LDP_ROUTE] = ldp_pfe_engine;
    pfe[RSVP_ROUTE] = rsvp_pfe_engine;
}


/*To be implemented as state machine*/
int
ping(char *node_name, char *dst_prefix){

    trace_rc_t trace_rc = TRACE_INCOMPLETE;
    mpls_label_stack_t *mpls_label_stack = NULL;
    trace_route_pref_order_t pref_order = FIRST_PREF_ORDER;

    node_t *node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name); 
    node_t *next_node = node;

    printf("Source Node : %s, Prefix traced : %s\n", node_name, dst_prefix);

    do{
        mpls_label_stack = pfe[pref_order](node, dst_prefix, &trace_rc, &next_node);
        
        if(mpls_label_stack){
            transient_mpls_pfe_engine(next_node, mpls_label_stack, &next_node);

            /*Two possibilities :
             * 1. PHP router ended tunnel Or labelled Dest reached - mpls_label_stack will be empty
             * 2. Tunnel abrubtly ended - mpls_label_stack will not be empty*/

            if(IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack)){
                /* Auto Done : :D :
                 * Get the prefix from mpls_label_stack and feed it to Ist pref order to next_node*/
                free_mpls_label_stack(mpls_label_stack);
            }
            else{
                free_mpls_label_stack(mpls_label_stack);
                return -1;    
            }
        }
        
        if(trace_rc == TRACE_COMPLETE){
            printf("Trace Complete\n");
            return 0;
        }

        /*trace is incomplete*/
        if(node == next_node){
            pref_order = get_next_preferred_route_type(pref_order);
            if(pref_order == ROUTE_TYPE_MAX){
                printf("Node %s : No route to %s\n", node->node_name, dst_prefix);
                return -1;
            }
            continue;
        }
        else{
            node = next_node;
            pref_order = FIRST_PREF_ORDER;
        }

    } while(1);
    return 0;
}

mpls_label_stack_t *
get_new_mpls_label_stack(){

        mpls_label_stack_t *mpls_stack = calloc(1, sizeof(mpls_label_stack_t));
            mpls_stack->stack = get_new_stack();
                return mpls_stack;
}

void
free_mpls_label_stack(mpls_label_stack_t *mpls_label_stack){

        free_stack(mpls_label_stack->stack);
            free(mpls_label_stack);
}

void
PUSH_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack, mpls_label_t label){

       push(mpls_label_stack->stack, (void *)label);
}

mpls_label_t
POP_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack){

        return (mpls_label_t)pop(mpls_label_stack->stack);
}

void
SWAP_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack, mpls_label_t label){

        POP_MPLS_LABEL(mpls_label_stack);
            PUSH_MPLS_LABEL(mpls_label_stack, label);
}

char *
get_str_stackops(MPLS_STACK_OP stackop){

    switch(stackop){

        case PUSH:
            return "PUSH";
        case POP:
            return "POP";
        case SWAP:
            return "SWAP";
        default:
            return "STACK_OPS_UNKNOWN";
    }
}

