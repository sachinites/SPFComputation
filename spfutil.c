/*
 * =====================================================================================
 *
 *       Filename:  spfutil.c
 *
 *    Description:  Implementation of spfutil.h
 *
 *        Version:  1.0
 *        Created:  Sunday 27 August 2017 12:49:32  IST
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

#include "spfutil.h"


void
copy_nh_list(node_t *src_nh_list[], node_t *dst_nh_list[]){
    
    unsigned int i = 0;
    for(; i < MAX_NXT_HOPS; i++){
        dst_nh_list[i] = src_nh_list[i];
    }
}

int
is_nh_list_empty(node_t *nh_list[]){

    return (nh_list[0] == NULL);
}

char *
get_str_level(LEVEL level){

    switch(level){
        case LEVEL1:
            return "LEVEL1";
        case LEVEL2:
            return "LEVEL2";
        default:
            if(level == (LEVEL1 | LEVEL2))
                return "LEVEL12";
            else
                return "LEVEL_UNKNOWN";
    }
}

char*
get_str_node_area(AREA area){

    switch(area){
        case AREA1:
            return "AREA1";
        case AREA2:
            return "AREA2";
        case AREA3:
            return "AREA3";
        case AREA4:
            return "AREA4";
        case AREA5:
            return "AREA5";
        case AREA6:
            return "AREA6";
        default:
            return "AREA_UNKNOWN";
    }
}


