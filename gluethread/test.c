/*
 * =====================================================================================
 *
 *       Filename:  test.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  Monday 12 March 2018 02:15:28  IST
 *       Revision:  1.0
 *       Compiler:  gcc
 *
 *         Author:  Er. Abhishek Sagar, Networking Developer (AS), sachinites@gmail.com
 *        Company:  Brocade Communications(Jul 2012- Mar 2016), Current : Juniper Networks(Apr 2017 - Present)
 *        
 *        This file is part of the XXX distribution (https://github.com/sachinites).
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

#include "glthread.h"
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct _person{

    int age;
    int weight;
    glthread_t glthread;
} person_t ;

THREAD_TO_STRUCT(thread_to_person, person_t, glthread, glthreadptr);

int main(int argc, char **argv){

    person_t person[5];
    memset(person, 0, sizeof(person_t) * 5);
    person[0].age = 1;
    person[0].weight = 2;
    person[1].age = 3;
    person[1].weight = 4;
    person[2].age = 5;
    person[2].weight = 6;
    person[3].age = 7;
    person[3].weight = 8;
    person[4].age = 9;
    person[4].weight = 10;

    glthread_add_next(&person[0].glthread, &person[1].glthread);
    glthread_add_last(&person[0].glthread, &person[2].glthread);
    glthread_add_last(&person[0].glthread, &person[3].glthread);
    glthread_add_last(&person[0].glthread, &person[4].glthread);

    glthread_t *curr = NULL, *prev = NULL;
    person_t *per = NULL;

#if 0
    ITERATE_THREAD_BEGIN(&person[0].glthread, curr){
        per = thread_to_person(curr);
        if(per->age == 3)
            remove_glthread(curr);
    } ITERATE_THREAD_END(&person[0].glthread, curr);
#endif

    ITERATE_THREAD_BEGIN(&person[0].glthread, curr){
        per = thread_to_person(curr);
        printf("Age = %d\n", per->age);
        printf("Weight = %d\n", per->weight);
    } ITERATE_THREAD_END(&person[0].glthread, curr);

    delete_thread_list(&person[0].glthread);

    int i = 0;
    for(; i < 5 ; i++){
        printf("%x %x\n", person[i].glthread.left, person[i].glthread.right);
    }
    return 0;
}
