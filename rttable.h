/*
 * =====================================================================================
 *
 *       Filename:  rttable.h
 *
 *    Description:  This file defines the structure for implementing basic routing table. Note that, Our emphasis is not on Data structures but concepts, so we will implement routing table in a simpler way. In Industry routing table is not inimplemented this way however but rather using patricia tree. Our RT will support Longest prefix match in a not so efficieant way.
 *
 *        Version:  1.0
 *        Created:  Thursday 28 September 2017 04:00:56  IST
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

#ifndef __RTTABLE__
#define __RTTABLE__

#include "instanceconst.h"
#include <time.h>
#include <string.h>

typedef struct LL ll_t;

typedef struct nh_t_{
    char oif[IF_NAME_SIZE];
    char protected_link[IF_NAME_SIZE];
    char nh_name[NODE_NAME_SIZE]; /*IPNH | FA | RLFA*/
    char gwip[PREFIX_LEN + 1];    /*IPNH only, else 0.0.0.0*/
    nh_type_t nh_type;            /*IPNH | LSPNH*/
    char proxy_nbr_name[NODE_NAME_SIZE];    /*RLFAs only*/
    char rlfa_name[NODE_NAME_SIZE]; /*same as nh_name in case of RLFA*/
    unsigned int ldplabel;
    char router_id[PREFIX_LEN]; /*FA| RLFA only*/
    //unsigned int root_metric;
    //unsigned int dest_metric;
}nh_t;

typedef struct rttable_entry_{
    
    struct dest{
        char prefix[16];
        char mask;
    } dest;

    int version; 
    LEVEL level;
    int cost; 
    int primary_nh_count[NH_MAX]; 
    nh_t primary_nh[NH_MAX][MAX_NXT_HOPS];
    int backup_nh_count;
    nh_t backup_nh[MAX_NXT_HOPS*2];
    FLAG flags;
    time_t last_refresh_time;
} rttable_entry_t;

typedef struct rttable_{
    
    char table_name[16];
    ll_t *rt_list;
} rttable;

#define RT_ENTRY_MATCH(rtptr, _prefix, _mask) \
    (strncmp(rtptr->dest.prefix, _prefix, PREFIX_LEN + 1) == 0 && rtptr->dest.mask == _mask)

#define GET_NEW_RT_ENTRY()                  (calloc(1, sizeof(rttable_entry_t)));
#define FLUSH_RT_ENTRY(rtptr)               (memset(rtptr, 0, sizeof(rttable_entry_t)));
#define GET_RT_TABLE(rttableptr)            (rttableptr->rt_list)

/* A backup LSPNH nexthop could be LDPNH or RSVPNH, this fn is used to check which
 * one is the backup nexthop type. This fn should be called to test backup nexthops only.
 * Primary nexthops are never LDP nexthops in IGPs*/

static inline boolean
is_backup_nexthop_rsvp(nh_t *nh){

    if(nh->nh_type != LSPNH)
        return FALSE;
    /*It is either LDP Or RSVP nexthop. We know that RSVP nexthop are filled
     *exactly in IPNH manner in the internal_nh_t structure since they are treared as
     *IP adjacency (called forward Adjacencies) in the topology during SPF run*/
    if(strlen(nh->rlfa_name) == 0)
        return TRUE;
    return FALSE;
}

void
add_primary_nh(rttable_entry_t *rt_entry, nh_t *nh);

void
delete_primary_nh_by_name(rttable_entry_t *rt_entry, char *nh_name);

void
set_backup_nh(rttable_entry_t *rt_entry, nh_t *bck_nh);

rttable_entry_t *
rt_route_lookup(rttable *rttable, char *prefix, char mask);

/*return -1 on failure, 1 on success*/
int
rt_route_install(rttable *rttable, rttable_entry_t *rt_entry);

int
rt_route_delete(rttable *rttable, char *prefix, char mask);

int
rt_route_update(rttable *rttable, rttable_entry_t *rt_entry);

rttable_entry_t *
get_longest_prefix_match(rttable *rttable, char *prefix);

void
rt_table_destory(rttable *rttable);

rttable *
init_rttable(char *table_name);

void
show_routing_table(rttable *rttable, char *prefix, char mask);

int
show_traceroute(char *node_name, char *dst_prefix);

int
show_backup_traceroute(char *node_name, char *dst_prefix);

void
rt_route_remove_backup_nh(rttable *rttable, char *prefix, char mask);

#endif /* __RTTABLE__ */

