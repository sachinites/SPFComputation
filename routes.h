/*
 * =====================================================================================
 *
 *       Filename:  routes.h
 *
 *    Description:  This file defines the data structure for routes and prefixes to be installed in RIB
 *
 *        Version:  1.0
 *        Created:  Wednesday 30 August 2017 02:09:28  IST
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

#ifndef __ROUTES__
#define __ROUTES__

#include "graphconst.h"

typedef struct prefix_{

    char prefix[PREFIX_LEN + 1];
    unsigned char mask;/*Numeric value [0-32]*/
} prefix_t;

prefix_t *
create_new_prefix(const char *prefix, unsigned char mask);

#define STR_PREFIX(prefix_t_ptr)    (prefix_t_ptr ? prefix_t_ptr->prefix : "NIL")
#define PREFIX_MASK(prefix_t_ptr)   (prefix_t_ptr ? prefix_t_ptr->mask : 0)
#endif /* __ROUTES__ */
