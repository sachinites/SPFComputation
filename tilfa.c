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
        TLIFA_SEGMENT_LST_HEAD(tilfa_info, level); 

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
    
    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        init_glthread(&(node->tilfa_info->post_convergence_spf_path[level_it]));
    }
    init_glthread(&node->tilfa_info->tilfa_segment_list_head_L1);
    init_glthread(&node->tilfa_info->tilfa_segment_list_head_L2);
}

boolean
tilfa_update_config(tilfa_info_t *tilfa_info,
                     char *protected_link,
                     boolean link_protection,
                     boolean node_protection,
                     boolean add_or_update){

    glthread_t *curr;
    boolean found = FALSE;
    boolean config_change = FALSE;
    tilfa_lcl_config_t *tilfa_lcl_config = NULL;

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


    if(add_or_update){
        if(found){
            if(link_protection != tilfa_lcl_config->link_protection){
                tilfa_lcl_config->link_protection = link_protection;
                config_change = TRUE;
            }

            if(node_protection != tilfa_lcl_config->node_protection){
                tilfa_lcl_config->node_protection = node_protection;
                config_change = TRUE;
            }

            return config_change;
        }
        else{
            tilfa_lcl_config = calloc(1, sizeof(tilfa_lcl_config_t));
            strncpy(tilfa_lcl_config->protected_link, protected_link, IF_NAME_SIZE);
            tilfa_lcl_config->protected_link[IF_NAME_SIZE -1] = '\0';
            tilfa_lcl_config->link_protection = link_protection;
            tilfa_lcl_config->node_protection = node_protection;
            init_glthread(&tilfa_lcl_config->config_glue);
            glthread_add_next(&tilfa_info->tilfa_lcl_config_head, 
                    &tilfa_lcl_config->config_glue);
            return TRUE;
        }
    }
    /*config delete*/
    if(found){
        remove_glthread(&tilfa_lcl_config->config_glue);
        free(tilfa_lcl_config);
        return TRUE;
    }
    return FALSE;
}

void
show_tilfa_database(node_t *node){

    int i = 0;
    glthread_t *curr = NULL;
    tilfa_lcl_config_t *tilfa_lcl_config = NULL;
    tilfa_segment_list_t *tilfa_segment_list = NULL;
    
    tilfa_info_t *tilfa_info = node->tilfa_info;

    if(!tilfa_info) return;

    if(!tilfa_info->tilfa_gl_var.is_enabled){
        printf("Tilfa Status : Disabled");
        return;
    }
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

    ITERATE_GLTHREAD_BEGIN(&tilfa_info->tilfa_segment_list_head_L1, curr){

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

static void
tilfa_clear_post_convergence_path(
        glthread_t *post_convergence_spf_path_head){


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


void
clear_tilfa_results(node_t *node, LEVEL level){

    tilfa_info_t *tilfa_info = node->tilfa_info;
    
    if(!tilfa_info) return;
    
    tilfa_clear_post_convergence_path(
        &tilfa_info->post_convergence_spf_path[level]);

    if(level == LEVEL1)
        tilfa_clear_segments_list(&(tilfa_info->tilfa_segment_list_head_L1), 0);
    else if(level == LEVEL2)
        tilfa_clear_segments_list(&(tilfa_info->tilfa_segment_list_head_L2), 0);
}
