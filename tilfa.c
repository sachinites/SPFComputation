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

static internal_nh_t **
tilfa_lookup_post_convergence_active_primary_nexthops_of_pnodes
    (tilfa_info_t *tilfa_info, node_t *node, LEVEL level,
    spf_result_t **spf_result /*O/P*/){


     assert(!(*spf_result));

     ll_t *lst = tilfa_get_post_convergence_spf_result_list(
                    tilfa_info, level);

    spf_result_t *res = singly_ll_search_by_key(lst, (void *)node);
    if(!res) return NULL;

    *spf_result = res;

    internal_nh_t **active_primary_nxthops = 
        res->tilfa_post_c_active_nxthops_for_pnodes[level];
    
    if(!active_primary_nxthops[0])
        return NULL;

    return active_primary_nxthops;
}

static internal_nh_t *
tilfa_lookup_post_convergence_primary_nexthops
            (tilfa_info_t *tilfa_info, node_t *node, LEVEL level){

    ll_t *lst = tilfa_get_post_convergence_spf_result_list(
                tilfa_info, level);    

    spf_result_t *res = singly_ll_search_by_key(lst, (void *)node);
    if(!res) return NULL;

    return res->next_hop[IPNH];
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
                             boolean flush_old_lst){

    singly_ll_node_t *curr = NULL, *prev = NULL;
    singly_ll_node_t *curr1 = NULL, *prev1 = NULL;

    spf_result_t *spf_res = NULL;
    tilfa_remote_spf_result_t *tilfa_rem_spf_result = NULL;

    ll_t *inner_lst = NULL;
    
    ll_t *outer_lst = tilfa_info->remote_spf_results[level]; 
    
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

    return tilfa_rem_spf_result->rem_spf_result_lst;
}


static uint32_t
tilfa_dist_from_x_to_y(tilfa_info_t *tilfa_info,
                node_t *x, node_t *y, LEVEL level){

    /*Get spf result of remote node X*/
    ll_t *x_spf_result_lst = 
        tilfa_get_remote_spf_result_lst(tilfa_info, x, level, FALSE);

    if(is_singly_ll_empty(x_spf_result_lst)){
        spf_computation(x, &x->spf_info, level, 
            TILFA_RUN, x_spf_result_lst);
    }

    spf_result_t *y_res = singly_ll_search_by_key(
                x_spf_result_lst, (void *)y); 

    if(!y_res) return INFINITE_METRIC;

    return (uint32_t)y_res->spf_metric;
}

segment_list_t *
tilfa_get_segment_list(node_t *node,
                       protected_resource_t *pr_res,
                       LEVEL level,
                       uint8_t *n_segment_list/*O/P*/){

    glthread_t *curr;

    if(!pr_res) assert(0);

    tilfa_info_t *tilfa_info = node->tilfa_info;
    if(!tilfa_info) return NULL;

    if(level != LEVEL1 && level != LEVEL2){
        assert(0);
    }
    
    *n_segment_list = 0;

    glthread_t *tilfa_segment_list =
            &tilfa_info->tilfa_segment_list_head[level];

    if(IS_GLTHREAD_LIST_EMPTY(tilfa_segment_list)) return NULL;

    ITERATE_GLTHREAD_BEGIN(tilfa_segment_list, curr){

        tilfa_segment_list_t *tilfa_segment_list = 
            tilfa_segment_list_to_segment_list(curr);

        if(!tlfa_protected_resource_equal(pr_res, &tilfa_segment_list->pr_res)){
            continue;
        }

        if(tilfa_segment_list->n_segment_list){
            *n_segment_list = tilfa_segment_list->n_segment_list;
            return tilfa_segment_list->segment_list;
        }
    } ITERATE_GLTHREAD_END(tilfa_segment_list, curr);
    return NULL;
}

void
init_tilfa(node_t *node){

    assert(node->tilfa_info == NULL);

    LEVEL level_it;
   
    node->tilfa_info = calloc(1, sizeof(tilfa_info_t));
    
    node->tilfa_info->tilfa_gl_var.max_segments_allowed = TILFA_MAX_SEGMENTS;
    init_glthread(&node->tilfa_info->tilfa_lcl_config_head);
    memset(&node->tilfa_info->current_resource_pruned, 
                        0, sizeof(protected_resource_t)); 
    
    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        
        node->tilfa_info->tilfa_pre_convergence_spf_results[level_it] = 
            init_singly_ll();
        singly_ll_set_comparison_fn(node->tilfa_info->
            tilfa_pre_convergence_spf_results[level_it], 
            spf_run_result_comparison_fn);

        node->tilfa_info->tilfa_post_convergence_spf_results[level_it] = init_singly_ll();
        singly_ll_set_comparison_fn(node->tilfa_info->\
            tilfa_post_convergence_spf_results[level_it], spf_run_result_comparison_fn);

        init_glthread(&(node->tilfa_info->post_convergence_spf_path[level_it]));

        node->tilfa_info->remote_spf_results[level_it] = init_singly_ll();
    
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
    
    printf("\t\t Tilfa Results:\n");
    
    LEVEL level_it;
    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        
        ITERATE_GLTHREAD_BEGIN(&tilfa_info->tilfa_segment_list_head[level_it], curr){

            tilfa_segment_list = tilfa_segment_list_to_segment_list(curr);
            printf("\t\t\tProtected Resource : Link");
            printf("\t\t\tResource Name : (%s)%s\n", 
                    tilfa_segment_list->pr_res.plr_node->node_name,
                    tilfa_segment_list->pr_res.protected_link->intf_name);
            printf("\t\t\tProtection type : LP : %sset   NP : %sset\n",
                    tilfa_segment_list->pr_res.link_protection ? "" : "un",
                    tilfa_segment_list->pr_res.node_protection ? "" : "un");
            printf("\t\t\t\tn_segment_list = %d\n", tilfa_segment_list->n_segment_list);

            for(i = 0 ; i < tilfa_segment_list->n_segment_list; i++){
                if(i == 0){
                    printf("\t\t\t\t\t%s %s , ", 
                            tilfa_segment_list->segment_list[i].oif->intf_name,
                            tilfa_segment_list->segment_list[i].gw_ip);
                }
                printf("%u(%s)  ", 
                        tilfa_segment_list->segment_list[i].mpls_label_out[i],
                        get_str_stackops(tilfa_segment_list->segment_list[i].stack_op[i])); 
            }
            printf("\n");
        } ITERATE_GLTHREAD_END(&tilfa_info->tilfa_segment_list_head_L1, curr);
    }
}

static void
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

        tilfa_segment_list = tilfa_segment_list_to_segment_list(curr);
        
        if(!pr_res){
            remove_glthread(&tilfa_segment_list->segment_list_glue);
            free(tilfa_segment_list);
            tilfa_segment_list = NULL;
            continue;
        }
        else if(tlfa_protected_resource_equal(&tilfa_segment_list->pr_res,
                    pr_res)){
            remove_glthread(&tilfa_segment_list->segment_list_glue);
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

   tilfa_clear_remote_spf_results(tilfa_info, 0, level);
}

static void
tilfa_clear_all_post_convergence_results(node_t *spf_root, LEVEL level, 
        protected_resource_t *pre_res){

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

    tilfa_clear_segments_list(&(tilfa_info->tilfa_segment_list_head[level]), pre_res);
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
            protected_resource_t *pr_res, 
            tilfa_lcl_config_t *tilfa_lcl_config){

    pr_res->plr_node = plr_node;
    pr_res->protected_link = get_interface_from_intf_name(plr_node, 
                        tilfa_lcl_config->protected_link);
    
    if(!pr_res->protected_link){
        pr_res->plr_node = NULL;
        return;
    }
     
    pr_res->link_protection = tilfa_lcl_config->link_protection;
    pr_res->node_protection = tilfa_lcl_config->node_protection;
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

    ITERATE_GLTHREAD_BEGIN(&tilfa_info->tilfa_lcl_config_head, curr){
        
        tilfa_lcl_config = tilfa_lcl_config_to_config_glue(curr);
        tilfa_fill_protected_resource_from_config(spf_root,
            &tilfa_info->current_resource_pruned,
            tilfa_lcl_config);
        if(tilfa_info->current_resource_pruned.protected_link){
            tilfa_run_post_convergence_spf(spf_root, level, 
                    &tilfa_info->current_resource_pruned);
        }
    } ITERATE_GLTHREAD_END(&tilfa_info->tilfa_lcl_config_head, curr);
}

/* Pass node as NULL to clear reverse spf results of all
 * nodes*/
void
tilfa_clear_remote_spf_results(tilfa_info_t *tilfa_info,
                            node_t *node, LEVEL level){

    singly_ll_node_t *curr = NULL, *prev = NULL;
    singly_ll_node_t *curr1 = NULL, *prev1 = NULL;

    spf_result_t *spf_res = NULL;
    tilfa_remote_spf_result_t *tilfa_rem_spf_result = NULL;

    ll_t *inner_lst = NULL;
    ll_t *outer_lst = tilfa_info->remote_spf_results[level];

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
        assert(is_singly_ll_empty(
            tilfa_info->remote_spf_results[level]));
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

static boolean
tilfa_does_nexthop_overlap(internal_nh_t *one_nh, 
                internal_nh_t *nh_lst){

    if(is_nh_list_empty2(nh_lst))
        return FALSE;

    int i = 0;

    for(; i < MAX_NXT_HOPS; i++){
        
        if(is_empty_internal_nh(&nh_lst[i]))
            return FALSE;

        if(one_nh->oif == nh_lst[i].oif)
            return TRUE;
    }
    return FALSE;
}

static internal_nh_t **
tilfa_p_node_qualification_test_wrt_root(
                node_t *spf_root,
                node_t *node_to_test, 
                protected_resource_t *pr_res,
                LEVEL level){

/* Algorithm :
 * step 1 : For a given potential p-node "node_to_test", 
 * get the primary nexthops from post-convergene results
 * step 2 : Get the primary nexthops for a p-node from a
 * pre-convergence spf results
 * step 3 : Ignore all nexthops of step 1 whose ifindex overlaps with
 * nexthops obtained from step 2 i.e. 
 * Remaining Nexthops = nexthops of step 1 - nexthops of step 2. Reason
 * being tilfa backups must not coincide with primary nexthops.
 * step 4 : Test extended p-node criteria of "node_to_test" through remaining
 * nexthops as nbrs of PLR obtained in step 3
 * step 5 : Add +ve results obtained in step 4 to nexthop array
 * */
    tilfa_info_t *tilfa_info = spf_root->tilfa_info;
    uint8_t n = 0;
    
    spf_result_t *res = NULL;

    internal_nh_t **tilfa_post_c_active_nxthops_for_pnodes = 
        tilfa_lookup_post_convergence_active_primary_nexthops_of_pnodes
            (tilfa_info, node_to_test, level, &res);

    assert(res);

    if(tilfa_post_c_active_nxthops_for_pnodes)
        return tilfa_post_c_active_nxthops_for_pnodes;

    tilfa_post_c_active_nxthops_for_pnodes = 
        res->tilfa_post_c_active_nxthops_for_pnodes[level];

    internal_nh_t *post_convergence_nhps = 
        tilfa_lookup_post_convergence_primary_nexthops(tilfa_info,
            node_to_test, level);
    
    if(!post_convergence_nhps || 
        is_nh_list_empty2(post_convergence_nhps))
        return NULL;

    internal_nh_t *pre_convergence_nhps =
        tilfa_lookup_pre_convergence_primary_nexthops(tilfa_info,
            node_to_test, level);

    if(!pre_convergence_nhps || 
        is_nh_list_empty2(pre_convergence_nhps))
        return NULL;

    int i = 0;
    node_t *nbr_node = NULL;

    edge_t *edge = GET_EGDE_PTR_FROM_EDGE_END(
                        pr_res->protected_link);
    node_t *protected_node = edge->to.node;

    for(; i < MAX_NXT_HOPS; i++){
        if(is_empty_internal_nh(&post_convergence_nhps[i]))
            break;
        /*check if backup nexthop overlaps with any primary nxthop*/
        if(tilfa_does_nexthop_overlap(&post_convergence_nhps[i],
                pre_convergence_nhps))
            continue;

        /*Now test p-node condition*/
        nbr_node = post_convergence_nhps[i].node;
        assert(nbr_node);
        assert(post_convergence_nhps[i].nh_type == IPNH);
       
        assert(pr_res->plr_node == spf_root);
        
        uint32_t dist_nbr_to_pnode = tilfa_dist_from_x_to_y(
            tilfa_info, nbr_node, node_to_test , level);
        uint32_t dist_nbr_to_S = tilfa_dist_from_x_to_y(
            tilfa_info, nbr_node, spf_root, level);
        uint32_t dist_S_to_pnode = tilfa_dist_from_self(
            tilfa_info, node_to_test, level);

        /*loop free wrt Source*/
        if(!(dist_nbr_to_pnode < dist_nbr_to_S + dist_S_to_pnode))
            continue;

        /*I think downstream criteria is automatically met since the
         * p-node lies on post-convergence path*/

        if(pr_res->node_protection){
            uint32_t dist_nbr_to_E = tilfa_dist_from_x_to_y(
                tilfa_info, nbr_node, protected_node, level);
            uint32_t dist_E_to_pnode = tilfa_dist_from_x_to_y(
                tilfa_info, protected_node, node_to_test, level);
            if(dist_nbr_to_pnode < dist_nbr_to_E + dist_E_to_pnode){
                tilfa_post_c_active_nxthops_for_pnodes[n++] = 
                    &post_convergence_nhps[i];
                continue;
            }
        }

        if(pr_res->link_protection){
            if(dist_nbr_to_pnode < 
                (dist_nbr_to_S + edge->metric[level])){
                tilfa_post_c_active_nxthops_for_pnodes[n++] 
                    = &post_convergence_nhps[i];
            }
        }
    }

    if(tilfa_post_c_active_nxthops_for_pnodes[0])
        return tilfa_post_c_active_nxthops_for_pnodes;
    return NULL;
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

    uint32_t dist_q_to_d = tilfa_dist_from_x_to_y(tilfa_info,
                node_to_test, destination, level);
    
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
    dist_q_to_S = tilfa_dist_from_x_to_y(tilfa_info,
            node_to_test, spf_root, level);
    
    dist_S_to_d = tilfa_dist_from_self(tilfa_info,
            destination, level);

    if(!(dist_q_to_d < dist_q_to_S + dist_S_to_d)){
        return FALSE;
    }

    if(pr_res->node_protection){
        /*Test Node protection status*/
        dist_q_to_protected_node = tilfa_dist_from_x_to_y(tilfa_info,
            node_to_test, protected_node, level);
        dist_protected_node_to_d = tilfa_dist_from_x_to_y(tilfa_info,
            protected_node, destination, level);

        if(dist_q_to_d < dist_q_to_protected_node + dist_protected_node_to_d){
            return TRUE;
        }
    }
    /*If node protection is not feasilble, check for link protection*/
    if(pr_res->link_protection){
        /*Test Link protection Status*/ 
        if(dist_q_to_d < dist_q_to_S + edge->metric[level]){
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
tilfa_record_segment_list(node_t *spf_root, int n, 
                          internal_nh_t **nexthops, 
                          node_t *p_node, 
                          node_t *q_node, 
                          LEVEL level, 
                          int pq_distance,
                          node_t *dest_node, 
                          protected_resource_t *pr_res){

    printf("%s : Destination %s, level %s\n", 
        __FUNCTION__, dest_node->node_name, get_str_level(level));

    printf("\tProtected Resource : Link");
    printf("\tResource Name : (%s)%s\n",
            pr_res->plr_node->node_name,
            pr_res->protected_link->intf_name);
    printf("\tProtection type : LP : %sset   NP : %sset\n",
            pr_res->link_protection ? "" : "un",
            pr_res->node_protection ? "" : "un");

    printf("\t\tnexthop count : %d\n", n);

    int i = 0;

    for( ; i < n ; i++){

        printf("\t\t\toif = %s, gw_ip = %s, node = %s, P-node = %s"
               ", Q-node = %s, pq_distance = %d\n",
            nexthops[i]->oif->intf_name, 
            next_hop_gateway_pfx(nexthops[i]),
            nexthops[i]->node->node_name, 
            p_node->node_name, 
            q_node->node_name, pq_distance);
    }
}
static void
print_raw_tilfa_path(internal_nh_t **nexthops, node_t *p_node, 
                 node_t *q_node, node_t *dest){

    printf("DEST = %s\n", dest->node_name);

    if(!nexthops){
        printf("Nexthop Array : NULL"); 
    }
    else if(!nexthops[0]){
        printf("Nexthop Array : Empty");
    }
    else{
        int i = 0;
        for( ; nexthops[i]; i++){
            printf("oif = %s, gw = %s\n", 
                nexthops[i]->oif->intf_name, 
                nexthops[i]->gw_prefix);
        }
    }
    printf("p_node = %s, q_node = %s\n", 
            p_node ? p_node->node_name : "Nil" ,
            q_node ? q_node->node_name : "Nil");
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
    internal_nh_t **nxthops = NULL;
    boolean search_for_q_node = TRUE;
    boolean search_for_p_node = FALSE;
    glthread_t *dest_entry = last_entry;
    
    node_t *dst_node = 
        GET_PRED_INFO_NODE_FROM_GLTHREAD(dest_entry);

    glthread_t *p_node = NULL;
    glthread_t *q_node = dest_entry;

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

        nxthops = NULL;

        if(last_entry == dest_entry){
            
            /*Examinining the Destination*/
            /*Q-node Test : Pass, recede.... */
            q_node = last_entry;
            goto done;
        }
        else{
            /*if we are searching Q-node*/
            if(search_for_q_node == TRUE){

                if(tilfa_q_node_qualification_test_wrt_destination(
                    spf_root, curr_node, dst_node, pr_res, level)){
                    /*Q-node Test : Pass, recede.... */
                    q_node = last_entry;
                    goto done;
                }
                else{
                    /*q_node is our final Q-node now*/
                    search_for_q_node = FALSE;
                    search_for_p_node = TRUE;
                    /*check if curr_node is also a p-node,
                     * simply continue with same curr node*/
                    continue;
                }
            }
            else if(search_for_p_node == TRUE){
                /*Test if curr node is a p-node*/
                nxthops = tilfa_p_node_qualification_test_wrt_root(
                        spf_root, curr_node, pr_res, level);

                if(nxthops){
                    p_node = last_entry;
                    search_for_p_node = FALSE;
                }
                else{
                    /*P-node Test failed, recede....*/
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
        nxthops = tilfa_p_node_qualification_test_wrt_root(
                  spf_root, curr_node, 
                  pr_res, level);

        if(nxthops){
            p_node = last_entry;
            search_for_p_node = FALSE;
        }
        else{
            /*There is no p-node exists*/
        }
    }
    else if(search_for_q_node == TRUE &&
            search_for_p_node == FALSE){

        /*Check if first-hop node is a Q-node ?*/
        if(tilfa_q_node_qualification_test_wrt_destination(
            spf_root, curr_node, dst_node, pr_res, level)){
            q_node = last_entry;;
            search_for_q_node = FALSE;
            search_for_p_node = TRUE;
            nxthops = tilfa_p_node_qualification_test_wrt_root(
                    spf_root, curr_node, pr_res, level);

            if(nxthops){
                p_node = last_entry;
                search_for_p_node = FALSE;
            }

        }
        else{
            /*There is not even a q-node*/
        }
    }
    else if(search_for_q_node == TRUE &&
            search_for_p_node == TRUE){
        
        /* We shall be looking either for q-node or 
         * p-node, but not both at any given time*/
        assert(0);    
    }

    TILFA_FOUND:
    print_raw_tilfa_path(nxthops, 
        p_node ? GET_PRED_INFO_NODE_FROM_GLTHREAD(p_node) : 0,
        q_node ? GET_PRED_INFO_NODE_FROM_GLTHREAD(q_node): 0, 
        dst_node);
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

    /*We also need primary nexthops of P-nodes on the Post-Convergence SPF
     *path. Using these Primary nexthops, We shall explore extended p-space*/
    compute_tilfa_post_convergence_spf_primary_nexthops(spf_root, level);

    /*Now compute PC-spf paths to all destinations*/
    compute_spf_paths(spf_root, level, TILFA_RUN);

    /*Add the protected resources back to the topology*/
    tilfa_topology_unprune_protected_resource(spf_root, pr_res);

    /*Compute segment lists now*/
    tilfa_compute_segment_lists(spf_root, level, pr_res);
}

