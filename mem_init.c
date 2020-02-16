/*
 * =====================================================================================
 *
 *       Filename:  mem_init.c
 *
 *    Description:  This file integrates the Linux Memory Manager with the SPF Project
 *
 *        Version:  1.0
 *        Created:  02/15/2020 11:42:22 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Juniper Networks (https://csepracticals.wixsite.com/csepracticals), sachinites@gmail.com
 *        Company:  Juniper Networks
 *
 *        This file is part of the SPFComputation distribution (https://github.com/sachinites) 
 *        Copyright (c) 2019 Abhishek Sagar.
 *        This program is free software: you can redistribute it and/or modify it under the terms of the GNU General 
 *        Public License as published by the Free Software Foundation, version 3.
 *        
 *        This program is distributed in the hope that it will be useful, but
 *        WITHOUT ANY WARRANTY; without even the implied warranty of
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *        General Public License for more details.
 *
 *        visit website : https://csepracticals.wixsite.com/csepracticals for more courses and projects
 *                                  
 * =====================================================================================
 */

#include "BitOp/bitarr.h"
#include "LinkedList/LinkedListApi.h"
#include "Queue/Queue.h"
#include "Stack/stack.h"
#include "complete_spf_path.h"
#include "data_plane.h"
#include "instance.h"
#include "Libtrace/libtrace.h"
#include "prefix.h"
#include "routes.h"
#include "spfcomputation.h"
#include "igp_sr_ext.h"
#include "mpls/rsvp.h"
#include "mpls/ldp.h"
#include "spring_adjsid.h"
#include "tilfa.h"
#include "gluethread/glthread.h"
#include "LinuxMemoryManager/uapi_mm.h"

static boolean is_mm_registered = FALSE;

void
init_memory_manager(){

    if(is_mm_registered)
        return;
    is_mm_registered = TRUE;

    mm_init();
    MM_REG_STRUCT(bit_array_t);
    MM_REG_STRUCT(ll_t);
    MM_REG_STRUCT(singly_ll_node_t);
    MM_REG_STRUCT(Queue_t);
    MM_REG_STRUCT(stack_t);
    MM_REG_STRUCT(pred_info_t);
    MM_REG_STRUCT(spf_path_result_t);
    MM_REG_STRUCT(internal_un_nh_t);
    MM_REG_STRUCT(rt_un_entry_t);
    MM_REG_STRUCT(rt_un_table_t);
    MM_REG_STRUCT(mpls_label_stack_t);
    //MM_REG_STRUCT(node_t);
    MM_REG_STRUCT(edge_t);
    MM_REG_STRUCT(instance_t);
    MM_REG_STRUCT(traceoptions);
    MM_REG_STRUCT(prefix_t);
    MM_REG_STRUCT(routes_t);
    MM_REG_STRUCT(internal_nh_t);
    MM_REG_STRUCT(srgb_t);
    MM_REG_STRUCT(rsvp_tunnel_t);
    MM_REG_STRUCT(ldp_config_t);
    MM_REG_STRUCT(rsvp_config_t);
    MM_REG_STRUCT(spf_result_t);
    MM_REG_STRUCT(self_spf_result_t);
    MM_REG_STRUCT(lan_intf_adj_sid_t);
    MM_REG_STRUCT(p2p_intf_adj_sid_t);
    MM_REG_STRUCT(lan_adj_sid_subtlv_t);
    MM_REG_STRUCT(p2p_adj_sid_subtlv_t);
    MM_REG_STRUCT(prefix_sid_subtlv_t);
    MM_REG_STRUCT(tilfa_remote_spf_result_t);
    MM_REG_STRUCT(tilfa_info_t);
    MM_REG_STRUCT(tilfa_lcl_config_t);
    MM_REG_STRUCT(protected_resource_t);     
    MM_REG_STRUCT(tilfa_cfg_globals_t);
    MM_REG_STRUCT(glthread_t);
    //MM_REG_STRUCT(gen_segment_list_t);
    //MM_REG_STRUCT(tilfa_segment_list_t);
}
