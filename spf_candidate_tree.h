/*
 * =====================================================================================
 *
 *       Filename:  spf_candidate_tree.h
 *
 *    Description:  Front end APIs over generic candidate_tree.h
 *
 *        Version:  1.0
 *        Created:  Monday 21 May 2018 11:41:17  IST
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

#ifndef __SPF_CANDIDATE_TREE__
#define __SPF_CANDIDATE_TREE__

#include "instance.h"
#include "Tree/candidate_tree.h"

/*import from level.c*/
extern LEVEL glevel;

static int
spf_candidate_tree_compare_fn(void *_node1, void *_node2){

    node_t *node1 = (node_t *)_node1;
    node_t *node2 = (node_t *)_node2;
    assert(glevel == LEVEL1 || glevel == LEVEL2);

    if(node1->spf_metric[glevel] < node2->spf_metric[glevel])
        return -1;
    if(node1->spf_metric[glevel] > node2->spf_metric[glevel])
        return 1;
    if(node1->node_type[glevel] == PSEUDONODE &&
            node2->node_type[glevel] != PSEUDONODE)
        return -1;
    if(node1->node_type[glevel] != PSEUDONODE &&
            node2->node_type[glevel] == PSEUDONODE)
        return 1;
    return 0;
}

#define SPF_CANDIDATE_TREE_INIT(ctreeptr)       \
    (CANDIDATE_TREE_INIT(ctreeptr, (rboffset(node_t, candiate_tree_node)), TRUE));  \
    register_rbtree_compare_fn(ctreeptr, (_redblack_compare_func)spf_candidate_tree_compare_fn)

#define SPF_RE_INIT_CANDIDATE_TREE(ctreeptr)    \
    RE_INIT_CANDIDATE_TREE(ctreeptr)

#define SPF_IS_CANDIDATE_TREE_EMPTY(ctreeptr)   \
    IS_CANDIDATE_TREE_EMPTY(ctreeptr)

RBNODE_TO_STRUCT(rbnode_to_spf_node, node_t, candiate_tree_node);

#define SPF_INSERT_NODE_INTO_CANDIDATE_TREE(ctreeptr, nodeptr, _level)       \
    glevel = _level;                                                         \
    INSERT_NODE_INTO_CANDIDATE_TREE(ctreeptr, (&nodeptr->candiate_tree_node))

static inline node_t *
SPF_GET_CANDIDATE_TREE_TOP(candidate_tree_t *ctreeptr){

    rbnode *_rbnode = GET_CANDIDATE_TREE_TOP(ctreeptr);
    if(!_rbnode) return NULL;
    return rbnode_to_spf_node(_rbnode);
}

#define SPF_CANDIDATE_TREE_NODE_INIT(ctreeptr, nodeptr)                     \
    CANDIDATE_TREE_NODE_INIT(ctreeptr, &nodeptr->candiate_tree_node)

#define SPF_REMOVE_CANDIDATE_TREE_TOP(ctreeptr)                             \
    REMOVE_CANDIDATE_TREE_TOP(ctreeptr)

#define SPF_DESTROY_CANDIDATE_TREE(ctreeptr)                                \
    FREE_CANDIDATE_TREE_INTERNALS(ctreeptr)

#define SPF_CANDIDATE_TREE_NODE_REFRESH(ctreeptr, nodeptr, _level)  \
    glevel = level;                                                 \
    CANDIDATE_TREE_NODE_REFRESH(ctreeptr, &nodeptr->candiate_tree_node)
#endif /* __SPF_CANDIDATE_TREE__ */
