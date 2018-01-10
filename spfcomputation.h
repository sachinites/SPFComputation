/*
 * =====================================================================================
 *
 *       Filename:  spfcomputation.h
 *
 *    Description:  This file declares the Data structures for SPFComputation
 *
 *        Version:  1.0
 *        Created:  Sunday 27 August 2017 02:42:41  IST
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
 *        the Free Software Foundation, version [MAX_LEVEL].
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

#ifndef __SPFCOMPUTATION__
#define __SPFCOMPUTATION__


/*-----------------------------------------------------------------------------
 *  Do not #include graph.h in this file, as it will create circular dependency.
 *  Keep this file independant of graph.h using forward declaration
 *-----------------------------------------------------------------------------*/
typedef struct _node_t node_t;
typedef struct edge_end_ edge_end_t;

/*We need to enhance this structure more to persistently store all spf result run
  for each node in the network at spf_root only*/

/*Internal nexthop data structure - more or less same as nh_t*/

typedef struct internal_nh_t_{
    LEVEL level;
    edge_end_t *oif;    /*caching oif is enough to keep track of primary nexthops and backup LFAs*/
    node_t *node;       /*This info is valid if this structure represents RLFAs*/
    char gw_prefix[PREFIX_LEN + 1];
} internal_nh_t;

/*macros to operate on above internal_nh_t DS*/

#define next_hop_type(_internal_nh_t)                \
    ((GET_EGDE_PTR_FROM_FROM_EDGE_END(_internal_nh_t.oif))->etype)

#define get_direct_next_hop_metric(_internal_nh_t, _level)  \
    ((GET_EGDE_PTR_FROM_FROM_EDGE_END(_internal_nh_t.oif))->metric[_level])

#define next_hop_node(_internal_nh_t)                \
    (_internal_nh_t.node)

#define next_hop_gateway_pfx_mask(_internal_nh_t)    \
    ((GET_EGDE_PTR_FROM_FROM_EDGE_END(_internal_nh_t.oif))->to.prefix[_internal_nh_t.level]->mask

#define next_hop_oif_name(_internal_nh_t)            \
    (_internal_nh_t.oif->intf_name)

#define set_next_hop_gw_pfx(_internal_nh_t, pfx)            \
    (strncpy(_internal_nh_t.gw_prefix, pfx, PREFIX_LEN));   \
    _internal_nh_t.gw_prefix[PREFIX_LEN] = '\0'

#define init_internal_nh_t(_internal_nh_t)           \
    memset(&_internal_nh_t, 0, sizeof(internal_nh_t))

#define intialize_internal_nh_t(_internal_nh_t, _level, _oif_edge, _node)  \
    _internal_nh_t.level = _level;                                         \
    _internal_nh_t.oif   = &_oif_edge->from;                               \
    _internal_nh_t.node  = _node;

#define copy_internal_nh_t(_src, _dst)    \
    (_dst).level = (_src).level;          \
    (_dst).oif   = (_src).oif;            \
    (_dst).node  = (_src).node;           \
    set_next_hop_gw_pfx((_dst), (_src).gw_prefix)

#define is_internal_nh_t_equal(_nh1, _nh2)  \
    (_nh1.level == _nh2.level && _nh1.node == _nh2.node && _nh1.oif == _nh2.oif)

#define is_internal_nh_t_empty(_nh) \
    (_nh.node == NULL && _nh.oif == NULL)


typedef struct spf_result_{

    struct _node_t *node; /* Next hop details are stored in the node itself*/
    unsigned int spf_metric;
    unsigned int lsp_metric;
    internal_nh_t next_hop[NH_MAX][MAX_NXT_HOPS];
} spf_result_t;

/* spf result of a node wrt to spf_root */
typedef struct self_spf_result_{

    spf_result_t *res;
    struct _node_t *spf_root;

} self_spf_result_t ;

/*A DS to hold level independant SPF configuration
 * and results*/

typedef enum {

    FORWARD_RUN,/*To compute LFA and RLFAs*/
    FULL_RUN,     /*To compute Main routes*/
    PRC_RUN
} spf_type_t;

typedef struct spf_level_info_{

    node_t *node;
    unsigned int version; /* Version of spf run on this level*/
    unsigned int node_level_flags;
    spf_type_t spf_type;
} spf_level_info_t;


typedef struct rttable_ rttable;
typedef struct _node_t node_t;

typedef struct spf_info_{

    spf_level_info_t spf_level_info[MAX_LEVEL];
    char spff_multi_area; /* use not known : set to 1 if this node is Attached to other L2 node present in specifically other area*/

    /*spf info containers for routes*/
    ll_t *routes_list;/*Routes computed as a result of SPF run, routes computed are not level specific*/
    ll_t *priority_routes_list;/*Always add route in this list*/
    ll_t *deferred_routes_list;

    /*Routing table*/
    rttable *rttable;
} spf_info_t;

#define GET_SPF_INFO_NODE(spf_info_ptr, _level)  \
    (spf_info_ptr->spf_level_info[_level].node)

#define SPF_RUN_TYPE(spfrootptr, _level)    \
    (spfrootptr->spf_info.spf_level_info[_level].spf_type)

#define GET_SPF_RESULT(_spf_info, _node_ptr, _level)    \
        singly_ll_search_by_key(_spf_info->spf_level_info[_level].node->spf_run_result[_level], _node_ptr)

typedef struct _node_t node_t;

void
spf_computation(node_t *spf_root,
        spf_info_t *spf_info,
        LEVEL level, spf_type_t spf_type);

int
route_search_comparison_fn(void * route, void *key);

int
spf_run_result_comparison_fn(void *spf_result_ptr, void *node_ptr);

int
self_spf_run_result_comparison_fn(void *self_spf_result_ptr, void *node_ptr);

void
partial_spf_run(node_t *spf_root, LEVEL level);

unsigned int 
DIST_X_Y(node_t *X, node_t *Y, LEVEL _level);

void
spf_only_intitialization(node_t *spf_root, LEVEL level);


#endif /* __SPFCOMPUTATION__ */
