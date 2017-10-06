/*
 * =====================================================================================
 *
 *       Filename:  spfutil.c
 *
 *    Description:  Implementation of spfutil.h
 *
 *        Version:  1.0
 *        Created:  Sunday 27 August 2017 12:49:32  IST
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

#include "spfutil.h"
#include "logging.h"
#include <arpa/inet.h>
#include "bitsop.h"

extern instance_t *instance;

void
copy_nh_list(node_t *src_nh_list[], node_t *dst_nh_list[]){
    
    unsigned int i = 0;
    for(; i < MAX_NXT_HOPS; i++){
        dst_nh_list[i] = src_nh_list[i];
    }
}

int
is_nh_list_empty(node_t *nh_list[]){

    return (nh_list[0] == NULL);
}

char *
get_str_level(LEVEL level){

    switch(level){
        case LEVEL1:
            return "LEVEL1";
        case LEVEL2:
            return "LEVEL2";
        default:
            if(level == (LEVEL1 | LEVEL2))
                return "LEVEL12";
            else
                return "LEVEL_UNKNOWN";
    }
}

char*
get_str_node_area(AREA area){

    switch(area){
        case AREA1:
            return "AREA1";
        case AREA2:
            return "AREA2";
        case AREA3:
            return "AREA3";
        case AREA4:
            return "AREA4";
        case AREA5:
            return "AREA5";
        case AREA6:
            return "AREA6";
        default:
            return "AREA_UNKNOWN";
    }
}

/* set spf_info->spff_multi_area bit, if computing node is
 * L2 Attached. This fn is called assuming current spf run is 
 * L2 */
void
spf_determine_multi_area_attachment(spf_info_t *spf_info,
                                    node_t *spf_root){

    singly_ll_node_t *list_node = NULL;
    spf_result_t *res = NULL;
    AREA myarea = spf_root->area;
    
    spf_info->spff_multi_area = 0;
       
    ITERATE_LIST(spf_root->spf_run_result[LEVEL2], list_node){
        
        res = (spf_result_t *)list_node->data;
        if(res->node->area != myarea && 
                is_two_way_nbrship(res->node, spf_root, LEVEL2)){
            spf_info->spff_multi_area = 1;
            sprintf(LOG, "spf_root : %s is L2 Attached with remote Area node : %s", 
                            spf_root->node_name, res->node->node_name); TRACE();
            break;   
        }
    }

    if(spf_info->spff_multi_area == 0){
        sprintf(LOG, "spf_root : %s is not L2 Attached with remote Area", spf_root->node_name); TRACE();
    }
}

void
apply_mask(char *prefix, char mask, char *str_prefix){

    unsigned int binary_prefix = 0, i = 0;
    inet_pton(AF_INET, prefix, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    for(; i < (32 - mask); i++)
        UNSET_BIT(binary_prefix, i);
    binary_prefix = htonl(binary_prefix);
    inet_ntop(AF_INET, &binary_prefix, str_prefix, 16);
    str_prefix[15] = '\0';
}

