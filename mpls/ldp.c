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

int
create_targeted_ldp_tunnel(node_t *ingress_lsr, /*Ingress LSR*/
        char *edgress_lsr_rtr_id,                             /*Egress LSR router id*/
        edge_end_t *oif, char *gw_ip,
        node_t *proxy_nbr){                                   /*oif from ingress LSR to immediate strict nexthop*/

    boolean rc = FALSE;
    glthread_t *curr = NULL;

    rt_un_table_t *inet_0_rib = NULL,
                  *inet_3_rib = NULL,
                  *mpls_0_rib = NULL;

    node_t *next_node = NULL;

    rt_un_entry_t *rt_un_entry = NULL;
    internal_un_nh_t *nexthop = NULL,
                     *new_nexthop = NULL;

    inet_0_rib = ingress_lsr->spf_info.rib[INET_0];
    inet_3_rib = ingress_lsr->spf_info.rib[INET_3];
    mpls_label_t outgoing_ldp_label = 0 ,
                 incoming_ldp_label = 0;

    rt_key_t inet_key;
    memset(&inet_key, 0, sizeof(rt_key_t));
    strncpy(inet_key.u.prefix.prefix, edgress_lsr_rtr_id, PREFIX_LEN);
    inet_key.u.prefix.mask = 32;

    /*This is Non production code compliance*/
    node_t *edgress_lsr = get_system_id_from_router_id(ingress_lsr, edgress_lsr_rtr_id, LEVEL1);
    if(!edgress_lsr){
        edgress_lsr = get_system_id_from_router_id(ingress_lsr, edgress_lsr_rtr_id, LEVEL2);   
    }

    if(!edgress_lsr){
        printf("Error : could not find edgress_lsr node with router id %s/32", edgress_lsr_rtr_id);
        return -1;
    }

    if(!oif){
        int nh_type = -1;
        /*If strict next hop is not given, choose the IGP nexthop*/
        rt_un_entry = inet_3_rib->rt_un_route_lookup(inet_3_rib, &inet_key);     
        nh_type = IPV4_LDP_NH;
        if(!rt_un_entry){
            nh_type = -1;
            printf("No route for %s/32 in %s table\n", edgress_lsr_rtr_id, inet_3_rib->rib_name);
            rt_un_entry = inet_0_rib->rt_un_route_lookup(inet_0_rib, &inet_key);
            nh_type = IPV4_NH;
            if(!rt_un_entry){
                printf("No route for %s/32 in %s table\n", edgress_lsr_rtr_id, inet_0_rib->rib_name);
                return -1;   
            }
        }

        if(nh_type == IPV4_LDP_NH){
            nexthop = GET_FIRST_NH(rt_un_entry, IPV4_LDP_NH, PRIMARY_NH);
            if(!nexthop){
                nh_type = IPV4_NH;
                rt_un_entry = inet_0_rib->rt_un_route_lookup(inet_0_rib, &inet_key);
                if(!rt_un_entry){
                    printf("No IPV4 route found for %s/32 in inet.0 tables on node %s\n",
                        edgress_lsr_rtr_id, ingress_lsr->node_name);
                    return -1;
                }
                nexthop = GET_FIRST_NH(rt_un_entry, IPV4_NH, PRIMARY_NH);
                if(!nexthop){
                    printf("No IPV4 nexthop for route %s/32 in inet.0 tables on node %s\n",
                        edgress_lsr_rtr_id, ingress_lsr->node_name);
                    return -1;
                }
            }
        }
        else{
            nexthop = GET_FIRST_NH(rt_un_entry, IPV4_NH, PRIMARY_NH);
            nh_type = IPV4_NH;
            if(!nexthop){
                printf("No IPV4 nexthop for route %s/32 in inet.0 tables on node %s\n",
                        edgress_lsr_rtr_id, ingress_lsr->node_name);
                return -1;
            }
        }

        if(nh_type == IPV4_LDP_NH){
            /*No action needed, there is already LDP nexthop installed locally towards egress LSR*/
            next_node = nexthop->nh_node;
            outgoing_ldp_label = nexthop->nh.inet3_nh.mpls_label_out[0];
            goto NEXT_NODE;
        }
        else if(nh_type == IPV4_NH){
            new_nexthop = malloc_un_nexthop();
            memcpy(new_nexthop, nexthop, sizeof(internal_un_nh_t));    
            /*ldpify new nexthop*/
            if(!new_nexthop->nh_node->ldp_config.is_enabled){
                printf("IGP nexthop %s is not LDP enabled\n", new_nexthop->nh_node->node_name);
                free_un_nexthop(new_nexthop);
                return -1;
            }

            outgoing_ldp_label = get_ldp_label_binding(new_nexthop->nh_node, edgress_lsr_rtr_id, 32);
            if(!outgoing_ldp_label){
                printf("Could not get LDP label binding for IP %s from node %s", 
                        edgress_lsr_rtr_id, new_nexthop->nh_node->node_name);
                free_un_nexthop(new_nexthop);
                return -1;
            }

            new_nexthop->protocol = LDP_PROTO;
            new_nexthop->nh.inet3_nh.mpls_label_out[0] = outgoing_ldp_label;
            new_nexthop->nh.inet3_nh.stack_op[0] = PUSH;
            new_nexthop->flags = 0;
            SET_BIT(new_nexthop->flags, PRIMARY_NH);
            SET_BIT(new_nexthop->flags, IPV4_LDP_NH);
            /* Backup properties need to be filled by caller
             * depending on whether this tunnel if primary 
             * tunnel or backup tunnel*/
            new_nexthop->lfa_type = NO_LFA;
            new_nexthop->protected_link = NULL;
            new_nexthop->root_metric = 0;
            new_nexthop->dest_metric = 0;
            time(&new_nexthop->last_refresh_time);
            init_glthread(&new_nexthop->glthread);

            /*Now install it in inet.3 table*/
            rc = inet_3_rt_un_route_install_nexthop(inet_3_rib, &inet_key, rt_un_entry->level, new_nexthop);
            if(!rc){
                printf("Failed to install LDP nexthop in %s\n", inet_3_rib->rib_name);
                free_un_nexthop(new_nexthop);
                return -1;
            }
        }
        next_node = new_nexthop->nh_node;
    }
    else{
        /*Request LDP label from proxy_nbr for edgress_lsr_rtr_id*/
        if(!proxy_nbr->ldp_config.is_enabled){
            printf("LDP is not enabled on proxy nbr node %s\n", proxy_nbr->node_name);
            return -1;
        }

        boolean is_exist = FALSE;
        /*Check if LDP nexthop is already installed via proxy nbr in local inet.3 table*/
        rt_un_entry = inet_3_rib->rt_un_route_lookup(inet_3_rib, &inet_key); 

        ITERATE_GLTHREAD_BEGIN(&rt_un_entry->nh_list_head, curr){
            nexthop = glthread_to_unified_nh(curr);
            if(nexthop->oif == oif && !strncpy(nexthop->gw_prefix, gw_ip, PREFIX_LEN) && 
                    nexthop->protocol == LDP_PROTO){
                assert(nexthop->nh_node == proxy_nbr);
                is_exist = TRUE;
                break;
            }
        }ITERATE_GLTHREAD_END(&rt_un_entry->nh_list_head, curr);

        if(is_exist){
            next_node = nexthop->nh_node;
            outgoing_ldp_label = nexthop->nh.inet3_nh.mpls_label_out[0];
            goto NEXT_NODE;
        }

        outgoing_ldp_label = get_ldp_label_binding(proxy_nbr, edgress_lsr_rtr_id, 32);    

        if(!outgoing_ldp_label){
            printf("Could not get LDP label binding for IP %s from node %s. Tunnel construction aborted\n", 
                edgress_lsr_rtr_id, proxy_nbr->node_name);
            return -1;
        }

        new_nexthop = malloc_un_nexthop();
        new_nexthop->protocol = LDP_PROTO; 
        new_nexthop->oif = oif;
        new_nexthop->nh_node = proxy_nbr;
        strncpy(new_nexthop->gw_prefix, gw_ip, PREFIX_LEN);
        new_nexthop->nh.inet3_nh.mpls_label_out[0] = outgoing_ldp_label;
        new_nexthop->nh.inet3_nh.stack_op[0] = PUSH; 
        SET_BIT(new_nexthop->flags, PRIMARY_NH);
        SET_BIT(new_nexthop->flags, IPV4_LDP_NH);
        new_nexthop->ref_count = 0; 
        /* Backup properties need to be filled by caller
         * depending on whether this tunnel if primary 
         * tunnel or backup tunnel*/
        new_nexthop->lfa_type = NO_LFA;
        new_nexthop->protected_link = NULL;
        new_nexthop->root_metric = 0;
        new_nexthop->dest_metric = 0;
        time(&new_nexthop->last_refresh_time);
        init_glthread(&new_nexthop->glthread);

        /*Now install it in inet.3 table*/
        rc = inet_3_rt_un_route_install_nexthop(inet_3_rib, &inet_key, rt_un_entry->level, new_nexthop);
        if(!rc){
            printf("Failed to install LDP nexthop in %s. Tunnel construction aborted\n", inet_3_rib->rib_name);
            free_un_nexthop(new_nexthop);
            return -1;
        }
        next_node = new_nexthop->nh_node;
    }

NEXT_NODE:
    while(next_node != edgress_lsr){
        printf("Installing LDP transit tunnel on node : %s\n", next_node->node_name);
        incoming_ldp_label = outgoing_ldp_label;
        mpls_0_rib = next_node->spf_info.rib[MPLS_0];  
        inet_0_rib = next_node->spf_info.rib[INET_0]; 

        /*Check if Transit LDP tunnel already exists*/
        RT_ENTRY_LABEL(&inet_key) = incoming_ldp_label;
        rt_un_entry = mpls_0_rib->rt_un_route_lookup(mpls_0_rib, &inet_key); 
        if(!rt_un_entry){
            rt_un_entry = inet_0_rib->rt_un_route_lookup(inet_0_rib, &inet_key);    
            if(!rt_un_entry){
                printf("Node %s do not have IGP route to %s/32. LDP Tunnel construction aborted\n", 
                        next_node->node_name, edgress_lsr_rtr_id);
                return -1;
            }
            nexthop = GET_FIRST_NH(rt_un_entry, IPV4_NH, PRIMARY_NH);
            if(!nexthop){
                printf("Node %s do not have IGP nexthop to %s/32. LDP Tunnel construction aborted\n",
                        next_node->node_name, edgress_lsr_rtr_id);
                return -1;
            }
            /*Now derieve LDP transit nexthop from IGP nexthop here*/
            new_nexthop = malloc_un_nexthop();
            memcpy(new_nexthop, nexthop, sizeof(internal_un_nh_t));
            /*ldpify new nexthop*/

            if(!new_nexthop->nh_node->ldp_config.is_enabled){
                printf("IGP nexthop %s is not LDP enabled. Tunnel construction aborted\n", 
                        new_nexthop->nh_node->node_name);
                free_un_nexthop(new_nexthop);
                return -1;
            }

            outgoing_ldp_label = get_ldp_label_binding(new_nexthop->nh_node, edgress_lsr_rtr_id, 32);

            if(!outgoing_ldp_label){
                printf("Could not get LDP label binding for IP %s from node %s. Tunnel construction aborted", 
                        edgress_lsr_rtr_id, new_nexthop->nh_node->node_name);
                free_un_nexthop(new_nexthop);
                return -1;
            }

            new_nexthop->protocol = LDP_PROTO;
            new_nexthop->nh.inet3_nh.mpls_label_out[0] = outgoing_ldp_label;
            new_nexthop->nh.inet3_nh.stack_op[0] = SWAP;
            new_nexthop->flags = 0;
            SET_BIT(new_nexthop->flags, PRIMARY_NH);
            SET_BIT(new_nexthop->flags, LDP_TRANSIT_NH);
            /* Backup properties need to be filled by caller
             * depending on whether this tunnel if primary 
             * tunnel or backup tunnel*/
            new_nexthop->lfa_type = NO_LFA;
            new_nexthop->protected_link = NULL;
            new_nexthop->root_metric = 0;
            new_nexthop->dest_metric = 0;
            time(&new_nexthop->last_refresh_time);
            init_glthread(&new_nexthop->glthread);

            /*Now install it in inet.3 table*/
            rc = mpls_0_rt_un_route_install_nexthop(mpls_0_rib, &inet_key, rt_un_entry->level, new_nexthop);
            if(!rc){
                printf("Failed to install LDP transit nexthop in %s on node %s. Tunnel construction aborted\n", 
                        mpls_0_rib->rib_name, next_node->node_name);
                free_un_nexthop(new_nexthop);
                return -1;
            }
            next_node = new_nexthop->nh_node;
        }
        else{
            nexthop = GET_FIRST_NH(rt_un_entry, LDP_TRANSIT_NH, PRIMARY_NH);
            if(!nexthop){
                /*Get the IGP nexthop*/
                rt_un_entry = inet_0_rib->rt_un_route_lookup(inet_0_rib, &inet_key);
                if(!rt_un_entry){
                    printf("Failed to get IGP routing entry on node %s for %s/32. Tunnel construction aborted\n", 
                            next_node->node_name, edgress_lsr_rtr_id);  
                    return -1; 
                }
                nexthop = GET_FIRST_NH(rt_un_entry, IPV4_NH, PRIMARY_NH);
                if(!nexthop){
                    printf("Failed to get IGP nexthop on node %s for %s/32. Tunnel construction aborted\n",
                            next_node->node_name, edgress_lsr_rtr_id);
                    return -1;
                }


                new_nexthop = malloc_un_nexthop();
                memcpy(new_nexthop, nexthop, sizeof(internal_un_nh_t));    
                /*ldpify (transit) new nexthop*/
                if(!new_nexthop->nh_node->ldp_config.is_enabled){
                    printf("IGP nexthop %s is not LDP enabled\n", new_nexthop->nh_node->node_name);
                    free_un_nexthop(new_nexthop);
                    return -1;
                }

                outgoing_ldp_label = get_ldp_label_binding(new_nexthop->nh_node, edgress_lsr_rtr_id, 32);
                if(!outgoing_ldp_label){
                    printf("Could not get LDP label binding for IP %s from node %s. Tunnel construction aborted\n", 
                            edgress_lsr_rtr_id, new_nexthop->nh_node->node_name);
                    free_un_nexthop(new_nexthop);
                    return -1;
                }

                new_nexthop->protocol = LDP_PROTO;
                new_nexthop->nh.inet3_nh.mpls_label_out[0] = outgoing_ldp_label;
                new_nexthop->nh.inet3_nh.stack_op[0] = SWAP;
                new_nexthop->flags = 0;
                SET_BIT(new_nexthop->flags, PRIMARY_NH);
                SET_BIT(new_nexthop->flags, LDP_TRANSIT_NH);
                /* Backup properties need to be filled by caller
                 * depending on whether this tunnel if primary 
                 * tunnel or backup tunnel*/
                new_nexthop->lfa_type = NO_LFA;
                new_nexthop->protected_link = NULL;
                new_nexthop->root_metric = 0;
                new_nexthop->dest_metric = 0;
                time(&new_nexthop->last_refresh_time);
                init_glthread(&new_nexthop->glthread);

                /*Now install it in mpls.0 table*/
                rc = mpls_0_rt_un_route_install_nexthop(mpls_0_rib, &inet_key, rt_un_entry->level, new_nexthop);
                if(!rc){
                    printf("Failed to install LDP transit nexthop in %s. Tunnel construction aborted\n\n", mpls_0_rib->rib_name);
                    free_un_nexthop(new_nexthop);
                    return -1;
                }

                next_node = new_nexthop->nh_node;
            }
            else{
                printf("LDP transit nexthop already exists on node %s for %s/32\n", 
                    next_node->node_name, edgress_lsr_rtr_id);
                outgoing_ldp_label = nexthop->nh.inet3_nh.mpls_label_out[0];
                next_node = nexthop->nh_node;
            }
        }
    }

    /*We are on egress LSR node now*/
    mpls_0_rib = next_node->spf_info.rib[MPLS_0];
    RT_ENTRY_LABEL(&inet_key) = outgoing_ldp_label;
    rt_un_entry = mpls_0_rib->rt_un_route_lookup(mpls_0_rib, &inet_key);

    if(!rt_un_entry){

        new_nexthop = malloc_un_nexthop();  
        new_nexthop->protocol = LDP_PROTO;
        new_nexthop->nh.inet3_nh.mpls_label_out[0] = 0;
        new_nexthop->nh.inet3_nh.stack_op[0] = POP;
        new_nexthop->flags = 0;
        SET_BIT(new_nexthop->flags, PRIMARY_NH);
        SET_BIT(new_nexthop->flags, LDP_TRANSIT_NH);
        /* Backup properties need to be filled by caller
         * depending on whether this tunnel if primary 
         * tunnel or backup tunnel*/
        new_nexthop->lfa_type = NO_LFA;
        new_nexthop->protected_link = NULL;
        new_nexthop->root_metric = 0;
        new_nexthop->dest_metric = 0;
        time(&new_nexthop->last_refresh_time);
        init_glthread(&new_nexthop->glthread);

        /*Now install it in inet.3 table*/
        rc = mpls_0_rt_un_route_install_nexthop(mpls_0_rib, &inet_key, LEVEL1/*dont matter*/, new_nexthop);

        if(!rc){
            printf("Failed to install LDP transit nexthop in %s on node %s\n", 
                    mpls_0_rib->rib_name, next_node->node_name);
            free_un_nexthop(new_nexthop);
            return -1;
        }
        printf("Installed transit LDP nexthop for route %s on node %s\n", edgress_lsr_rtr_id, 
            next_node->node_name);
    }
    else{
        printf("LDP local route already exists on egress lsr node %s for route %s/32\n", 
            next_node->node_name, edgress_lsr_rtr_id);
    }
    return 0;
}
