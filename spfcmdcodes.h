/*
 * =====================================================================================
 *
 *       Filename:  spfdcm.h
 *
 *    Description:  This file decaleres the structures and conmmand code constants for spf clis
 *
 *        Version:  1.0
 *        Created:  Sunday 03 September 2017 10:32:31  IST
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

#ifndef __SPFCMDCODES__H
#define __SPFCMDCODES__H

/*CLI command codes*/

#define CMDCODE_NODE_SLOT_ENABLE    1   /*config node <node-name> [no] slot <slot-name> enable*/
#define CMDCODE_INSTANCE_IGNOREBIT_ENABLE   2   /*config node <node-name> [no] ignorebit enable*/
#define CMDCODE_INSTANCE_ATTACHBIT_ENABLE   3   /*config node <node-name [no] attachbit enable>*/

#endif /* __SPFCMDCODES__H */
