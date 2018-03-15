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
    return nh->oif;
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
init_un_next_hop(internal_un_nh_t *nh, NH_TYPE2 nh_type){
    memset(nh, 0 , sizeof(internal_un_nh_t));
    nh->nh_type = nh_type;
}

void
copy_un_next_hop_t(internal_un_nh_t *src, internal_un_nh_t *dst){
    memcpy(dst, src, sizeof(internal_un_nh_t));
}

boolean
is_un_nh_t_equal(internal_un_nh_t *nh1, internal_un_nh_t *nh2){
    return memcmp(nh1, nh2, sizeof(internal_un_nh_t)) == 0;
}

void
free_un_nexthop(internal_un_nh_t *nh){
    free(nh);
}

internal_un_nh_t *
malloc_un_nexthop(){
    return calloc(1, sizeof(internal_un_nh_t));
}
