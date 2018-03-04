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

    UNKNOWN,
    SWAP,
    CONTINUE = SWAP,
    NEXT,
    POP
} MPLS_STACK_OP;

typedef enum{

    PENULTIMATE_HOP_POPPING,
    ULTIMATE_HOP_POPPING,
    HOP_POPPING_NONE
} HOP_POPPING;


typedef struct _mpls_rt_entry{

    mpls_label incoming_label; /*key*/
    mpls_label outgoing_label;
    char gwip[PREFIX_LEN + 1];
    char oif[IF_NAME_SIZE];
    MPLS_STACK_OP stack_op;
    HOP_POPPING hop_popping_type;   
    unsigned int version; 
} mpls_rt_entry_t;

typedef struct _mpls_rt_table{

    ll_t *entries;    
} mpls_rt_table;

void
install_mpls_forwarding_entry(mpls_rt_entry_t *mpls_rt_entry);

void
delete_mpls_forwarding_entry(mpls_label incoming_label);

void
update_mpls_forwarding_entry(mpls_label incoming_label, 
    mpls_rt_entry_t *mpls_rt_entry);

mpls_rt_table *
init_mpls_rt_table(char *table_name);


#endif /* __MPLS_RT__ */
