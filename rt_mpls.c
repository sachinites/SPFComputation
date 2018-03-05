/*
 * =====================================================================================
 *
 *       Filename:  rt_mpls.c
 *
 *    Description:  Interface API Implementation for MPLS forwarding table
 *
 *        Version:  1.0
 *        Created:  Sunday 04 March 2018 08:39:15  IST
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

#include "rt_mpls.h"
#include "rttable.h"
#include "instance.h"
#include "routes.h"
#include "LinkedList/LinkedListApi.h"

extern instance_t *instance;

void
show_mpls_traceroute(char *ingress_lsr_name, char *dst_prefix){

    rttable_entry_t * rt_entry = NULL;
    node_t *node = NULL;
    unsigned int i = 0;
    nh_t *prim_nh = NULL;
    mpls_label_t mpls_label = 0;

    printf("Source Node : %s, Prefix traced : %s\n", ingress_lsr_name, dst_prefix);

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, ingress_lsr_name);
    rt_entry = get_longest_prefix_match(node->spf_info.rttable, dst_prefix);

    if(!rt_entry){
        printf("Node %s : No unicast route to prefix : %s\n", ingress_lsr_name, dst_prefix);
        return;
    }

    prefix_t temp_prefix, 
             *prefix = NULL;
    init_prefix_key(&temp_prefix, rt_entry->dest.prefix, rt_entry->dest.mask);
    routes_t *unicast_route = search_route_in_spf_route_list(&node->spf_info, 
                            &temp_prefix, rt_entry->level);
    if(!unicast_route){
        printf("Error : Route not present in inet.0\n");
        return;
    }

    node_t *hosting_node = unicast_route->hosting_node;
    prefix = node_local_prefix_search(hosting_node, rt_entry->level, dst_prefix, rt_entry->dest.mask);

    assert(prefix);

    if(!prefix->prefix_sid){
        printf("Desitnation is not SR Enabled (No Prefix SID assigned)\n");
        return;
    }
    mpls_label = PREFIX_SID_VALUE(prefix);

    mpls_label_stack_t *mpls_stack = get_new_mpls_label_stack();
    PUSH_MPLS_LABEL(mpls_stack, mpls_label);

    /*Now feed the mpls stack to MPLS forwarding Engine in a do..while(1) loop*/

    do{

        mpls_rt_table_t *mpls_0 = GET_MPLS_TABLE_HANDLE(node);
        mpls_rt_entry_t *mpls_rt_entry = look_up_mpls_rt_entry(mpls_0, 
                    GET_MPLS_LABEL_STACK_TOP(mpls_stack));

        if(!mpls_rt_entry){
            printf("Node %s : No MPLS route to %s\n", node->node_name, dst_prefix);
            free_mpls_label_stack(mpls_stack);
            return;
        }

        if(mpls_rt_entry->prim_nh_count == 0){
            printf("Node : %s : No Next hop entry for InLabel : %u\n", 
            node->node_name, mpls_rt_entry->incoming_label);
            return;
        }

        switch(mpls_rt_entry->mpls_nh[0].stack_op){
            case SWAP:
                SWAP_MPLS_LABEL(mpls_stack, mpls_rt_entry->mpls_nh[0].outgoing_label);
                printf("SWAP : [%u,%u]", mpls_rt_entry->incoming_label, mpls_rt_entry->mpls_nh[0].outgoing_label);
             break;
            case NEXT:
                PUSH_MPLS_LABEL(mpls_stack, mpls_rt_entry->mpls_nh[0].outgoing_label);
                printf("PUSH : [%u,%u]", mpls_rt_entry->incoming_label, mpls_rt_entry->mpls_nh[0].outgoing_label);
             break;
            case POP:
                POP_MPLS_LABEL(mpls_stack);
                printf("POP : [%u, ]", mpls_rt_entry->incoming_label);
            break;
            default:
              ;
        }

        printf(" -- > %s (%s)\n", mpls_rt_entry->mpls_nh[0].oif, mpls_rt_entry->mpls_nh[0].gwip);
        /*Now get the primary nexthop via mpls_rt_entry->oif && mpls_rt_entry->gwip
         * from unicast routing table only*/         
        rt_entry = get_longest_prefix_match(node->spf_info.rttable, dst_prefix);
        node = NULL;
        for(i = 0; i < rt_entry->primary_nh_count[IPNH]; i++){
            
            prim_nh = &rt_entry->primary_nh[IPNH][i];
            if(strncmp(prim_nh->oif, mpls_rt_entry->mpls_nh[0].oif, IF_NAME_SIZE) == 0 &&
                strncmp(prim_nh->gwip, mpls_rt_entry->mpls_nh[0].gwip, PREFIX_LEN + 1) == 0){
                node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, prim_nh->nh_name);
                break;
            }
        }
    } while(!IS_MPLS_LABEL_STACK_EMPTY(mpls_stack));
}

void
install_mpls_forwarding_entry(mpls_rt_table_t *mpls_rt_table, 
                    mpls_rt_entry_t *mpls_rt_entry){

    mpls_rt_entry_t *existing_mpls_rt_entry = 
                look_up_mpls_rt_entry(mpls_rt_table, mpls_rt_entry->incoming_label);

    if(existing_mpls_rt_entry){
        printf("Error : Entry for InLabel %u already exists\n", mpls_rt_entry->incoming_label);
        return;
    }
    INSTALL_MPLS_RT_ENTRY(mpls_rt_table, mpls_rt_entry);
}


void
delete_mpls_forwarding_entry(mpls_rt_table_t *mpls_rt_table,
                    mpls_label_t incoming_label){

    mpls_rt_entry_t *mpls_rt_entry = look_up_mpls_rt_entry(mpls_rt_table, incoming_label);
    
    if(!mpls_rt_entry){
        printf("Warning : InLabel : %u, Mpls forwarding entry do not exist\n", incoming_label);
        return;
    }

    DELETE_MPLS_RT_ENTRY(mpls_rt_table, mpls_rt_entry);
}


void
update_mpls_forwarding_entry(mpls_rt_table_t *mpls_rt_table, mpls_label_t incoming_label,
                    mpls_rt_entry_t *mpls_rt_entry){

    assert(incoming_label == mpls_rt_entry->incoming_label);

    singly_ll_node_t *list_node = NULL;
    list_node = singly_ll_get_node_by_data_ptr(mpls_rt_table->entries, mpls_rt_entry);
    
    if(!list_node){
        printf("Warning : InLabel : %u, Mpls forwarding entry do not exist\n", incoming_label);
        return;
    }

    free(list_node->data);
    list_node->data = mpls_rt_entry;
}

mpls_rt_table_t *
init_mpls_rt_table(char *table_name){

    mpls_rt_table_t *mpls_rt_table = calloc(1, sizeof(mpls_rt_table_t));
    mpls_rt_table->entries = init_singly_ll();
    return mpls_rt_table;
}

mpls_rt_entry_t *
look_up_mpls_rt_entry(mpls_rt_table_t *mpls_rt_table, mpls_label_t incoming_label){

    singly_ll_node_t *list_node = NULL;
    mpls_rt_entry_t *mpls_rt_entry = NULL;

    ITERATE_LIST_BEGIN(mpls_rt_table->entries, list_node){

        mpls_rt_entry = list_node->data;
        if(mpls_rt_entry->incoming_label == incoming_label)
            return mpls_rt_entry;    
    } ITERATE_LIST_END;
    return NULL;
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

    pop(mpls_label_stack->stack);
    PUSH_MPLS_LABEL(mpls_label_stack, label);
}

void
process_mpls_label_stack(node_t *node, mpls_label_stack_t *mpls_label_stack){

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

static void
show_mpls_rt_entry(mpls_rt_entry_t *mpls_rt_entry){

    unsigned int i = 0;
    mpls_rt_nh_t *mpls_nh = NULL;

    printf("%-12u ", mpls_rt_entry->incoming_label);

    for(; i < mpls_rt_entry->prim_nh_count; i++){
        mpls_nh = &mpls_rt_entry->mpls_nh[i];
        if(mpls_nh->stack_op != POP)
            printf("%-12u ", mpls_nh->outgoing_label);
        else
            printf("%-12s ", "-");
        printf("%-12u %-12s %-12s %-12s %-12s\n", mpls_rt_entry->version,
                "-",
                get_str_stackops(mpls_nh->stack_op),
                mpls_nh->gwip, mpls_nh->oif);
        if(i < mpls_rt_entry->prim_nh_count -1) 
            printf("%-12s ", "");
    }
}



void
show_mpls_rt_table(node_t *node){


   singly_ll_node_t *list_node = NULL;
   mpls_rt_entry_t *mpls_rt_entry = NULL;

   mpls_rt_table_t *mpls_rt_table = GET_MPLS_TABLE_HANDLE(node);
   
   /*Hdr*/
   printf("%-12s %-12s %-12s %-12s %-12s %-12s %s\n", "InLabel", "OutLabel", "Version","Pfx Idx", "Action", "Gateway", "Oif");
   printf("===================================================================================\n");

   ITERATE_LIST_BEGIN(mpls_rt_table->entries, list_node){

        mpls_rt_entry = list_node->data;
        show_mpls_rt_entry(mpls_rt_entry);
   } ITERATE_LIST_END;
}
