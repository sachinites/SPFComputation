/*
 * =====================================================================================
 *
 *       Filename:  instanceconst.h
 *
 *    Description:  This file contains Graph Constants only
 *
 *        Version:  1.0
 *        Created:  Thursday 24 August 2017 01:51:01  IST
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

#ifndef __GRAPHCONST__
#define __GRAPHCONST__

#include <assert.h>

#define IF_NAME_SIZE            16
#define NODE_NAME_SIZE          16
#define MAX_NODE_INTF_SLOTS     10
#define PREFIX_LEN              15
#define PREFIX_LEN_WITH_MASK    (PREFIX_LEN + 3)
#define MAX_NXT_HOPS            1
#define INFINITE_METRIC         (0xFFFFFFFF -1)
/* instance global flags */

#define IGNOREATTACHED  1   /*If this bit is set, then L1-only router will not install default gateway to L1L2 router of the local Area*/


/*Edge properties*/
typedef enum{
    UNIDIRECTIONAL,
    BIDIRECTIONAL,
    DIRECTION_UNKNOWN
} DIRECTION;

typedef enum{
    OUTGOING,
    INCOMING,
    EDGE_END_DIRN_UNKNOWN
} EDGE_END_DIRN;

typedef enum{
    LEVEL_UNKNOWN,
    LEVEL1,
    LEVEL2,
    MAX_LEVEL,
    LEVEL12 = LEVEL1 | LEVEL2
} LEVEL;

static inline LEVEL 
get_other_level(LEVEL level){

    LEVEL l;
    assert(level == LEVEL1 || level == LEVEL2);
    l = (level == LEVEL1) ? LEVEL2 : LEVEL1;
    return l;
}

typedef enum{
    NON_PSEUDONODE,
    PSEUDONODE
} NODE_TYPE;

/*Let The routers can have any area 
 * among these*/

typedef enum{
    AREA1,
    AREA2,
    AREA3,
    AREA4,
    AREA5,
    AREA6,
    AREA_UNKNOWN
} AREA;

typedef enum nh_type{

    IPNH,
    LSPNH
} nh_type_t;

typedef enum{

    FALSE,
    TRUE
} boolean;
#endif /* __GRAPHCONST__ */
