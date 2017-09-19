/*
 * =====================================================================================
 *
 *       Filename:  spfcomputation.h
 *
 *    Description:  This file declares the Data structures for SPFComputation
 *
 *        Version:  1.0
 *        Created:  Sunday 27 August 2017 02:42:41  IST
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
 *        the Free Software Foundation, version [MAX_LEVEL].
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

#ifndef __SPFCOMPUTATION__
#define __SPFCOMPUTATION__

/*-----------------------------------------------------------------------------
 *  Do not #include graph.h in this file, as it will create circular dependency.
 *  Keep this file independant of graph.h using forward declaration
 *-----------------------------------------------------------------------------*/

typedef struct spf_result_{

    struct _node_t *node; /* Next hop details are stored in the node itself*/
    unsigned int spf_metric;
} spf_result_t;

/*A DS to hold level independant SPF configuration
 * and results*/

typedef struct spf_level_info_{

    unsigned int version; /* Version of spf run on this level*/
} spf_level_info_t;

typedef struct spf_info_{

    spf_level_info_t spf_level_info[MAX_LEVEL];
    char spff_multi_area; /* use not known : set to 1 if this node is Attached to other L2 node present in specifically other area*/
    /*spf info containers for routes*/
    ll_t *routes;/*Routes computed as a result of SPF run, routes computed are not level specific*/
    ll_t *priority_routes;/*Always add route in this list*/
    ll_t *deferred_routes;

} spf_info_t;

/* A DS to hold level specific SPF configs and 
 * results*/

typedef struct _node_t node_t;


typedef enum {

    SKELETON_RUN,
    FULL_RUN
} spf_type_t;


void
spf_computation(node_t *spf_root,
        spf_info_t *spf_info,
        LEVEL level, spf_type_t spf_type);

#endif /* __SPFCOMPUTATION__ */
