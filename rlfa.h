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

#include "instance.h"

typedef ll_t p_space;
typedef ll_t q_space;


p_space *
find_p_space(node_t *node, edge_t *edge);

p_space *
find_extended_p_space(node_t *node, edge_t *edge);

q_space *
find_q_space(node_t *node, edge_t *edge);


#endif /* __RLFA__ */
