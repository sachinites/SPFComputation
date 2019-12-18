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

extern instance_t *instance;

static ll_t *
tilfa_get_post_convergence_spf_result_list(
        tilfa_info_t *tilfa_info, LEVEL level){

    return tilfa_info->tilfa_spf_results[level];
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
tilfa_get_rev_spf_result_lst(tilfa_info_t *tilfa_info, 
                             node_t *node, LEVEL level,
                             boolean flush_old_lst){

    singly_ll_node_t *curr = NULL, *prev = NULL;
    singly_ll_node_t *curr1 = NULL, *prev1 = NULL;

    spf_result_t *spf_res = NULL;
    tilfa_rev_spf_result_t *tilfa_rev_spf_result = NULL;

    ll_t *inner_lst = NULL;
    
    ll_t *outer_lst = tilfa_info->rev_spf_results[level]; 
    
    assert(node);

    ITERATE_LIST_BEGIN2(outer_lst, curr, prev){

        tilfa_rev_spf_result = curr->data;
        if(tilfa_rev_spf_result->node == node){
            inner_lst = tilfa_rev_spf_result->rev_spf_result_lst;
            
            if(!flush_old_lst) return inner_lst;

            ITERATE_LIST_BEGIN2(inner_lst, curr1, prev1){    
                spf_res = curr1->data;
                free(spf_res);
            }ITERATE_LIST_END2(inner_lst, curr1, prev1);
            delete_singly_ll(inner_lst);
            return inner_lst;
        }
    } ITERATE_LIST_END2(outer_lst, curr, prev);

    tilfa_rev_spf_result = calloc(1, sizeof(tilfa_rev_spf_result_t));
    tilfa_rev_spf_result->node = node;
    tilfa_rev_spf_result->rev_spf_result_lst = init_singly_ll();

    singly_ll_set_comparison_fn(
            tilfa_rev_spf_result->rev_spf_result_lst,
            spf_run_result_comparison_fn);

    return tilfa_rev_spf_result->rev_spf_result_lst;
}


static uint32_t
tilfa_dist_from_x_to_y(tilfa_info_t *tilfa_info,
                node_t *x, node_t *y, LEVEL level){

    /*Get reverse spf result of Y*/
    ll_t *y_rev_spf_result_lst = 
        tilfa_get_rev_spf_result_lst(tilfa_info, y, level, FALSE);

    if(is_singly_ll_empty(y_rev_spf_result_lst)){
        /*Run reverse spf with y as root*/
        inverse_topology(instance, level);
        spf_computation(y, &y->spf_info, level, 
            TILFA_RUN, y_rev_spf_result_lst);
        inverse_topology(instance, level);
    }

    spf_result_t *x_res = singly_ll_search_by_key(
                y_rev_spf_result_lst, (void *)x); 

    if(!x_res) return INFINITE_METRIC;

    return (uint32_t)x_res->spf_metric;
}

segment_list_t *
tilfa_get_segment_list(node_t *node,
                       node_t *plr_node,
                       protected_resource_t *pr_res,
                       LEVEL level,
                       uint8_t *n_segment_list/*O/P*/){

    glthread_t *curr;

    if(!plr_node || !pr_res) assert(0);

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

        node->tilfa_info->tilfa_spf_results[level_it] = init_singly_ll();
        singly_ll_set_comparison_fn(node->tilfa_info->\
            tilfa_spf_results[level_it], spf_run_result_comparison_fn);

        init_glthread(&(node->tilfa_info->post_convergence_spf_path[level_it]));

        node->tilfa_info->rev_spf_results[level_it] = init_singly_ll();
    
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
tilfa_clear_spf_results(node_t *node, LEVEL level){

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

   ITERATE_LIST_BEGIN(tilfa_info->tilfa_spf_results[level], list_node){

       result = list_node->data;
       free(result);
       result = NULL;
   }ITERATE_LIST_END;
   
   delete_singly_ll(tilfa_info->tilfa_spf_results[level]);

   tilfa_clear_rev_spf_results(tilfa_info, 0, level);
}


static void
clear_tilfa_results(node_t *spf_root, LEVEL level, 
    protected_resource_t *pre_res){

    tilfa_info_t *tilfa_info = spf_root->tilfa_info;
    
    if(!tilfa_info) return;
   
    tilfa_clear_spf_results(spf_root, level);

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
    link->is_tilfa_pruned = TRUE;
    if(pr_res->link_protection){
        /*Mo more work*/
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

static void
tilfa_compute_segment_lists(node_t *spf_root, LEVEL level, 
                            protected_resource_t *pr_res){


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
tilfa_clear_rev_spf_results(tilfa_info_t *tilfa_info,
                            node_t *node, LEVEL level){

    singly_ll_node_t *curr = NULL, *prev = NULL;
    singly_ll_node_t *curr1 = NULL, *prev1 = NULL;

    spf_result_t *spf_res = NULL;
    tilfa_rev_spf_result_t *tilfa_rev_spf_result = NULL;

    ll_t *inner_lst = NULL;
    ll_t *outer_lst = tilfa_info->rev_spf_results[level];

    ITERATE_LIST_BEGIN2(outer_lst, curr, prev){

        tilfa_rev_spf_result = curr->data;
        if(node && tilfa_rev_spf_result->node != node){
            ITERATE_LIST_CONTINUE2(outer_lst, curr, prev);
        }

        inner_lst = tilfa_rev_spf_result->rev_spf_result_lst;

        ITERATE_LIST_BEGIN2(inner_lst, curr1, prev1){

            spf_res = curr1->data;
            free(spf_res);
            curr1->data = NULL; 
        } ITERATE_LIST_END2(inner_lst, curr1, prev1);
        
        delete_singly_ll(inner_lst);
        free(inner_lst);
        tilfa_rev_spf_result->rev_spf_result_lst = NULL;

        free(tilfa_rev_spf_result);
        ITERATIVE_LIST_NODE_DELETE2(outer_lst, curr, prev);
        
        if(node) return;

    } ITERATE_LIST_END2(outer_lst, curr, prev);

    if(!node){
        assert(is_singly_ll_empty(
            tilfa_info->rev_spf_results[level]));
    }
}

void
tilfa_run_post_convergence_spf(node_t *spf_root, LEVEL level, 
                               protected_resource_t *pr_res){

    clear_tilfa_results(spf_root, level, pr_res);

    /* Compute Primary next hops before Pruning the protected resource.
     * These primary nexthops are required to disqualify the TILFA
     * backups which overlaps with primary nexthops of the destination*/
    compute_tilfa_pre_convergence_spf_primary_nexthops(spf_root, level);

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
