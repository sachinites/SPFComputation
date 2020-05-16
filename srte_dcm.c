/*
 * =====================================================================================
 *
 *       Filename:  srte_dcm.c
 *
 *    Description:  iThis file implements CLIs related to SRTE
 *
 *        Version:  1.0
 *        Created:  04/28/2020 01:40:21 PM
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

#include "libcli.h"
#include "cmdtlv.h"
#include "spfcmdcodes.h"
#include "instance.h"
#include <stdio.h>

extern instance_t *instance;

static int
srte_policy_config_handler(param_t *param, ser_buff_t *tlv_buf, 
                           op_mode enable_or_disable){

    node_t *node = NULL;
    char *node_name = NULL;
    int cmd_code = -1, i = 0;
    tlv_struct_t *tlv = NULL;
    
    cmd_code = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
    } TLV_LOOP_END;

    node = (node_t *)singly_ll_search_by_key(instance->instance_node_list, node_name);

    switch(cmd_code){
        case CMDCODE_CONFIG_SRTE_SEG_LST:
        break;
        case CMDCODE_CONFIG_SRTE_POLICY_TO_ADDR:
        break;
        case CMDCODE_CONFIG_SRTE_TUNNEL_MEMBER_SEG_LST:
        break;
        default:
            ;
    }
    return 0;
}

void srte_init_dcm(param_t *spring){

    param_t *show   = libcli_get_show_hook();
    param_t *debug  = libcli_get_debug_hook();
    param_t *run    = libcli_get_run_hook();
    param_t *debug_show = libcli_get_debug_show_hook();

    /*config node <node-name> source-packet-routing segment-list <seg-lst-name>*/
    {
        static param_t seg_lst;
        init_param(&seg_lst, CMD, "segment-list", 0, 0, INVALID, 0, "NAme of SRTE Segment List");
        libcli_register_param(spring, &seg_lst);
        {
            static param_t seg_lst_name;
            init_param(&seg_lst_name, LEAF, 0, 0,  0, STRING, "seg-lst-name", "SRTE Segment List Name");
            libcli_register_param(&seg_lst, &seg_lst_name);
            {
                /*config node <node-name> source-packet-routing segment-list <seg-lst-name> <hop-name> <value>*/
                static param_t seg_lst_hop_name;
                init_param(&seg_lst_hop_name, LEAF, 0, 0, 0, STRING, "seg-lst-hop-name", "SRTE Segment List Hop Name");
                libcli_register_param(&seg_lst_name, &seg_lst_hop_name);
                {
                    static param_t hop_value;
                    init_param(&hop_value, LEAF, 0, srte_policy_config_handler, 0, STRING, "hop-value", "Hop Identifier [mpls Label Or IP-address]");
                    libcli_register_param(&seg_lst_hop_name, &hop_value);
                    set_param_cmd_code(&hop_value, CMDCODE_CONFIG_SRTE_SEG_LST);
                }
            }
        }
    }


    static param_t spring_path;
    {
        /*config node <node-name> spring spring-path*/
        init_param(&spring_path, CMD, "spring-path", 0, 0, INVALID, 0, "Source-Packet-Routing Path");
        libcli_register_param(spring, &spring_path);
        {
            /*config node <node-name> spring spring-path <path-name>*/
            static param_t spring_tunn_name;
            init_param(&spring_tunn_name, LEAF, 0, 0, 0, STRING, "srte-tunnel-name", "SRTE Static Tunnel Name");
            libcli_register_param(&spring_path, &spring_tunn_name);
            {
                /*config node <node-name> spring spring-path <path-name> to <dest-address>*/
                static param_t to;
                init_param(&to, CMD, "to", 0 , 0, INVALID, 0, "Tunnel Destination Address");
                libcli_register_param(&spring_tunn_name, &to);
                {
                    static param_t to_addr;
                    init_param(&to_addr, LEAF, 0, srte_policy_config_handler, 0, STRING, "dest-address", "SRTE Tunnel Destination Address");
                    libcli_register_param(&to, &to_addr);
                    set_param_cmd_code(&to_addr, CMDCODE_CONFIG_SRTE_POLICY_TO_ADDR);
                }
            }

            {
                /*config node <node-name> spring spring-path <path-name> primary <seg-lst-name>*/
                static param_t primary;
                init_param(&primary, CMD, "primary", 0 , 0, INVALID, 0, "Primary Seg-Lst");
                libcli_register_param(&spring_tunn_name, &primary);
                {
                    static param_t seg_lst_name;
                    init_param(&seg_lst_name, LEAF, 0, srte_policy_config_handler, 0, STRING, "seg-lst-name", "SRTE Segment List Name");
                    libcli_register_param(&primary, &seg_lst_name);
                    set_param_cmd_code(&seg_lst_name, CMDCODE_CONFIG_SRTE_TUNNEL_MEMBER_SEG_LST);
                }
            }
        }
    }
    support_cmd_negation(&spring_path);
}
