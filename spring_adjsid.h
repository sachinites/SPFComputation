/*
 * =====================================================================================
 *
 *       Filename:  spring_adjsid.h
 *
 *    Description:  This file defines the data structures and functions to be used by Adjacency SIDs
 *
 *        Version:  1.0
 *        Created:  Wednesday 19 September 2018 06:50:15  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the SPF distribution (https://github.com/sachinites).
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

#ifndef __ADJ_SID__
#define __ADJ_SID__

#include "gluethread/glthread.h"
#include "instanceconst.h"
#include "igp_sr_ext.h"

/*Adjacecncy SID*/

/*If this bit set, this Adj has global scope, else local scope*/
#define ADJ_SID_FLAG_SCOPE  1

/*If unset, Adj is IPV4, else IPV6*/
#define ADJ_SID_ADDRESS_FAMILY_F_FLAG   7

/*If set, the Adj-SID is eligible for
 * protection*/
#define ADJ_SID_BACKUP_B_FLAG   6

/*If set, then the Adj-SID carries a value.
 * By default the flag is SET.*/
#define ADJ_SID_VALUE_V_FLAG 5

/*If set, then the value/index carried by
 * the Adj-SID has local significance. By default the flag is
 * SET.*/
#define ADJ_SID_LOCAL_SIGNIFICANCE_L_FLAG   4

/*When set, the S-Flag indicates that the
 * Adj-SID refers to a set of adjacencies (and therefore MAY be
 * assigned to other adjacencies as well).*/
#define ADJ_SID_SET_S_FLAG  3

/* When set, the P-Flag indicates that
 * the Adj-SID is persistently allocated, i.e., the Adj-SID value
 * remains consistent across router restart and/or interface flap 
 */
#define ADJ_SID_PERSISTENT_P_FLAG   2

/*Default flag settings for static Adj sids*/
#define SET_STATIC_ADJ_SID_DEFAULT_FLAG(flag)             \
    UNSET_BIT(flag, ADJ_SID_FLAG_SCOPE);                  \
    UNSET_BIT(flag, ADJ_SID_ADDRESS_FAMILY_F_FLAG);       \
    UNSET_BIT(flag, ADJ_SID_BACKUP_B_FLAG);               \
    SET_BIT(flag,   ADJ_SID_VALUE_V_FLAG);                \
    SET_BIT(flag,   ADJ_SID_LOCAL_SIGNIFICANCE_L_FLAG);   \
    UNSET_BIT(flag, ADJ_SID_SET_S_FLAG);                  \
    SET_BIT(flag,   ADJ_SID_PERSISTENT_P_FLAG)

#define PRINT_ADJ_SID_FLAGS(flag)   \
    printf("F:%u B:%u V:%u L:%u S:%u P:%u SC:%u",  \
        IS_BIT_SET(flag, ADJ_SID_ADDRESS_FAMILY_F_FLAG) ? 1 : 0, \
        IS_BIT_SET(flag, ADJ_SID_BACKUP_B_FLAG) ? 1 : 0,         \
        IS_BIT_SET(flag, ADJ_SID_VALUE_V_FLAG) ? 1 : 0,          \
        IS_BIT_SET(flag, ADJ_SID_LOCAL_SIGNIFICANCE_L_FLAG) ? 1 : 0, \
        IS_BIT_SET(flag, ADJ_SID_SET_S_FLAG) ? 1 : 0,                \
        IS_BIT_SET(flag, ADJ_SID_PERSISTENT_P_FLAG) ? 1 : 0,         \
        IS_BIT_SET(flag, ADJ_SID_FLAG_SCOPE))

/*pg 155, book*/
typedef enum{

    PROTECTED_ADJ_SID,
    UNPROTECTED_ADJ_SID,
    ADJ_SID_PROTECTION_MAX
} ADJ_SID_PROTECTION_TYPE;

typedef enum{

    ADJ_SID_TYPE_INDEX,
    ADJ_SID_TYPE_LABEL
} ADJ_SID_TYPE;

typedef enum{

    DYNAMIC,
    STATIC
} ADJ_SID_ALOC_TYPE;

/*All these members are advertised by IGP SR TLVs*/
typedef struct _p2p_adj_sid_subtlv_t{

    BYTE type;      /*Constant = 31*/
    BYTE length;
    BYTE flags;
    BYTE weight;    /*Used for parallel Adjacencies, section 3.4.1,
                      draft-ietf-spring-segment-routing-13 pg 15*/
    /* If local label value is specified (L and V flag sets), then
     * this field contains label value encoded as last 20 bits.
     * if index into srgb is specified, then this field contains
     * is a 4octet value indicating the offset in the SID/Label space
     * advertised bu this router. In this case L and V flag are unset.
     * */
    segment_id_subtlv_t sid[ADJ_SID_PROTECTION_MAX];/*Two SIDs are associated with each adjacency*/
    /*draft-ietf-spring-segment-routing-13 pg 15*/
} p2p_adj_sid_subtlv_t;

/*LAN ADJ SID*/
typedef struct _lan_adj_sid_subtlv_t{

    BYTE type;      /*Constant = 32*/
    BYTE length;
    BYTE flags;
    BYTE weight;
    //BYTE system_id[6];  /*RFC compliant*/
    char system_id[PREFIX_LEN]; /*Our implementation compliant*/
    segment_id_subtlv_t sid[ADJ_SID_PROTECTION_MAX];/*Two SIDs are associated with each adjacency*/
} lan_adj_sid_subtlv_t;

typedef struct p2p_intf_adj_sid_{

    ADJ_SID_TYPE adj_sid_type;
    segment_id_subtlv_t sid;
    FLAG flags;
} p2p_intf_adj_sid_t;

typedef struct lan_intf_adj_sid_{

    ADJ_SID_TYPE adj_sid_type;
    segment_id_subtlv_t sid;
    char nbr_system_id[PREFIX_LEN];
    FLAG flags;
    glthread_t cfg_glthread;
} lan_intf_adj_sid_t;

GLTHREAD_TO_STRUCT(glthread_to_cfg_lan_adj_sid, lan_intf_adj_sid_t, cfg_glthread, glthreadptr);
   
void 
print_lan_adj_sid_info(lan_intf_adj_sid_t *lan_intf_adj_sid);

void
print_p2p_adj_sid_info(p2p_intf_adj_sid_t *p2p_intf_adj_sid);

void
set_adj_sid(node_t *node, char *intf_name, LEVEL level, unsigned int label, char *nbr_sys_id, int cmdcode);

void
show_node_adj_sids(node_t *node);

boolean
is_lan_static_adj_sid_exist(glthread_t *lan_adj_sid_list, unsigned int label);

boolean
is_static_adj_sid_exist_on_interface(edge_end_t *interface, mpls_label_t label);

boolean
is_static_adj_sid_in_use(node_t *node, mpls_label_t label);

/*Get the adj sid of the least cost adjacency. Node1 and Node2
 * must be nbrs (LAN or P2P)*/
mpls_label_t
get_adj_sid_minimum(node_t *node1, node_t *node2, LEVEL level);

#endif /* __ADJ_SID__ */
