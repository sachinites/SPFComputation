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
#include "rt_mpls.h"
#include "spfcomputation.h"
#include "routes.h"
#include "ldp.h"

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
#if 1
boolean
inet_0_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, 
                            internal_un_nh_t *nexthop){
    
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
        glthread_add_next(&rib->head, &rt_un_entry->glthread);
        rib->count++;
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
#endif

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
inet_3_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, 
                            internal_un_nh_t *nexthop){
    
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
        glthread_add_next(&rib->head, &rt_un_entry->glthread);
        rib->count++;
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
mpls_0_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, 
                            internal_un_nh_t *nexthop){
    
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
        glthread_add_next(&rib->head, &rt_un_entry->glthread);
        rib->count++;
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

    mpls_label_t ldp_label = 0,
                 outgoing_label = 0;

    switch(nxthop_type){
        case IPV4_RSVP_NH:
            break; /*Later*/
        case IPV4_LDP_NH:
        {
            if(is_node_best_prefix_originator(nexthop_node, route)){
                /* No need to label the packet when src and dest are 
                 * direct nbrs - that is there is not transient nxthop*/
                SET_BIT(un_nh->flags, IPV4_NH);
                UNSET_BIT(un_nh->flags, IPV4_LDP_NH);
                return un_nh;
            }
            SET_BIT(un_nh->flags, IPV4_LDP_NH);
            ldp_label = get_ldp_label_binding(nexthop_node, 
                        route->rt_key.u.prefix.prefix, route->rt_key.u.prefix.mask);

            if(!ldp_label){
                /*Means downstream nbr has not advertised the LDP label. Try for Spring label
                 * This feature is LDPoSR*/
                break;
            }
            outgoing_label = ldp_label;;
            un_nh->nh.inet3_nh.mpls_label_out[0] = outgoing_label;
            un_nh->nh.inet3_nh.stack_op[0] = PUSH;
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

    //un_nh->nh.mpls0_nh.mpls_label_in = nexthop->mpls_label_in;
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
flush_rib(rt_un_table_t *rib){
    
    rib->count = 0;
    glthread_t *curr = NULL;
    rt_un_entry_t *rt_un_entry = NULL;

    ITERATE_GLTHREAD_BEGIN(&rib->head, curr){

        rt_un_entry = glthread_to_rt_un_entry(curr);
        free_rt_un_entry(rt_un_entry);
    } ITERATE_GLTHREAD_END(&rib->head, curr);

    init_glthread(&rib->head);
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
        
            printf("%s/%u\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key));

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

        printf("%s/%u\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), RT_ENTRY_MASK(&rt_un_entry->rt_key));

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

        printf("%s/%u, Inlabel : %u\n", RT_ENTRY_PFX(&rt_un_entry->rt_key), 
            RT_ENTRY_MASK(&rt_un_entry->rt_key), RT_ENTRY_LABEL(&rt_un_entry->rt_key));

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
