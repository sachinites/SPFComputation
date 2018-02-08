/*
 * =====================================================================================
 *
 *       Filename:  spfutil.h
 *
 *    Description:  This file contains utilities for SPFComputation project
 *
 *        Version:  1.0
 *        Created:  Sunday 27 August 2017 01:52:40  IST
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

#ifndef __SPFUTIL__
#define __SPFUTIL__

#include "instance.h"

#define IS_LEVEL_SET(input_level, level)    ((input_level) & (level))
#define SET_LEVEL(input_level, level)       ((input_level) |= (level))

void
print_nh_list2(internal_nh_t *nh_list);

boolean
is_present2(internal_nh_t *list, internal_nh_t *nh);

void
copy_nh_list2(internal_nh_t *src_nh_list, internal_nh_t *dst_nh_list);

boolean
is_nh_list_empty2(internal_nh_t *nh_list);

void
union_direct_nh_list2(internal_nh_t *src_direct_nh_list, internal_nh_t *dst_nh_list);

char *
get_str_level(LEVEL level);

void
union_nh_list2(internal_nh_t *src_nh_list, internal_nh_t *dst_nh_list);

char*
get_str_node_area(AREA area);

boolean
is_all_nh_list_empty2(node_t *node, LEVEL level);

void
spf_determine_multi_area_attachment(spf_info_t *spf_info,
                                    node_t *spf_root);
#if 0
#define THREAD_NODE_TO_STRUCT(structure, member_name, fn_name)                      \
    static inline structure * fn_name (char *member_addr){                          \
        unsigned int member_offset = (unsigned int)&((structure *)0)->member_name;  \
        return (structure *)(member_addr - member_offset);                          \
    }
#endif

void
apply_mask(char *prefix, char mask, char *str_prefix);

node_t *
get_system_id_from_router_id(node_t *ingress_lsr,
        char *tail_end_ip, LEVEL level);

boolean
is_broadcast_link(edge_t *edge, LEVEL level);

/*Return True if D is a member node on S's broadcast LAN segment connected 
 * by interface 'interface' */

boolean
is_broadcast_member_node(node_t *S, edge_t *interface, node_t *D, LEVEL level);

unsigned int
get_nh_count(internal_nh_t *nh_list);

void
empty_nh_list(node_t *node, LEVEL level, nh_type_t nh);

boolean
is_empty_internal_nh(internal_nh_t *nh);

void
add_to_nh_list(internal_nh_t *nh_list, internal_nh_t *nh);

char *
hrs_min_sec_format(unsigned int seconds);

#define get_next_hop_empty_slot(internal_nhlist_ptr) \
        (&(internal_nhlist_ptr[get_nh_count(internal_nhlist_ptr)]))

#endif /* __SPFUTIL__ */ 

