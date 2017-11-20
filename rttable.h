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

typedef struct LL ll_t;

typedef struct nh_t_{
    nh_type_t nh_type;
    char oif[IF_NAME_SIZE];
    char nh_name[NODE_NAME_SIZE];
    char gwip[16];
}nh_t;

typedef struct rttable_entry_{
    
    struct dest{
        char prefix[16];
        char mask;
    } dest;

    int version; 
    LEVEL level;
    int cost; 
    int primary_nh_count; 
    nh_t primary_nh[MAX_NXT_HOPS];
    nh_t backup_nh;
    FLAG flags;

} rttable_entry_t;

typedef struct rttable_{
    
    char table_name[16];
    ll_t *rt_list;
    char visit_flag;
} rttable;

#define RT_ENTRY_MATCH(rtptr, _prefix, _mask) \
    (strncmp(rtptr->dest.prefix, _prefix, PREFIX_LEN + 1) == 0 && rtptr->dest.mask == _mask)

#define GET_NEW_RT_ENTRY()                  (calloc(1, sizeof(rttable_entry_t)));
#define FLUSH_RT_ENTRY(rtptr)               (memset(rtptr, 0, sizeof(rttable_entry_t)));
#define GET_BACK_UP_NH(rtptr)               (&(rtptr->backup_nh))
#define SET_BACK_UP_NH(rtptr, bnhptr)       (rtptr->backup_nh = *bnhptr)
#define GET_RT_TABLE(rttableptr)            (rttableptr->rt_list)
#define MARK_RT_TABLE_VISITED(rttableptr)   (rttableptr->visit_flag = 1)
#define UNMARK_RT_TABLE_VISITED(rttableptr) (rttableptr->visit_flag = 0)
#define IS_RT_TABLE_VISITED(rttableptr)     (rttableptr->visit_flag == 1)


#define ITERATE_PR_NH_BEGIN(rt_entry)     \
do{                                       \
    unsigned int _i = 0;                  \
    nh_t *_nh = NULL;                     \
    for(; _i < MAX_NXT_HOPS; _i++){       \
        _nh = &rt_entry->primary_nh[i];
                
#define ITERATE_PR_NH_END  }}while(0)

void
apply_mask(char *prefix, char mask, char *subnet);

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
show_routing_table(rttable *rttable);

int
show_traceroute(char *node_name, char *dst_prefix);

void
rt_route_remove_backup_nh(rttable *rttable, char *prefix, char mask);

#endif /* __RTTABLE__ */

