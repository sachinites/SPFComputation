/*
 * =====================================================================================
 *
 *       Filename:  rsvp.h
 *
 *    Description:  This file implements RSVP related minimal functionalities
 *
 *        Version:  1.0
 *        Created:  Saturday 17 February 2018 12:45:15  IST
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

#ifndef __RSVP__
#define __RSVP__

#include "instanceconst.h"

#define RSVP_LABEL_RANGE_MIN     300000
#define RSVP_LABEL_RANGE_MAX     500000

typedef struct _node_t node_t;
typedef struct edge_end_ edge_end_t;

typedef struct _rsvp_config_{
    
    boolean is_enabled; /*Is RSVP enabled on the node*/
} rsvp_config_t;

void
enable_rsvp(node_t *node);

void
disable_rsvp(node_t *node);

mpls_label_t
get_rsvp_label_binding(node_t *down_stream_node,
        char *prefix, char mask);

int
create_targeted_rsvp_tunnel(node_t *ingress_lsr, LEVEL level , /*Ingress LSR*/
        char *edgress_lsr_rtr_id,                             /*Egress LSR router id*/
        edge_end_t *oif, char *gw_ip,
        node_t *proxy_nbr);
                     
#endif /* __RSVP__*/
