/*
 * =====================================================================================
 *
 *       Filename:  advert.c
 *
 *    Description:  This file implements the routing to distribute node's config/info to other nodes in the network
 *
 *        Version:  1.0
 *        Created:  Sunday 08 October 2017 01:37:36  IST
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

#include <assert.h>
#include <unistd.h>
#include "advert.h"
#include "instance.h"
#include "logging.h"
#include "spfutil.h"
#include "Queue.h"

static FLAG flood = 1;

void
abort_flooding(void){

    sprintf(LOG, "Flooding aborted"); TRACE();
    flood = 0;
}

void
init_flooding(void){

    flood = 1;
}

FLAG
get_flooding_status(void){

    return flood;
}

extern void
add_route(node_t *lsp_reciever,
        node_t *lsp_generator,
        LEVEL info_dist_level,
        char *prefix, char mask,
        unsigned int metric,
        FLAG prefix_flags,
        node_t *hosting_node);

extern void
delete_route(node_t *lsp_reciever,
        node_t *lsp_generator,
        LEVEL info_dist_level,
        char *prefix, char mask,
        FLAG prefix_flags,
        unsigned int metric,
        node_t *hosting_node);


static char *
advert_id_str(ADVERT_ID_T advert_id){

    switch(advert_id)
    {
        case TLV128:
            return "TLV128";
        case TLV2:
            return "TLV2";
        default:
                assert(0);
    }       
}

static char *
add_or_remove_t_str(ADD_OR_REMOVE_T add_or_remove){

    switch(add_or_remove){
        case AD_CONFIG_ADDED:
            return "AD_CONFIG_ADDED";
        case AD_CONFIG_REMOVED:
            return "AD_CONFIG_REMOVED";
        case AD_CONFIG_UPDATED:
            return "AD_CONFIG_UPDATED";
        default:
            assert(0);
    }
}

void
lsp_distribution_routine(node_t *lsp_generator,
                node_t *lsp_receiver,
                dist_info_hdr_t *dist_info){
    
     switch(dist_info->advert_id){
        case TLV128:
            switch(dist_info->add_or_remove){
                case AD_CONFIG_ADDED:
                case AD_CONFIG_REMOVED:
                case AD_CONFIG_UPDATED:
                    partial_spf_run(lsp_receiver, dist_info->info_dist_level);
                break;
            default:
                break;   
            }
        break;
        
        case TLV2:
            switch(dist_info->add_or_remove){
                case AD_CONFIG_ADDED:
                case AD_CONFIG_REMOVED:
                case AD_CONFIG_UPDATED:
                    spf_computation(lsp_receiver, &lsp_receiver->spf_info, dist_info->info_dist_level, FULL_RUN);
                break;
            default:
                break;   
            }
        break;

        case OVERLOAD:
            switch(dist_info->add_or_remove){
                case AD_CONFIG_ADDED:
                case AD_CONFIG_REMOVED:
                case AD_CONFIG_UPDATED:
                    /*Trigger full spf run if router overloads/or unoverloads*/
                    spf_computation(lsp_receiver, &lsp_receiver->spf_info, dist_info->info_dist_level, FULL_RUN);
                break;
            default:
                break;   
            }
        break;
     }
}

void
prefix_distribution_routine(node_t *lsp_generator, 
                            node_t *lsp_receiver, 
                            dist_info_hdr_t *dist_info){


    sprintf(LOG, "LSP GEN = %s, LSP RECV = %s, ADVERT ID = %s, ADD_OR_REMOVE = %s, DIST LVL = %s", 
                lsp_generator->node_name, lsp_receiver->node_name, 
                advert_id_str(dist_info->advert_id), add_or_remove_t_str(dist_info->add_or_remove), 
                get_str_level(dist_info->info_dist_level)); TRACE(); 

    switch(dist_info->advert_id){
        case TLV128:
            switch(dist_info->add_or_remove){
                case AD_CONFIG_ADDED:
                    {
                        tlv128_ip_reach_t *ad_msg = 
                                (tlv128_ip_reach_t *)(dist_info->info_data);
                        add_route(lsp_receiver, lsp_generator, dist_info->info_dist_level, ad_msg->prefix, 
                                  ad_msg->mask, ad_msg->metric, ad_msg->prefix_flags, ad_msg->hosting_node);  
                    }
                break;
                case AD_CONFIG_REMOVED:
                    {
                        tlv128_ip_reach_t *ad_msg = 
                                (tlv128_ip_reach_t *)dist_info->info_data;
                        delete_route(lsp_receiver, lsp_generator, dist_info->info_dist_level, ad_msg->prefix, 
                                  ad_msg->mask, ad_msg->metric, ad_msg->prefix_flags, ad_msg->hosting_node);  
                    }
                break;
                case AD_CONFIG_UPDATED:
                break;
                default:
                    assert(0);
            }
            break;
            default:
            assert(0);
    }
}

static void
init_instance_traversal(instance_t * instance){

    singly_ll_node_t *list_node = NULL;
    node_t *node = NULL;

    ITERATE_LIST_BEGIN(instance->instance_node_list, list_node){
        node = (node_t *)list_node->data;
        node->traversing_bit = 0;
    }ITERATE_LIST_END;  
}

/*fn to simulate LSP generation and distribution at its simplest.*/
void
generate_lsp(instance_t *instance, 
                  node_t *lsp_generator,
                  info_dist_fn_ptr fn_ptr, dist_info_hdr_t *dist_info){

    node_t  *curr_node = NULL,
            *nbr_node = NULL;

    edge_t *edge1 = NULL,  /*Edge connecting curr node with PN*/
            *edge2 = NULL; /*Edge connecting PN to its nbr*/
    LEVEL level_of_info_dist = dist_info->info_dist_level;
    
     init_flooding(); 
     /*distribute the info to self*/
     fn_ptr(lsp_generator, lsp_generator, dist_info);

    if(get_flooding_status() == 0)
        return;

    /*distribute info in the network at a given level*/
     Queue_t *q = initQ();
     init_instance_traversal(instance);

     lsp_generator->traversing_bit = 1;
     enqueue(q, lsp_generator);

     unsigned int propogation_delay = 1;

     while(!is_queue_empty(q)){
         curr_node = deque(q);
         sleep(propogation_delay); /*Let us introduce some delay in information propogation*/
         ITERATE_NODE_PHYSICAL_NBRS_BEGIN(curr_node, nbr_node, edge1, 
                                         edge2, level_of_info_dist){
             if(nbr_node->traversing_bit)
                 continue;
              sprintf(LOG, "LSP Distribution Src : %s, Des Node : %s", 
                    lsp_generator->node_name, nbr_node->node_name); TRACE();
             fn_ptr(lsp_generator, nbr_node, dist_info);
             nbr_node->traversing_bit = 1;
             enqueue(q, nbr_node);
         }
         ITERATE_NODE_PHYSICAL_NBRS_END;
     }
     assert(is_queue_empty(q));
     free(q);
     q = NULL;
}

