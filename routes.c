/*
 * =====================================================================================
 *
 *       Filename:  routes.c
 *
 *    Description:  This file implements the construction of routing table building
 *
 *        Version:  1.0
 *        Created:  Monday 04 September 2017 12:05:40  IST
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

#include "routes.h"
#include "spfutil.h"
#include "logging.h"
#include "bitsop.h"

extern instance_t *instance;
extern void
spf_computation(node_t *spf_root,
                spf_info_t *spf_info,
                LEVEL level);

/*Routine to build the routing table*/

static void
install_default_route(spf_info_t *spf_info,
                      LEVEL level){


}


void
spf_postprocessing(spf_info_t *spf_info, /* routes are stored globally*/
                   node_t *spf_root,     /* computing node which stores the result (list) of spf run*/
                   LEVEL level){         /*Level of spf run*/

    
    /*-----------------------------------------------------------------------------
     *  If this is L2 run, then set my spf_info_t->spff_multi_area bit, and schedule
     *  SPF L1 run to ensure L1 routes are uptodate before updating L2 routes
     *-----------------------------------------------------------------------------*/
    sprintf(LOG, "Entered ... "); TRACE();
       
    if(level == LEVEL2 && spf_info->spf_level_info[LEVEL1].version){
        /*If at least 1 SPF L1 run has been triggered*/
        
        spf_determine_multi_area_attachment(spf_info, spf_root);  
        /*Schedule level 1 spf run, just to make sure L1 routes are up
         *      * to date before building L2 routes*/
        sprintf(LOG, "L2 spf run, triggering L1 SPF run first before building L2 routes"); TRACE();
        spf_computation(spf_root, spf_info, LEVEL1);
    }

    build_routing_table(spf_info, spf_root, level);

    start_route_installation(spf_info, level);
}


void
build_routing_table(spf_info_t *spf_info,
                    node_t *spf_root, LEVEL level){

    singly_ll_node_t *list_node = NULL;
    spf_result_t *result = NULL;


    sprintf(LOG, "Entered ... spf_root : %s, Level : %u", spf_root->node_name, level); TRACE();
    
    /*Walk over the SPF result list computed in spf run
     * in the same order. Note that order of this list is :
     * most distant router from spf root is first*/

    ITERATE_LIST(spf_root->spf_run_result[level], list_node){
        
         result = (spf_result_t *)list_node->data;
       
        /*If computing router is a pure L1 router, and if computing
         * router is connected to L1L2 router with in its own area, then
         * computing router should install the default gw in RIB*/
         
        if(level == LEVEL1                                         &&       /*The current run us LEVEL1*/
           !IS_BIT_SET(spf_root->instance_flags, IGNOREATTACHED)   &&       /*By default, router should process Attached Bit*/
           spf_info->spff_multi_area == 0                          &&       /*computing router L1-only router. For L1-only router this bit is never set*/
           result->node != NULL                                    &&       /*first fragment of the node whose result is being processed exist*/
           result->node->attached){                                         /*Router neing inspected is L1L2 router*/

                install_default_route(spf_info, level);
        }
    }
}

void
start_route_installation(spf_info_t *spf_info,
                         LEVEL level){

    sprintf(LOG, "Entered ... Level : %u", level); TRACE();

}
