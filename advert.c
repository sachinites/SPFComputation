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


char *
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

void
lsp_distribution_routine(node_t *lsp_generator,
                node_t *lsp_receiver,
                dist_info_hdr_t *dist_info){
    
     switch(dist_info->advert_id){
        case TLV128:
                partial_spf_run(lsp_receiver, dist_info->info_dist_level);
                break;

        case TLV2:
                spf_computation(lsp_receiver, &lsp_receiver->spf_info, dist_info->info_dist_level, FULL_RUN);
                break;

        case OVERLOAD:
                /*Trigger full spf run if router overloads/or unoverloads*/
                spf_computation(lsp_receiver, &lsp_receiver->spf_info, dist_info->info_dist_level, FULL_RUN);
                break;  
        default:
            ; 
     }
}

void
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

    LEVEL level_of_info_dist = dist_info->info_dist_level,
          level_it = LEVEL_UNKNOWN;

    init_flooding(); 
    /*distribute the info to self*/
    fn_ptr(lsp_generator, lsp_generator, dist_info);

    if(get_flooding_status() == 0)
        return;

    /*distribute info in the network at a given level*/
    Queue_t *q = initQ();

    for(level_it = LEVEL1 ; level_it < MAX_LEVEL; level_it++){

        if(!IS_LEVEL_SET(level_of_info_dist, level_it))
            continue;

        init_instance_traversal(instance);

        lsp_generator->traversing_bit = 1;
        enqueue(q, lsp_generator);

        unsigned int propogation_delay = 0;

        while(!is_queue_empty(q)){

            curr_node = deque(q);

            sleep(propogation_delay); /*Let us introduce some delay in information propogation*/

            ITERATE_NODE_PHYSICAL_NBRS_BEGIN(curr_node, nbr_node, edge1, 
                                            edge2, level_it){

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
        reuse_q(q);
    }
    free(q);
    q = NULL;
}

