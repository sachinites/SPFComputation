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

#include "routes.h"
#include "data_plane.h"
#include "prefix.h"
#include "spfutil.h"
#include "Queue.h"
#include "spftrace.h"
#include "complete_spf_path.h"
#include "spf_candidate_tree.h"
#include "no_warn.h"
#include "sr_tlv_api.h"
#include "LinuxMemoryManager/uapi_mm.h"

extern instance_t *instance;
extern void init_instance_traversal(instance_t * instance);
extern boolean tilfa_is_link_pruned(edge_t *edge);
extern boolean tilfa_is_node_pruned(node_t *node);
extern void tilfa_clear_post_convergence_spf_path(
            glthread_t *post_convergence_spf_path_head);

static unsigned int spf_level_version[MAX_LEVEL] = {0, 0, 0};

typedef struct tilfa_info_ tilfa_info_t;

extern glthread_t *
tilfa_get_post_convergence_spf_path_head(
        tilfa_info_t *tilfa_info, LEVEL level);

extern
spf_path_result_t *
TILFA_GET_SPF_PATH_RESULT(node_t *node, node_t *candidate_node,
                          LEVEL level);

static int
pred_info_compare_fn(void *_pred_info_1, void *_pred_info_2){

    pred_info_t *pred_info_1 = _pred_info_1,
                *pred_info_2 = _pred_info_2;

    if(pred_info_1->node == pred_info_2->node/* && 
        pred_info_1->oif == pred_info_2->oif &&
        !strncmp(pred_info_1->gw_prefix, pred_info_2->gw_prefix, PREFIX_LEN)*/)
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
clear_spf_predecessors(glthread_t *spf_predecessors){

    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_predecessors, curr){

        pred_info_t *pred_info = glthread_to_pred_info(curr);
        remove_glthread(curr);
        XFREE(pred_info);
    } ITERATE_GLTHREAD_END(spf_predecessors, curr);
}

void
add_pred_info_to_spf_predecessors(spf_info_t *spf_info, 
                                  glthread_t *spf_predecessors,
                                  node_t *pred_node,
                                  edge_end_t *oif, 
                                  char *gw_prefix,
                                  LEVEL level){

    glthread_t *curr = NULL;
    pred_info_t *temp = NULL;

    pred_info_t *pred_info = NULL;
    
    pred_info = XCALLOC(1, pred_info_t);

    pred_info->oif = oif;
    
    if(gw_prefix)
        strncpy(pred_info->gw_prefix, gw_prefix, PREFIX_LEN);

    pred_info->node = pred_node;

    init_glthread(&(pred_info->glue));

    /*Check for duplicates*/
    ITERATE_GLTHREAD_BEGIN(spf_predecessors, curr){
        temp = glthread_to_pred_info(curr);
        if(pred_info_compare_fn((void *)pred_info, (void *)temp) == 0){
            XFREE(pred_info);
            return;
        }
    } ITERATE_GLTHREAD_END(spf_predecessors, curr);
    
    glthread_add_next(spf_predecessors, &(pred_info->glue));
}

void
del_pred_info_from_spf_predecessors(glthread_t *spf_predecessors, node_t *pred_node,
                                 edge_end_t *oif, char *gw_prefix){

    pred_info_t pred_info, *lst_pred_info = NULL;
    pred_info.node = pred_node;
    pred_info.oif = oif;
    strncpy(pred_info.gw_prefix, gw_prefix, PREFIX_LEN);

    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_predecessors, curr){
        
        lst_pred_info = glthread_to_pred_info(curr);
        if(pred_info_compare_fn(&pred_info, lst_pred_info))
            continue;
        remove_glthread(&(lst_pred_info->glue));
        XFREE(lst_pred_info);
        return; 
    } ITERATE_GLTHREAD_END(spf_predecessors, curr);
    assert(0);
}

boolean
is_pred_exist_in_spf_predecessors(glthread_t *spf_predecessors, 
                                pred_info_t *pred_info){
    
    glthread_t *curr = NULL;
    pred_info_t *lst_pred_info = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_predecessors, curr){
        
        lst_pred_info = glthread_to_pred_info(curr);
        if(pred_info_compare_fn(lst_pred_info, pred_info))
            continue;
        return TRUE;
    } ITERATE_GLTHREAD_END(spf_predecessors, curr);
    return FALSE;
}

void
print_local_spf_predecessors(glthread_t *spf_predecessors){

    glthread_t *curr = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_predecessors, curr){

        pred_info_t *pred_info = glthread_to_pred_info(curr);
        printf("Node-name = %s, oif = %s, gw-prefix = %s\n", 
                pred_info->node->node_name, 
                pred_info->oif->intf_name, 
                pred_info->gw_prefix);
    } ITERATE_GLTHREAD_END(spf_predecessors, curr);
}

void
union_spf_predecessorss(glthread_t *spf_predecessors1, 
                     glthread_t *spf_predecessors2){

    glthread_t *curr = NULL;
    pred_info_t *pred_info = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_predecessors2, curr){

        pred_info = glthread_to_pred_info(curr);
        if(is_pred_exist_in_spf_predecessors(spf_predecessors1, pred_info))
            continue;
        remove_glthread(&pred_info->glue);
        glthread_add_next(spf_predecessors1, &pred_info->glue);
    } ITERATE_GLTHREAD_END(spf_predecessors2, curr);
}

/*A function to print the path*/
void
print_spf_paths(glthread_t *path, void *arg){

    glthread_t *curr = NULL;
    pred_info_t *pred_info = NULL;
    pred_info_wrapper_t *pred_info_wrapper = NULL;
    boolean first = TRUE;
    char *prev_gw_prefix = NULL;

    ITERATE_GLTHREAD_BEGIN(path, curr){

        pred_info_wrapper = glthread_to_pred_info_wrapper(curr);
        pred_info = pred_info_wrapper->pred_info;
        if(first){
            printf("%s(%s) -> ", pred_info->node->node_name, 
                pred_info->oif->intf_name);
            first = FALSE;
            prev_gw_prefix = pred_info->gw_prefix;
            continue;
        }
        if(!curr->right)
            break;
        printf("(%s)%s(%s) -> ", prev_gw_prefix, 
            pred_info->node->node_name, pred_info->oif->intf_name);
            prev_gw_prefix = pred_info->gw_prefix;
    } ITERATE_GLTHREAD_END(path, curr);

    printf("(%s)%s", prev_gw_prefix, pred_info->node->node_name);
}

void
print_sr_tunnel_paths(glthread_t *path, void *arg){

    glthread_t *curr = NULL;
    pred_info_t *pred_info = NULL;
    pred_info_wrapper_t *pred_info_wrapper = NULL;
    boolean first = TRUE;
    char *prev_gw_prefix = NULL;
    unsigned int prefix_sid = (unsigned int)arg;
    mpls_label_t incoming_label = 0 ,
                 outgoing_label = 0;

    ITERATE_GLTHREAD_BEGIN(path, curr){

        pred_info_wrapper = glthread_to_pred_info_wrapper(curr);
        pred_info = pred_info_wrapper->pred_info;
        
        if(first){

            incoming_label = get_label_from_srgb_index(pred_info->node->srgb, prefix_sid);
            printf("(%u) %s(%s) -> ", incoming_label, pred_info->node->node_name, 
                                     pred_info->oif->intf_name);
            first = FALSE;
            prev_gw_prefix = pred_info->gw_prefix;
            continue;
        }

        if(!curr->right)
            break;

        outgoing_label = get_label_from_srgb_index(pred_info->node->srgb, prefix_sid);

        printf("(%s)%s(%s) (%u) -> ", prev_gw_prefix, 
            pred_info->node->node_name, pred_info->oif->intf_name, outgoing_label);
            prev_gw_prefix = pred_info->gw_prefix;

    } ITERATE_GLTHREAD_END(path, curr);

    outgoing_label = get_label_from_srgb_index(pred_info->node->srgb, prefix_sid);
    printf("(%s)%s (%u)", prev_gw_prefix, pred_info->node->node_name, outgoing_label);
}

static void
construct_spf_path_recursively(node_t *spf_root, 
                               glthread_t *spf_predecessors, 
                               LEVEL level, 
                               nh_type_t nh, 
                               glthread_t *path, 
                               spf_path_processing_fn_ptr fn_ptr, 
                               void *fn_ptr_arg,
                               boolean is_post_conv_path){

    glthread_t *curr = NULL;
    pred_info_t *pred_info = NULL;
    spf_path_result_t *res = NULL;

    ITERATE_GLTHREAD_BEGIN(spf_predecessors, curr){

        pred_info = glthread_to_pred_info(curr);
        pred_info_wrapper_t pred_info_wrapper;
        pred_info_wrapper.pred_info = pred_info;
        init_glthread(&pred_info_wrapper.glue);
        glthread_add_next(path, &pred_info_wrapper.glue);
        if(!is_post_conv_path){
            res = GET_SPF_PATH_RESULT(spf_root, pred_info->node, level, nh);
        }
        else{
            res = TILFA_GET_SPF_PATH_RESULT(spf_root, pred_info->node, level); 
        }
        assert(res);

        construct_spf_path_recursively(spf_root, &res->pred_db, 
                level, nh, path, fn_ptr, fn_ptr_arg, is_post_conv_path);

        if(pred_info->node == spf_root){
            fn_ptr(path, fn_ptr_arg);
            printf("\n");
        }
        remove_glthread(path->right);
    } ITERATE_GLTHREAD_END(spf_predecessors, curr);
}

void
trace_spf_path_to_destination_node(node_t *spf_root, 
                                   node_t *dst_node, 
                                   LEVEL level, 
                                   spf_path_processing_fn_ptr fn_ptr,
                                   void *fn_ptr_arg,
                                   boolean is_post_conv_path){

   nh_type_t nh;
   glthread_t path;
   spf_path_result_t *res = NULL;
   pred_info_wrapper_t pred_info_wrapper;
   pred_info_t pred_info;

   init_glthread(&path);

   /*Optimization - If Full spf run hasnt been run since the last
    * time user triggered the command to display all SR tunnels, 
    * then there is no need to recompute all tunnel paths again*/
#if 0
   if((spf_level_version[level] != 
           spf_root->spf_info.spf_level_info[level].version) ||
           !spf_level_version[level]){

       spf_level_version[level] = 
           spf_root->spf_info.spf_level_info[level].version;
    
       if(!is_post_conv_path)
           compute_spf_paths(spf_root, level, FULL_RUN);
       else
           compute_spf_paths(spf_root, level, TILFA_RUN);
   }
#endif
   /*Add destination node as pred_info*/
   memset(&pred_info, 0 , sizeof(pred_info_t));
   pred_info.node = dst_node;
   pred_info_wrapper.pred_info = &pred_info;
   init_glthread(&pred_info_wrapper.glue);
   glthread_add_next(&path, &pred_info_wrapper.glue);    
   nh = IPNH;
   if(!is_post_conv_path){
       res = GET_SPF_PATH_RESULT(spf_root, dst_node, level, nh);
   }
   else{
       res = TILFA_GET_SPF_PATH_RESULT(spf_root, dst_node, level);
   }
   if(!res){
       return;
   }
   glthread_t *spf_predecessors = &res->pred_db;
   construct_spf_path_recursively(spf_root, spf_predecessors, 
                                    level, nh, &path, fn_ptr, fn_ptr_arg,
                                    is_post_conv_path);
}

sr_tunn_trace_info_t
show_sr_tunnels(node_t *spf_root, char *str_prefix){

    sr_tunn_trace_info_t reason;
    routes_t *route = NULL;
    singly_ll_node_t *list_node = NULL; 
    prefix_pref_data_t route_pref, prefix_pref;
    prefix_t *prefix = NULL;
    node_t *dst_node = NULL;

    printf("SR Tunnel : Root : %s, prefix : %s\n", spf_root->node_name, str_prefix);
    
    memset(&reason, 0, sizeof(sr_tunn_trace_info_t));

    route = search_route_in_spf_route_list_by_lpm(&spf_root->spf_info, str_prefix, SPRING_T);

    if(!route){
       reason.curr_node = spf_root;
       reason.succ_node = 0;
       reason.level = LEVEL_UNKNOWN;
       reason.reason = PREFIX_UNREACHABLE;
       printf("No SR tunnel exist\n");
       return reason;  
    }

    route_pref = route_preference(route->flags, route->level);

    ITERATE_LIST_BEGIN(route->like_prefix_list, list_node){

        prefix = list_node->data;
        prefix_pref = route_preference(prefix->prefix_flags, route->level);
        if(route_pref.pref != prefix_pref.pref)
            break;
        dst_node = prefix->hosting_node;
        trace_spf_path_to_destination_node(spf_root, dst_node, route->level, 
            print_sr_tunnel_paths, (void *)(PREFIX_SID_INDEX(prefix)), FALSE);      
    } ITERATE_LIST_END;
    
    return reason;
}

static void
run_spf_paths_dijkastra(node_t *spf_root, 
                        LEVEL level, 
                        candidate_tree_t *ctree,
                        spf_type_t spf_type){

    node_t *candidate_node = NULL,
    *nbr_node = NULL;

    edge_t *edge = NULL; 

    glthread_t *curr = NULL;
    pred_info_t *pred_info = NULL,
                *pred_info_copy = NULL;

    spf_path_result_t *res = NULL;

    /* There is no need to compute per nexthop for IPV4 links and
     * RSVP LSPs. Just treat RSVP nexthops as IPV4 nexthops 
     * in the computation since remote RSVP LSPs are indistinguishable
     * from IGP links*/
    nh_type_t nh = IPNH;

    sprintf(instance->traceopts->b, "Node : %s : Running %s() with spf_root = %s, at %s", 
            spf_root->node_name, __FUNCTION__, spf_root->node_name, get_str_level(level));
    trace(instance->traceopts, DIJKSTRA_BIT);

    while(!SPF_IS_CANDIDATE_TREE_EMPTY(ctree)){

        /*Take the node with miminum spf_metric off the candidate tree*/
        candidate_node = SPF_GET_CANDIDATE_TREE_TOP(ctree);
        SPF_REMOVE_CANDIDATE_TREE_TOP(ctree);
        candidate_node->is_node_on_heap = FALSE;

#ifdef __ENABLE_TRACE__    
        sprintf(instance->traceopts->b, "Node : %s : Candidate node removed : %s(spf_metric = %u)", 
                spf_root->node_name, candidate_node->node_name, candidate_node->spf_metric[level]);
        trace(instance->traceopts, DIJKSTRA_BIT);
#endif
        if(candidate_node->node_type[level] != PSEUDONODE){
            if(spf_type != TILFA_RUN){

                /*copy spf path list from node to its result*/
                res = GET_SPF_PATH_RESULT(spf_root, candidate_node, level, nh);
                assert(!res);
                res = XCALLOC(1, spf_path_result_t);
                init_glthread(&res->pred_db);
                init_glthread(&res->glue);
                glthread_add_next(&spf_root->spf_path_result[level][nh], &res->glue);
#ifdef __ENABLE_TRACE__
                sprintf(instance->traceopts->b, "Node : %s : New Result Recorded for node %s for NH type : %s", 
                        spf_root->node_name, candidate_node->node_name, nh == IPNH ? "IPNH" : "LSPNH");
                trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                res->node = candidate_node;
                clear_spf_predecessors(&res->pred_db);
                if(!IS_GLTHREAD_LIST_EMPTY(&candidate_node->pred_lst[level][nh])){
                    glthread_add_next(&res->pred_db, candidate_node->pred_lst[level][nh].right);
                    init_glthread(&candidate_node->pred_lst[level][nh]);
                }
            }
            else if(spf_type == TILFA_RUN){
                res = TILFA_GET_SPF_PATH_RESULT(spf_root, candidate_node, level);
                assert(!res);
                res = XCALLOC(1, spf_path_result_t);
                init_glthread(&res->pred_db);
                init_glthread(&res->glue);
                glthread_add_next(tilfa_get_post_convergence_spf_path_head(
                            spf_root->tilfa_info, level), &res->glue);
#ifdef __ENABLE_TRACE__
                sprintf(instance->traceopts->b, "Node : %s : New Result Recorded for node %s for NH type : %s", 
                        spf_root->node_name, candidate_node->node_name, nh == IPNH ? "IPNH" : "LSPNH");
                trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                res->node = candidate_node;
                clear_spf_predecessors(&res->pred_db);
                if(!IS_GLTHREAD_LIST_EMPTY(&candidate_node->pred_lst[level][nh])){
                    glthread_add_next(&res->pred_db, candidate_node->pred_lst[level][nh].right);
                    init_glthread(&candidate_node->pred_lst[level][nh]);
                }
            }
            else
                assert(0);
        }

        /*Iterare over all the nbrs of Candidate node*/

        ITERATE_NODE_LOGICAL_NBRS_BEGIN(candidate_node, nbr_node, edge, level){
            /* Two way handshake check. Nbr-ship should be two way with nbr, even if nbr is PN. Do
             * not consider the node for SPF computation if we find 2-way nbrship is broken. */
#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "Node : %s : Exploring : Candidate Node = %s, Nbr = %s, oif = %s",
                    spf_root->node_name, candidate_node->node_name, nbr_node->node_name, edge->from.intf_name);
            trace(instance->traceopts, DIJKSTRA_BIT);
#endif
            if(!is_two_way_nbrship(candidate_node, nbr_node, level) || 
                    edge->status == 0){
                sprintf(instance->traceopts->b, "Node : %s : Two way nbr ship failed for Candidate Node = %s, Nbr = %s",
                        spf_root->node_name, candidate_node->node_name, nbr_node->node_name);
                trace(instance->traceopts, DIJKSTRA_BIT);
                continue;
            }

            if(tilfa_is_link_pruned(edge) ||
                    tilfa_is_node_pruned(nbr_node)){
                continue; 
            }

            if((unsigned long long)candidate_node->spf_metric[level] + (IS_OVERLOADED(candidate_node, level) 
                        ? (unsigned long long)INFINITE_METRIC : (unsigned long long)edge->metric[level]) < 
                    (unsigned long long)nbr_node->spf_metric[level]){

#ifdef __ENABLE_TRACE__
                sprintf(instance->traceopts->b, "Node : %s : Candidate Node : %s, Nbr Node %s, pred DB cleared", 
                        spf_root->node_name, candidate_node->node_name, nbr_node->node_name);
                trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                clear_spf_predecessors(&nbr_node->pred_lst[level][nh]);
                assert(IS_GLTHREAD_LIST_EMPTY(&nbr_node->pred_lst[level][nh]));

                if(candidate_node->node_type[level] != PSEUDONODE){
#ifdef __ENABLE_TRACE__                            
                    sprintf(instance->traceopts->b, "Node : %s : Node = %s , predecossor Added = %s",
                            spf_root->node_name,  nbr_node->node_name, candidate_node->node_name);
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    add_pred_info_to_spf_predecessors(&spf_root->spf_info, &nbr_node->pred_lst[level][nh], 
                            candidate_node, &edge->from, 
                            nbr_node->node_type[level] != PSEUDONODE ? \
                            edge->to.prefix[level]->prefix : NULL, level);
                }
                else{
                    /*copy (do not move) all predecessors of PN into nbr node*/
#ifdef __ENABLE_TRACE__                            
                    sprintf(instance->traceopts->b, "Node : %s : Candidate Node = %s (PN case), presecessor copied to %s",
                            spf_root->node_name,  candidate_node->node_name, nbr_node->node_name);
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    ITERATE_GLTHREAD_BEGIN(&candidate_node->pred_lst[level][nh], curr){

                        pred_info = glthread_to_pred_info(curr);  
                        pred_info_copy = XCALLOC(1, pred_info_t);
                        memcpy(pred_info_copy, pred_info, sizeof(pred_info_t));
#ifdef __ENABLE_TRACE__                                
                        sprintf(instance->traceopts->b, "Node : %s : Predecessor copied = %s", 
                                spf_root->node_name, pred_info->node->node_name);
                        trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                        init_glthread(&pred_info_copy->glue);
                        strncpy(pred_info_copy->gw_prefix, edge->to.prefix[level]->prefix, PREFIX_LEN);
                        glthread_add_next(&nbr_node->pred_lst[level][nh], &pred_info_copy->glue);   
                    } ITERATE_GLTHREAD_END(&candidate_node->pred_lst[level][nh], curr);
                }

                nbr_node->spf_metric[level] =  IS_OVERLOADED(candidate_node, level) ? 
                    INFINITE_METRIC : candidate_node->spf_metric[level] + edge->metric[level]; 
#ifdef __ENABLE_TRACE__                
                sprintf(instance->traceopts->b, "Node : %s : Node = %s metric improved to = %u",
                        spf_root->node_name,  nbr_node->node_name, nbr_node->spf_metric[level]);
                trace(instance->traceopts, DIJKSTRA_BIT);
#endif

                if(nbr_node->is_node_on_heap == FALSE){
                    SPF_INSERT_NODE_INTO_CANDIDATE_TREE(ctree, nbr_node, level);
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Node : %s : Node %s Added to Candidate tree", 
                            spf_root->node_name, nbr_node->node_name);
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    nbr_node->is_node_on_heap = TRUE;
                }
                else{
                    /* We should remove the node and then add again into candidate tree*/
                    SPF_CANDIDATE_TREE_NODE_REFRESH(ctree, nbr_node, level);
                }
            }

            else if((unsigned long long)candidate_node->spf_metric[level] + (IS_OVERLOADED(candidate_node, level) 
                        ? (unsigned long long)INFINITE_METRIC : (unsigned long long)edge->metric[level]) == 
                    (unsigned long long)nbr_node->spf_metric[level]){

                if(candidate_node->node_type[level] != PSEUDONODE){
#ifdef __ENABLE_TRACE__                        
                    sprintf(instance->traceopts->b, "Node : %s : Node = %s , predecossor Added = %s",
                            spf_root->node_name,  nbr_node->node_name, candidate_node->node_name);
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    add_pred_info_to_spf_predecessors(&spf_root->spf_info, &nbr_node->pred_lst[level][nh], 
                            candidate_node, &edge->from, 
                            nbr_node->node_type[level] != PSEUDONODE ? \
                            edge->to.prefix[level]->prefix : NULL, level); 
                }
                else{
                    /*copy (do not move) all predecessors of PN into nbr node*/
#ifdef __ENABLE_TRACE__                            
                    sprintf(instance->traceopts->b, "Node : %s : Candidate Node = %s (PN case), presecessor copied to %s",
                            spf_root->node_name,  candidate_node->node_name, nbr_node->node_name);
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif

                    ITERATE_GLTHREAD_BEGIN(&candidate_node->pred_lst[level][nh], curr){

                        pred_info = glthread_to_pred_info(curr);  
                        pred_info_copy = XCALLOC(1, pred_info_t);
                        memcpy(pred_info_copy, pred_info, sizeof(pred_info_t));
#ifdef __ENABLE_TRACE__                                
                        sprintf(instance->traceopts->b, "Node : %s : Predecessor copied = %s", 
                                spf_root->node_name, pred_info->node->node_name);
                        trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                        init_glthread(&pred_info_copy->glue);
                        strncpy(pred_info_copy->gw_prefix, edge->to.prefix[level]->prefix, PREFIX_LEN);
                        glthread_add_next(&nbr_node->pred_lst[level][nh], &pred_info_copy->glue);   
                    } ITERATE_GLTHREAD_END(&candidate_node->pred_lst[level][nh], curr);
                }

                if(nbr_node->is_node_on_heap == FALSE){
                    SPF_INSERT_NODE_INTO_CANDIDATE_TREE(ctree, nbr_node, level);
#ifdef __ENABLE_TRACE__                    
                    sprintf(instance->traceopts->b, "Node : %s : Node %s Added to Candidate tree", 
                            spf_root->node_name, nbr_node->node_name);
                    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
                    nbr_node->is_node_on_heap = TRUE;
                }
            }
        }
        ITERATE_NODE_LOGICAL_NBRS_END;

        /*Delete the PN's predecessor list*/
        if(candidate_node->node_type[level] == PSEUDONODE){
#ifdef __ENABLE_TRACE__            
            sprintf(instance->traceopts->b, "Node : %s : PN = %s, Clean up pred db",
                    spf_root->node_name, candidate_node->node_name);
            trace(instance->traceopts, DIJKSTRA_BIT); 
#endif
            ITERATE_GLTHREAD_BEGIN(&candidate_node->pred_lst[level][nh], curr){

                pred_info = glthread_to_pred_info(curr);
                remove_glthread(&pred_info->glue);
                XFREE(pred_info);
            } ITERATE_GLTHREAD_END(&candidate_node->pred_lst[level][nh], curr);
        }
#ifdef __ENABLE_TRACE__        
        sprintf(instance->traceopts->b, "Node : %s : Node = %s has been processed",
                spf_root->node_name, candidate_node->node_name);
        trace(instance->traceopts, DIJKSTRA_BIT);
#endif
    } /* while loop ends*/
#ifdef __ENABLE_TRACE__    
    sprintf(instance->traceopts->b, "Node : %s : Running %s() with spf_root = %s, at %s Finished", 
            spf_root->node_name, __FUNCTION__, spf_root->node_name, get_str_level(level));
    trace(instance->traceopts, DIJKSTRA_BIT);
#endif
}

void
spf_clear_spf_path_result(node_t *spf_root, LEVEL level){

    nh_type_t nh;
    glthread_t *curr = NULL, *curr1= NULL;
    spf_path_result_t *spf_path_result = NULL;
    pred_info_t *pred_info = NULL;

    ITERATE_NH_TYPE_BEGIN(nh){

        ITERATE_GLTHREAD_BEGIN(&spf_root->spf_path_result[level][nh], curr){

            spf_path_result = glthread_to_spf_path_result(curr);        
            ITERATE_GLTHREAD_BEGIN(&spf_path_result->pred_db, curr1){

                pred_info = glthread_to_pred_info(curr1);
                remove_glthread(&pred_info->glue);
                XFREE(pred_info);
            } ITERATE_GLTHREAD_END(&spf_path_result->pred_db, curr1);
            remove_glthread(&spf_path_result->glue);
            XFREE(spf_path_result);
        } ITERATE_GLTHREAD_END(&spf_root->spf_path_result[level][nh], curr);
        init_glthread(&spf_root->spf_path_result[level][nh]);
    }ITERATE_NH_TYPE_END;
}

void
compute_spf_paths(node_t *spf_root, LEVEL level, spf_type_t spf_type){

    node_t *curr_node = NULL, *nbr_node = NULL;
    edge_t *edge = NULL;

    /* TILFA rsults has been cleared from the caller
     * tilfa_run_post_convergence_spf()*/
    if(spf_type != TILFA_RUN){
        spf_clear_spf_path_result(spf_root, level);
    }
    else{
        tilfa_clear_post_convergence_spf_path(
            tilfa_get_post_convergence_spf_path_head(spf_root->tilfa_info, level));
    }
    
    SPF_RE_INIT_CANDIDATE_TREE(&instance->ctree);
    SPF_INSERT_NODE_INTO_CANDIDATE_TREE(&instance->ctree, spf_root, level);
    spf_root->is_node_on_heap = TRUE;

    /*Initialize all metric to infinite*/
    spf_root->spf_metric[level] = 0;
    spf_root->lsp_metric[level] = 0;

    Queue_t *q = initQ();
    init_instance_traversal(instance);
    spf_root->traversing_bit = 1;

    enqueue(q, spf_root);

    while(!is_queue_empty(q)){

        curr_node = deque(q);
        ITERATE_NODE_LOGICAL_NBRS_BEGIN(curr_node, nbr_node, edge, level){

            if(nbr_node->traversing_bit)
                continue;

            nbr_node->spf_metric[level] = INFINITE_METRIC;
            nbr_node->lsp_metric[level] = INFINITE_METRIC;

            nbr_node->traversing_bit = 1;
            enqueue(q, nbr_node);

        } ITERATE_NODE_LOGICAL_NBRS_END;
    }
    run_spf_paths_dijkastra(spf_root, level, &instance->ctree, spf_type);
    assert(is_queue_empty(q));
    XFREE(q);
    q = NULL;
    SPF_RE_INIT_CANDIDATE_TREE(&instance->ctree);
}
