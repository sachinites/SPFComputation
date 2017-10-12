/*
 * =====================================================================================
 *
 *       Filename:  advert.h
 *
 *    Description:  This file declares the DS to Advertise/desitribute the local config
 *    to other nodes in the network.
 *
 *        Version:  1.0
 *        Created:  Sunday 08 October 2017 01:32:23  IST
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

#ifndef __ADVERT__
#define __ADVERT__

#include "instanceconst.h"

typedef struct instance_ instance_t;
typedef struct _node_t node_t;

typedef enum{

  AD_CONFIG_ADDED,
  AD_CONFIG_REMOVED,
  AD_CONFIG_UPDATED
} ADD_OR_REMOVE_T;

typedef enum{

   PREFIX_ADD_DELETE_ADVERT,
   PREFIX_LEAK_ADVERT,
   LINK_METRIC_CHANGE_ADVERT
} ADVERT_ID_T;

typedef struct dist_info_hdr_t_{

    node_t *lsp_generator; /* pointer to the node which has generated this info*/
    LEVEL info_dist_level; /* Level at which this information is being distributed in the network*/
    ADD_OR_REMOVE_T add_or_remove;
    ADVERT_ID_T advert_id;
    char *info_data;
} dist_info_hdr_t;

typedef void (*info_dist_fn_ptr)(node_t *, node_t *, dist_info_hdr_t *);

void
generate_lsp(instance_t *instance, 
                  node_t *lsp_generator, 
                  info_dist_fn_ptr fn_ptr, dist_info_hdr_t *dist_info);
                  
/* Information advertising structures*/

/* PREFIX_ADD_DELETE_ADVERT*/
typedef struct prefix_add_del_advert_{

    char *prefix;
    char mask;
    unsigned int metric;
    LEVEL prefix_level;
    char up_down_bit;
} prefix_add_del_advert_t;

void
prefix_distribution_routine(node_t *lsp_generator,
                            node_t *lsp_receiver,
                            dist_info_hdr_t *dist_info);

#endif /* __ADVERT__ */
