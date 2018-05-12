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

    node_t *node;
    edge_end_t *oif;
    char gw_prefix[PREFIX_LEN];
    glthread_t glue;
} pred_info_t;

GLTHREAD_TO_STRUCT(glthread_to_pred_info, pred_info_t, glue, glthreadptr);

void
init_spf_paths_lists(instance_t *instance, LEVEL level);

/*clear the results in the spf path list in all nodes of graph
 * at level L*/
void
clear_spf_path_list(glthread_t *spf_path_list);

/*Add predecessor info to path list*/
typedef struct _glthread glthread_t;

void
add_pred_info_to_spf_path_list(glthread_t *spf_path_list, 
                           node_t *pred_node, 
                           edge_end_t *oif, char *gw_prefix);

/*Del predecessor info from path list*/
void
del_pred_info_from_spf_path_list(glthread_t *spf_path_list, node_t *pred_node, 
                                edge_end_t *oif, char *gw_prefix);

/*print the local spf path list*/
void
print_local_spf_path_list(glthread_t *spf_path_list);

boolean 
is_pred_exist_in_spf_path_list(glthread_t *spf_path_list,
                                pred_info_t *pred_info);

/*copy spf_path_list2 into spf_path_list1 without duplicates*/
void
union_spf_path_lists(glthread_t *spf_path_list1,
                     glthread_t *spf_path_list2);

void
trace_spf_path(node_t *spf_root, node_t *dst_node, LEVEL level);

#endif /* __COMPLETE_SPF_PATH__ */
