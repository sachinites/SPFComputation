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

#ifndef __DP__
#define __DP__

/*Unified Nexthop Data structure*/
#include "instanceconst.h"
#include "glthread.h"
#include "bitsop.h"
#include <time.h>
#include <stddef.h> /*For NULL*/

typedef struct routes_ routes_t;
typedef struct edge_end_ edge_end_t;
typedef struct _node_t node_t;

typedef enum{
    IGP_PROTO,   /*nexthop installed by IGP*/
    LDP_PROTO,   /*nexthop installed by LDP*/
    L_IGP_PROTO, /*nexthop installed by SPRING*/
    RSVP_PROTO,  /*nexthop installed by RSVP*/
    UNKNOWN_PROTO
} PROTOCOL;

static inline char *
protocol_name(PROTOCOL proto){
    switch(proto){
        case IGP_PROTO:
            return "IGP_PROTO";
        case LDP_PROTO:
            return "LDP_PROTO";
        case L_IGP_PROTO:
            return "L_IGP_PROTO";
        case RSVP_PROTO:
            return "RSVP_PROTO";
        default:
            assert(0);
    }
}

typedef enum{

    INET_0,
    INET_3,
    MPLS_0,
    RIB_COUNT
} rib_type_t;


typedef struct rt_key_{

    struct rt_pfx{
        char prefix[PREFIX_LEN + 1];
        unsigned char mask;
    };
    struct rt_u{
        struct rt_pfx prefix;
        mpls_label_t label; /*Incoming label*/
    };
    struct rt_u u;
} rt_key_t;

#define RT_ENTRY_PFX(rt_key_t_ptr)    \
    ((rt_key_t_ptr)->u.prefix.prefix)

#define RT_ENTRY_MASK(rt_key_t_ptr)   \
    ((rt_key_t_ptr)->u.prefix.mask)

#define RT_ENTRY_LABEL(rt_key_t_ptr)  \
    ((rt_key_t_ptr)->u.label)


/*MPLS Data plane*/

typedef enum {

    STACK_OPS_UNKNOWN,
    SWAP,
    CONTINUE = SWAP,
    NEXT,
    PUSH = NEXT,
    POP
} MPLS_STACK_OP;

/*MPLS stack*/
typedef struct stack stack_t;

typedef struct _mpls_label_stack{
        stack_t *stack;
} mpls_label_stack_t;

mpls_label_stack_t *
get_new_mpls_label_stack();

void
free_mpls_label_stack(mpls_label_stack_t *mpls_label_stack);

void
PUSH_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack, mpls_label_t label);

mpls_label_t
POP_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack);

void
SWAP_MPLS_LABEL(mpls_label_stack_t *mpls_label_stack, mpls_label_t label);


#define GET_MPLS_LABEL_STACK_TOP(mpls_label_stack_t_ptr)        \
    ((mpls_label_t)getTopElem(mpls_label_stack_t_ptr->stack))

#define IS_MPLS_LABEL_STACK_EMPTY(mpls_label_stack_t_ptr)       \
    isStackEmpty(mpls_label_stack_t_ptr->stack)

/*MPLS data plane*/

typedef struct internal_un_nh_t_{

    /*Common properties (All 4 fields are applicable for Unicast Nexthops)*/
    PROTOCOL protocol;  /*protocol which installed this nexthop*/
    edge_end_t *oif;        /*use it only for intf name*/
    node_t *nh_node;        /*This member should be removed*/
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
        //mpls_label_t mpls_label_in;
        mpls_label_t mpls_label_out[MPLS_STACK_OP_LIMIT_MAX];
        MPLS_STACK_OP stack_op[MPLS_STACK_OP_LIMIT_MAX];
    } ;

    union u_t{
        struct inet_3_nh_t inet3_nh;
        struct mpls_0_nh_t mpls0_nh;
    }; 

    union u_t nh;

    /*Bits 0 and 1 is used to identify whether the nexthop is primary
     * or backup nexthop. Same can also be identified using NULL check
     * on protected_link member*/
    #define PRIMARY_NH      0
    /* Bits 1 - 7 should be mutually exclusive. We need to distinguish
     * between following nexthop types since one or more of them are installed in
     * same table inet.3/mpls.0 and have same semantics*/
    #define IPV4_NH         1
    #define IPV4_RSVP_NH    2 /*When RSVP TE Tunnels are advertised as FA*/
    #define IPV4_LDP_NH     3 /*When next hop is LDP nexthop*/
    #define IPV4_SPRING_NH  4 /*When spring Tunnels are advertised as FA*/
    #define RSVP_TRANSIT_NH     5 /*Not supported*/
    #define LDP_TRANSIT_NH      6 /*supported*/
    #define SPRING_TRANSIT_NH   7 /*Supported*/

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
    time_t last_refresh_time;

    glthread_t glthread;
} internal_un_nh_t;

GLTHREAD_TO_STRUCT(glthread_to_unified_nh, internal_un_nh_t, glthread, glthreadptr);

static inline char *
get_str_nexthop_type(char flags){

    if(IS_BIT_SET(flags, IPV4_NH))
        return "IPV4_NH";

    if(IS_BIT_SET(flags, IPV4_RSVP_NH))
        return "IPV4_RSVP_NH";
        
    if(IS_BIT_SET(flags, IPV4_LDP_NH))
        return "IPV4_LDP_NH";
     
    if(IS_BIT_SET(flags, IPV4_SPRING_NH))
        return "IPV4_SPRING_NH";

    if(IS_BIT_SET(flags, RSVP_TRANSIT_NH))
        return "RSVP_TRANSIT_NH";

    if(IS_BIT_SET(flags, LDP_TRANSIT_NH ))
        return "LDP_TRANSIT_NH";

    if(IS_BIT_SET(flags, SPRING_TRANSIT_NH))
        return "SPRING_TRANSIT_NH";

    return "UNKNOWN";
}

typedef struct rt_un_entry_{

    rt_key_t rt_key;
    glthread_t nh_list_head;
    FLAG flags; /*Flags for this routing entry*/
    LEVEL level;
    time_t last_refresh_time;
    glthread_t glthread;
} rt_un_entry_t;

GLTHREAD_TO_STRUCT(glthread_to_rt_un_entry, rt_un_entry_t, glthread, glthreadptr);

static inline internal_un_nh_t *
GET_FIRST_NH(rt_un_entry_t *rt_un_entry, FLAG nh_type, 
             FLAG is_primary){

    glthread_t *curr = NULL;
    internal_un_nh_t *nexthop = NULL;

    ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr){
        
        nexthop = glthread_to_unified_nh(curr);
        if(IS_BIT_SET(nexthop->flags, is_primary) &&
            IS_BIT_SET(nexthop->flags, nh_type))
            return nexthop;
    } ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr);
    return NULL;
}

typedef struct internal_nh_t_ internal_nh_t;

typedef struct rt_un_table_{

    unsigned int count;
    glthread_t head; /*List of nexthops - primary and backups both*/
    char *rib_name;
    /*CRUD*/
    boolean (*rt_un_route_install_nexthop)(struct rt_un_table_ *, rt_key_t *, LEVEL , internal_un_nh_t *);
    boolean (*rt_un_route_install)(struct rt_un_table_ *, rt_un_entry_t *);
    rt_un_entry_t * (*rt_un_route_lookup)(struct rt_un_table_ *, rt_key_t *);
    boolean (*rt_un_route_update)(struct rt_un_table_ *, rt_un_entry_t *);
    boolean (*rt_un_route_delete)(struct rt_un_table_ *, rt_key_t *);
    boolean (*rt_un_nh_t_equal)(internal_un_nh_t *, internal_un_nh_t *);
} rt_un_table_t;

rt_un_table_t *
init_rib(rib_type_t rib);

void
free_rt_un_entry(rt_un_entry_t *rt_un_entry);

internal_un_nh_t *
lookup_clone_next_hop(rt_un_table_t *rib, rt_un_entry_t *rt_un_entry, internal_un_nh_t *nexthop);

#define UN_RTENTRY_PFX_MATCH(rt_un_entry_t_ptr, rt_key_ptr) \
    (strncmp(RT_ENTRY_PFX(rt_key_ptr), RT_ENTRY_PFX(&rt_un_entry->rt_key), PREFIX_LEN) == 0 &&    \
            RT_ENTRY_MASK(rt_key) == RT_ENTRY_MASK(&rt_un_entry->rt_key))

#define UN_RTENTRY_LABEL_MATCH(rt_un_entry_t_ptr, rt_key_ptr) \
    (RT_ENTRY_LABEL(&rt_un_entry_t_ptr->rt_key) == RT_ENTRY_LABEL(rt_key_ptr))

void
free_un_nexthop(internal_un_nh_t *nh);

internal_un_nh_t *
malloc_un_nexthop();

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

boolean
rt_un_route_delete(rt_un_table_t *rib, rt_key_t *rt_key);

void
flush_rib(rt_un_table_t *rib, LEVEL level);

internal_un_nh_t *
inet_0_unifiy_nexthop(internal_nh_t *nexthop, PROTOCOL proto);

internal_un_nh_t *
inet_3_unifiy_nexthop(internal_nh_t *nexthop, PROTOCOL proto,
                    unsigned int nxthop_type, routes_t *route);

internal_un_nh_t *
mpls_0_unifiy_nexthop(internal_nh_t *nexthop, PROTOCOL proto);

boolean
mpls_0_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, LEVEL level,
        internal_un_nh_t *nexthop);

boolean
inet_0_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, LEVEL level,
        internal_un_nh_t *nexthop);

boolean
inet_3_rt_un_route_install_nexthop(rt_un_table_t *rib, rt_key_t *rt_key, LEVEL level,
        internal_un_nh_t *nexthop);

int 
get_stack_top_index(internal_un_nh_t *nh);

int 
ping(char *node_name, char *dst_prefix);

rt_un_entry_t *
get_longest_prefix_match2(rt_un_table_t *rib, char *prefix);

static inline MPLS_STACK_OP
get_internal_un_nh_stack_top_operation(internal_un_nh_t *nxthop){

    int i = MPLS_STACK_OP_LIMIT_MAX -1;
    for(;i >= 0; i--){
        if(nxthop->nh.inet3_nh.mpls_label_out[i] == 0 &&
            nxthop->nh.inet3_nh.stack_op[i] == STACK_OPS_UNKNOWN)
            continue;
        return nxthop->nh.inet3_nh.stack_op[i];
    }
    return STACK_OPS_UNKNOWN;
}

static inline mpls_label_t
get_internal_un_nh_stack_top_label(internal_un_nh_t *nxthop){

    
    int i = MPLS_STACK_OP_LIMIT_MAX -1;
    for(;i >= 0; i--){
        if(nxthop->nh.inet3_nh.mpls_label_out[i] == 0 &&
            nxthop->nh.inet3_nh.stack_op[i] == STACK_OPS_UNKNOWN)
            continue;
        return nxthop->nh.inet3_nh.mpls_label_out[i];
    }
    return 0;
}

char *
get_str_stackops(MPLS_STACK_OP stackop);

#endif /* __DP__ */
