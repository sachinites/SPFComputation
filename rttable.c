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

/*str_prefix is O/P.
 *char str_prefix[16]
 * */

void
apply_mask(char *prefix, char mask, char *str_prefix){

   unsigned int binary_prefix = 0, i = 0;
   inet_pton(AF_INET, prefix, &binary_prefix);
   binary_prefix = htonl(binary_prefix); 
   for(; i < (32 - mask); i++)
       UNSET_BIT(binary_prefix, i);
   binary_prefix = htonl(binary_prefix);
   inet_ntop(AF_INET, &binary_prefix, str_prefix, 16);
   str_prefix[15] = '\0';
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

    apply_mask(rt_entry->dest.prefix, rt_entry->dest.mask, rt_entry->dest.prefix);

    if(rt_route_lookup(rttable, rt_entry->dest.prefix, rt_entry->dest.mask)){
        printf("%s() : Error : Attempt to add Duplicate route : %s/%d", __FUNCTION__, rt_entry->dest.prefix, rt_entry->dest.mask);
        return -1;
    }
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

    memcpy(rt_entry1->dest.prefix, rt_entry->dest.prefix, 16); 
    rt_entry1->dest.mask   = rt_entry->dest.mask;
    rt_entry1->version     = rt_entry->version;
    rt_entry1->cost        = rt_entry->cost;
    rt_entry1->primary_nh_count = rt_entry->primary_nh_count;
    
    for(; i < MAX_NXT_HOPS; i++){
        rt_entry1->primary_nh[i] = rt_entry->primary_nh[i];
    }

    rt_entry1->backup_nh = rt_entry->backup_nh;
    return 1;
}

rttable_entry_t *
get_longest_prefix_match(rttable *rttable, char *prefix, char mask){

    return NULL;
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
    printf("Destination           Version        Cost         Gateway             Nxt-Hop        OIF      Backup\n");

    ITERATE_LIST(GET_RT_TABLE(rttable), list_node){

        rt_entry = (rttable_entry_t *)list_node->data;
        sprintf(subnet, "%s/%d", rt_entry->dest.prefix, rt_entry->dest.mask);
        printf("%-20s      %-4d        %-6d     %-20s    %-8s   %-22s      %s\n",
                subnet, rt_entry->version,  rt_entry->cost,
                rt_entry->primary_nh[0].gwip, rt_entry->primary_nh[0].nh_name, rt_entry->primary_nh[0].oif, 
                rt_entry->backup_nh.nh_name);
    }
}


void
show_traceroute(char *node_name, char *dst_prefix){


}
