/*
 * =====================================================================================
 *
 *       Filename:  sr_mapping_server.h
 *
 *    Description:  This file declares the structures used for SR Mapping Server functionality
 *
 *        Version:  1.0
 *        Created:  Saturday 24 March 2018 05:03:31  IST
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

#ifndef __MAPPINGSERVER__
#define __MAPPINGSERVER__

#include "igp_sr_ext.h"

typedef struct _node_t node_t;

void
init_mapping_server(node_t *node, LEVEL level);

/*Structure to define Mapping server local
 * policy - the prefix to sid mappings to be
 * advertised*/
/*Mapping server SID/LABEL Binding TLV*/
/*TLV used by mapping server to advertise prefix-sid ranges*/

typedef struct srms_sid_label_binding_tlv_{

    BYTE type; /*constant = 149*/
    glthread_t glthread; /*Attachement glue*//*Do not change the position of this member !!*/
    BYTE length;
    FLAG flags;
    BYTE weight;
    unsigned short int range;
    char mask;
    char fec_prefix;
    unsigned int fec_prefix2;
    prefix_sid_subtlv_t prefix_sid; 
} srms_sid_label_binding_tlv_t;

GLTHREAD_TO_STRUCT(glthread_to_srms_sid_lbl_binding_tlv, \
        srms_sid_label_binding_tlv_t, glthread, glthreadptr);

/*Flags used for srms_sid_label_binding_tlv_t*/
#define SRMS_ATTACHED_FLAG  3
#define SRMS_DOWN_FLAG      4
#define SRMS_SCOPE_FLAG     5
#define SRMS_MIRROR_FLAG    6
#define SRMS_ADDRESS_FAMILY_FLAG    7

/*Flags used for prefix_sid_subtlv_t*/
#define SRMS_SUBTLV_NODE_SID_FLAG  6

void
construct_srms_sid_mapping_entry(srms_sid_label_binding_tlv_t *srms_sid_label_binding_tlv,
    sr_mapping_entry_t *mapping_entry_out);

/*Local policy MUST be non-overlapping*/
boolean
srms_local_policy_configuration_verifier(node_t *node, 
                                         LEVEL level , 
                                         srms_sid_label_binding_tlv_t *srms_local_policy);

typedef enum{

   SRMS_RANGES_UNKNOWN,
   SRMS_RANGES_NON_OVERLAPPING,
   SRMS_RANGES_IDENTICAL,
   SRMS_RANGES_OVERLAPPING,
   SRMS_RANGES_PREFIX_CONFLICT,
   SRMS_RANGES_SID_CONFLICT 
} srms_ranges_comparison_result_t;

srms_ranges_comparison_result_t
srms_ranges_comparison(srms_sid_label_binding_tlv_t *srms_local_policy1,
                       srms_sid_label_binding_tlv_t *srms_local_policy2);

#define ADD_SRMS_LCL_POLICY_TO_ACTIVE_MAPPING(nodeptr, _level, srms_sid_label_binding_tlv_t_ptr) \
    singly_ll_add_node_by_val(nodeptr->active_mapping_policy[_level], srms_sid_label_binding_tlv_t_ptr)
    
#define ADD_SRMS_LCL_POLICY_TO_BACKUP_MAPPING(nodeptr, _level, srms_sid_label_binding_tlv_t_ptr) \
    singly_ll_add_node_by_val(nodeptr->backup_mapping_policy[_level], srms_sid_label_binding_tlv_t_ptr)
     
void
srms_clean_srms_mapping_cached_policies(node_t *node, LEVEL level);


#endif /* __MAPPINGSERVER__ */
