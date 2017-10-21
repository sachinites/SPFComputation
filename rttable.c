/*
 * =====================================================================================
 *
 *       Filename:  rttable.c
 *
 *    Description:  Implementation of routing table
 *
 *        Version:  1.0
 *        Created:  Thursday 28 September 2017 04:46:28  IST
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

#include "rttable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "bitsop.h"
#include "logging.h"
#include "spfutil.h"
#include "LinkedListApi.h"


extern instance_t *instance;

void
add_primary_nh(rttable_entry_t *rt_entry, nh_t *nh){

    if(rt_entry->primary_nh_count == MAX_NXT_HOPS){
        printf("%s() : Error : MAX_NXT_HOPS limit exceeded\n", __FUNCTION__);
        return;
    }

    rt_entry->primary_nh[rt_entry->primary_nh_count] = *nh;
    rt_entry->primary_nh_count++;
}

void
delete_primary_nh_by_name(rttable_entry_t *rt_entry, char *nh_name){

    int i = 0;
    for( ; i < rt_entry->primary_nh_count; i++){
        if(strncmp(rt_entry->primary_nh[i].nh_name, nh_name, strlen(nh_name)))
            continue;

        memset(&rt_entry->primary_nh[i], 0 , sizeof(nh_t));
        rt_entry->primary_nh_count--;
    }
}

void
set_backup_nh(rttable_entry_t *rt_entry, nh_t *bck_nh){

    rt_entry->backup_nh = *bck_nh;

}

rttable_entry_t *
rt_route_lookup(rttable *rttable, char *prefix, char mask){

   singly_ll_node_t *list_node = NULL;
   rttable_entry_t *rt_entry = NULL;

   ITERATE_LIST(GET_RT_TABLE(rttable), list_node){
       
        rt_entry = (rttable_entry_t *)list_node->data;   
        if(!RT_ENTRY_MATCH(rt_entry, prefix, mask))
            continue;
        return rt_entry;
   }

   return NULL;
}

int
rt_route_install(rttable *rttable, rttable_entry_t *rt_entry){

    sprintf(LOG, "Added route %s/%d to Routing table for level%d, nh_name = %s", 
        rt_entry->dest.prefix, rt_entry->dest.mask, rt_entry->level, rt_entry->primary_nh[0].nh_name); TRACE();
    singly_ll_add_node_by_val(GET_RT_TABLE(rttable), rt_entry);
    return 1;
}

int
rt_route_delete(rttable *rttable, char *prefix, char mask){

    singly_ll_node_t *list_node = NULL;

    rttable_entry_t *temp = NULL, 
                    *rt_entry = rt_route_lookup(rttable, prefix, mask);

    if(!rt_entry){
        printf("%s() : Warning route for %s/%d not found in routing table\n", __FUNCTION__, prefix, mask);
        return -1;
    }

    ITERATE_LIST(GET_RT_TABLE(rttable), list_node){
    
        temp = (rttable_entry_t *)list_node->data;
        if(temp == rt_entry)
            break;
    }
     
    singly_ll_delete_node(GET_RT_TABLE(rttable), list_node);
    return 1;
}

int
rt_route_update(rttable *rttable, rttable_entry_t *rt_entry){

    int i = 0;

    rttable_entry_t *rt_entry1 = rt_route_lookup(rttable, rt_entry->dest.prefix, rt_entry->dest.mask);
    if(!rt_entry1){
        printf("%s() : Warning route for %s/%d not found in routing table\n", 
                    __FUNCTION__, rt_entry->dest.prefix, rt_entry->dest.mask);
        return -1;
    }

    memcpy(rt_entry1->dest.prefix, rt_entry->dest.prefix, PREFIX_LEN + 1); 
    rt_entry1->dest.mask   = rt_entry->dest.mask;
    rt_entry1->version     = rt_entry->version;
    rt_entry1->cost        = rt_entry->cost;
    rt_entry1->level       = rt_entry->level;
    rt_entry1->primary_nh_count = rt_entry->primary_nh_count;
    
    for(; i < MAX_NXT_HOPS; i++){
        rt_entry1->primary_nh[i] = rt_entry->primary_nh[i];
    }

    rt_entry1->backup_nh = rt_entry->backup_nh;
    return 1;
}

rttable_entry_t *
get_longest_prefix_match(rttable *rttable, char *prefix){

    singly_ll_node_t *list_node = NULL;
    rttable_entry_t *rt_entry = NULL, 
                    *lpm_rt_entry = NULL;
    char subnet[PREFIX_LEN_WITH_MASK + 1];
    char longest_mask = 0;

    ITERATE_LIST(GET_RT_TABLE(rttable), list_node){

        rt_entry = (rttable_entry_t *)list_node->data;
        memset(subnet, 0, PREFIX_LEN_WITH_MASK + 1);
        apply_mask(prefix, rt_entry->dest.mask, subnet);
        if(strncmp(subnet, rt_entry->dest.prefix, strlen(subnet)) == 0){
            if( rt_entry->dest.mask > longest_mask){
                longest_mask = rt_entry->dest.mask;
                lpm_rt_entry = rt_entry;
            }
        }
    }
    return lpm_rt_entry;
}


void
rt_table_destory(rttable *rttable){

    delete_singly_ll(GET_RT_TABLE(rttable));
    GET_RT_TABLE(rttable)->head = NULL;
    GET_RT_TABLE(rttable)->comparison_fn = NULL;
}

rttable *
init_rttable(char *table_name){

    rttable *rttable = calloc(1, sizeof(*rttable));
    strncpy(rttable->table_name, table_name, 15);
    rttable->table_name[15] = '\0';
    rttable->rt_list = init_singly_ll();
    return rttable;
}

void
show_routing_table(rttable *rttable){

    assert(rttable);
    singly_ll_node_t *list_node = NULL;
    rttable_entry_t *rt_entry = NULL;
    char subnet[PREFIX_LEN_WITH_MASK + 1];

    printf("Table %s\n", rttable->table_name);
    printf("Destination           Version        Cost       Lvl      Gateway             Nxt-Hop        OIF      Backup\n");

    ITERATE_LIST(GET_RT_TABLE(rttable), list_node){

        rt_entry = (rttable_entry_t *)list_node->data;
        memset(subnet, 0, PREFIX_LEN_WITH_MASK + 1);
        sprintf(subnet, "%s/%d", rt_entry->dest.prefix, rt_entry->dest.mask);
        printf("%-20s      %-4d        %-6d     %-4d    %-20s    %-8s   %-22s      %s\n",
                subnet, rt_entry->version, rt_entry->cost,
                rt_entry->level, rt_entry->primary_nh[0].gwip, rt_entry->primary_nh[0].nh_name, 
                rt_entry->primary_nh[0].oif, rt_entry->backup_nh.nh_name);
    }
}


/*-----------------------------------------------------------------------------
 *  Path trace for Destination dst_prefix is invoked on node node_name
 *-----------------------------------------------------------------------------*/

typedef struct _node_t node_t;

void
show_traceroute(char *node_name, char *dst_prefix){

    node_t *node = NULL;
    rttable_entry_t * rt_entry = NULL;
    unsigned int i = 1;
     
    printf("Source Node : %s, Prefix traced : %s\n", node_name, dst_prefix);
    do{
        node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);
        rt_entry = get_longest_prefix_match(node->spf_info.rttable, dst_prefix);
        if(!rt_entry){
            printf("Node %s : No route to prefix : %s\n", node_name, dst_prefix);
            break;
        }

        /*IF the best route present in routing table is the local route
         * means destination has arrived*/
        if(strncmp(rt_entry->primary_nh[0].nh_name, node_name, strlen(node_name)) == 0){
            printf("Trace Complete\n");
            break;
        }

        printf("%u. %s(%s)--->(%s)%s\n", i++, node->node_name, rt_entry->primary_nh[0].oif, 
                rt_entry->primary_nh[0].gwip, rt_entry->primary_nh[0].nh_name);

        node_name = rt_entry->primary_nh[0].nh_name;
    }
    while(1);
}

void
rt_route_remove_backup_nh(rttable *rttable, char *prefix, char mask){

   rttable_entry_t *rt_entry = NULL; 
   
   rt_entry = rt_route_lookup(rttable, prefix, mask);
   assert(rt_entry);

   memset(&rt_entry->backup_nh, 0, sizeof(nh_t)); 
}
