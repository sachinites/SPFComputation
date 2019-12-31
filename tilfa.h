/*
 * =====================================================================================
 *
 *       Filename:  tilfa.h
 *
 *    Description:  This file contains the definition and declaration of Data structures for TILFA 
 *
 *        Version:  1.0
 *        Created:  12/08/2019 07:04:30 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Juniper Networks (https://csepracticals.wixsite.com/csepracticals), sachinites@gmail.com
 *        Company:  Juniper Networks
 *
 *        This file is part of the SPFComputation distribution (https://github.com/sachinites) 
 *        Copyright (c) 2019 Abhishek Sagar.
 *        This program is free software: you can redistribute it and/or modify it under the terms of the GNU General 
 *        Public License as published by the Free Software Foundation, version 3.
 *        
 *        This program is distributed in the hope that it will be useful, but
 *        WITHOUT ANY WARRANTY; without even the implied warranty of
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *        General Public License for more details.
 *
 *        visit website : https://csepracticals.wixsite.com/csepracticals for more courses and projects
 *                                  
 * =====================================================================================
 */

#ifndef __TILFA__
#define __TILFA__

#include "instance.h"
#include "data_plane.h"
#include <stdint.h>
#include "complete_spf_path.h"

typedef struct edge_end_ interface_t;

typedef enum{

    TILFA_PREFIX_SID_REFERENCE,
    TILFA_ADJ_SID
} tilfa_seg_type;

typedef struct gen_segment_list_{

    interface_t *oif;
    char gw_ip[PREFIX_LEN + 1];

    struct s_t{
        tilfa_seg_type seg_type;
        union gen_segment_list_u_{
            node_t *node;
            mpls_label_t adj_sid;
        }u;
    };
    struct s_t inet3_mpls_label_out[MPLS_STACK_OP_LIMIT_MAX];
    MPLS_STACK_OP inet3_stack_op[MPLS_STACK_OP_LIMIT_MAX];

    struct s_t mpls0_mpls_label_out[MPLS_STACK_OP_LIMIT_MAX];
    MPLS_STACK_OP mpls0_stack_op[MPLS_STACK_OP_LIMIT_MAX];

} gen_segment_list_t;

char *
tilfa_print_one_liner_segment_list(
            gen_segment_list_t *gen_segment_list, 
            boolean inet3, boolean mpls0);


static inline boolean
tilfa_is_gensegment_lst_empty_inet3(
        gen_segment_list_t *gen_segment_list){

    if(gen_segment_list->inet3_stack_op[0] == STACK_OPS_UNKNOWN)
        return TRUE;
    return FALSE;
}

static inline boolean
tilfa_is_gensegment_lst_empty_mpls0(
        gen_segment_list_t *gen_segment_list){

    if(gen_segment_list->mpls0_stack_op[0] == STACK_OPS_UNKNOWN)
        return TRUE;
    return FALSE;
}

typedef struct segment_list_{

    interface_t *oif;
    char gw_ip[PREFIX_LEN + 1];
    mpls_label_t mpls_label_out[MPLS_STACK_OP_LIMIT_MAX];
    MPLS_STACK_OP stack_op[MPLS_STACK_OP_LIMIT_MAX];
} segment_list_t;

typedef struct protected_resource_{
    
    node_t *plr_node;
    interface_t *protected_link;
    boolean link_protection;
    boolean node_protection;
    int ref_count;
} protected_resource_t;

static inline void tilfa_unlock_protected_resource(
        protected_resource_t *pr_res){

    pr_res->ref_count--;
    assert(pr_res->ref_count >= 0);
    if(!pr_res->ref_count)
        free(pr_res);
}

static inline void tilfa_lock_protected_resource(
        protected_resource_t *pr_res){

    pr_res->ref_count++;
}

static inline boolean
tlfa_protected_resource_equal(protected_resource_t *pr_res1,
                              protected_resource_t *pr_res2){

    if(pr_res1 == pr_res2) return TRUE;
    
    if(pr_res1->plr_node == pr_res2->plr_node &&
       pr_res1->protected_link == pr_res2->protected_link &&
       pr_res1->link_protection == pr_res2->link_protection &&
       pr_res1->node_protection == pr_res2->node_protection){

        return TRUE;
    }
    return FALSE;
}

typedef struct tilfa_segment_list_{

    node_t *dest;
    protected_resource_t *pr_res;
    uint8_t n_segment_list;
    gen_segment_list_t gen_segment_list[MAX_NXT_HOPS];
    glthread_t gen_segment_list_glue;
} tilfa_segment_list_t;
GLTHREAD_TO_STRUCT(tilfa_segment_list_to_gensegment_list, 
                    tilfa_segment_list_t, gen_segment_list_glue, curr);

typedef struct tilfa_lcl_config_{

  char protected_link[IF_NAME_SIZE]; /*key*/
  boolean link_protection;
  boolean node_protection;
  glthread_t config_glue;
} tilfa_lcl_config_t;
GLTHREAD_TO_STRUCT(tilfa_lcl_config_to_config_glue, 
        tilfa_lcl_config_t, config_glue, curr);

typedef struct tilfa_cfg_globals_{
    
    boolean is_enabled;
    uint8_t max_segments_allowed;
} tilfa_cfg_globals_t;

typedef struct tilfa_remote_spf_result_{

    node_t *node; /*root of rev spf result*/
    ll_t *rem_spf_result_lst;
} tilfa_remote_spf_result_t;

typedef struct tilfa_info_ {

    tilfa_cfg_globals_t tilfa_gl_var;
    glthread_t tilfa_lcl_config_head;

    protected_resource_t *current_resource_pruned;

    ll_t *tilfa_pre_convergence_spf_results[MAX_LEVEL];

    /* SPF results after pruning of reources*/
    ll_t *tilfa_post_convergence_spf_results[MAX_LEVEL];
    
    /*SPF Results of FORWARD run without Pruning of Resources*/
    glthread_t post_convergence_spf_path[MAX_LEVEL];

    /* SPF Results triggered on a remote node.
     * Required for PQ node evaluation*/
    ll_t *pre_convergence_remote_forward_spf_results[MAX_LEVEL];

    ll_t *pre_convergence_remote_reverse_spf_results[MAX_LEVEL];

    glthread_t tilfa_segment_list_head[MAX_LEVEL];

    boolean is_tilfa_pruned;
} tilfa_info_t;

gen_segment_list_t *
tilfa_get_segment_list(node_t *node, 
                       protected_resource_t *pr_res,
                       node_t *dst_node,
                       LEVEL level,
                       uint8_t *n_segment_list/*O/P*/);

void
init_tilfa(node_t *node);

boolean
tilfa_update_config(node_t *plr_node,
                    char *protected_link,
                    boolean link_protection,
                    boolean node_protection);

boolean
tilfa_topology_prune_protected_resource(node_t *node,
    protected_resource_t *pr_res);

void
tilfa_topology_unprune_protected_resource(node_t *node,
    protected_resource_t *pr_res);

void
tilfa_run_post_convergence_spf(node_t *spf_root, LEVEL level,
                               protected_resource_t *pr_res);

spf_path_result_t *
tilfa_lookup_spf_path_result(node_t *node, node_t *candidate_node,
                             LEVEL level);

void
compute_tilfa(node_t *spf_root, LEVEL level);

void
tilfa_clear_preconvergence_remote_spf_results(tilfa_info_t *tilfa_info, 
                            node_t *node, LEVEL level,
                            boolean reverse_spf);

spf_path_result_t *
TILFA_GET_SPF_PATH_RESULT(node_t *spf_root, node_t *node, LEVEL level);

int
tilfa_copy_gensegment_list_stacks(gen_segment_list_t *src, 
                                  gen_segment_list_t *dst,
                                  boolean inet3, 
                                  boolean mpls0,
                                  int src_stack_index,
                                  int dst_stack_index);

#endif /* __TILFA__ */
