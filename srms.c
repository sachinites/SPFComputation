/*
 * =====================================================================================
 *
 *       Filename:  srms.c
 *
 *    Description:  Implementation of SR Mapping Server
 *
 *        Version:  1.0
 *        Created:  Saturday 24 March 2018 05:44:09  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the SPComputation distribution (https://github.com/sachinites).
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

#include "srms.h"
#include "LinkedListApi.h"
#include "instance.h"

/*Return true if the new policy being configured by the
 *  administrator do not create any conflict with already
 *  existing SRMS local policy db, else false.*/

/*Though overlapping SRMS policy ranges may not be conflicting,
 *  such ranges should not be acceptable as per the Cisco IOS XR*/

/*Though SRMS local policies can lead to prefix-conflicts/SID conflicts
 *  which is repairable, still we would not allow admin to configure such
 *  mapping ranges/local policies on mapping server.*/

/*But any of the conflicting senarios can happen if the network has
 *  more than one SRMS. This fn prevents the admin from configuring
 *  undesirable local policy on a Mapping server*/

boolean
srms_local_policy_configuration_verifier(node_t *node,
                                         LEVEL level ,
                                         srms_sid_label_binding_tlv_t *srms_local_policy){

    return FALSE;
}

/*To be called by SRMS clients only to choose one between the two SRMS ranges which
 * overlaps. The selected one is added to active_mapping_policy list, and rejected one
 * is added to backup_mapping_policy on SRMS client node*/
void
srmc_advertised_srms_overlapping_local_policy_selection(srms_sid_label_binding_tlv_t *srms_local_policy1,
                        srms_sid_label_binding_tlv_t *srms_local_policy2){


}

void
clean_srms_mapping_cached_policies(node_t *node, LEVEL level){
    delete_singly_ll(node->active_mapping_policy[level]);
    delete_singly_ll(node->backup_mapping_policy[level]);
}
