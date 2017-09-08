/*
 * =====================================================================================
 *
 *       Filename:  rlfa.h
 *
 *    Description:  This file declares the data structures used for computing Remote Loop free alternates
 *
 *        Version:  1.0
 *        Created:  Thursday 07 September 2017 03:35:42  IST
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

#ifndef __RLFA__
#define __RLFA__

#include "LinkedListApi.h"
#include "instanceconst.h"
#include "spfcomputation.h"

typedef ll_t * p_space_set_t;
typedef ll_t * q_space_set_t;
typedef ll_t * pq_space_set_t;

typedef struct _node_t node_t;
typedef struct _edge_t edge_t;

void
Compute_and_Store_Forward_SPF(node_t *spf_root,
                            spf_info_t *spf_info,
                            LEVEL level);
void
Compute_Neighbor_SPFs(node_t *spf_root, edge_t *edge, 
                      LEVEL level);

p_space_set_t 
compute_p_space(node_t *node, edge_t *failed_edge, LEVEL level);

p_space_set_t 
compute_extended_p_space(node_t *node, edge_t *failed_edge, LEVEL level);

q_space_set_t
compute_q_space(node_t *node, edge_t *failed_edge, LEVEL level);

pq_space_set_t
Intersect_Extended_P_and_Q_Space(p_space_set_t pset, q_space_set_t qset);


void
compute_rlfa(node_t *node, LEVEL level, edge_t *failed_edge, node_t *dest);


#endif /* __RLFA__ */
