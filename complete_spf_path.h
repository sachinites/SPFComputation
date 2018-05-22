/*
 * =====================================================================================
 *
 *       Filename:  complete_spf_path.h
 *
 *    Description:  This file implements the complete spf path functionality
 *
 *        Version:  1.0
 *        Created:  Saturday 12 May 2018 03:56:32  IST
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

#ifndef __COMPLETE_SPF_PATH__
#define __COMPLETE_SPF_PATH__

#include "instance.h"

/*Initialize the spf path list in all nodes of graph
 * at level L*/

typedef struct pred_info_t_{

    node_t *node;   /*predecessor node*/
    edge_end_t *oif;
    char gw_prefix[PREFIX_LEN];
    glthread_t glue;
} pred_info_t;

GLTHREAD_TO_STRUCT(glthread_to_pred_info, pred_info_t, glue, glthreadptr);

typedef struct spf_path_result_t_{

    node_t *node;   /*key*/
    glthread_t pred_db;
    glthread_t glue;
} spf_path_result_t;

GLTHREAD_TO_STRUCT(glthread_to_spf_path_result, spf_path_result_t, glue, glthreadptr);

typedef enum{

    SR_NOT_ENABLED_SPF_ROOT,
    SR_NOT_ENABLED_ON_NODE,
    NO_DEF_ROUTE_TO_L1L2,
    PREFIX_UNREACHABLE,
    LOCAL_LABEL_NOT_AVAILABLE,
    NEXT_HOP_LABEL_NOT_AVAILABLE,
    REASON_UNKNOWN
} sr_incomplete_tunnel_reason_t;

typedef struct sr_tunn_trace_info_t_{

    node_t *curr_node;
    node_t *succ_node;
    LEVEL level;
    sr_incomplete_tunnel_reason_t reason;
} sr_tunn_trace_info_t;

/*Fn pointer type to process the spf path. User may want to
 *  * do different processing on SPF paths*/

typedef void (*spf_path_processing_fn_ptr)(glthread_t *);

void
init_spf_paths_lists(instance_t *instance, LEVEL level);

/*clear the results in the spf path list in all nodes of graph
 * at level L*/
void
clear_spf_predecessors(glthread_t *spf_predecessors);

/*Add predecessor info to path list*/
typedef struct _glthread glthread_t;

void
add_pred_info_to_spf_predecessors(glthread_t *spf_predecessors, 
                           node_t *pred_node, 
                           edge_end_t *oif, char *gw_prefix);

/*Del predecessor info from path list*/
void
del_pred_info_from_spf_predecessors(glthread_t *spf_predecessors, node_t *pred_node, 
                                edge_end_t *oif, char *gw_prefix);

/*print the local spf path list*/
void
print_local_spf_predecessors(glthread_t *spf_predecessors);

boolean 
is_pred_exist_in_spf_predecessors(glthread_t *spf_predecessors,
                                pred_info_t *pred_info);

/*copy spf_predecessors2 into spf_predecessors1 without duplicates*/
void
union_spf_predecessorss(glthread_t *spf_predecessors1,
                     glthread_t *spf_predecessors2);

void
trace_spf_path(node_t *spf_root, node_t *dst_node, LEVEL level, 
                spf_path_processing_fn_ptr fn_ptr);

sr_tunn_trace_info_t
show_sr_tunnels(node_t *spf_root, char *prefix);

static inline spf_path_result_t *
GET_SPF_PATH_RESULT(node_t *node, node_t *node2, LEVEL level, nh_type_t nh){

    glthread_t *curr = NULL;
    ITERATE_GLTHREAD_BEGIN(&node->spf_path_result[level][nh], curr){

        spf_path_result_t *spf_path_result = glthread_to_spf_path_result(curr);
        if(spf_path_result->node == node2){
            return spf_path_result; 
        }
    } ITERATE_GLTHREAD_END(&node->spf_path_result[level][nh], curr);
    return NULL;
}

void
compute_spf_paths(node_t *spf_root, LEVEL level);

void
spf_clear_spf_path_result(node_t *spf_root, LEVEL level);

void
print_spf_paths(glthread_t *path);
    
#endif /* __COMPLETE_SPF_PATH__ */
