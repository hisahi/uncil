/*******************************************************************************
 
Uncil -- B+ tree header

Copyright (c) 2021 Sampo Hippeläinen (hisahi)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*******************************************************************************/

#ifndef UNCIL_UBTREE_H
#define UNCIL_UBTREE_H

#include "udef.h"
#include "umem.h"

#define UNC_BTREE_K 8

typedef struct Unc_BTreeRecord {
    Unc_Size first, second;
} Unc_BTreeRecord;

typedef struct Unc_BTreeNode {
    void *child[UNC_BTREE_K];
    struct Unc_BTreeNode *parent;
    Unc_Size keys[UNC_BTREE_K - 1];
    int numkeys;
    int leaf;
} Unc_BTreeNode;

typedef struct Unc_BTree {
    Unc_BTreeNode *root;
    Unc_BTreeNode *begin;
    Unc_Allocator *alloc;
} Unc_BTree;

void unc__initbtree(Unc_BTree *tree, Unc_Allocator *alloc);
Unc_BTreeRecord *unc__getbtree(Unc_BTree *tree, Unc_Size key);
int unc__putbtree(Unc_BTree *tree, Unc_Size key, int *created,
                  Unc_BTreeRecord **record);
/* you may not put values into a tree you are iterating over! */
int unc__iterbtreerecords(Unc_BTree *tree, 
                int (*iter)(Unc_Size key, Unc_BTreeRecord *value, void *udata),
                void *udata);
void unc__dropbtree(Unc_BTree *tree);

#endif /* UNCIL_UBTREE_H */
