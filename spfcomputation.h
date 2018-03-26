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

#include "instanceconst.h"
#include "rt_mpls.h"
#include "unified_nh.h"

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
    edge_end_t *oif;   
    /*eligible only for backups. This field can be used to 
     * distinguish whether the internal_nh_t_ is primary 
     * nexthop or backup nexthop. It is NULL for primary nexthop, 
     * and Non-NULL for backup nexthops.*/ 
    edge_end_t *protected_link; 
    node_t *node; 
    char gw_prefix[PREFIX_LEN + 1];
    nh_type_t nh_type;
    /*Fields valid if nexthop is a LFA/RLFA*/
    lfa_type_t lfa_type;
    node_t *proxy_nbr;
    node_t *rlfa;
    mpls_label_t mpls_label_in; /*Used for LDP/RSVP and SR routes*/

    /*Spring out information*/
    mpls_label_t mpls_label_out[MPLS_STACK_OP_LIMIT_MAX];/*For SR routes only*/
    MPLS_STACK_OP stack_op[MPLS_STACK_OP_LIMIT_MAX];

    unsigned int root_metric;
    unsigned int dest_metric;
    /*No of destinations covered by this RLFA backup*/
    unsigned int *ref_count; 
    boolean is_eligible;
} internal_nh_t;

/*macros to operate on above internal_nh_t DS*/

#define next_hop_type(_internal_nh_t)                \
    ((_internal_nh_t).nh_type)

#define get_direct_next_hop_metric(_internal_nh_t, _level)  \
    ((GET_EGDE_PTR_FROM_FROM_EDGE_END(_internal_nh_t.oif))->metric[_level])

#define next_hop_node(_internal_nh_t)                \
    (_internal_nh_t.node)

#define next_hop_gateway_pfx_mask(_internal_nh_t)    \
    ((GET_EGDE_PTR_FROM_FROM_EDGE_END(_internal_nh_t.oif))->to.prefix[_internal_nh_t.level]->mask

#define next_hop_gateway_pfx(_internal_nh_t_ptr)         \
    (_internal_nh_t_ptr->gw_prefix)

#define next_hop_oif_name(_internal_nh_t)            \
    ((_internal_nh_t).oif->intf_name)

#define backup_next_hop_protection_name(_internal_nh_t)     \
    ((_internal_nh_t).protected_link->intf_name)

#define set_next_hop_gw_pfx(_internal_nh_t, pfx)              \
    (strncpy((_internal_nh_t).gw_prefix, pfx, PREFIX_LEN));   \
    (_internal_nh_t).gw_prefix[PREFIX_LEN] = '\0'

#define init_internal_nh_t(_internal_nh_t)           \
    memset(&_internal_nh_t, 0, sizeof(internal_nh_t))

#define intialize_internal_nh_t(_internal_nh_t, _level, _oif_edge, _node)  \
    (_internal_nh_t).level = _level;                                         \
    (_internal_nh_t).oif   = &_oif_edge->from;                               \
    (_internal_nh_t).protected_link = NULL;                                  \
    (_internal_nh_t).node  = _node;                                          \
    (_internal_nh_t).nh_type = _oif_edge->etype;                             \
    (_internal_nh_t).lfa_type = UNKNOWN_LFA_TYPE;                            \
    (_internal_nh_t).proxy_nbr = NULL;                                       \
    (_internal_nh_t).rlfa = NULL;                                            \
    (_internal_nh_t).mpls_label_in = 0;                                           \
    (_internal_nh_t).root_metric = 0;                                        \
    (_internal_nh_t).dest_metric = 0;                                        \
    (_internal_nh_t).is_eligible = FALSE

#define copy_internal_nh_t(_src, _dst)    \
    (_dst).level = (_src).level;          \
    (_dst).oif   = (_src).oif;            \
    (_dst).protected_link   = (_src).protected_link;    \
    (_dst).node  = (_src).node;                         \
    set_next_hop_gw_pfx((_dst), (_src).gw_prefix);      \
    (_dst).nh_type = (_src).nh_type;                    \
    (_dst).lfa_type = (_src).lfa_type;                  \
    (_dst).proxy_nbr = (_src).proxy_nbr;                \
    (_dst).rlfa = (_src).rlfa;                          \
    (_dst).mpls_label_in = (_src).mpls_label_in;                  \
    (_dst).root_metric = (_src).root_metric;            \
    (_dst).dest_metric = (_src).dest_metric;            \
    (_dst).is_eligible = (_src).is_eligible

#define is_internal_nh_t_equal(_nh1, _nh2)                   \
    (_nh1.level == _nh2.level && _nh1.node == _nh2.node &&   \
    _nh1.oif == _nh2.oif && _nh1.nh_type == _nh2.nh_type &&  \
    _nh1.protected_link == _nh2.protected_link &&            \
    _nh1.lfa_type == _nh2.lfa_type &&                        \
    _nh1.rlfa == _nh2.rlfa && _nh1.mpls_label_in == _nh2.mpls_label_in && \
    _nh1.root_metric == _nh2.root_metric &&                     \
    _nh1.dest_metric == _nh2.dest_metric)

#define is_internal_nh_t_empty(_nh) \
    ((_nh).oif == NULL)


#define PRINT_ONE_LINER_NXT_HOP(_internal_nh_t_ptr)                                                                 \
    if(_internal_nh_t_ptr->protected_link == NULL)                                                                  \
            printf("\t%s----%s---->%-s(%s(%s)) Root Metric : %u, Dst Metric : %u\n",                                \
            _internal_nh_t_ptr->oif->intf_name,                                                                     \
            next_hop_type(*_internal_nh_t_ptr) == IPNH ? "IPNH" : "LSPNH",                                          \
            next_hop_type(*_internal_nh_t_ptr) == IPNH ? next_hop_gateway_pfx(_internal_nh_t_ptr) : "",             \
            _internal_nh_t_ptr->node ? _internal_nh_t_ptr->node->node_name : _internal_nh_t_ptr->rlfa->node_name,   \
            _internal_nh_t_ptr->node ? _internal_nh_t_ptr->node->router_id : _internal_nh_t_ptr->rlfa->router_id,   \
            _internal_nh_t_ptr->root_metric, _internal_nh_t_ptr->dest_metric);                                      \
    else                                                                                                            \
            printf("\t%s----%s---->%-s(%s(%s)) protecting : %s -- %s Root Metric : %u, Dst Metric : %u\n",          \
            nxthop->oif->intf_name,                                                                                 \
            next_hop_type(*_internal_nh_t_ptr) == IPNH ? "IPNH" : "LSPNH",                                          \
            next_hop_type(*_internal_nh_t_ptr) == IPNH ? next_hop_gateway_pfx(_internal_nh_t_ptr) : "",             \
            _internal_nh_t_ptr->node ? _internal_nh_t_ptr->node->node_name : _internal_nh_t_ptr->rlfa->node_name,   \
            _internal_nh_t_ptr->node ? _internal_nh_t_ptr->node->router_id : _internal_nh_t_ptr->rlfa->router_id,   \
            _internal_nh_t_ptr->protected_link->intf_name, get_str_lfa_type(_internal_nh_t_ptr->lfa_type),          \
            _internal_nh_t_ptr->root_metric, _internal_nh_t_ptr->dest_metric)

/*A backup LSPNH nexthop could be LDPNH or RSVPNH, this fn is used to check which
 *  one is the backup nexthop type. This fn should be called to test backup nexthops only.
 *  Primary nexthops are never LDP nexthops in IGPs*/

static inline boolean
is_internal_backup_nexthop_rsvp(internal_nh_t *nh){

    if(nh->nh_type != LSPNH)
        return FALSE;
    /*It is either LDP Or RSVP nexthop. We know that RSVP nexthop are filled
     *exactly in IPNH manner in the internal_nh_t structure since they are treared as
     *IP adjacency (called forward Adjacencies) in the topology during SPF run*/
    if(nh->rlfa == NULL)
        return TRUE;
    return FALSE;
}


typedef struct spf_result_{

    struct _node_t *node; /* Next hop details are stored in the node itself*/
    unsigned int spf_metric;
    unsigned int lsp_metric;
    internal_nh_t next_hop[NH_MAX][MAX_NXT_HOPS];
    node_backup_req_t backup_requirement[MAX_LEVEL];
} spf_result_t;

/*We dont need next_hop[][] memory*/
static inline spf_result_t *
get_forward_spf_result_t(){

    unsigned int next_hop_offset = (unsigned int)((char *)(&(((spf_result_t *)0)->lsp_metric)) + \
            sizeof(((spf_result_t *)0)->lsp_metric));
    return calloc(1, next_hop_offset);
}


/* spf result of a node wrt to spf_root */
typedef struct self_spf_result_{

    spf_result_t *res;
    struct _node_t *spf_root;

} self_spf_result_t ;

/*A DS to hold level independant SPF configuration
 * and results*/

typedef enum {

    SPF_RUN_UNKNOWN,
    FORWARD_RUN,/*To compute LFA and RLFAs*/
    REVERSE_SPF_RUN,
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
typedef struct _mpls_rt_table mpls_rt_table_t;

typedef enum{

    UNICAST_T,
    SPRING_T,
    TOPO_MAX
} rtttype_t;

static inline char *
get_topology_name(rtttype_t topo){

    switch(topo){
        case UNICAST_T:
            return "Unicast";
        case SPRING_T:
            return "SPRING";
        default:
            assert(0);
    }
}

typedef struct spf_info_{

    spf_level_info_t spf_level_info[MAX_LEVEL];
    char spff_multi_area; /* use not known : set to 1 if this node is Attached to other L2 node present in specifically other area*/

    /*spf info containers for routes*/
    ll_t *routes_list[TOPO_MAX];/*Routes computed as a result of SPF run, routes computed are not level specific*/
    ll_t *priority_routes_list[TOPO_MAX];/*Always add route in this list*/
    ll_t *deferred_routes_list[TOPO_MAX];

    /*Routing table*/
    rttable *rttable;
    rt_un_table_t *rib[RIB_COUNT];
    mpls_rt_table_t *mpls_rt_table;
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
