/*
 * =====================================================================================
 *
 *       Filename:  spfclihandler.h
 *
 *    Description:  This file declares the routines regarding CLI handlers
 *
 *        Version:  1.0
 *        Created:  Sunday 03 September 2017 10:46:08  IST
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

#ifndef __SPFCLIHANDLER__
#define __SPFCLIHANDLER__

#include "instance.h"
#include "libcli.h"

void
spf_node_slot_enable_disable(node_t *node, char *slot_name, 
                            op_mode enable_or_disable);

void
spf_node_slot_metric_change(node_t *node, char *slot_name,
                            LEVEL level, unsigned int new_metric);

void
display_instance_nodes(param_t *param, ser_buff_t *tlv_buf);

void
display_instance_node_interfaces(param_t *param, ser_buff_t *tlv_buf);

int
validate_ipv4_mask(char *mask);

int
show_route_tree_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable);

int
run_spf_run_all_nodes(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable);

void
inset_lsp_as_forward_adjacency(node_t *node, char *lsp_name, unsigned int metric, 
                           char *tail_end_ip, LEVEL level);

int
lfa_rlfa_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable);

int
debug_show_node_lfas(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable);

#endif /* __SPFCLIHANDLER__ */
