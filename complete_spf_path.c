/*
 * =====================================================================================
 *
 *       Filename:  complete_spf_path.c
 *
 *    Description:  This file implements the complete spf path functionality
 *
 *        Version:  1.0
 *        Created:  Saturday 12 May 2018 04:14:25  IST
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

#include "complete_spf_path.h"

static int
pred_info_compare_fn(void *_pred_info_1, void *_pred_info_2){

    pred_info_t *pred_info_1 = _pred_info_1,
                *pred_info_2 = _pred_info_2;

    if(pred_info_1->node == pred_info_2->node && 
        pred_info_1->oif == pred_info_2->oif &&
        !strncmp(pred_info_1->gw_prefix, pred_info_2->gw_prefix, PREFIX_LEN))
            return 0;
     return -1;
}

void
init_spf_paths_lists(instance_t *instance, LEVEL level){

    node_t *node = NULL;
    nh_type_t nh;
    singly_ll_node_t *list_node = NULL;

    ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
        node = list_node->data;
        ITERATE_NH_TYPE_BEGIN(nh){
            assert(IS_GLTHREAD_LIST_EMPTY(&(node->pred_lst[level][nh])));
            init_glthread(&(node->pred_lst[level][nh]));
        } ITERATE_NH_TYPE_END;
    } ITERATE_LIST_END;
}

void
clear_spf_path_list(glthread_t *spf_path_list){

    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_path_list, curr){

        pred_info_t *pred_info = glthread_to_pred_info(curr);
        free(pred_info);
    } ITERATE_GLTHREAD_END(spf_path_list, curr);

    delete_glthread_list(spf_path_list);
}

void
add_pred_info_to_spf_path_list(glthread_t *spf_path_list,
                               node_t *pred_node,
                               edge_end_t *oif, char *gw_prefix){

    glthread_t *curr = NULL;
    pred_info_t *temp = NULL;

    pred_info_t *pred_info = calloc(1, sizeof(pred_info_t));
    pred_info->node = pred_node;
    pred_info->oif = oif;
    strncpy(pred_info->gw_prefix, gw_prefix, PREFIX_LEN);
    init_glthread(&(pred_info->glue));

    /*Check for duplicates*/
    ITERATE_GLTHREAD_BEGIN(spf_path_list, curr){
        temp = glthread_to_pred_info(curr);
        assert(pred_info_compare_fn((void *)pred_info, (void *)temp));
    } ITERATE_GLTHREAD_END(spf_path_list, curr);
    
    glthread_add_next(spf_path_list, &(pred_info->glue));
}

void
del_pred_info_from_spf_path_list(glthread_t *spf_path_list, node_t *pred_node,
                                 edge_end_t *oif, char *gw_prefix){

    pred_info_t pred_info, *lst_pred_info = NULL;
    pred_info.node = pred_node;
    pred_info.oif = oif;
    strncpy(pred_info.gw_prefix, gw_prefix, PREFIX_LEN);

    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_path_list, curr){
        
        lst_pred_info = glthread_to_pred_info(curr);
        if(pred_info_compare_fn(&pred_info, lst_pred_info))
            continue;
        remove_glthread(&(lst_pred_info->glue));
        free(lst_pred_info);
        return; 
    } ITERATE_GLTHREAD_END(spf_path_list, curr);
    assert(0);
}

boolean
is_pred_exist_in_spf_path_list(glthread_t *spf_path_list, 
                                pred_info_t *pred_info){
    
    glthread_t *curr = NULL;
    pred_info_t *lst_pred_info = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_path_list, curr){
        
        lst_pred_info = glthread_to_pred_info(curr);
        if(pred_info_compare_fn(lst_pred_info, pred_info))
            continue;
        return TRUE;
    } ITERATE_GLTHREAD_END(spf_path_list, curr);
    return FALSE;
}

void
print_local_spf_path_list(glthread_t *spf_path_list){

    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_path_list, curr){

        pred_info_t *pred_info = glthread_to_pred_info(curr);
        printf("Node-name = %s, oif = %s, gw-prefix = %s\n", 
                pred_info->node->node_name, 
                pred_info->oif->intf_name, 
                pred_info->gw_prefix);
    } ITERATE_GLTHREAD_END(spf_path_list, curr);
}

void
union_spf_path_lists(glthread_t *spf_path_list1, 
                     glthread_t *spf_path_list2){

    glthread_t *curr = NULL;
    pred_info_t *pred_info = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_path_list2, curr){

        pred_info = glthread_to_pred_info(curr);
        if(is_pred_exist_in_spf_path_list(spf_path_list1, pred_info))
            continue;
        remove_glthread(&pred_info->glue);
        glthread_add_next(spf_path_list1, &pred_info->glue);
    } ITERATE_GLTHREAD_END(spf_path_list2, curr);
}

/*API to construct the SPF path from spf_root to dst_node*/

typedef struct pred_info_wrapper_t_{

    pred_info_t *pred_info;
    glthread_t glue;
} pred_info_wrapper_t;

GLTHREAD_TO_STRUCT(glthread_to_pred_info_wrapper, pred_info_wrapper_t, glue, glthreadptr);

static void
print_pred_info_wrapper_path_list(glthread_t *path){

    glthread_t *curr = NULL;
    pred_info_t *pred_info = NULL;
    pred_info_wrapper_t *pred_info_wrapper = NULL;

    ITERATE_GLTHREAD_BEGIN(path, curr){

        pred_info_wrapper = glthread_to_pred_info_wrapper(curr);
        pred_info = pred_info_wrapper->pred_info;
        printf("%s -> ", pred_info->node->node_name);
    } ITERATE_GLTHREAD_END(path, curr);
}


static void
print_spf_path_recursively(node_t *spf_root, glthread_t *spf_path_list, 
                           LEVEL level, nh_type_t nh, glthread_t *path){
    
    glthread_t *curr = NULL;
    pred_info_t *pred_info = NULL;
    spf_result_t *res = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_path_list, curr){

        pred_info = glthread_to_pred_info(curr);
        pred_info_wrapper_t pred_info_wrapper;
        pred_info_wrapper.pred_info = pred_info;
        init_glthread(&pred_info_wrapper.glue);
        glthread_add_next(path, &pred_info_wrapper.glue);
        res = GET_SPF_RESULT((&spf_root->spf_info), pred_info->node, level);
        assert(res);
        print_spf_path_recursively(spf_root, &res->spf_path_list[nh], 
                                    level, nh, path);
        if(pred_info->node == spf_root){
            print_pred_info_wrapper_path_list(path);
            printf("\n");
        }
        remove_glthread(path->right);
    } ITERATE_GLTHREAD_END(spf_path_list, curr);
}

void
trace_spf_path(node_t *spf_root, node_t *dst_node, LEVEL level){

   nh_type_t nh;
   glthread_t path;
   spf_result_t *res = NULL;
   init_glthread(&path);
   pred_info_wrapper_t pred_info_wrapper;
   pred_info_t pred_info;

   /*Add destination node as pred_info*/
   memset(&pred_info, 0 , sizeof(pred_info_t));
   pred_info.node = dst_node;
   pred_info_wrapper.pred_info = &pred_info;
   init_glthread(&pred_info_wrapper.glue);
   glthread_add_next(&path, &pred_info_wrapper.glue);               
             
   ITERATE_NH_TYPE_BEGIN(nh){    
       res = GET_SPF_RESULT((&spf_root->spf_info), dst_node, level);
       if(!res){
            printf("Spf root : %s, Dst Node : %s. Path not found", 
                spf_root->node_name, dst_node->node_name);
            return;
       }
       glthread_t *spf_path_list = &res->spf_path_list[nh];
       print_spf_path_recursively(spf_root, spf_path_list, level, nh, &path);
   } ITERATE_NH_TYPE_END;
}
