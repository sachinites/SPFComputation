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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "rttable.h"
#include "bitsop.h"
#include "logging.h"
#include "spfutil.h"
#include "LinkedListApi.h"


extern instance_t *instance;

void
add_primary_nh(rttable_entry_t *rt_entry, nh_t *nh){

    if(rt_entry->primary_nh_count[nh->nh_type] == MAX_NXT_HOPS){
        printf("%s() : Error : MAX_NXT_HOPS limit exceeded\n", __FUNCTION__);
        return;
    }

    rt_entry->primary_nh[nh->nh_type][rt_entry->primary_nh_count[nh->nh_type]] = *nh;
    rt_entry->primary_nh_count[nh->nh_type]++;
}

rttable_entry_t *
rt_route_lookup(rttable *rttable, char *prefix, char mask){

   singly_ll_node_t *list_node = NULL;
   rttable_entry_t *rt_entry = NULL;

   ITERATE_LIST_BEGIN(GET_RT_TABLE(rttable), list_node){
       
        rt_entry = (rttable_entry_t *)list_node->data;   
        if(!RT_ENTRY_MATCH(rt_entry, prefix, mask))
            continue;
        return rt_entry;
   }ITERATE_LIST_END;

   return NULL;
}

int
rt_route_install(rttable *rttable, rttable_entry_t *rt_entry){

    sprintf(LOG, "Added route %s/%d to Routing table for level%d", 
        rt_entry->dest.prefix, rt_entry->dest.mask, rt_entry->level); TRACE();

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

    sprintf(LOG, "Deleted route %s/%d from Routing table for level%d", 
        rt_entry->dest.prefix, rt_entry->dest.mask, rt_entry->level); TRACE();

    ITERATE_LIST_BEGIN(GET_RT_TABLE(rttable), list_node){
    
        temp = (rttable_entry_t *)list_node->data;
        if(temp == rt_entry)
            break;
    }ITERATE_LIST_END;
     
    singly_ll_delete_node(GET_RT_TABLE(rttable), list_node);
    free(temp);
    temp = NULL;
    return 1;
}

int
rt_route_update(rttable *rttable, rttable_entry_t *rt_entry){

    int i = 0;
    nh_type_t nh;
    
    rttable_entry_t *rt_entry1 = rt_route_lookup(rttable, rt_entry->dest.prefix, rt_entry->dest.mask);
    
    if(!rt_entry1){
        printf("%s() : Warning route for %s/%d not found in routing table\n", 
                    __FUNCTION__, rt_entry->dest.prefix, rt_entry->dest.mask);
        return -1;
    }
    
    sprintf(LOG, "Updated route %s/%d to Routing table for level%d", 
        rt_entry->dest.prefix, rt_entry->dest.mask, rt_entry->level); TRACE();

    memcpy(rt_entry1->dest.prefix, rt_entry->dest.prefix, PREFIX_LEN + 1); 
    rt_entry1->dest.mask   = rt_entry->dest.mask;
    rt_entry1->version     = rt_entry->version;
    rt_entry1->cost        = rt_entry->cost;
    rt_entry1->level       = rt_entry->level;

    ITERATE_NH_TYPE_BEGIN(nh){
        
        rt_entry1->primary_nh_count[nh] = rt_entry->primary_nh_count[nh];

        for(i = 0; i < MAX_NXT_HOPS; i++)
            rt_entry1->primary_nh[nh][i] = rt_entry->primary_nh[nh][i];
        
    } ITERATE_NH_TYPE_END;

    for(i = 0; i < MAX_NXT_HOPS*2; i++){
        memcpy(&rt_entry1->backup_nh[i], &rt_entry->backup_nh[i], sizeof(nh_t));
    }
    return 1;
}

rttable_entry_t *
get_longest_prefix_match(rttable *rttable, char *prefix){

    singly_ll_node_t *list_node = NULL;
    rttable_entry_t *rt_entry = NULL, 
                    *lpm_rt_entry = NULL,
                    *rt_default_entry = NULL;

    char subnet[PREFIX_LEN + 1];
    char longest_mask = 0;

    ITERATE_LIST_BEGIN(GET_RT_TABLE(rttable), list_node){

        rt_entry = (rttable_entry_t *)list_node->data;
        memset(subnet, 0, PREFIX_LEN + 1);
        apply_mask(prefix, rt_entry->dest.mask, subnet);
        if(strncmp("0.0.0.0", rt_entry->dest.prefix, strlen("0.0.0.0")) == 0 &&
                rt_entry->dest.mask == 0){
            rt_default_entry = rt_entry;   
        }
        else if(strncmp(subnet, rt_entry->dest.prefix, strlen(subnet)) == 0){
            if( rt_entry->dest.mask > longest_mask){
                longest_mask = rt_entry->dest.mask;
                lpm_rt_entry = rt_entry;
            }
        }
    }ITERATE_LIST_END;
    return lpm_rt_entry ? lpm_rt_entry : rt_default_entry;
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
    UNMARK_RT_TABLE_VISITED(rttable);
    return rttable;
}

void
show_routing_table(rttable *rttable){

    assert(rttable);
    singly_ll_node_t *list_node = NULL;
    rttable_entry_t *rt_entry = NULL;
    char subnet[PREFIX_LEN_WITH_MASK + 1];
    nh_type_t nh;
    unsigned int i = 0, 
                 j = 0, 
                 total_nx_hops = 0;

    printf("Table %s\n", rttable->table_name);
    printf("Destination           Version        Metric       Level   Gateway            Nxt-Hop                     OIF         Backup Score\n");
    printf("------------------------------------------------------------------------------------------------------------------------------------\n");

    ITERATE_LIST_BEGIN(GET_RT_TABLE(rttable), list_node){

        rt_entry = (rttable_entry_t *)list_node->data;
        memset(subnet, 0, PREFIX_LEN_WITH_MASK + 1);
        sprintf(subnet, "%s/%d", rt_entry->dest.prefix, rt_entry->dest.mask);

        /*handling local prefixes*/
        
        if(rt_entry->primary_nh_count[IPNH] == 0 && 
                rt_entry->primary_nh_count[LSPNH] == 0){

            sprintf(subnet, "%s/%d", rt_entry->dest.prefix, rt_entry->dest.mask);
            printf("%-20s      %-4d        %-3d (%-3s)     %-2d    %-15s    %-s|%-8s   %-12s      %s\n",
                    subnet, rt_entry->version, rt_entry->cost, 
                    IS_BIT_SET(rt_entry->flags, PREFIX_METRIC_TYPE_EXT) ? "EXT" : "INT", 
                    rt_entry->level, 
                    "Direct", rt_entry->primary_nh[IPNH][0].nh_name, 
                    "--", "--", "");
            continue;
        }

        printf("%-20s      %-4d        %-3d (%-3s)     %-2d    ", 
                subnet, rt_entry->version, rt_entry->cost, 
                IS_BIT_SET(rt_entry->flags, PREFIX_METRIC_TYPE_EXT) ? "EXT" : "INT", 
                rt_entry->level);

        ITERATE_NH_TYPE_BEGIN(nh){
            total_nx_hops += rt_entry->primary_nh_count[nh];
        } ITERATE_NH_TYPE_END;

        ITERATE_NH_TYPE_BEGIN(nh){
            
            for(i = 0; i < rt_entry->primary_nh_count[nh]; i++, j++){
                printf("%-15s    %-s|%-22s   %-12s        \n", 
                        nh == IPNH ? rt_entry->primary_nh[nh][i].gwip : "--",  
                        rt_entry->primary_nh[nh][i].nh_name,
                        rt_entry->primary_nh[nh][i].nh_type == IPNH ? "IPNH" : "LSPNH",
                        rt_entry->primary_nh[nh][i].oif);
                        if(j < total_nx_hops -1)
                            printf("%-20s      %-4s        %-3s  %-3s      %-2s    ", "","","","","");
            }
        } ITERATE_NH_TYPE_END;

        /*print the back up here*/
        for(i = 0; i < rt_entry->backup_nh_count; i++){     
            printf("%-20s      %-4s        %-3s  %-3s      %-2s    ", "","","","","");
            nh = rt_entry->backup_nh[i].nh_type;
            /*print the back as per its type*/
            switch(nh){
                case IPNH:
                    printf("%-15s    %s|%-22s   %-12s   %u\n", 
                            rt_entry->backup_nh[i].gwip,
                            rt_entry->backup_nh[i].nh_name,
                            "IPNH",
                            rt_entry->backup_nh[i].oif,
                            5000);
                    break;
                case LSPNH:
                    printf("%-15s    LDP->%-s|%-17s   %-12s   %u\n",
                            "",
                            rt_entry->backup_nh[i].rlfa_name,
                            rt_entry->backup_nh[i].router_id,
                            rt_entry->backup_nh[i].oif,
                            6000);
                    break;
                default:
                    assert(0);
            }
        }
    }ITERATE_LIST_END;
}

static void
mark_all_rttables_unvisited(){

    singly_ll_node_t *list_node = NULL;
    node_t *node = NULL;
    rttable *rt_table = NULL;

    ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
        
        node = list_node->data;
        rt_table = node->spf_info.rttable;
        UNMARK_RT_TABLE_VISITED(rt_table);
    } ITERATE_LIST_END;
}

/*-----------------------------------------------------------------------------
 *  Path trace for Destination dst_prefix is invoked on node node_name
 *-----------------------------------------------------------------------------*/

typedef struct _node_t node_t;

/* returns 0 if dest is found, 
 * -1 if dest is not found,
 *  1 if loop is found*/

int
show_traceroute(char *node_name, char *dst_prefix){

    node_t *node = NULL;
    rttable_entry_t * rt_entry = NULL;
    unsigned int i = 1, j = 0;

    nh_type_t nh ;
    mark_all_rttables_unvisited();
     
    printf("Source Node : %s, Prefix traced : %s\n", node_name, dst_prefix);

    do{
        node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

        if(IS_RT_TABLE_VISITED(node->spf_info.rttable)){
            printf("Node %s : encountered again. Loop Detected\n", node_name);
            return 1;
        }

        rt_entry = get_longest_prefix_match(node->spf_info.rttable, dst_prefix);

        if(!rt_entry){
            printf("Node %s : No route to prefix : %s\n", node_name, dst_prefix);
            return -1;
        }

        /*IF the best route present in routing table is the local route
         * means destination has arrived*/

        if(rt_entry->primary_nh_count[IPNH] == 0 &&
                rt_entry->primary_nh_count[LSPNH] == 0){
            printf("Trace Complete\n");
            return 0;
        }

        if(rt_entry->primary_nh_count[IPNH] == 1 && rt_entry->primary_nh_count[LSPNH] == 0){
            printf("%u. %s(%s)--->(%s)%s\n", i++, node->node_name, rt_entry->primary_nh[IPNH][0].oif, 
                rt_entry->primary_nh[IPNH][0].gwip, rt_entry->primary_nh[IPNH][0].nh_name);
             node_name = rt_entry->primary_nh[IPNH][0].nh_name;
        }
        else if(rt_entry->primary_nh_count[IPNH] == 0 && rt_entry->primary_nh_count[LSPNH] == 1){
            printf("%u. %s(%s)--->(%s)%s\n", i++, node->node_name, rt_entry->primary_nh[LSPNH][0].oif, 
                rt_entry->primary_nh[LSPNH][0].gwip, rt_entry->primary_nh[LSPNH][0].nh_name);
            node_name = rt_entry->primary_nh[LSPNH][0].nh_name;
        }
        else{
            ITERATE_NH_TYPE_BEGIN(nh){
                
                /* ToDo : This fn is to be re-written as recursive or degregated to print the complete
                 * ECMP paths. Currently only the last ecmp path is traced completely*/

                for(j = 0 ; j < rt_entry->primary_nh_count[nh]; j++){
                    printf("ECMP : %u. %s(%s)--->(%s)%s\n", i, node->node_name, rt_entry->primary_nh[nh][j].oif, 
                            rt_entry->primary_nh[nh][j].gwip, rt_entry->primary_nh[nh][j].nh_name);
                    node_name = rt_entry->primary_nh[nh][j].nh_name;
                }
            } ITERATE_NH_TYPE_END;
            i++;
        }
        MARK_RT_TABLE_VISITED(node->spf_info.rttable);
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
