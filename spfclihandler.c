/*
 * =====================================================================================
 *
 *       Filename:  spfclihandler.c
 *
 *    Description:  This file defines the routines to handle show/debug/config/clear etc commands
 *
 *        Version:  1.0
 *        Created:  Sunday 03 September 2017 10:46:52  IST
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

#include "spfclihandler.h"
#include <stdio.h>

void
spf_node_slot_enable_disable(node_t *node, char *slot_name,
                                op_mode enable_or_disable){


    unsigned int i = 0;
    edge_end_t *edge_end = NULL;
    edge_t *edge = NULL;
    char found = 0;

    for(; i < MAX_NODE_INTF_SLOTS; i++){
        edge_end = node->edges[i];
        if(!edge_end){
            return;
        }

        if(strncmp(edge_end->intf_name, slot_name, strlen(edge_end->intf_name)) == 0 &&
            strlen(edge_end->intf_name) == strlen(slot_name)){
            
            edge = GET_EGDE_PTR_FROM_EDGE_END(edge_end);
            edge->status = (enable_or_disable == CONFIG_DISABLE) ? 0 : 1;
            found = 1;
        }
    }
    if(!found)
        printf("%s() : INFO : Node : %s, slot %s not found\n", __FUNCTION__, node->node_name, slot_name);
}

