/*
 * =====================================================================================
 *
 *       Filename:  tilfa.c
 *
 *    Description:  This file contains the Implementation of TILFA
 *
 *        Version:  1.0
 *        Created:  12/08/2019 07:04:02 AM
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

#include <stdio.h>
#include "tilfa.h"
#include <assert.h>
#include "gluethread/glthread.h"
#include "complete_spf_path.h"
#include "no_warn.h"
#include "libcli.h"
#include "cmdtlv.h"
#include "spfcmdcodes.h"
#include "spfutil.h"
#include "spftrace.h"
#include "routes.h"
#include <stdint.h>

extern instance_t *instance;
typedef struct fn_ptr_arg_{

    tilfa_info_t *tilfa_info;
    protected_resource_t *pr_res;
    LEVEL level;
} fn_ptr_arg_t;

static ll_t *
tilfa_get_post_convergence_spf_result_list(
        tilfa_info_t *tilfa_info, LEVEL level){

    return tilfa_info->tilfa_post_convergence_spf_results[level];
}

static ll_t *
tilfa_get_pre_convergence_spf_result_list(
        tilfa_info_t *tilfa_info, LEVEL level){

    return tilfa_info->tilfa_pre_convergence_spf_results[level];
}

glthread_t *
tilfa_get_post_convergence_spf_path_head(
        tilfa_info_t *tilfa_info, LEVEL level){
 
    return &tilfa_info->post_convergence_spf_path[level];
}

static internal_nh_t *
tilfa_lookup_pre_convergence_primary_nexthops
            (tilfa_info_t *tilfa_info, node_t *node, LEVEL level){

    ll_t *lst = tilfa_get_pre_convergence_spf_result_list(
                tilfa_info, level);    

    spf_result_t *res = singly_ll_search_by_key(lst, (void *)node);
    if(!res) return NULL;

    return res->next_hop[IPNH];
}

static internal_nh_t *
tilfa_lookup_post_convergence_primary_nexthops
            (tilfa_info_t *tilfa_info, 
            node_t *node, 
            LEVEL level, nh_type_t nh){

    ll_t *lst = tilfa_get_post_convergence_spf_result_list(
                tilfa_info, level);    

    spf_result_t *res = singly_ll_search_by_key(lst, (void *)node);
    if(!res) return NULL;

    return res->next_hop[nh];
}

static uint32_t 
tilfa_dist_from_self(tilfa_info_t *tilfa_info, 
                node_t *node, LEVEL level){

    ll_t *lst = tilfa_get_pre_convergence_spf_result_list(
                tilfa_info, level);    

    spf_result_t *res = singly_ll_search_by_key(lst, (void *)node);

    if(!res) return INFINITE_METRIC;

    return (uint32_t)res->spf_metric;
}

static ll_t *
tilfa_get_remote_spf_result_lst(tilfa_info_t *tilfa_info, 
                             node_t *node, LEVEL level,
                             boolean flush_old_lst,
                             boolean reverse_spf){

    singly_ll_node_t *curr = NULL, *prev = NULL;
    singly_ll_node_t *curr1 = NULL, *prev1 = NULL;

    spf_result_t *spf_res = NULL;
    tilfa_remote_spf_result_t *tilfa_rem_spf_result = NULL;

    ll_t *inner_lst = NULL;
    
    ll_t *outer_lst = reverse_spf ? 
        tilfa_info->pre_convergence_remote_reverse_spf_results[level] : 
        tilfa_info->pre_convergence_remote_forward_spf_results[level]; 
    
    assert(node);

    ITERATE_LIST_BEGIN2(outer_lst, curr, prev){

        tilfa_rem_spf_result = curr->data;
        if(tilfa_rem_spf_result->node == node){
            inner_lst = tilfa_rem_spf_result->rem_spf_result_lst;
            
            if(!flush_old_lst) return inner_lst;

            ITERATE_LIST_BEGIN2(inner_lst, curr1, prev1){    
                spf_res = curr1->data;
                free(spf_res);
            }ITERATE_LIST_END2(inner_lst, curr1, prev1);
            delete_singly_ll(inner_lst);
            return inner_lst;
        }
    } ITERATE_LIST_END2(outer_lst, curr, prev);

    tilfa_rem_spf_result = calloc(1, sizeof(tilfa_remote_spf_result_t));
    tilfa_rem_spf_result->node = node;
    tilfa_rem_spf_result->rem_spf_result_lst = init_singly_ll();

    singly_ll_set_comparison_fn(
            tilfa_rem_spf_result->rem_spf_result_lst,
            spf_run_result_comparison_fn);

    singly_ll_add_node_by_val(outer_lst, (void *)tilfa_rem_spf_result);
    return tilfa_rem_spf_result->rem_spf_result_lst;
}


static uint32_t
tilfa_dist_from_x_to_y(tilfa_info_t *tilfa_info,
                node_t *x, node_t *y, LEVEL level){

    /*Get spf result of remote node X*/
    ll_t *x_spf_result_lst = 
        tilfa_get_remote_spf_result_lst(tilfa_info, x, level, FALSE, FALSE);

    if(is_singly_ll_empty(x_spf_result_lst)){
        spf_computation(x, &x->spf_info, level, 
            TILFA_RUN, x_spf_result_lst);
    }

    spf_result_t *y_res = singly_ll_search_by_key(
                x_spf_result_lst, (void *)y); 

    if(!y_res) return INFINITE_METRIC;

    return (uint32_t)y_res->spf_metric;
}

static uint32_t
tilfa_dist_from_x_to_y_reverse_spf(tilfa_info_t *tilfa_info,
                node_t *x, node_t *y, LEVEL level){

    ll_t *y_spf_result_lst = 
        tilfa_get_remote_spf_result_lst(tilfa_info, y, level, FALSE, TRUE);

    if(is_singly_ll_empty(y_spf_result_lst)){
        inverse_topology(instance, level);
        spf_computation(y, &y->spf_info, level, 
            TILFA_RUN, y_spf_result_lst);
        inverse_topology(instance, level);
    }

    spf_result_t *x_res = singly_ll_search_by_key(
                y_spf_result_lst, (void *)x); 

    if(!x_res) return INFINITE_METRIC;

    return (uint32_t)x_res->spf_metric;
}

void
init_tilfa(node_t *node){

    assert(node->tilfa_info == NULL);

    LEVEL level_it;
   
    node->tilfa_info = calloc(1, sizeof(tilfa_info_t));
    
    node->tilfa_info->tilfa_gl_var.max_segments_allowed = TILFA_MAX_SEGMENTS;
    init_glthread(&node->tilfa_info->tilfa_lcl_config_head);
    node->tilfa_info->current_resource_pruned = NULL;
    
    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        
        node->tilfa_info->tilfa_pre_convergence_spf_results[level_it] = 
            init_singly_ll();
        singly_ll_set_comparison_fn(node->tilfa_info->
            tilfa_pre_convergence_spf_results[level_it], 
            spf_run_result_comparison_fn);

        node->tilfa_info->tilfa_post_convergence_spf_results[level_it] = init_singly_ll();
        singly_ll_set_comparison_fn(node->tilfa_info->\
            tilfa_post_convergence_spf_results[level_it], spf_run_result_comparison_fn);

        node->tilfa_info->pre_convergence_remote_reverse_spf_results[level_it] = init_singly_ll();
        singly_ll_set_comparison_fn(node->tilfa_info->\
            pre_convergence_remote_reverse_spf_results[level_it], 
            spf_run_result_comparison_fn);

        init_glthread(&(node->tilfa_info->post_convergence_spf_path[level_it]));

        node->tilfa_info->pre_convergence_remote_forward_spf_results[level_it] = init_singly_ll();
    
        init_glthread(&node->tilfa_info->tilfa_segment_list_head[level_it]);
    }

    node->tilfa_info->is_tilfa_pruned = FALSE;
}

boolean
tilfa_update_config(node_t *plr_node,
                     char *protected_link,
                     boolean link_protection,
                     boolean node_protection){

    glthread_t *curr;
    boolean found = FALSE;
    boolean config_change = FALSE;
    tilfa_lcl_config_t *tilfa_lcl_config = NULL;
    tilfa_info_t *tilfa_info = plr_node->tilfa_info;

    ITERATE_GLTHREAD_BEGIN(&tilfa_info->tilfa_lcl_config_head, curr){

        tilfa_lcl_config = 
            tilfa_lcl_config_to_config_glue(curr);

        if(strncmp(tilfa_lcl_config->protected_link, 
                    protected_link, IF_NAME_SIZE)){
            continue;
        }
        found = TRUE;
        break;
    } ITERATE_GLTHREAD_END(&tilfa_info->tilfa_lcl_config_head, curr);

    if(found){
        if(link_protection == TRUE || link_protection == FALSE) {
            if(link_protection != tilfa_lcl_config->link_protection){
                tilfa_lcl_config->link_protection = link_protection;
                config_change = TRUE;
            }
        }
        if(node_protection == TRUE || node_protection == FALSE) {
            if(node_protection != tilfa_lcl_config->node_protection){
                tilfa_lcl_config->node_protection = node_protection;
                config_change = TRUE;
            } 
        }
    }
    else if(link_protection != DONT_KNOW || 
            node_protection != DONT_KNOW){
        tilfa_lcl_config = calloc(1, sizeof(tilfa_lcl_config_t));
        strncpy(tilfa_lcl_config->protected_link, protected_link, IF_NAME_SIZE);
        tilfa_lcl_config->protected_link[IF_NAME_SIZE -1] = '\0';
        if(link_protection == TRUE || link_protection == FALSE)
            tilfa_lcl_config->link_protection = link_protection;
        if(node_protection == TRUE || node_protection == FALSE)
            tilfa_lcl_config->node_protection = node_protection;
        init_glthread(&tilfa_lcl_config->config_glue);
        glthread_add_next(&tilfa_info->tilfa_lcl_config_head, 
                &tilfa_lcl_config->config_glue);
        config_change = TRUE;
    }
    if(tilfa_lcl_config->node_protection == FALSE &&
            tilfa_lcl_config->link_protection == FALSE){

        remove_glthread(&tilfa_lcl_config->config_glue);
        free(tilfa_lcl_config);
        config_change = TRUE;
    }
    return config_change;
}

void
show_tilfa_database(node_t *node){

    int i = 0;
    glthread_t *curr = NULL;
    tilfa_lcl_config_t *tilfa_lcl_config = NULL;
    tilfa_segment_list_t *tilfa_segment_list = NULL;
    
    tilfa_info_t *tilfa_info = node->tilfa_info;

    if(!tilfa_info) return;
#if 0
    if(!tilfa_info->tilfa_gl_var.is_enabled){
        printf("Tilfa Status : Disabled");
        return;
    }
#endif
    printf("Tilfa Globals : \n");
    printf("\tmax_segments_allowed = %d\n", 
        tilfa_info->tilfa_gl_var.max_segments_allowed);
    printf("\tTilfa Config : \n");
    
    ITERATE_GLTHREAD_BEGIN(&tilfa_info->tilfa_lcl_config_head, curr){
        
        tilfa_lcl_config =
            tilfa_lcl_config_to_config_glue(curr);

        printf("\t\t link protected : %s LP : %sset : NP : %sset\n",
                tilfa_lcl_config->protected_link,
                tilfa_lcl_config->link_protection ? "" : "un",
                tilfa_lcl_config->node_protection ? "" : "un");
    } ITERATE_GLTHREAD_END(&tilfa_info->tilfa_lcl_config_head, curr);
    
    printf("\tTilfa Results:\n");
    
    LEVEL level_it;
    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        
        ITERATE_GLTHREAD_BEGIN(&tilfa_info->tilfa_segment_list_head[level_it], curr){

            tilfa_segment_list = tilfa_segment_list_to_gensegment_list(curr);
            printf("\t\tProtected Resource Name : (%s)%s, Dest Protected : %s\n", 
                    tilfa_segment_list->pr_res->plr_node->node_name,
                    tilfa_segment_list->pr_res->protected_link->intf_name,
                    tilfa_segment_list->dest->node_name);
            printf("\t\tProtection type : LP : %sset   NP : %sset\n",
                    tilfa_segment_list->pr_res->link_protection ? "" : "un",
                    tilfa_segment_list->pr_res->node_protection ? "" : "un");
            printf("\t\t%s, n_segment_list = %d\n", 
                get_str_level(level_it), tilfa_segment_list->n_segment_list);

            for(i = 0 ; i < tilfa_segment_list->n_segment_list; i++){
                printf("%s\n", tilfa_print_one_liner_segment_list
                    (&tilfa_segment_list->gen_segment_list[i], TRUE, TRUE));
            }
        } ITERATE_GLTHREAD_END(&tilfa_info->tilfa_segment_list_head[level_it], curr);
    }
}

void
tilfa_clear_post_convergence_spf_path(
        glthread_t *post_convergence_spf_path_head){

    glthread_t *curr = NULL, *curr1= NULL;
    spf_path_result_t *spf_path_result = NULL;
    pred_info_t *pred_info = NULL;

        ITERATE_GLTHREAD_BEGIN(post_convergence_spf_path_head, curr){

            spf_path_result = glthread_to_spf_path_result(curr);

            ITERATE_GLTHREAD_BEGIN(&spf_path_result->pred_db, curr1){

                pred_info = glthread_to_pred_info(curr1);
                remove_glthread(&pred_info->glue);
                free(pred_info);
            } ITERATE_GLTHREAD_END(&spf_path_result->pred_db, curr1);

            remove_glthread(&spf_path_result->glue);
            free(spf_path_result);
        } ITERATE_GLTHREAD_END(post_convergence_spf_path_head, curr);

        init_glthread(post_convergence_spf_path_head);
}

static void
tilfa_clear_segments_list(
        glthread_t *tilfa_segment_list_head, 
        protected_resource_t *pr_res){

    glthread_t *curr;
    tilfa_segment_list_t *tilfa_segment_list = NULL;

    ITERATE_GLTHREAD_BEGIN(tilfa_segment_list_head, curr){

        tilfa_segment_list = tilfa_segment_list_to_gensegment_list(curr);
        
        if(!pr_res){
            remove_glthread(&tilfa_segment_list->gen_segment_list_glue);
            tilfa_unlock_protected_resource(tilfa_segment_list->pr_res);
            free(tilfa_segment_list);
            tilfa_segment_list = NULL;
            continue;
        }
        else if(tlfa_protected_resource_equal(tilfa_segment_list->pr_res,
                    pr_res)){
            remove_glthread(&tilfa_segment_list->gen_segment_list_glue);
            tilfa_unlock_protected_resource(tilfa_segment_list->pr_res);
            free(tilfa_segment_list);
            tilfa_segment_list = NULL;
            return;
        }
    } ITERATE_GLTHREAD_END(tilfa_segment_list_head, curr);
}

static void
tilfa_clear_all_pre_convergence_results(node_t *node, LEVEL level){

   tilfa_info_t *tilfa_info = node->tilfa_info;
    
   if(!tilfa_info) return;
    
   singly_ll_node_t *list_node = NULL;

   spf_result_t *result = NULL;

   ITERATE_LIST_BEGIN(tilfa_info->tilfa_pre_convergence_spf_results[level], list_node){

       result = list_node->data;
       free(result);
       result = NULL;
   }ITERATE_LIST_END;
   
   delete_singly_ll(tilfa_info->tilfa_pre_convergence_spf_results[level]);

   tilfa_clear_preconvergence_remote_spf_results(tilfa_info, 0, level, FALSE);
   
   tilfa_clear_preconvergence_remote_spf_results(tilfa_info, 0, level, TRUE);

   tilfa_clear_segments_list(&(tilfa_info->tilfa_segment_list_head[level]), 0);
}

static void
tilfa_clear_all_post_convergence_results(node_t *spf_root, LEVEL level, 
        protected_resource_t *pr_res){

    tilfa_info_t *tilfa_info = spf_root->tilfa_info;

    if(!tilfa_info) return;

    singly_ll_node_t *list_node = NULL;

    spf_result_t *result = NULL;

    ITERATE_LIST_BEGIN(tilfa_info->tilfa_post_convergence_spf_results[level], list_node){

        result = list_node->data;
        free(result);
        result = NULL;
    }ITERATE_LIST_END;

    delete_singly_ll(tilfa_info->tilfa_post_convergence_spf_results[level]);

    tilfa_clear_post_convergence_spf_path(
            tilfa_get_post_convergence_spf_path_head(spf_root->tilfa_info, level));

    tilfa_clear_segments_list(&(tilfa_info->tilfa_segment_list_head[level]), pr_res);
}

boolean
tilfa_topology_prune_protected_resource(node_t *node,
    protected_resource_t *pr_res){

    assert(pr_res);
    assert(pr_res->plr_node == node);
    edge_t *link = GET_EGDE_PTR_FROM_FROM_EDGE_END(pr_res->protected_link);
    if(pr_res->link_protection){
        link->is_tilfa_pruned = TRUE;
    }
    if(pr_res->node_protection){
        node_t *nbr_node = link->to.node;
        nbr_node->tilfa_info->is_tilfa_pruned = TRUE;
    }
    return TRUE; 
}

void
tilfa_topology_unprune_protected_resource(node_t *node,
    protected_resource_t *pr_res){

    assert(pr_res);
    assert(pr_res->plr_node == node);
    edge_t *link = GET_EGDE_PTR_FROM_FROM_EDGE_END(pr_res->protected_link);
    link->is_tilfa_pruned = FALSE;
    node_t *nbr_node = link->to.node;
    nbr_node->tilfa_info->is_tilfa_pruned = FALSE;
}

static void
compute_tilfa_post_convergence_spf_primary_nexthops(node_t *spf_root, LEVEL level){

    assert(is_singly_ll_empty(tilfa_get_post_convergence_spf_result_list
        (spf_root->tilfa_info, level)));
    spf_computation(spf_root, &spf_root->spf_info, level, 
        TILFA_RUN, tilfa_get_post_convergence_spf_result_list(spf_root->tilfa_info, level));
}

static void
compute_tilfa_pre_convergence_spf_primary_nexthops(node_t *spf_root, LEVEL level){

    assert(is_singly_ll_empty(tilfa_get_pre_convergence_spf_result_list(
        spf_root->tilfa_info, level)));
    spf_computation(spf_root, &spf_root->spf_info, level, 
        TILFA_RUN, tilfa_get_pre_convergence_spf_result_list(spf_root->tilfa_info, level));
}

spf_path_result_t *
tilfa_lookup_spf_path_result(node_t *node, node_t *candidate_node, 
                             LEVEL level){

    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(&node->tilfa_info->post_convergence_spf_path[level], curr){

        spf_path_result_t *spf_path_result = glthread_to_spf_path_result(curr);
        if(spf_path_result->node == candidate_node){
            return spf_path_result;
        }
    } ITERATE_GLTHREAD_END(&node->tilfa_info->post_convergence_spf_path[level], curr);
    return NULL;    
}

static void
tilfa_fill_protected_resource_from_config(node_t *plr_node,
            protected_resource_t **pr_res, 
            tilfa_lcl_config_t *tilfa_lcl_config){

    protected_resource_t *temp_pr_res = 
        calloc(1, sizeof(protected_resource_t));

    temp_pr_res->plr_node = plr_node;
    temp_pr_res->protected_link = get_interface_from_intf_name(plr_node, 
                        tilfa_lcl_config->protected_link);
    
    if(!temp_pr_res->protected_link){
        temp_pr_res->plr_node = NULL;
        free(temp_pr_res);
        return;
    }
     
    temp_pr_res->link_protection = tilfa_lcl_config->link_protection;
    temp_pr_res->node_protection = tilfa_lcl_config->node_protection;

    if(*pr_res){
        tilfa_unlock_protected_resource(*pr_res);
    }
    *pr_res = temp_pr_res;
    tilfa_lock_protected_resource(*pr_res);
}

void
compute_tilfa(node_t *spf_root, LEVEL level){

    tilfa_info_t *tilfa_info = spf_root->tilfa_info;
    
    if(!tilfa_info) return;
#if 0
    if(!tilfa_info->tilfa_gl_var.is_enabled){
        return;
    }
#endif
    glthread_t *curr;
    tilfa_lcl_config_t *tilfa_lcl_config = NULL;

    sprintf(instance->traceopts->b, "Node : %s : %s() triggered, level %s",
        spf_root->node_name, __FUNCTION__, get_str_level(level));
    trace(instance->traceopts, SPF_EVENTS_BIT);

    tilfa_clear_all_pre_convergence_results(spf_root, level);

    compute_tilfa_pre_convergence_spf_primary_nexthops(spf_root, level);

    if(IS_GLTHREAD_LIST_EMPTY(&tilfa_info->tilfa_lcl_config_head))
        return;

    ITERATE_GLTHREAD_BEGIN(&tilfa_info->tilfa_lcl_config_head, curr){
        
        tilfa_lcl_config = tilfa_lcl_config_to_config_glue(curr);
        tilfa_fill_protected_resource_from_config(spf_root,
            &tilfa_info->current_resource_pruned,
            tilfa_lcl_config);
        if(tilfa_info->current_resource_pruned && 
            tilfa_info->current_resource_pruned->protected_link){
            tilfa_run_post_convergence_spf(spf_root, level, 
                    tilfa_info->current_resource_pruned);
        }
    } ITERATE_GLTHREAD_END(&tilfa_info->tilfa_lcl_config_head, curr);
}

/* Pass node as NULL to clear remote spf results of all
 * nodes. Pass reverse_spf as TRUE or FALSE to clear remote
 * spf results for forward or reverse spf run*/
void
tilfa_clear_preconvergence_remote_spf_results(tilfa_info_t *tilfa_info,
                            node_t *node, LEVEL level, 
                            boolean reverse_spf){

    singly_ll_node_t *curr = NULL, *prev = NULL;
    singly_ll_node_t *curr1 = NULL, *prev1 = NULL;

    spf_result_t *spf_res = NULL;
    tilfa_remote_spf_result_t *tilfa_rem_spf_result = NULL;

    ll_t *inner_lst = NULL;
    ll_t *outer_lst = reverse_spf ? 
        tilfa_info->pre_convergence_remote_reverse_spf_results[level]:
        tilfa_info->pre_convergence_remote_forward_spf_results[level];

    ITERATE_LIST_BEGIN2(outer_lst, curr, prev){

        tilfa_rem_spf_result = curr->data;
        if(node && tilfa_rem_spf_result->node != node){
            ITERATE_LIST_CONTINUE2(outer_lst, curr, prev);
        }

        inner_lst = tilfa_rem_spf_result->rem_spf_result_lst;

        ITERATE_LIST_BEGIN2(inner_lst, curr1, prev1){

            spf_res = curr1->data;
            free(spf_res);
            curr1->data = NULL; 
        } ITERATE_LIST_END2(inner_lst, curr1, prev1);
        
        delete_singly_ll(inner_lst);
        free(inner_lst);
        tilfa_rem_spf_result->rem_spf_result_lst = NULL;

        free(tilfa_rem_spf_result);
        ITERATIVE_LIST_NODE_DELETE2(outer_lst, curr, prev);
        
        if(node) return;

    } ITERATE_LIST_END2(outer_lst, curr, prev);

    if(!node){
        assert(is_singly_ll_empty(outer_lst));
    }
}

spf_path_result_t *
TILFA_GET_SPF_PATH_RESULT(node_t *spf_root, node_t *node, LEVEL level){

    glthread_t *curr = NULL;
    glthread_t *spf_path_head = 
        tilfa_get_post_convergence_spf_path_head(spf_root->tilfa_info, level); 

    ITERATE_GLTHREAD_BEGIN(spf_path_head, curr){

        spf_path_result_t *spf_path_result = glthread_to_spf_path_result(curr);

        if(spf_path_result->node == node){
            return spf_path_result;
        }
    } ITERATE_GLTHREAD_END(spf_path_head, curr);
    return NULL;
}

/*Tilfa CLI handlers*/
int
show_tilfa_handler(param_t *param, 
                   ser_buff_t *tlv_buf, 
                   op_mode enable_or_disable){

    int cmdcode = -1;
    node_t *node = NULL;
    char *node_name = NULL;

    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else
            assert(0);
    }TLV_LOOP_END

    cmdcode = EXTRACT_CMD_CODE(tlv_buf);
    
    node = (node_t *)singly_ll_search_by_key(
            instance->instance_node_list, node_name);

    switch(cmdcode){

        case CMDCODE_DEBUG_SHOW_TILFA:
            show_tilfa_database(node);
            break;
        default:
            assert(0);
    }
    return 0;
}

boolean
tilfa_is_link_pruned(edge_t *edge){

    return edge->is_tilfa_pruned;
}

boolean 
tilfa_is_node_pruned(node_t *node){

    return node->tilfa_info->is_tilfa_pruned;
}

/*Code Duplicacy detected !!*/
static boolean
tilfa_does_nexthop_overlap(internal_nh_t *one_nh, 
                internal_nh_t *nh_lst){

    if(is_nh_list_empty2(nh_lst))
        return FALSE;

    int i = 0;

    for(; i < MAX_NXT_HOPS; i++){
        
        if(is_empty_internal_nh(&nh_lst[i]))
            return FALSE;

        switch(nh_lst[i].nh_type){
            case UNICAST:
                switch(one_nh->nh_type){
                    case UNICAST:
                        if(one_nh->oif == nh_lst[i].oif)
                            return TRUE;
                    break;
                    case LSP:
                        if(one_nh->oif == nh_lst[i].oif)
                            return TRUE;
                    break;
                    default:
                    ;
                }
            break;
            case LSP:
                switch(one_nh->nh_type){
                    case UNICAST:
                        if(one_nh->oif == nh_lst[i].oif)
                            return TRUE; 
                    break;
                    case LSP:
                        if(one_nh->oif == nh_lst[i].oif)
                            return TRUE;
                    break;
                    default:
                    ;
                }
            break;
            default:
                ;
        }
    }
    return FALSE;
}

static boolean
tilfa_does_nexthop_overlap2(internal_nh_t *one_nh, 
                internal_nh_t **nh_lst){

    int i = 0;
    //return FALSE; 
    for(; i < MAX_NXT_HOPS; i++){
    
        if(!nh_lst[i]) return FALSE;
        
        switch(nh_lst[i]->nh_type){
            case UNICAST:
                switch(one_nh->nh_type){
                    case UNICAST:
                        if(one_nh->oif == nh_lst[i]->oif)
                            return TRUE;
                    break;
                    case LSP:
                        if(one_nh->oif == nh_lst[i]->oif)
                            return TRUE;
                    break;
                    default:
                    ;
                }
            break;
            case LSP:
                switch(one_nh->nh_type){
                    case UNICAST:
                        if(one_nh->oif == nh_lst[i]->oif)
                            return TRUE; 
                    break;
                    case LSP:
                        if(one_nh->oif == nh_lst[i]->oif)
                            return TRUE;
                    break;
                    default:
                    ;
                }
            break;
            default:
                ;
        }
    }
    return FALSE;
}

static int
tilfa_compute_first_hop_segments(node_t *spf_root, 
                node_t *first_hop_node,
                internal_nh_t *dst_pre_convergence_nhps,
                internal_nh_t **first_hop_segments,
                LEVEL level){

   int i = 0, n = 0;
   spf_result_t *res = NULL;
   internal_nh_t *first_hop_segment = NULL;
   internal_nh_t *first_hop_segment_array = NULL;
   
   tilfa_info_t *tilfa_info = spf_root->tilfa_info;
   nh_type_t nh = LSPNH;

   memset(first_hop_segments, 0, 
        sizeof(internal_nh_t *) * MAX_NXT_HOPS);

   /* First collect RSVP LSP NHs, followed
    * by IPNH. This is done to reject IPNHs over
    * RSVP LSP NHs in case of conflicts*/
   do{
       first_hop_segment_array = 
           tilfa_lookup_post_convergence_primary_nexthops(tilfa_info,
                   first_hop_node, level, nh);

       assert(first_hop_segment_array);

       for( i = 0; i < MAX_NXT_HOPS; i++){

           first_hop_segment = &first_hop_segment_array[i];
           if(is_internal_nh_t_empty((*first_hop_segment))){
               break;
           }
           if(first_hop_segment->node != first_hop_node)
               continue;
           if(tilfa_does_nexthop_overlap(first_hop_segment,
                       dst_pre_convergence_nhps))
               continue;
            /* Weed out conflicting nexthops. For example,
             * Two RSVP LSPs can go out of same physical interface
             * Or RSVP LSP oif can coincide with IPNH*/
            if(tilfa_does_nexthop_overlap2(first_hop_segment, 
                first_hop_segments))  /*This needs to be addressed, incompatible arg passed*/
               continue;
           
           first_hop_segments[n++] = first_hop_segment;
           if(n == MAX_NXT_HOPS) return n;
       }
       if(nh == LSPNH)
           nh = IPNH;
       else break;
   }while(1);
   return n;
}

static boolean
tilfa_p_node_qualification_test_wrt_root(
                node_t *spf_root,
                node_t *node_to_test,
                node_t *first_hop_node,
                node_t *dst_node, 
                protected_resource_t *pr_res,
                LEVEL level,
                internal_nh_t **first_hop_segments){

    tilfa_info_t *tilfa_info = spf_root->tilfa_info;
    
    internal_nh_t *dst_pre_convergence_nhps =
        tilfa_lookup_pre_convergence_primary_nexthops(tilfa_info,
            dst_node, level);

    if(!dst_pre_convergence_nhps || 
        is_nh_list_empty2(dst_pre_convergence_nhps)){
        return FALSE;
    }

    /* Now check if node_to_test is a p-node from first_hop_node
     * of spf_root*/

    if(node_to_test == first_hop_node){
        /*node_to_test is already a p-node*/
        return (tilfa_compute_first_hop_segments(spf_root,
                 first_hop_node, 
                 dst_pre_convergence_nhps, 
                 first_hop_segments, level) != 0);
    }

    edge_t *edge = GET_EGDE_PTR_FROM_EDGE_END(
                        pr_res->protected_link);
    
    node_t *protected_node = edge->to.node;

    /*Now test p-node condition*/
    uint32_t dist_nbr_to_pnode = tilfa_dist_from_x_to_y(
            tilfa_info, first_hop_node, node_to_test , level);
    uint32_t dist_nbr_to_S = tilfa_dist_from_x_to_y(
            tilfa_info, first_hop_node, spf_root, level);
    uint32_t dist_S_to_pnode = tilfa_dist_from_self(
            tilfa_info, node_to_test, level);

    /*loop free wrt Source*/
    if(!(dist_nbr_to_pnode < dist_nbr_to_S + dist_S_to_pnode))
        return FALSE;

    /*I think downstream criteria is automatically met since the
     * p-node lies on post-convergence path*/

    uint32_t dist_E_to_pnode = tilfa_dist_from_x_to_y(
            tilfa_info, protected_node, node_to_test, level);

    if(pr_res->node_protection){
        uint32_t dist_nbr_to_E = tilfa_dist_from_x_to_y(
                tilfa_info, first_hop_node, protected_node, level);
        
        if(dist_nbr_to_pnode < dist_nbr_to_E + dist_E_to_pnode){
            
            /* node_to_test is qualified to be p-node through 
             * first_hop_node*/
            return (tilfa_compute_first_hop_segments(spf_root, 
                    first_hop_node, 
                    dst_pre_convergence_nhps, 
                    first_hop_segments, level) != 0);
        }
    }

    if(pr_res->link_protection){
        if(dist_nbr_to_pnode < 
                (dist_nbr_to_S + edge->metric[level] + dist_E_to_pnode)){

            /* node_to_test is qualified to be p-node through 
             * first_hop_node*/
            return (tilfa_compute_first_hop_segments(spf_root, 
                        first_hop_node, 
                        dst_pre_convergence_nhps, 
                        first_hop_segments, level) != 0);
        }
    }

    return FALSE;
}

static boolean
tilfa_q_node_qualification_test_wrt_destination(
                node_t *spf_root,
                node_t *node_to_test, 
                node_t *destination,
                protected_resource_t *pr_res,
                LEVEL level){


    /*All nodes are Q-nodes wrt to self*/

    tilfa_info_t *tilfa_info = spf_root->tilfa_info;

    if(node_to_test == destination)
        return TRUE;

    uint32_t dist_q_to_d = tilfa_dist_from_x_to_y_reverse_spf(tilfa_info,
                destination, node_to_test, level);
    
    uint32_t dist_q_to_protected_node = INFINITE_METRIC,
             dist_protected_node_to_d = INFINITE_METRIC,
             dist_q_to_S = INFINITE_METRIC,
             dist_S_to_d = INFINITE_METRIC;

    if(dist_q_to_d == INFINITE_METRIC)
        return FALSE;

    assert(pr_res->plr_node == spf_root);

    edge_t *edge = GET_EGDE_PTR_FROM_EDGE_END(
                pr_res->protected_link);
    node_t *protected_node = edge->to.node;

    /* Mandatory condition should be satisified : 
     * q node must be loop-free node wrt S*/
    dist_q_to_S = tilfa_dist_from_x_to_y_reverse_spf(tilfa_info,
            spf_root, node_to_test, level);
    
    dist_S_to_d = tilfa_dist_from_self(tilfa_info,
            destination, level);

    if(!(dist_q_to_d < dist_q_to_S + dist_S_to_d)){
        return FALSE;
    }

    dist_protected_node_to_d = tilfa_dist_from_x_to_y(tilfa_info,
            protected_node, destination, level);

    if(pr_res->node_protection){
        /*Test Node protection status*/
        dist_q_to_protected_node = tilfa_dist_from_x_to_y_reverse_spf(tilfa_info,
            protected_node, node_to_test, level);

        if(dist_q_to_d < dist_q_to_protected_node + dist_protected_node_to_d){
            return TRUE;
        }
    }
    /*If node protection is not feasilble, check for link protection*/
    if(pr_res->link_protection){
        /*Test Link protection Status*/ 
        if(dist_q_to_d < dist_q_to_S + edge->metric[level] 
                + dist_protected_node_to_d){
            return TRUE;
        }
    }
    return FALSE;
}

static boolean
tilfa_is_destination_impacted(tilfa_info_t *tilfa_info, 
                              node_t *dest, LEVEL level,
                              protected_resource_t *pr_res){

    internal_nh_t *pre_convergence_nhps = NULL;

    pre_convergence_nhps = tilfa_lookup_pre_convergence_primary_nexthops
                            (tilfa_info, dest, level);

    if(is_nh_list_empty2(pre_convergence_nhps)){
        return FALSE;
    }

    /*Just check the OIF overlap*/
    int i = 0;

    for(; i < MAX_NXT_HOPS; i++){

        if(is_empty_internal_nh(&pre_convergence_nhps[i]))
            return FALSE;

        if(pr_res->protected_link == pre_convergence_nhps[i].oif)
            return TRUE;

    }
    return FALSE;
}

static void
print_raw_tilfa_path(node_t *spf_root, 
                 internal_nh_t **nexthops, 
                 node_t *p_node, 
                 node_t *q_node, 
                 node_t *dest, 
                 int q_distance, 
                 int pq_distance){

    sprintf(instance->traceopts->b, "%s()\nPLR node = %s, DEST = %s", 
        __FUNCTION__, spf_root->node_name, dest->node_name);
    trace(instance->traceopts, TILFA_BIT);

    if(!nexthops){
        sprintf(instance->traceopts->b, "Nexthop Array : NULL"); 
        trace(instance->traceopts, TILFA_BIT);
    }
    else if(!nexthops[0]){
        sprintf(instance->traceopts->b, "Nexthop Array : Empty");
        trace(instance->traceopts, TILFA_BIT);
    }
    else{
        int i = 0;
        for( ; nexthops[i]; i++){
            sprintf(instance->traceopts->b, "oif = %s, gw = %s", 
                nexthops[i]->oif->intf_name, 
                nexthops[i]->gw_prefix);
            trace(instance->traceopts, TILFA_BIT);
        }
    }

    sprintf(instance->traceopts->b, "p_node = %s(%d), q_node = %s(%d)", 
            p_node ? p_node->node_name : "Nil" ,
            pq_distance,
            q_node ? q_node->node_name : "Nil",
            q_distance);
    trace(instance->traceopts, TILFA_BIT);
}

void
tilfa_genseglst_fill_first_hop_segment(
                node_t *spf_root,
                LEVEL level,
                gen_segment_list_t *gen_segment_list, 
                internal_nh_t *nxthop_ptr, 
                int *stack_top, 
                node_t *dest){

    int i = 0;
    gen_segment_list->oif = nxthop_ptr->oif;  /*Actual Physical Interface*/
    strncpy(gen_segment_list->gw_ip, nxthop_ptr->gw_prefix, PREFIX_LEN);
    gen_segment_list->gw_ip[PREFIX_LEN] = '\0';
    gen_segment_list->nxthop = nxthop_ptr->node;

    if(nxthop_ptr->nh_type == LSPNH){

        /* Copy the entire RSVP LSP label stack into gen_segment_list
         * inet3 label stack with bottom most stack operation as PUSH*/
        for( i = 0 ; i < MPLS_STACK_OP_LIMIT_MAX; i++){

            if(nxthop_ptr->stack_op[i] == STACK_OPS_UNKNOWN)
                break;
            tilfa_insert_adj_sid_in_label_stack(gen_segment_list, *stack_top + i, 
                TRUE, FALSE, spf_root, nxthop_ptr->node, level, 
                nxthop_ptr->mpls_label_out[i], RSVP_LSP_LABEL);
            gen_segment_list->inet3_stack_op[*stack_top + i] = PUSH;
        }

        /* Copy the entire RSVP LSP label stack into gen_segment_list
         * mpls.0 label stack with bottom most stack operation as SWAP*/
        for( i = 0 ; i < MPLS_STACK_OP_LIMIT_MAX; i++){

            if(nxthop_ptr->stack_op[i] == STACK_OPS_UNKNOWN)
                break;
            tilfa_insert_adj_sid_in_label_stack(gen_segment_list, *stack_top + i, 
                FALSE, TRUE, spf_root, nxthop_ptr->node, level, 
                nxthop_ptr->mpls_label_out[i], RSVP_LSP_LABEL);
            gen_segment_list->mpls0_stack_op[*stack_top + i] = PUSH;
        }

        /*Overwrite the top most label with SWAP if RSVP LSP tail-end
         * destination is also a nexthop*/
        if(nxthop_ptr->node == dest)
            gen_segment_list->mpls0_stack_op[*stack_top + i] = SWAP;

        *stack_top += i;
        gen_segment_list->is_fhs_rsvp_lsp = TRUE;
    }
}

static boolean 
tilfa_attempt_connect_p_q_by_prefix_sid(node_t *spf_root,
                                     node_t *p_node, 
                                     node_t *q_node, 
                                     LEVEL level,
                                     protected_resource_t *pr_res){

    tilfa_info_t *tilfa_info = spf_root->tilfa_info;

    edge_t *edge = GET_EGDE_PTR_FROM_EDGE_END(
            pr_res->protected_link);

    node_t *protected_node = edge->to.node;

    uint32_t dist_pnode_to_qnode = tilfa_dist_from_x_to_y(
            tilfa_info, p_node, q_node, level);
    uint32_t dist_pnode_to_S = tilfa_dist_from_x_to_y(
            tilfa_info, p_node, spf_root, level);
    uint32_t dist_S_to_qnode = tilfa_dist_from_self(
            tilfa_info, q_node, level);

    /*loop free wrt Source*/
    if(!(dist_pnode_to_qnode < dist_pnode_to_S + dist_S_to_qnode))
        return FALSE;

    /*I think downstream criteria is automatically met since the
     * curr_node lies on post-convergence path*/

    uint32_t dist_pnode_to_E = tilfa_dist_from_x_to_y(
            tilfa_info, p_node, protected_node, level);
    uint32_t dist_E_to_qnode = tilfa_dist_from_x_to_y(
            tilfa_info, protected_node, q_node, level);

    if(pr_res->node_protection){
        if(dist_pnode_to_qnode < dist_pnode_to_E + dist_E_to_qnode){
            return TRUE;
        }
    }
    if(pr_res->link_protection){
        if(dist_pnode_to_qnode <
                (dist_pnode_to_S + edge->metric[level] + dist_E_to_qnode)){
            return TRUE;
        }
    }
    return FALSE;
}

/* This fn is used to store Adj sid or RSVP LSP label in inet3/mpls
 * label stack. RSVP LSP label are stored in adj-sid notion in 
 * label stack*/
void
tilfa_insert_adj_sid_in_label_stack(gen_segment_list_t *gen_segment_list,
                  int stack_index,
                  boolean inet3,
                  boolean mpls0,
                  node_t *from_node,
                  node_t *to_node,
                  LEVEL level,
                  mpls_label_t adj_sid,
                  tilfa_seg_type seg_type){

    assert((inet3 && !mpls0) ||
            (!inet3 && mpls0));

    assert(seg_type == TILFA_ADJ_SID || 
        seg_type == RSVP_LSP_LABEL);

    if(inet3){
        gen_segment_list->inet3_mpls_label_out[stack_index].seg_type
            = seg_type;
        gen_segment_list->inet3_mpls_label_out[stack_index].u.adj_sid.from_node = from_node;
        gen_segment_list->inet3_mpls_label_out[stack_index].u.adj_sid.to_node = to_node;
        gen_segment_list->inet3_mpls_label_out[stack_index].u.adj_sid.adj_sid =
            (adj_sid == NO_TAG && from_node && to_node && seg_type == TILFA_ADJ_SID) ?
            get_adj_sid_minimum(from_node, to_node, level):
            adj_sid;
    }
    else if(mpls0){
        gen_segment_list->mpls0_mpls_label_out[stack_index].seg_type
            = seg_type;
        gen_segment_list->mpls0_mpls_label_out[stack_index].u.adj_sid.from_node = from_node;
        gen_segment_list->mpls0_mpls_label_out[stack_index].u.adj_sid.to_node = to_node;
        gen_segment_list->mpls0_mpls_label_out[stack_index].u.adj_sid.adj_sid =
            (adj_sid == NO_TAG && from_node && to_node && seg_type == TILFA_ADJ_SID) ?
            get_adj_sid_minimum(from_node, to_node, level):
            adj_sid;
    }
}

/* A function to compute the segment list connecting non-adjacent P - Q
 * nodes. Will introduce ecmp3 version of the function if i come across
 * more optimized algorithm to connect P-Q nodes.*/
static int
tilfa_compute_segment_list_connecting_p_q_nodes
                    ( node_t *spf_root, 
                      glthread_t *p_node_thread, 
                      glthread_t *q_node_thread, 
                      node_t *dest,
                      LEVEL level, 
                      gen_segment_list_t *gensegment_list,
                      int q_distance, 
                      int pq_distance,
                      protected_resource_t *pr_res){


    int stack_top = 0;
    uint32_t adj_sid = 0;
    boolean is_pq_prefix_sid_connected = FALSE;

    glthread_t *p_node_thread_temp = p_node_thread;
    glthread_t *q_node_thread_temp = q_node_thread;

    node_t *q_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node_thread);
    node_t *p_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(p_node_thread);
    
    while(1){

        is_pq_prefix_sid_connected = tilfa_attempt_connect_p_q_by_prefix_sid(
            spf_root, p_node, q_node, level, pr_res);
        
        if(is_pq_prefix_sid_connected){
            
            /*Push prefix sid of q-node*/
            gensegment_list->inet3_mpls_label_out[stack_top].seg_type = 
                TILFA_PREFIX_SID_REFERENCE;
            gensegment_list->inet3_mpls_label_out[stack_top].u.node = q_node;
            gensegment_list->inet3_stack_op[stack_top] = PUSH;
            gensegment_list->mpls0_mpls_label_out[stack_top].seg_type = 
                TILFA_PREFIX_SID_REFERENCE;
            gensegment_list->mpls0_mpls_label_out[stack_top].u.node = q_node;
            gensegment_list->mpls0_stack_op[stack_top] = PUSH;
            stack_top++;
            p_node = q_node;
            p_node_thread_temp = q_node_thread_temp;
            q_node_thread_temp = q_node_thread;
            q_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node_thread_temp);
            if(p_node == q_node)
                break;
            is_pq_prefix_sid_connected = FALSE;
        }
        else{
            
            q_node_thread_temp = q_node_thread_temp->left;
            q_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node_thread_temp);
            if(q_node == p_node){
                
                /*Push the Adj segment p_node --> p_node_next*/
                tilfa_insert_adj_sid_in_label_stack(gensegment_list, stack_top, 
                    TRUE, FALSE, p_node, 
                    GET_PRED_INFO_NODE_FROM_GLTHREAD(p_node_thread_temp->right), 
                    level, NO_TAG, TILFA_ADJ_SID);
                gensegment_list->inet3_stack_op[stack_top] = PUSH;

                tilfa_insert_adj_sid_in_label_stack(gensegment_list, stack_top, 
                    FALSE, TRUE, p_node, 
                    GET_PRED_INFO_NODE_FROM_GLTHREAD(p_node_thread_temp->right), 
                    level, NO_TAG, TILFA_ADJ_SID);
                gensegment_list->mpls0_stack_op[stack_top] = PUSH;

                stack_top++;
                p_node_thread_temp = p_node_thread_temp->right;
                p_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(p_node_thread_temp);
                q_node_thread_temp = q_node_thread;
                q_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node_thread_temp);
                if(p_node == q_node) break;
            }
        }
    }
    return stack_top;
}

static int
tilfa_compute_segment_list_from_tilfa_raw_results
                    ( node_t *spf_root, 
                      node_t *p_node, 
                      node_t *q_node, 
                      node_t *dest,
                      LEVEL level, 
                      gen_segment_list_t *gensegment_list,
                      int q_distance, 
                      int pq_distance,
                      internal_nh_t **first_hop_segments){

    tilfa_info_t *tilfa_info = spf_root->tilfa_info;

    assert(pq_distance >=0 && q_distance >= 0);

    if(!first_hop_segments[0])
        return 0;
  
    /* Now analyse the relationship between primary nexthops, 
     * p-node, q-node and Destination node and prepare segment list
    */                                                                              
    int i = 0;
    node_t *nxthop_node = NULL;
    int stack_top = 0;
    mpls_label_t adj_sid = 0;

    /*First hop node shall be same for all FHS*/
    nxthop_node = first_hop_segments[0]->node;
    assert(nxthop_node);

        /*In Case 1.x and 2.x are scenarios where P and Q node is a single
         * PQ node. We will vary PQ node to cover all possible positions along
         * post-C path. When PQ node is : 
         * case 1.1 : Direct Nexthop and destination is remote(pure LFA)
         * case 1.2 : Direct Nexthop + penultimate hop node (Triangle case, LFA)
         * case 1.3 : Direct nexthop + Destination(Directly connected Destination Case)
         * case 2.1 : PQ node is strictly between nexthop and penultimate hop of Destination(RLFA case)
         * case 2.2 : PQ node is strictly beyond nexthop and also a penultimate hop of Destination(RLFA case)
         * case 2.3 : PQ node is strictly beyond nexthop and a Destination
         */

        /* Case 1.1 : Direct Nexthop and destination is remote(pure LFA)
         * S ------NH(pq-node)-----A----B---- D
         * */
        if(nxthop_node == p_node && 
                p_node == q_node &&
                q_distance >= 2){

            assert(q_distance >= 2 && pq_distance == 0);

            while(first_hop_segments[i]){
                
                stack_top = 0;
                tilfa_genseglst_fill_first_hop_segment(spf_root, level, 
                        (&gensegment_list[i]), first_hop_segments[i], &stack_top, dest);

                /* inet.3
                 * push prefix sid of Destination only*/
                gensegment_list[i].inet3_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].inet3_mpls_label_out[stack_top].u.node = dest;
                gensegment_list[i].inet3_stack_op[stack_top] = PUSH;
                /* mpls.0
                 * swap prefix sid of Destination only*/
                gensegment_list[i].mpls0_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].mpls0_mpls_label_out[stack_top].u.node = dest;
                gensegment_list[i].mpls0_stack_op[stack_top] = SWAP;
                i++;
            }
        } 
        
        /*Case 1.2 : Direct Nexthop + penultimate hop node (Triangle case, LFA)
         * S ------NH(pq-node)------- D
         * */
        else if(nxthop_node == p_node && 
                p_node == q_node &&
                q_distance == 1){

            assert(q_distance == 1 && pq_distance == 0);

            while(first_hop_segments[i]){

                stack_top = 0;
                tilfa_genseglst_fill_first_hop_segment(spf_root, level,
                        (&gensegment_list[i]), first_hop_segments[i], &stack_top, dest);

                /*inet.3*/
                /*PUSH the prefix sid of Destination*/
                gensegment_list[i].inet3_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].inet3_mpls_label_out[stack_top].u.node = dest;
                gensegment_list[i].inet3_stack_op[stack_top] = PUSH;
                
                /*mpls.0*/
                /*No need to push anything else in mpls0 stack*/
                gensegment_list[i].mpls0_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].mpls0_mpls_label_out[stack_top].u.node = dest;
                gensegment_list[i].mpls0_stack_op[stack_top] = SWAP;
                i++;
            }
        }
        /* Case 1.3 : Direct nexthop + Destination(Directly connected Destination Case)
         * S ------------------- D(pq-node)
         * */
        else if(nxthop_node == p_node && 
                p_node == q_node && 
                q_node == dest){

            assert(pq_distance == 0 && q_distance == 0);

            while(first_hop_segments[i]){

                stack_top = 0;
                tilfa_genseglst_fill_first_hop_segment(spf_root, level,
                        (&gensegment_list[i]), first_hop_segments[i], &stack_top, dest);

                /* inet.3 */
                gensegment_list[i].inet3_stack_op[stack_top] = STACK_OPS_UNKNOWN;

                /* mpls.0 */
                gensegment_list[i].mpls0_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].mpls0_mpls_label_out[stack_top].u.node = NULL;
                gensegment_list[i].mpls0_stack_op[stack_top] = POP;
                i++;
            }
        }
        /* Case 2.1 : PQ node is strictly between nexthop and penultimate hop of Destination(RLFA case)
         * S----NH------A(pq-node)--------B-----DEST
         * */
        else if(nxthop_node != p_node && 
                p_node == q_node &&
                q_distance >= 2){

            assert(q_distance >= 2 && pq_distance == 0);

            while(first_hop_segments[i]){

                stack_top = 0;
                tilfa_genseglst_fill_first_hop_segment(spf_root, level,
                        (&gensegment_list[i]), first_hop_segments[i], &stack_top, dest);

                /*inet.3*/
                /*First push the PQ node and then the destination*/
                gensegment_list[i].inet3_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].inet3_mpls_label_out[stack_top].u.node = p_node;
                gensegment_list[i].inet3_stack_op[stack_top] = PUSH;
                gensegment_list[i].inet3_mpls_label_out[stack_top+1].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].inet3_mpls_label_out[stack_top+1].u.node = dest;
                gensegment_list[i].inet3_stack_op[stack_top+1] = PUSH;

                /*mpls.0*/
                /*Push the PQ node only*/
                gensegment_list[i].mpls0_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].mpls0_mpls_label_out[stack_top].u.node = p_node;
                gensegment_list[i].mpls0_stack_op[stack_top] = PUSH;
                i++;
            }
        }

        /* Case 2.2 : PQ node is strictly beyond nexthop and also a penultimate hop of Destination(RLFA case)
         * S----NH------A(pq-node)----------DEST
         * */
        else if(nxthop_node != p_node && 
                p_node == q_node &&
                q_distance == 1){

            assert(q_distance == 1 && pq_distance == 0);

            while(first_hop_segments[i]){

                stack_top = 0;
                tilfa_genseglst_fill_first_hop_segment(spf_root, level,
                        (&gensegment_list[i]), first_hop_segments[i], &stack_top, dest);


                /*inet.3*/
                /*Push the PQ node only*/
                gensegment_list[i].inet3_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].inet3_mpls_label_out[stack_top].u.node = p_node;
                gensegment_list[i].inet3_stack_op[stack_top] = PUSH;

                /*mpls.0*/
                /*Swap the PQ node only*/
                gensegment_list[i].mpls0_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].mpls0_mpls_label_out[stack_top].u.node = p_node;
                gensegment_list[i].mpls0_stack_op[stack_top] = SWAP;
                i++;
            }
        }
        
        /* Case 2.3 : PQ node is strictly beyond nexthop and a Destination
         * S----NH------A----------DEST(pq-node)
         * */
        else if(nxthop_node != p_node && 
                p_node == q_node &&
                q_node == dest){

            assert(q_distance == 0 && pq_distance == 0);

            while(first_hop_segments[i]){

                stack_top = 0;
                tilfa_genseglst_fill_first_hop_segment(spf_root, level,
                        (&gensegment_list[i]), first_hop_segments[i], &stack_top, dest);

                /*inet.3*/
                /*Push the PQ node only*/
                gensegment_list[i].inet3_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].inet3_mpls_label_out[stack_top].u.node = p_node;
                gensegment_list[i].inet3_stack_op[stack_top] = PUSH;

                /*mpls.0*/
                /*Swap the PQ node only*/
                gensegment_list[i].mpls0_mpls_label_out[stack_top].seg_type = TILFA_PREFIX_SID_REFERENCE;
                gensegment_list[i].mpls0_mpls_label_out[stack_top].u.node = p_node;
                gensegment_list[i].mpls0_stack_op[stack_top] = SWAP;
                i++;
            }
        }

        /*Leveraging ECMP case*/
        else if(pq_distance > 0){

            assert(p_node != q_node);
            gen_segment_list_t gen_segment_list_temp;
            
            memcpy(&gen_segment_list_temp, &gensegment_list[0], 
                sizeof(gen_segment_list_t));

            while(first_hop_segments[i]){

                stack_top = 0;
                tilfa_genseglst_fill_first_hop_segment(spf_root, level,
                        (&gensegment_list[i]), first_hop_segments[i], &stack_top, dest);

                    if(nxthop_node != p_node){
                        /*push the prefix sid of p-node in inet.3 and mpls0*/
                        gensegment_list[i].inet3_mpls_label_out[stack_top].seg_type = 
                            TILFA_PREFIX_SID_REFERENCE;
                        gensegment_list[i].inet3_mpls_label_out[stack_top].u.node = p_node;
                        gensegment_list[i].inet3_stack_op[stack_top] = PUSH;
                        
                        gensegment_list[i].mpls0_mpls_label_out[stack_top].seg_type = 
                            TILFA_PREFIX_SID_REFERENCE;
                        gensegment_list[i].mpls0_mpls_label_out[stack_top].u.node = p_node;
                        gensegment_list[i].mpls0_stack_op[stack_top] = PUSH;
                        stack_top++;
                    }
                    /* inet.3 
                     * step 1 : Push the entire gen_segment_list_temp
                     * step 2 : if(q-node is not dest) push prefix sid of dest
                     * step 3:  if(q-node is dest), no more action required*/
                    int next_stack_index = tilfa_copy_gensegment_list_stacks(
                        &gen_segment_list_temp, &gensegment_list[i], TRUE, FALSE, 0,
                        stack_top);

                    if(q_node != dest){

                        gensegment_list[i].inet3_mpls_label_out[next_stack_index].seg_type = 
                            TILFA_PREFIX_SID_REFERENCE;
                        gensegment_list[i].inet3_mpls_label_out[next_stack_index].u.node = dest;
                        gensegment_list[i].inet3_stack_op[next_stack_index] = PUSH;
                    }
                    else if(q_node == dest){
                        /*No Action required*/
                    }

                    /* mpls.0
                     * if q-node is Dest
                     *  Push the entire gen_segment_list_temp
                     *  if last label in gen_segment_list_temp is prefix sid of dest, then do not push the last label in stack
                     *  else if last label in gen_segment_list_temp is adj-sid, then change stack op to SWAP with adj-sid
                     * else
                     *  Push the entire gen_segment_list_temp
                     * */
                    next_stack_index = tilfa_copy_gensegment_list_stacks(
                        &gen_segment_list_temp, &gensegment_list[i], FALSE, TRUE, 0,
                        stack_top);

                    if(q_node != dest){
                        /*No more Action required*/  
                    }
                    else if(q_node == dest){
                        if(gensegment_list[i].mpls0_mpls_label_out[next_stack_index - 1].seg_type == 
                            TILFA_PREFIX_SID_REFERENCE &&
                            gensegment_list[i].mpls0_mpls_label_out[next_stack_index - 1].u.node == dest){
                            gensegment_list[i].mpls0_mpls_label_out[next_stack_index - 1].u.node = NULL;
                            gensegment_list[i].mpls0_stack_op[next_stack_index - 1] = STACK_OPS_UNKNOWN;
                        }
                        else if(gensegment_list[i].mpls0_mpls_label_out[next_stack_index - 1].seg_type ==
                                 TILFA_ADJ_SID){
                                gensegment_list[i].mpls0_stack_op[next_stack_index - 1] = SWAP;
                            }
                    }
                i++;
            }
        }
    return i;
}

static boolean
tilfa_is_fhs_overlap(
        tilfa_segment_list_t *tilfa_segment_list,
        int count,
        gen_segment_list_t *gen_segment_list){

    int i;

    return FALSE;
    for(i = 0; i < count; i++){

        if(gen_segment_list->oif == 
                tilfa_segment_list->gen_segment_list[i].oif)
            return TRUE;
    }
    return FALSE;;
}

static void
tilfa_merge_tilfa_segment_lists_by_destination(
        tilfa_segment_list_t *dst, 
        tilfa_segment_list_t *src){

    int i = 0,
        j = 0;

    tilfa_segment_list_t temp;
    memset(&temp, 0, sizeof(tilfa_segment_list_t));

    tilfa_segment_list_t *array[] = {dst, src};
    
    int k = 0; 
    /*copy all RSVP LSP FHS to temp*/
    for( ; k < 2; k++){
        for( i = 0; i < array[k]->n_segment_list; i++){

            if(!array[k]->gen_segment_list[i].is_fhs_rsvp_lsp)
                continue;
            
            if( k == 1){
                if(tilfa_is_fhs_overlap(&temp, j, 
                        &array[k]->gen_segment_list[i]))
                    continue;
            }
            memcpy(&temp.gen_segment_list[j], &array[k]->gen_segment_list[i], 
                    sizeof(gen_segment_list_t));
            j++;

            if(j == MAX_NXT_HOPS){
                memcpy(array[k]->gen_segment_list, temp.gen_segment_list,
                        sizeof(temp.gen_segment_list));
                array[k]->n_segment_list = MAX_NXT_HOPS;
                return;
            }
        }
    }

    /*Now copy IPNH from src and dst*/
    for( k = 0; k < 2; k++){
        for( i = 0; i < array[k]->n_segment_list; i++){

            if(array[k]->gen_segment_list[i].is_fhs_rsvp_lsp)
                continue;

            if(tilfa_is_fhs_overlap(&temp, j, 
                        &array[k]->gen_segment_list[i]))
                continue;

            memcpy(&temp.gen_segment_list[j], &array[k]->gen_segment_list[i], 
                    sizeof(gen_segment_list_t));
            j++;

            if(j == MAX_NXT_HOPS){
                memcpy(array[k]->gen_segment_list, temp.gen_segment_list,
                        sizeof(temp.gen_segment_list));
                array[k]->n_segment_list = MAX_NXT_HOPS;
                return;
            }
        }
    }
    memcpy(dst->gen_segment_list, temp.gen_segment_list,
        sizeof(temp.gen_segment_list));
    dst->n_segment_list = j;
}

static void
tilfa_record_segment_list(node_t *spf_root, 
                          LEVEL level, 
                          node_t *dst_node, 
                          tilfa_segment_list_t *tilfa_segment_list){

    glthread_t *curr;
    tilfa_segment_list->dest = dst_node;
    tilfa_segment_list_t *tilfa_segment_list_ptr = NULL;
    
    init_glthread(&tilfa_segment_list->gen_segment_list_glue);

    ITERATE_GLTHREAD_BEGIN(&spf_root->tilfa_info->tilfa_segment_list_head[level], curr){

        tilfa_segment_list_ptr = tilfa_segment_list_to_gensegment_list(curr);
        
        if(tilfa_segment_list_ptr->dest != dst_node) continue;

        tilfa_merge_tilfa_segment_lists_by_destination(
                tilfa_segment_list_ptr, 
                tilfa_segment_list);
        free(tilfa_segment_list);
        return;
    } ITERATE_GLTHREAD_END(&spf_root->tilfa_info->tilfa_segment_list_head[level], curr);

    glthread_add_next(&spf_root->tilfa_info->tilfa_segment_list_head[level], 
        &tilfa_segment_list->gen_segment_list_glue);
}

static void
tilfa_examine_tilfa_path_for_segment_list(
        glthread_t *path, void *arg){

    glthread_t *curr;
    
    fn_ptr_arg_t *fn_ptr_arg = (fn_ptr_arg_t *)arg;
    tilfa_info_t *tilfa_info = fn_ptr_arg->tilfa_info;
    protected_resource_t *pr_res = fn_ptr_arg->pr_res;
    LEVEL level = fn_ptr_arg->level;

    glthread_t *spf_root_entry = path->right;
    node_t *spf_root = 
        GET_PRED_INFO_NODE_FROM_GLTHREAD(spf_root_entry);

    glthread_t *first_hop_node_entry = path->right->right;

    node_t *first_hop_node = 
        GET_PRED_INFO_NODE_FROM_GLTHREAD(first_hop_node_entry);

    glthread_t *last_entry;
    ITERATE_GLTHREAD_BEGIN(path, curr){
        last_entry = curr;
    } ITERATE_GLTHREAD_END(path, curr);

    /*Traverse the path from Destination to first hop node*/
    
    node_t *curr_node = NULL;
    boolean search_for_q_node = TRUE;
    boolean search_for_p_node = FALSE;
    
    internal_nh_t *first_hop_segments[MAX_NXT_HOPS];
    
    glthread_t *dest_entry = last_entry;
    
    node_t *dst_node = 
        GET_PRED_INFO_NODE_FROM_GLTHREAD(dest_entry);

    glthread_t *p_node = NULL;
    glthread_t *q_node = dest_entry;

    /*Distance of Q-node from Destination along post-C path*/
    uint32_t q_distance = 0xFFFFFFFF;
    /*Distance of P-node from Q-node along post-C path*/
    int pq_distance = -1;

    curr_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(last_entry);
    
    while(last_entry != first_hop_node_entry){
        
        if(search_for_q_node == TRUE &&
            search_for_p_node == TRUE){
            assert(0);
        }

        if(search_for_q_node == FALSE && 
            search_for_p_node == FALSE){
            break;
        }

        if(last_entry == dest_entry){
            
            /* Examinining the Destination*/
            /* Q-node Test : Pass, recede.... */
            q_node = last_entry;
            q_distance = 0;
            goto done;
        }
        else{
            /*if we are searching Q-node*/
            if(search_for_q_node == TRUE){

                if(tilfa_q_node_qualification_test_wrt_destination(
                    spf_root, curr_node, dst_node, pr_res, level)){
                    /*Q-node Test : Pass, recede.... */
                    q_node = last_entry;
                    q_distance++;
                    goto done;
                }
                else{
                    /*q_node is our final Q-node now*/
                    search_for_q_node = FALSE;
                    search_for_p_node = TRUE;

                    /*Rewind by 1 along the post-c path*/
                    curr_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node);
                    last_entry = last_entry->right;
                    assert(curr_node == 
                        GET_PRED_INFO_NODE_FROM_GLTHREAD(last_entry));
                    /* check if curr_node is also a p-node,
                     * simply continue with same curr node*/
                    continue;
                }
            }
            else if(search_for_p_node == TRUE){
                /*Test if curr node is a p-node*/
                if(tilfa_p_node_qualification_test_wrt_root(
                            spf_root, curr_node, first_hop_node, 
                            dst_node, pr_res, level, 
                            first_hop_segments)){

                    p_node = last_entry;
                    search_for_p_node = FALSE;
                    if(pq_distance == -1)
                        pq_distance = 0;
                }
                else{
                    /*P-node Test failed, recede....*/
                    if(pq_distance == -1)
                        pq_distance = 1;
                    else
                        pq_distance++;
                    goto done;
                }
            }
            else{
                assert(0);
            }
        }
        done:
            last_entry = last_entry->left;
            curr_node = GET_PRED_INFO_NODE_FROM_GLTHREAD(last_entry);
    } /* while ends*/
    
    /* We are here, because the node under inspection is
     * a direct nbr of root. Check what is our status..*/
    
    if( search_for_q_node == FALSE &&
        search_for_p_node == FALSE){

        goto TILFA_FOUND; 
    }

    else if(search_for_q_node == FALSE &&
        search_for_p_node == TRUE){

        assert(!p_node);

        /*All direct nbrs are p-nodes*/
        if(tilfa_p_node_qualification_test_wrt_root(
                  spf_root, curr_node, first_hop_node, 
                  dst_node, pr_res, level,
                  first_hop_segments)){
            
            p_node = last_entry;
            search_for_p_node = FALSE;
        }
        else{
            /* The direct nbr is not a p-node because Dest
             * pre-c primary nexthop overlap with p-node's
             * post-c primary nexthops*/
        }
    }
    else if(search_for_q_node == TRUE &&
            search_for_p_node == FALSE){

        /*Check if first-hop node is a Q-node ?*/
        if(tilfa_q_node_qualification_test_wrt_destination(
            spf_root, curr_node, dst_node, pr_res, level)){
            q_node = last_entry;
            q_distance++;
            search_for_q_node = FALSE;
            search_for_p_node = TRUE;
            if(tilfa_p_node_qualification_test_wrt_root(
                    spf_root, curr_node, first_hop_node, 
                    dst_node, pr_res, level,
                    first_hop_segments)){

                p_node = last_entry;
                search_for_p_node = FALSE;
                pq_distance = 0;
            }
            else{
                /* The direct nbr is not a p-node because Dest
                 * pre-c primary nexthop overlap with p-node's
                 * post-c primary nexthops*/
            }
        }
        else{
            /*now test the current node for p-node*/
            search_for_q_node = FALSE;
            search_for_p_node = TRUE;
            if(tilfa_p_node_qualification_test_wrt_root(
                        spf_root, curr_node, first_hop_node, 
                        dst_node, pr_res, level,
                        first_hop_segments)){
                
                p_node = last_entry;
                search_for_p_node = FALSE;
                pq_distance = 1;
                goto TILFA_FOUND;
            }
            else{
                /* The direct nbr is not a p-node because Dest
                 * pre-c primary nexthop overlap with p-node's
                 * post-c primary nexthops*/
                pq_distance = -1;
            }
        }
    }
    else if(search_for_q_node == TRUE &&
            search_for_p_node == TRUE){
        
        /* We shall be looking either for q-node or 
         * p-node, but not both at any given time*/
        assert(0);    
    }

    if(search_for_p_node == TRUE && 
        p_node == NULL){
       
       /*Tilfa not found as P-node do not exist*/
       return;
    }

    TILFA_FOUND:
    print_raw_tilfa_path(spf_root, first_hop_segments, 
        p_node ? GET_PRED_INFO_NODE_FROM_GLTHREAD(p_node) : 0,
        q_node ? GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node): 0, 
        dst_node, q_distance, pq_distance);
    
    tilfa_segment_list_t *tilfa_segment_list = 
        calloc(1, sizeof(tilfa_segment_list_t));

    if(pq_distance == 0){
        tilfa_segment_list->n_segment_list = 
            tilfa_compute_segment_list_from_tilfa_raw_results(
                    spf_root, GET_PRED_INFO_NODE_FROM_GLTHREAD(p_node),
                    GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node),
                    dst_node, level,
                    tilfa_segment_list->gen_segment_list,
                    q_distance, pq_distance,
                    first_hop_segments);
    }
    else{
        int segment_list_len_from_p_to_q =
            tilfa_compute_segment_list_connecting_p_q_nodes(
                    spf_root, 
                    p_node,
                    q_node,
                    dst_node, level,
                    tilfa_segment_list->gen_segment_list,
                    q_distance, pq_distance,
                    pr_res);

        if(!segment_list_len_from_p_to_q){
            free(tilfa_segment_list);
            return;
        }

        /* Now analyze the segment list against first hop segments
         * and populate segment lists for inet3 and mpls0*/

        tilfa_segment_list->n_segment_list = 
            tilfa_compute_segment_list_from_tilfa_raw_results(
                    spf_root, GET_PRED_INFO_NODE_FROM_GLTHREAD(p_node),
                    GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node),
                    dst_node, level,
                    /*In this case, the segment list at index 0 is input
                     * which will be copied to remaining other indexes*/
                    tilfa_segment_list->gen_segment_list,
                    q_distance, pq_distance,
                    first_hop_segments);
    }

    if(tilfa_segment_list->n_segment_list == 0){
        free(tilfa_segment_list);
        return;
    }

    tilfa_segment_list->pr_res = pr_res;
    tilfa_lock_protected_resource(pr_res);

    tilfa_record_segment_list(spf_root, level, dst_node, 
                              tilfa_segment_list);
}

static void
tilfa_compute_segment_lists_per_destination(
                        node_t *spf_root, 
                        LEVEL level,
                        protected_resource_t *pr_res,
                        node_t *dst_node){

    fn_ptr_arg_t fn_ptr_arg;
    fn_ptr_arg.tilfa_info = spf_root->tilfa_info;
    fn_ptr_arg.pr_res = pr_res;
    fn_ptr_arg.level = level;

    sprintf(instance->traceopts->b, "Node : %s : %s : "
            "Examining post-C to Dest %s", spf_root->node_name,
            get_str_level(level), dst_node->node_name);
    trace(instance->traceopts, TILFA_BIT);
    trace_spf_path_to_destination_node(spf_root, 
                    dst_node, level, 
                    tilfa_examine_tilfa_path_for_segment_list,
                    (void *)&fn_ptr_arg, TRUE);     
}



/*Main TILFA algorithm is implemented in this function*/
static void
tilfa_compute_segment_lists(node_t *spf_root, LEVEL level, 
                            protected_resource_t *pr_res){

    spf_result_t *result = NULL;
    singly_ll_node_t *curr = NULL;
    
    ll_t *pre_convergence_spf_result_lst = 
        tilfa_get_pre_convergence_spf_result_list(
            spf_root->tilfa_info, level);

    node_t *dst_node = NULL;

    sprintf(instance->traceopts->b, "Node : %s : level %s, Segment List "
            "Computation for Protected Res : %s, LP : %sset : NP : %sset",
        spf_root->node_name, get_str_level(level), 
        pr_res->protected_link->intf_name,
        pr_res->link_protection ? "" : "un", 
        pr_res->node_protection ? "" : "un");
    trace(instance->traceopts, SPF_EVENTS_BIT);

    ITERATE_LIST_BEGIN(pre_convergence_spf_result_lst, curr){

        result = curr->data;
        dst_node = result->node;

        if(dst_node->node_type[level] == PSEUDONODE)
            continue;

        if(!tilfa_is_destination_impacted(spf_root->tilfa_info,
            dst_node, level, pr_res)){
            sprintf(instance->traceopts->b, "Node : %s : level %s, Dest Node %s is not impacted",
                spf_root->node_name, get_str_level(level), dst_node->node_name);
            trace(instance->traceopts, TILFA_BIT);
            continue;
        }

        tilfa_compute_segment_lists_per_destination
            (spf_root, level, pr_res, dst_node);

    } ITERATE_LIST_END;
}

void
tilfa_run_post_convergence_spf(node_t *spf_root, LEVEL level, 
                               protected_resource_t *pr_res){

    /* Clear all intermediate or final results which are computed per-
     * protected resource*/
    tilfa_clear_all_post_convergence_results(spf_root, level, pr_res);

    /*Ostracize the protected resources from the topology*/
    tilfa_topology_prune_protected_resource(spf_root, pr_res);

    /* We also need primary nexthops of first-hop nodes along the 
     * Post-Convergence SPF path. However, Running full SPF is not 
     * required, but this is already available code we have.*/
    compute_tilfa_post_convergence_spf_primary_nexthops(spf_root, level);

    /*Now compute PC-spf paths to all destinations*/
    compute_spf_paths(spf_root, level, TILFA_RUN);

    /*Add the protected resources back to the topology*/
    tilfa_topology_unprune_protected_resource(spf_root, pr_res);

    /*Compute segment lists now*/
    tilfa_compute_segment_lists(spf_root, level, pr_res);
}

char *
tilfa_print_one_liner_segment_list(
            gen_segment_list_t *gen_segment_list,
            boolean inet3, boolean mpls0){

    static char buffer[1024];

    assert(inet3 | mpls0);

    memset(buffer, 0, 1024);

    int i = 0;
    int rc = 0;

    if(inet3){

        rc = snprintf(buffer, 1024, "\t\t\tinet3 %s, %s\t",
                gen_segment_list->oif->intf_name,
                gen_segment_list->gw_ip);
            
        for( i = 0; i < MPLS_STACK_OP_LIMIT_MAX; i++){

            if(gen_segment_list->inet3_stack_op[i] == STACK_OPS_UNKNOWN)
                break;

            switch(gen_segment_list->inet3_mpls_label_out[i].seg_type){
                case TILFA_PREFIX_SID_REFERENCE:
                    if(gen_segment_list->inet3_stack_op[i] != POP){
                        rc += snprintf(buffer + rc, 1024, "<Node-Sid %s>(%s)  ",
                            gen_segment_list->inet3_mpls_label_out[i].u.node->node_name,
                            get_str_stackops(gen_segment_list->inet3_stack_op[i]));
                    }
                    else{
                        rc += snprintf(buffer + rc, 1024, "(%s)  ",
                            get_str_stackops(gen_segment_list->inet3_stack_op[i]));
                    }
                break;
                case TILFA_ADJ_SID:
                    if(TILFA_SEGLIST_IS_INET3_ADJ_SID_SET(gen_segment_list, i)){
                    rc += snprintf(buffer + rc, 1024, "%u(%s)  ",
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.adj_sid,
                            get_str_stackops(gen_segment_list->inet3_stack_op[i]));
                    }
                    else{
                    rc += snprintf(buffer + rc, 1024, "[%s-->%s](%s)  ",
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.from_node->node_name,
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.to_node->node_name,
                            get_str_stackops(gen_segment_list->inet3_stack_op[i]));
                    }
                break;
                case RSVP_LSP_LABEL:
                    if(TILFA_SEGLIST_IS_INET3_ADJ_SID_SET(gen_segment_list, i)){
                    rc += snprintf(buffer + rc, 1024, "%u(%s)  ",
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.adj_sid,
                            get_str_stackops(gen_segment_list->inet3_stack_op[i]));
                    }
                    else{
                    assert(0); /*Label must be allocated for RSVP LSP*/
                    rc += snprintf(buffer + rc, 1024, "[%s-->>%s](%s)  ",
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.from_node->node_name,
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.to_node->node_name,
                            get_str_stackops(gen_segment_list->inet3_stack_op[i]));
                    }
                break;
                default:
                    ;
            }
        }
    }

    if(mpls0){

        if(inet3){
            rc += snprintf(buffer + rc, 1024, "\n");
        }

        rc += snprintf(buffer + rc, 1024, "\t\t\tmpls0 %s, %s\t",
                gen_segment_list->oif->intf_name,
                gen_segment_list->gw_ip);

        for( i = 0; i < MPLS_STACK_OP_LIMIT_MAX; i++){
            if(gen_segment_list->mpls0_stack_op[i] == STACK_OPS_UNKNOWN)
                break;

            switch(gen_segment_list->mpls0_mpls_label_out[i].seg_type){
                case TILFA_PREFIX_SID_REFERENCE:
                if(gen_segment_list->mpls0_stack_op[i] != POP){
                    rc += snprintf(buffer + rc, 1024, "<Node-Sid %s>(%s)  ",
                            gen_segment_list->mpls0_mpls_label_out[i].u.node->node_name,
                            get_str_stackops(gen_segment_list->mpls0_stack_op[i]));
                }
                else{
                    rc += snprintf(buffer + rc, 1024, "(%s)  ",
                            get_str_stackops(gen_segment_list->mpls0_stack_op[i]));

                }
                break;
                case TILFA_ADJ_SID:
                    if(TILFA_SEGLIST_IS_MPLS0_ADJ_SID_SET(gen_segment_list, i)){
                    rc += snprintf(buffer + rc, 1024, "%u(%s)  ",
                            gen_segment_list->mpls0_mpls_label_out[i].u.adj_sid.adj_sid,
                            get_str_stackops(gen_segment_list->mpls0_stack_op[i]));
                    }
                    else{
                    rc += snprintf(buffer + rc, 1024, "[%s-->%s](%s)  ",
                            gen_segment_list->mpls0_mpls_label_out[i].u.adj_sid.from_node->node_name,
                            gen_segment_list->mpls0_mpls_label_out[i].u.adj_sid.to_node->node_name,
                            get_str_stackops(gen_segment_list->mpls0_stack_op[i]));
                    }
                break;
                case RSVP_LSP_LABEL:
                    if(TILFA_SEGLIST_IS_INET3_ADJ_SID_SET(gen_segment_list, i)){
                    rc += snprintf(buffer + rc, 1024, "%u(%s)  ",
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.adj_sid,
                            get_str_stackops(gen_segment_list->inet3_stack_op[i]));
                    }
                    else{
                    assert(0); /*Label must be allocated for RSVP LSP*/
                    rc += snprintf(buffer + rc, 1024, "[%s-->>%s](%s)  ",
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.from_node->node_name,
                            gen_segment_list->inet3_mpls_label_out[i].u.adj_sid.to_node->node_name,
                            get_str_stackops(gen_segment_list->inet3_stack_op[i]));
                    }
                default:
                    ;
            }
        }
    }
    return buffer;
}

int
tilfa_copy_gensegment_list_stacks(
        gen_segment_list_t *src,
        gen_segment_list_t *dst,
        boolean inet3,
        boolean mpls0,
        int src_start_stack_index,
        int dst_start_stack_index){

    /*copy only one at a time*/
    assert((inet3 && !mpls0) ||
            (!inet3 && mpls0));
    
    if(inet3){

        for(; src_start_stack_index < MPLS_STACK_OP_LIMIT_MAX; 
            src_start_stack_index++, dst_start_stack_index++){

            if(src->inet3_stack_op[src_start_stack_index] == STACK_OPS_UNKNOWN)
                return dst_start_stack_index;

            dst->inet3_mpls_label_out[dst_start_stack_index].seg_type =
                src->inet3_mpls_label_out[src_start_stack_index].seg_type;
            dst->inet3_mpls_label_out[dst_start_stack_index].u.node =
                src->inet3_mpls_label_out[src_start_stack_index].u.node;
            dst->inet3_stack_op[dst_start_stack_index] =
                src->inet3_stack_op[src_start_stack_index];
        }
    }
    else if(mpls0){

        for(;src_start_stack_index < MPLS_STACK_OP_LIMIT_MAX; 
            src_start_stack_index++, dst_start_stack_index++){

            if(src->mpls0_stack_op[src_start_stack_index] == STACK_OPS_UNKNOWN)
                return dst_start_stack_index;

            dst->mpls0_mpls_label_out[dst_start_stack_index].seg_type =
                src->mpls0_mpls_label_out[src_start_stack_index].seg_type;
            dst->mpls0_mpls_label_out[dst_start_stack_index].u.node =
                src->mpls0_mpls_label_out[src_start_stack_index].u.node;
            dst->mpls0_stack_op[dst_start_stack_index] =
                src->mpls0_stack_op[src_start_stack_index];
        }
    }
    return dst_start_stack_index;
}

/* We execute MPLs label stack of nexthop from top to
 * bottom on the packet (i.e. from higher index to index = 0).*/
boolean
tilfa_fill_nxthop_from_segment_lst(routes_t *route,
                                   internal_nh_t *nxthop, 
                                   gen_segment_list_t *gensegment_lst,
                                   protected_resource_t *pr_res,
                                   boolean inet3,
                                   boolean mpls0){

    assert((inet3 && !mpls0) || 
            (!inet3 && mpls0));
     
    nxthop->level = route->level;
    nxthop->oif = gensegment_lst->oif;
    nxthop->protected_link = pr_res->protected_link;
    nxthop->node = gensegment_lst->nxthop;
    strncpy(nxthop->gw_prefix, gensegment_lst->gw_ip, PREFIX_LEN);
    nxthop->nh_type = LSPNH;
    nxthop->lfa_type = TILFA;
    nxthop->proxy_nbr = gensegment_lst->nxthop;
    nxthop->rlfa = 0;      /*Not Valid for TILFA*/

    int i = 0;
    node_t *srgb_node = gensegment_lst->nxthop;

    if(inet3){
        for(i = 0; i < MPLS_STACK_OP_LIMIT_MAX; i++){
            if(gensegment_lst->inet3_stack_op[i] == STACK_OPS_UNKNOWN)
                continue;
            if(TILFA_SEGLIST_IS_INET3_ADJ_SID_SET(gensegment_lst, i)){
                nxthop->mpls_label_out[i] = TILFA_GET_INET3_ADJ_SID(gensegment_lst, i);
                srgb_node = tilfa_get_adj_sid_to_node(gensegment_lst, i, TRUE, FALSE);
            }
            else if(gensegment_lst->inet3_mpls_label_out[i].seg_type == TILFA_PREFIX_SID_REFERENCE){
                
                if(!is_node_spring_enabled(gensegment_lst->inet3_mpls_label_out[i].u.node, route->level))
                    return FALSE;

                nxthop->mpls_label_out[i] = PREFIX_SID_LABEL(
                    srgb_node->srgb,
                    (prefix_t *)ROUTE_GET_BEST_PREFIX(route));
                srgb_node = gensegment_lst->inet3_mpls_label_out[i].u.node;
            }
            if(nxthop->mpls_label_out[i] == NO_TAG)
                return FALSE;
            nxthop->stack_op[i] = gensegment_lst->inet3_stack_op[i];
        }
    }
    else if(mpls0){
        for(i = 0; i < MPLS_STACK_OP_LIMIT_MAX; i++){
            if(gensegment_lst->mpls0_stack_op[i] == STACK_OPS_UNKNOWN)
                continue;
             if(TILFA_SEGLIST_IS_MPLS0_ADJ_SID_SET(gensegment_lst, i)){
                nxthop->mpls_label_out[i] = TILFA_GET_MPLS0_ADJ_SID(gensegment_lst, i);
                 srgb_node = tilfa_get_adj_sid_to_node(gensegment_lst, i, FALSE, TRUE);
             }
             else if(gensegment_lst->mpls0_mpls_label_out[i].seg_type == TILFA_PREFIX_SID_REFERENCE){
                
                if(!is_node_spring_enabled(gensegment_lst->mpls0_mpls_label_out[i].u.node, route->level))
                    return FALSE;

                nxthop->mpls_label_out[i] = PREFIX_SID_LABEL(
                    srgb_node->srgb,
                    (prefix_t *)ROUTE_GET_BEST_PREFIX(route));
                    srgb_node = gensegment_lst->mpls0_mpls_label_out[i].u.node;
             }
             if(nxthop->mpls_label_out[i] == NO_TAG)
                 return FALSE;
            nxthop->stack_op[i] = gensegment_lst->mpls0_stack_op[i];
        }
    }
    nxthop->root_metric = 0;
    nxthop->dest_metric = 0;
    nxthop->ref_count = NULL;
    nxthop->is_eligible = TRUE;
    return TRUE;
}

void
route_fetch_tilfa_backups(node_t *spf_root, 
                          routes_t *route, 
                          boolean inet3, 
                          boolean mpls0){

    assert((inet3 && !mpls0) || 
            (!inet3 && mpls0));
     
    assert(route->rt_type == SPRING_T);

    int i = 0;
    node_t *ecmp_dst = NULL;
    prefix_t *prefix = NULL;
    singly_ll_node_t *list_node = NULL;
    tilfa_segment_list_t *tilfa_segment_list = NULL;

    tilfa_info_t *tilfa_info = spf_root->tilfa_info;

    glthread_t *curr;
    glthread_t *tilfa_segment_list_head =
        &tilfa_info->tilfa_segment_list_head[route->level];

    internal_nh_t tilfa_bck_up_lcl;
    internal_nh_t *tilfa_bck_up = NULL;

    prefix_pref_data_t prefix_pref;
    prefix_pref_data_t route_pref = route_preference(route->flags, route->level);
    
    ITERATE_LIST_BEGIN(route->like_prefix_list, list_node){
    
        prefix = list_node->data;
        prefix_pref = route_preference(prefix->prefix_flags, route->level);
        
        if(route_pref.pref != prefix_pref.pref)
            break;

        ecmp_dst = prefix->hosting_node;

        ITERATE_GLTHREAD_BEGIN(tilfa_segment_list_head, curr){

            tilfa_segment_list = tilfa_segment_list_to_gensegment_list(curr);
            
            if(tilfa_segment_list->dest != ecmp_dst)
                continue;
            
            for( i = 0; i < tilfa_segment_list->n_segment_list; i++){
                
                if(tilfa_fill_nxthop_from_segment_lst(route, &tilfa_bck_up_lcl, 
                                  &tilfa_segment_list->gen_segment_list[i],
                                  tilfa_segment_list->pr_res, inet3, mpls0)){
                    
                    tilfa_bck_up = calloc(1, sizeof(internal_nh_t));
                    copy_internal_nh_t(tilfa_bck_up_lcl, (*tilfa_bck_up));
                    ROUTE_ADD_NH(route->backup_nh_list[LSPNH], tilfa_bck_up);
                }
            }
        } ITERATE_GLTHREAD_END(tilfa_segment_list, curr);
    }ITERATE_LIST_END;
}

