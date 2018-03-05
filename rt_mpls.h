/*
 * =====================================================================================
 *
 *       Filename:  rt_mpls.h
 *
 *    Description:  Interface API for MPLS forwarding table
 *
 *        Version:  1.0
 *        Created:  Sunday 04 March 2018 08:37:35  IST
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

#ifndef __MPLS_RT__
#define __MPLS_RT__

#include "instanceconst.h"
#include "LinkedList/LinkedListApi.h"

typedef enum {

    STACK_OPS_UNKNOWN,
    SWAP,
    CONTINUE = SWAP,
    NEXT,
    PUSH = NEXT,
    POP
} MPLS_STACK_OP;

typedef enum{

    PENULTIMATE_HOP_POPPING,
    ULTIMATE_HOP_POPPING,
    HOP_POPPING_NONE
} HOP_POPPING;

typedef struct _mpls_rt_nh{

    mpls_label_t outgoing_label;
    MPLS_STACK_OP stack_op;
    char gwip[PREFIX_LEN + 1];
    char oif[IF_NAME_SIZE];
} mpls_rt_nh_t;

typedef struct _mpls_rt_entry{

    mpls_label_t incoming_label; /*key*/
    unsigned int version;
    unsigned int prim_nh_count;
    mpls_rt_nh_t mpls_nh[0]; 
} mpls_rt_entry_t;

typedef struct _mpls_rt_table{
    ll_t *entries;    
} mpls_rt_table_t;

void
install_mpls_forwarding_entry(mpls_rt_table_t *mpls_rt_table, 
                    mpls_rt_entry_t *mpls_rt_entry);

void
delete_mpls_forwarding_entry(mpls_rt_table_t *mpls_rt_table,
                    mpls_label_t incoming_label);

void
update_mpls_forwarding_entry(mpls_rt_table_t *mpls_rt_table, mpls_label_t incoming_label, 
                    mpls_rt_entry_t *mpls_rt_entry);

mpls_rt_table_t *
init_mpls_rt_table(char *table_name);

mpls_rt_entry_t *
look_up_mpls_rt_entry(mpls_rt_table_t *mpls_rt_table, mpls_label_t incoming_label);

/*MPLS stack*/
typedef struct stack stack_t;

typedef struct _mpls_label_stack{
    stack_t *stack;
} mpls_label_stack_t;

mpls_label_stack_t *
get_new_mpls_label_stack();

void
free_mpls_label_stack(mpls_label_stack_t *mpls_label_stack);

void
PUSH_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack, mpls_label_t label);

mpls_label_t
POP_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack);

void
SWAP_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack, mpls_label_t label);


#define GET_MPLS_LABEL_STACK_TOP(mpls_label_stack_t_ptr)        \
    ((mpls_label_t)getTopElem(mpls_label_stack_t_ptr->stack))

#define IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack_t_ptr)       \
    isStackEmpty(mpls_label_stack_t_ptr->stack)

#define INSTALL_MPLS_RT_ENTRY(mpls_rt_table_t_ptr, mpls_rt_entry_ptr)   \
    (singly_ll_add_node_by_val(mpls_rt_table_t_ptr->entries, mpls_rt_entry_ptr))

#define DELETE_MPLS_RT_ENTRY(mpls_rt_table_t_ptr, mpls_rt_entry_ptr)   \
    {singly_ll_delete_node_by_data_ptr(mpls_rt_table_t_ptr->entries, mpls_rt_entry_ptr); \
    free(mpls_rt_entry_ptr);}


    
/*MPLS table contains only incoming/outgoing labels. For dst_prefix, if
 * IGP and MPLS path both are present, then MPLS path is preferred over IGP.
 * This behavior could be administratively changed. For a dst_prefix, find the
 * associated local label. This label will be both - incoming and outgoing label.
 * Stack this label onto stack. Feed this stack to MPLS forwarding engine*/

void
show_mpls_traceroute(char *ingress_lsr_name, char *dst_prefix);


typedef struct _node_t node_t;

void
process_mpls_label_stack(node_t *node, mpls_label_stack_t *mpls_label_stack);

void
show_mpls_rt_table(node_t *node);

char *
get_str_stackops(MPLS_STACK_OP stackop);

#define GET_MPLS_TABLE_HANDLE(nodeptr)  (nodeptr->spf_info.mpls_rt_table)

#endif /* __MPLS_RT__ */
