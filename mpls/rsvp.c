/*
 * =====================================================================================
 *
 *       Filename:  rsvp.c
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

#include "rsvp.h"
#include "instance.h"
#include "spfutil.h"

void
enable_rsvp(node_t *node){

    if(node->rsvp_config.is_enabled == TRUE)
        return;

    node->rsvp_config.is_enabled = TRUE;
}

void
disable_rsvp(node_t *node){

    if(node->rsvp_config.is_enabled == FALSE)
        return;

    node->rsvp_config.is_enabled = FALSE;
}

void
show_mpls_rsvp_label_local_bindings(node_t *node){

    if(node->rsvp_config.is_enabled == FALSE){
        printf("RSVP not enabled\n");
        return;
    }

    printf("Node : %s RSVP local Bindings:\n", node->node_name);
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
            printf("\t%-22s %u\n", str_prefix_with_mask, 
                get_rsvp_label_binding(node, prefix->prefix, prefix->mask));
        } ITERATE_LIST_END;
    }
}

static char buff[NODE_NAME_SIZE + PREFIX_LEN_WITH_MASK];

mpls_label_t
get_rsvp_label_binding(node_t *down_stream_node, 
                                        char *prefix, char mask){
    /*To simulate the RSVP label distrubution in the network, we will 
     * use heuristics. Each router uses this heuristics to generate
     * RSVP labels for all prefixes in the network*/

    /*The label generated should be locally unique. This heuristics do not
     * guarantees this but the probablity of collision is almost zero*/

    if(down_stream_node->rsvp_config.is_enabled == FALSE){
        return 0;
    }

    memset(buff, 0 , sizeof(buff));
    apply_mask2(prefix, mask, buff);
    
    strncpy(buff + PREFIX_LEN_WITH_MASK, down_stream_node->node_name, NODE_NAME_SIZE);
    buff[NODE_NAME_SIZE + PREFIX_LEN_WITH_MASK - 1] = '\0';
    
    mpls_label_t label = hash_code(buff, sizeof(buff));
    label = label % (RSVP_LABEL_RANGE_MAX - LDP_LABEL_RANGE_MIN);
    label += RSVP_LABEL_RANGE_MIN;

    return label;
}
