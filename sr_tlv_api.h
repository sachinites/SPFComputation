/*
 * =====================================================================================
 *
 *       Filename:  sr_tlv_api.h
 *
 *    Description: Defines the APIs to process SR related TLVs 
 *
 *        Version:  1.0
 *        Created:  Monday 26 February 2018 01:48:38  IST
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

#ifndef __SR_TLV_API__
#define __SR_TLV_API__

typedef struct _node_t node_t;

void
set_node_sid(node_t *node, unsigned int node_sid_value);

void
unset_node_sid(node_t *node);

void
set_interface_address_prefix_sid(char *interface, unsigned int prefix_sid_value);

void
unset_interface_address_prefix_sid(char *interface);

void
set_interface_adj_sid(char *interface, unsigned int adj_sid_value);

void
unset_interface_adj_sid(char *interface);

#endif /* __SR_TLV_API__ */
