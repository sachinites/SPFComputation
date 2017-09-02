/*
 * =====================================================================================
 *
 *       Filename:  lfa.c
 *
 *    Description:  This file implements RFC 5286, LFA - Loop Free Alternates
 *
 *        Version:  1.0
 *        Created:  Sunday 03 September 2017 12:39:30  IST
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

/*
                                                                                 
LFA Link/Link-and-node Protection                                                                  
====================================
                                                                                 
    Inequality 1 : Loop Free Criteria                                            
                    DIST(N,D) < DIST(N,S) + DIST(S,D)  - Means, the LFA N will n t send the traffic back to S in case of primary link failure (S-E link failure)
                                                                                 
    Inequality 2 :  Downstream Path Criteria                                     
                    DIST(N,D) < DIST(S,D)              - Means, Select LFA among nbrs such that, N is a downstream router to Destination D

    Inequality 3 : Node protection Criteria
                    DIST(N,D) < DIST(N,E) + DIST(E,D)  - Means, S can safely re-route traffic to N, since N's path to D do not contain failed Next Hop E.
                                                                                 
           +-----+                      +-----+                                  
           |  S  +----------------------+  N  |                                  
           +--+--+                      +-+---+                                  
              |                           |                        STEPS : To compute LFA, do the following steps
              |                           |                             1. Run SPF on S to know DIST(S,D)
              |                           |                             2. Run SPF on all nbrs of S except primary-NH(S) to know DIST(N,D) and DIST(N,S)       
              |                           |                             3. Filter nbrs of S using inequality 1         
              |                           |                             4. Narrow down the subset further using inequality 2         
              |                           |                             4. Remain is the potential set of LFAs which each surely provides Link protection, but do not guarantee node protection         
              |                           |                             5. We need to investigate this subset of potential LFAs to possibly find one LFA which provide node protection(hence link protection)        
              |                           |                             6. Test the remaining set for inequality 3
              |                           |                                     
              |                           |                             
              |         +------+          |                             
              |         |      |          |                             
              +---------+  E   +----------+                                      
                        |      |                                                 
                        +---+--+                                                 
                            |                                                    
                            |                                                    
                            |                                                    
                            |                                                    
                            |                                                    
                            |                                                    
                        +--++--+                                                 
                        |  D   |                                                 
                        |      |                                                 
                        +------+                                                 
                                                                                 
                                                                                                                                          
*/                                                                                                                                          
                                                                                                                                          
