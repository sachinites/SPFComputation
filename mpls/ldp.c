/*
 * =====================================================================================
 *
 *       Filename:  ldp.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Saturday 17 February 2018 12:47:21  IST
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

#include "ldp.h"
#include "instance.h"

/*Return ever increasing LDP label
 *  * starting from 5000 to 6000 (for example)*/
mpls_label_t
get_new_ldp_label(void){

        static mpls_label_t ldp_start_label_value = 5000;
            return ldp_start_label_value++;
}

void
enable_ldp(node_t *node){

    if(node->ldp_config.is_enabled == TRUE)
        return;

    node->ldp_config.is_enabled = TRUE;
    
    /*Assign local LDP labels to all prefixes on
     * this node*/
    LEVEL level_it;
    singly_ll_node_t *list_node = NULL;
    prefix_t *prefix = NULL;

    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        ITERATE_LIST_BEGIN(GET_NODE_PREFIX_LIST(node, level_it), list_node){
            prefix = list_node->data;     
            prefix->ldp_lcl_label = get_new_ldp_label();
        }ITERATE_LIST_END;
    }
}

void
disable_ldp(node_t *node){

    if(node->ldp_config.is_enabled == FALSE)
        return;

    node->ldp_config.is_enabled = FALSE;
    
    /*Assign local LDP labels to all prefixes on
     * this node*/
    LEVEL level_it;
    singly_ll_node_t *list_node = NULL;
    prefix_t *prefix = NULL;

    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        ITERATE_LIST_BEGIN(GET_NODE_PREFIX_LIST(node, level_it), list_node){
            prefix = list_node->data;     
            prefix->ldp_lcl_label = 0;
        }ITERATE_LIST_END;
    }
}

void
show_mpls_ldp_label_local_bindings(node_t *node){

    if(node->ldp_config.is_enabled == FALSE){
        printf("LDP not enabled\n");
        return;
    }

    printf("Node : %s LDP local Bindings:\n", node->node_name);
    printf("\tPrefix/msk          Lcl Label\n");
    printf("\t================================\n");

    singly_ll_node_t *list_node = NULL;
    LEVEL level_it;
    prefix_t *prefix = NULL;
    char str_prefix_with_mask[PREFIX_LEN_WITH_MASK + 1];
    
    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        ITERATE_LIST_BEGIN(GET_NODE_PREFIX_LIST(node, level_it), list_node){
            prefix = list_node->data;
            memset(str_prefix_with_mask, 0, PREFIX_LEN_WITH_MASK + 1);
            apply_mask2(prefix->prefix, prefix->mask, str_prefix_with_mask);
            printf("\t%-22s %u\n", str_prefix_with_mask, prefix->ldp_lcl_label);
        } ITERATE_LIST_END;
    }
}

/*This fn simulates the LDP label distribution*/
remote_label_binding_t *
get_remote_label_binding(node_t *self_node, 
                         char *prefix, 
                         char mask, 
                         unsigned int *count){

   /*Get the IGP primary next hop(s) for route prefix/mask
    * and return nexthop(s) local labels for the prefix*/ 
}


