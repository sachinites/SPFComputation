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

typedef struct edge_end_ edge_end_t;
typedef struct _node_t node_t;

typedef enum{
    IGP_PROTO,   /*nexthop installed by IGP*/
    LDP_PROTO,   /*nexthop installed by LDP*/
    SPRING_PROTO,/*nexthop installed by SPRING*/
    RSVP_PROTO,  /*nexthop installed by RSVP*/
    UNKNOWN_PROTO
} PROTOCOL;

typedef enum{

    INET_0,
    INET_3,
    MPLS_0,
    RIB_COUNT
} rib_type_t;

typedef struct internal_un_nh_t_{

    /*Common properties (All 4 fields are applicable for Unicast Nexthops)*/
    PROTOCOL protocol;  /*protocol which installed this nexthop*/
    char oif[IF_NAME_SIZE];        /*use it only for intf name*/
    node_t *nh_node;
    char gw_prefix[PREFIX_LEN + 1];
    
    /*inet.0 next hop*/
    /*No extra field required for inet.0 nexthop*/

    /*inet.3 nexthop*/
    struct inet_3_nh_t{
        mpls_label_t mpls_label_out[MPLS_STACK_OP_LIMIT_MAX];
        MPLS_STACK_OP stack_op[MPLS_STACK_OP_LIMIT_MAX];    
    };

    /*mpls.0 nexthop*/
    struct mpls_0_nh_t{
        mpls_label_t mpls_label_in;
        mpls_label_t mpls_label_out[MPLS_STACK_OP_LIMIT_MAX];
        MPLS_STACK_OP stack_op[MPLS_STACK_OP_LIMIT_MAX];
    } ;

    union u_t{
        struct inet_3_nh_t inet3_nh;
        struct mpls_0_nh_t mpls0_nh;
    }; 

    union u_t nh;
    #define PRIMARY_NH  0
    #define BACKUP_NH   1
    FLAG flags;
    unsigned int ref_count; /*How many routes using this as Nexthop*/

    /*A primary nexthop can have backups. Below fields represent
     * backup nexthop for this primary nexthop.*/ 
    
    /*These fielda are valid only if this
      nexthop is a backup nexthop. For primary nexthop in inet.0 
      table, this backup could be inet backup or LDP backup Or RSVP backup i.e.
      in table inet.0 or inet.3 only*/
    lfa_type_t lfa_type;
    edge_end_t *protected_link;
    //struct internal_un_nh_t_ *backup_nh;
    unsigned int root_metric;
    unsigned int dest_metric;
} internal_un_nh_t;


void
free_un_nexthop(internal_un_nh_t *nh);

internal_un_nh_t *
malloc_un_nexthop();

#define NEXTHOP_FLAG_IS_ELIGIBLE    0 /*If for some reason this nexthop is not eligible*/

unsigned int 
get_direct_un_next_hop_metric(internal_un_nh_t *nh);

node_t *
get_un_next_hop_node(internal_un_nh_t *nh);

char *
get_un_next_hop_gateway_pfx(internal_un_nh_t *nh);

char *
get_un_next_hop_oif_name(internal_un_nh_t *nh);

char *
get_un_next_hop_protected_link_name(internal_un_nh_t *nh);

void
set_un_next_hop_gw_pfx(internal_un_nh_t *nh, char *prefix);

void
init_un_next_hop(internal_un_nh_t *nh);

void
copy_un_next_hop_t(internal_un_nh_t *src, internal_un_nh_t *dst);

boolean
is_un_nh_t_equal(internal_un_nh_t *nh1, internal_un_nh_t *nh2);

boolean
is_un_next_hop_empty(internal_un_nh_t *nh);

void
free_un_nexthop(internal_un_nh_t *nh);

#endif /* __UNIFIED_NH__ */
