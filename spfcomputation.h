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

typedef struct spf_result_{

    struct _node_t *node; /* Next hop details are stored in the node itself*/
    unsigned int spf_metric;
    unsigned int lsp_metric;
    struct _node_t *next_hop[NH_MAX][MAX_NXT_HOPS];
} spf_result_t;

/* spf result of a node wrt to spf_root */
typedef struct self_spf_result_{

    spf_result_t *res;
    struct _node_t *spf_root;

} self_spf_result_t ;

/*A DS to hold level independant SPF configuration
 * and results*/

typedef enum {

    SKELETON_RUN,/*To compute LFA and RLFAs*/
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

#endif /* __SPFCOMPUTATION__ */
