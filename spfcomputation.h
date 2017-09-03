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
#include "heap_interface.h"


/* Router global flags*/
/*When this bit is set on a router, router will ignore the ATTACHED bit flag advertised by the L1L2 router of the Area in its L1 LSP */
#define IGNORE_ATTACH_BIT   (1 << 0)
/*TRUE if this Router is connecteded to atleast one other Router in other AREA through L2 link*/
#define MULTI_AREA  (1 << 1)

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

    /*Candidate tree should be part of spf_info. 
     * At any fiven point of time, either L1 or 
     * L2 SPF computation is done which uses candidate tree. 
     * Hence, candidate tree need not be level specific*/    
    candidate_tree_t ctree;     
    spf_level_info_t spf_level_info[MAX_LEVEL];
    ll_t *routes;/*Routes computed as a result of SPF run, routes computed are not level specific*/

    char spff_multi_area; /* use not known : set to 1 if this node is Attached to other L2 node present in other area*/
} spf_info_t;

/* A DS to hold level specific SPF configs and 
 * results*/


#endif /* __SPFCOMPUTATION__ */
