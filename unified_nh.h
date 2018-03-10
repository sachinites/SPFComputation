/*
 * =====================================================================================
 *
 *       Filename:  unified_nh.h
 *
 *    Description:  This file implements the Unified NExthop structure and operations on it
 *
 *        Version:  1.0
 *        Created:  Friday 09 March 2018 12:59:51  IST
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

#ifndef __UNIFIED_NH__
#define __UNIFIED_NH__

/*Unified Nexthop Data structure*/
#include "rt_mpls.h"
#include "instanceconst.h"

typedef enum{

    INVALID_NH_TYPE,
    IP_NH,
    RSVPNH,
    LDPNH,
    SPRNH,
    NEXT_HOP_TYPE_MAX
} NH_TYPE2;

typedef struct edge_end_ edge_end_t;
typedef struct _node_t node_t;

typedef struct internal_un_nh_t_{

    /*Next hop Identifier*/
    NH_TYPE2 nh_type;

    /*Common properties*/
    LEVEL level;
    edge_end_t *oif;
    node_t *nh_node;

    /*These fielda are valid only if this
      nexthop is a backup nexthop*/
    lfa_type_t lfa_type;
    edge_end_t *protected_link;

    /*IP NH - primary and backup both*/
    struct ip_nh_t{
        char gw_prefix[PREFIX_LEN + 1];
    };

    /*RSVP NH - Primary and backup*/

    struct rsvp_nh_t{
        mpls_label_t mpls_label;
    };

    /*LDP backups (RLFAs)*/
    struct ldp_nh_t{
        char gw_prefix[PREFIX_LEN + 1];
        node_t *rlfa;
        mpls_label_t mpls_label;
    };

    /*Spring Primary nexthop and LFAs and RLFAs*/
    struct spr_nh_t{
        char gw_prefix[PREFIX_LEN + 1];
        mpls_label_stack_t mpls_label_stack;
    };

    union u_t{
        struct ip_nh_t ipnh;
        struct rsvp_nh_t rsvpnh;
        struct ldp_nh_t ldpnh;
        struct spr_nh_t sprnh;
    }; 

    union u_t nh;
    unsigned int root_metric;
    unsigned int dest_metric;
    FLAG flags;
    unsigned int ref_count; /*How many routes using this as Nexthop*/
} internal_un_nh_t;

void
free_un_nexthop(internal_un_nh_t *nh);

internal_un_nh_t *
malloc_un_nexthop();

/*Should be used if the nexthop being used is SPRING type*/
void
init_spring_nexthop(internal_un_nh_t *nh);


#define NEXTHOP_FLAG_IS_ELIGIBLE    0 /*If for some reason this nexthop is not eligible*/

#define un_next_hop_type(un_internal_nh_t_ptr)  \
    (un_internal_nh_t_ptr->nh_type)

unsigned int 
get_direct_un_next_hop_metric(internal_un_nh_t *nh, LEVEL level);

node_t *
un_next_hop_node(internal_un_nh_t *nh);

char
get_un_next_hop_gateway_pfx_mask(internal_un_nh_t *nh);

char *
get_un_next_hop_gateway_pfx(internal_un_nh_t *nh);

char *
get_un_next_hop_oif_name(internal_un_nh_t *nh);

char *
get_un_next_hop_protected_link_name(internal_un_nh_t *nh);

void
set_un_next_hop_gw_pfx(internal_un_nh_t *nh, char *prefix);

void
init_un_next_hop(internal_un_nh_t *nh, NH_TYPE2 nh_type);

void
copy_un_next_hop_t(internal_un_nh_t *src, internal_un_nh_t *dst);

boolean
is_un_nh_t_equal(internal_un_nh_t *nh1, internal_un_nh_t *nh2);

boolean
is_un_next_hop_empty(internal_un_nh_t *nh);

void
free_un_nexthop(internal_un_nh_t *nh);

#endif /* __UNIFIED_NH__ */
