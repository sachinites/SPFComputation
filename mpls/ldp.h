/*
 * =====================================================================================
 *
 *       Filename:  ldp.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Saturday 17 February 2018 12:45:15  IST
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

#ifndef __LDP__
#define __LDP__

#include "instanceconst.h"

typedef struct _node_t node_t;

typedef struct _ldp_config_{
    
    boolean is_enabled; /*Is LDP enabled on the node*/
} ldp_config_t;

void
enable_ldp(node_t *node);

void
disable_ldp(node_t *node);

/*Return ever increasing LDP label
 *  * starting from 5000 to 6000 (for example)*/
mpls_label_t
get_new_ldp_label(void);

typedef struct remote_label_binding_{

    mpls_label_t outgoing_label;  /*IGP downstream node lcl LDP label*/  
    char oif_name[IF_NAME_SIZE];
    char peer_gw[PREFIX_LEN];
    node_t *peer_node;
} remote_label_binding_t;

remote_label_binding_t *
get_remote_label_binding(node_t *self_node, char *prefix, char mask, unsigned int *count);

#endif /* __LDP__*/
