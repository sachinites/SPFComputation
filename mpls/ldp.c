/*
 * =====================================================================================
 *
 *       Filename:  ldp.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Saturday 17 February 2018 12:47:21  IST
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

#include "ldp.h"
#include "instance.h"
#include "spfutil.h"

void
enable_ldp(node_t *node){

    if(node->ldp_config.is_enabled == TRUE)
        return;

    node->ldp_config.is_enabled = TRUE;
}

void
disable_ldp(node_t *node){

    if(node->ldp_config.is_enabled == FALSE)
        return;

    node->ldp_config.is_enabled = FALSE;
}

void
show_mpls_ldp_label_local_bindings(node_t *node){

    if(node->ldp_config.is_enabled == FALSE){
        printf("LDP not enabled\n");
        return;
    }

    printf("Node : %s LDP local Bindings:\n", node->node_name);
    printf("\tPrefix/msk          Lcl Label\n");
    printf("\t================================\n");

    singly_ll_node_t *list_node = NULL;
    LEVEL level_it;
    prefix_t *prefix = NULL;
    char str_prefix_with_mask[PREFIX_LEN_WITH_MASK + 1];
    
    for(level_it = LEVEL1; level_it < MAX_LEVEL; level_it++){
        ITERATE_LIST_BEGIN(GET_NODE_PREFIX_LIST(node, level_it), list_node){
            prefix = list_node->data;
            memset(str_prefix_with_mask, 0, PREFIX_LEN_WITH_MASK + 1);
            apply_mask2(prefix->prefix, prefix->mask, str_prefix_with_mask);
            printf("\t%-22s %u\n", str_prefix_with_mask, 
                get_ldp_label_binding(node, prefix->prefix, prefix->mask));
        } ITERATE_LIST_END;
    }
}

static char buff[NODE_NAME_SIZE + PREFIX_LEN_WITH_MASK];

mpls_label_t
get_ldp_label_binding(node_t *down_stream_node, 
                                        char *prefix, char mask){
    /*To simulate the LDP label distrubution in the network, we will 
     * use heuristics. Each router uses this heuristics to generate
     * LDP labels for all prefixes in the network*/

    /*The label generated should be locally unique. This heuristics do not
     * guarantees this but the probablity of collision is almost zero*/

    if(down_stream_node->ldp_config.is_enabled == FALSE){
        return 0;
    }

    memset(buff, 0 , sizeof(buff));
    apply_mask2(prefix, mask, buff);
    
    strncpy(buff + PREFIX_LEN_WITH_MASK, down_stream_node->node_name, NODE_NAME_SIZE);
    buff[NODE_NAME_SIZE + PREFIX_LEN_WITH_MASK - 1] = '\0';
    
    mpls_label_t label = hash_code(buff, sizeof(buff));
    label = label % (LDP_LABEL_RANGE_MAX - LDP_LABEL_RANGE_MIN);
    label += LDP_LABEL_RANGE_MIN;

    return label;
}

/*Function to create targeted LDP tunnel for RLFA functionality*/
int
create_targeted_ldp_tunnel(node_t *ingress_lsr, LEVEL level , /*Ingress LSR*/
        char *edgress_lsr_rtr_id,                             /*Egress LSR router id*/
        edge_end_t *oif, char *gw_ip,
        node_t *proxy_nbr){                   /*oif from ingress LSR to immediate strict nexthop*/
#if 0
    rt_un_table_t * inet_3_rib = NULL;

    if(!proxy_nbr->ldp_config.is_enabled){
        printf("LDP Tunnel not created. LDP not enabled on node : %s\n", proxy_nbr->node_name);
        return -1
    }

    /*egress LSr should be searched from withthe same level. However,
     * for simplicity, we are searching it network wide.*/

    node_t *egress_node = get_system_id_from_router_id(ingress_lsr, edgress_lsr_rtr_id, level);
    if(!egres_node){
        printf("Error : Egress Lsr node with router id : %s not found\n", edgress_lsr_rtr_id);
        return -1 
    }

    if(proxy_nbr == egres_node){
        printf("Error : One hop Tunnel is not created\n");
        return -1;
    }

    mpls_label_t ldp_label = get_ldp_label_binding(proxy_nbr, edgress_lsr_rtr_id, 32);
    
    if(!ldp_label){
        printf("Error : Could not get LDP label for prefix %s/%u from downstream node %s\n", 
            edgress_lsr_rtr_id, 32, proxy_nbr->node_name);
        return -1;
    }
    
    rt_key_t inet_key;
    memset(&inet_key, 0, sizeof(rt_key_t));
    strncpy(inet_key.u.prefix.prefix, edgress_lsr_rtr_id, PREFIX_LEN);
    inet_key.u.prefix.mask = 32;

    inet_3_rib = ingress_lsr->spf_info.rib[INET_3];

    rt_un_entry_t *rt_un_entry = inet_3_rib->rt_un_route_lookup(inet_3_rib, *inet_key);
    if(rt_un_entry){


    }

    internal_un_nh_t *nxthop = malloc_un_nexthop();
    nxthop->protocol = LDP_PROTO;
    nxthop->oif = oif;
    nxthop->nh_node = proxy_nbr;
    strncpy(nxthop->gw_prefix, gw_ip, PREFIX_LEN);

    nxthop->nh.inet3_nh.mpls_label_out[0] = ldp_label;
    nxthop->nh.inet3_nh.stack_op[0] = PUSH;

    /*By default it is primary, caller needs to unset 
     * it if this nexthop is meant for backup*/
    SET_BIT(nxthop->flags, PRIMARY_NH); 
    SET_BIT(nxthop->flags, IPV4_LDP_NH);
    
    nxthop->ref_count = 0;

    /*Backup properties need to be filled by caller
     * depending on whether this tunnel if primary 
     * tunnel or backup tunnel*/
    nxthop->lfa_type = NO_LFA;
    nxthop->protected_link = NULL;
    nxthop->root_metric = 0;
    nxthop->dest_metric = 0;

    time(&nxthop->last_refresh_time);
    init_glthread(&nxthop->glthread);

    glthread_t tunnel_hop_list;
    init_glthread(&tunnel_hop_list);

    glthread_add_last(&tunnel_hop_list, &nxthop->glthread);

    node_t *next_node = NULL;
    edge_end_t *next_oif = NULL;
    char *next_gw_ip = NULL;
    internal_un_nh_t *next_nexthop = NULL,
                     *ldp_nexthop = NULL;

    mpls_label_t ldp_label_in = 0,
                 ldp_label_out = 0;
    
    rt_un_table_t *inet_0_rib = NULL; 
    rt_un_entry_t *rt_un_entry = NULL;
    next_node = proxy_nbr;

    do{
        inet_0_rib = next_node->spf_info.rib[INET_0];
        rt_un_entry = inet_0_rib->rt_un_route_lookup(inet_0_rib, &inet_key);
        if(!rt_un_entry){
            printf("Error : No inet.0 route to Dest %s on node %s\n", 
                    edgress_lsr_rtr_id, 32, next_node->node_name);
            goto cleanup;
        }

        next_nxthop = GET_FIRST_NH(rt_un_entry, IPV4_NH, PRIMARY_NH);
        if(!next_nxthop){
            printf("Error : No inet.0 nexthop found to Dest %s on node %s\n",
                    edgress_lsr_rtr_id, 32, next_node->node_name);
            goto cleanup;
        }

        ldp_label_in = get_ldp_label_binding(next_node, edgress_lsr_rtr_id, 32); 
        next_oif = next_nxthop->oif;
        next_gw_ip = next_nxthop->gw_prefix;
        ldp_label_out = get_ldp_label_binding(next_nxthop->nh_node, edgress_lsr_rtr_id, 32);

        ldp_nexthop =  malloc_un_nexthop();
        ldp_nexthop->protocol = LDP_PROTO;
        ldp_nexthop->oif = next_oif;
        ldp_nexthop->nh_node = next_nxthop->nh_node;
        strncpy(ldp_nexthop->gw_prefix, next_gw_ip, PREFIX_LEN);

        ldp_nxthop->nh.inet3_nh.mpls_label_out[0] = ldp_label_out;
        ldp_nxthop->nh.inet3_nh.stack_op[0] = PUSH;

        SET_BIT(ldp_nxthop->flags, PRIMARY_NH);
        SET_BIT(ldp_nxthop->flags, TRANSIT_LDP_NH);
        ldp_nxthop->ref_count = 0;

        ldp_nxthop->lfa_type = NO_LFA;
        ldp_nxthop->protected_link = NULL;
        ldp_nxthop->root_metric = 0;
        ldp_nxthop->dest_metric = 0;
        time(&ldp_nxthop->last_refresh_time); 
        init_glthread(&ldp_nxthop->glthread);
        glthread_add_last(&tunnel_hop_list, &ldp_nxthop->glthread);


        next_node = next_nxthop->nh_node;
    } while(next_node != egres_node);
#endif
    return 0;
}

