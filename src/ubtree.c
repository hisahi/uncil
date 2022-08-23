/*******************************************************************************
 
Uncil -- B-tree impl

Copyright (c) 2021-2022 Sampo Hippeläinen (hisahi)

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

#define UNCIL_DEFINES

#include "ubtree.h"
#include "udebug.h"
#include "uerr.h"

#define K UNC_BTREE_K

void unc0_initbtree(Unc_BTree *tree, Unc_Allocator *alloc) {
    tree->root = NULL;
    tree->begin = NULL;
    tree->alloc = alloc;
    ASSERT(K > 2);
}

/* get pointer to record k attached to node n */
#define ATTACHED(n, k) (((Unc_BTreeRecord *)((char *)(n) \
                        + sizeof(Unc_BTreeNode))) + (k))

static Unc_BTreeNode *bplusnewnode(Unc_Allocator *alloc,
                                   Unc_BTreeNode *parent,
                                   int leaf) {
    Unc_BTreeNode *node = unc0_mallocz(alloc, 0, sizeof(Unc_BTreeNode)
                    + (leaf ? (K - 1) * sizeof(Unc_BTreeRecord) : 0));
    if (node) {
        node->parent = parent;
        node->numkeys = 0;
        node->leaf = leaf;
        if (leaf)
            node->child[K - 1] = NULL;
    }
    return node;
}

static int bplusinsertnonleaf(Unc_BTree *tree, Unc_BTreeNode *node,
                              Unc_Size key, Unc_BTreeNode *child);
static int bplusinsertleaf(Unc_BTree *tree, Unc_BTreeNode *node,
                    Unc_Size key, int i, Unc_BTreeRecord **record);

static int bpluscleave(Unc_BTree *tree, Unc_BTreeNode *node1,
                        Unc_Size key, Unc_BTreeRecord **record) {
    Unc_BTreeNode *parent = node1->parent;
    Unc_BTreeNode *node2 = bplusnewnode(tree->alloc, parent, node1->leaf);
    if (!node2) return UNCIL_ERR_MEM;

    ASSERT(node1->numkeys == K - 1);
    /* cleave full leaf (parent) */
    {
        int b = (K - 1) / 2, j, to2 = key > node1->keys[b];
        if (to2) ++b;
        /* replace reference in parent */
        if (parent) {
            ASSERT(!parent->leaf);
            for (j = 0; j <= parent->numkeys; ++j)
                if (parent->child[j] == node1)
                    parent->child[j] = node2;
        } else {
            /* create new parent */
            parent = bplusnewnode(tree->alloc, NULL, 0);
            if (!parent) return UNCIL_ERR_MEM;
            tree->root = node1->parent = parent;
            parent->child[0] = node2;
        }
        node2->parent = parent;
        if (node1->leaf) { /* leaf */
            node2->child[K - 1] = node1->child[K - 1];
            node1->child[K - 1] = node2;
            for (j = b; j < K - 1; ++j) {
                Unc_BTreeRecord *r = ATTACHED(node2, j - b);
                node2->keys[j - b] = node1->keys[j];
                /* copy attached record */
                *(Unc_BTreeRecord *)(node2->child[j - b] = r) = 
                    *(Unc_BTreeRecord *)(node1->child[j]);
            }
            node2->numkeys = K - 1 - b;
            node1->numkeys = b;
            bplusinsertleaf(tree, to2 ? node2 : node1, key, -1, record);
        } else {
            node2->numkeys = K - 1 - b;
            for (j = b; j < K - 1; ++j) {
                node2->child[j - b] = node1->child[j];
                node2->keys[j - b] = node1->keys[j];
            }
            node2->child[node2->numkeys] = node1->child[K - 1];
            node1->numkeys = b;
        }
        bplusinsertnonleaf(tree, parent, node1->keys[node1->numkeys - 1],
                                            node1);
    }
    
    return 0;
}

static int bplusinsertnonleaf(Unc_BTree *tree, Unc_BTreeNode *node,
                              Unc_Size key, Unc_BTreeNode *child) {
    ASSERT(node && !node->leaf);
    if (node && node->numkeys < K - 1) {
        int nk = node->numkeys, i = 0, j;
        while (i < node->numkeys && node->keys[i] < key)
            ++i;
        for (j = nk; j > i; --j) {
            node->keys[j] = node->keys[j - 1];
            node->child[j + 1] = node->child[j];
        }
        node->keys[i] = key;
        node->child[i + 1] = node->child[i];
        node->child[i] = child;
        ++node->numkeys;
        return 0;
    }
    return bpluscleave(tree, node, key, NULL);
}

static int bplusinsertleaf(Unc_BTree *tree, Unc_BTreeNode *node,
                    Unc_Size key, int i, Unc_BTreeRecord **record) {
    if (!node) {
        /* no root yet */
        Unc_BTreeNode *root = bplusnewnode(tree->alloc, NULL, 1);
        if (!root) return UNCIL_ERR_MEM;
        tree->begin = tree->root = node = root;
    }
    if (node && node->leaf && node->numkeys < K - 1) {
        int nk = node->numkeys, j;
        Unc_BTreeRecord *rec = ATTACHED(node, nk);
        if (i < 0) {
            i = 0;
            while (i < node->numkeys && node->keys[i] < key)
                ++i;
        }
        for (j = nk; j > i; --j) {
            node->keys[j] = node->keys[j - 1];
            node->child[j] = node->child[j - 1];
        }
        node->keys[i] = key;
        node->child[i] = rec;
        *record = rec;
        ++node->numkeys;
        return 0;
    }
    return bpluscleave(tree, node, key, record);
}

static int bplussearch(Unc_BTree *tree, Unc_Size key, int *created,
                  Unc_BTreeRecord **record) {
    Unc_BTreeNode *node = tree->root;
    int k = 0;
    while (node) {
        k = 0;
        while (k < node->numkeys && node->keys[k] < key)
            ++k;
        if (node->leaf) {
            if (k < node->numkeys && node->keys[k] == key) {
                *created = 0;
                *record = node->child[k];
                return 0;
            }
            break;
        }
        node = node->child[k];
    }
    ASSERT(tree->root == NULL || node != NULL);

    if (!*created) {
        *record = NULL;
        return 0;
    }
    *created = 1;
    return bplusinsertleaf(tree, node, key, k, record);
}

Unc_BTreeRecord *unc0_getbtree(Unc_BTree *tree, Unc_Size key) {
    int n = 0;
    Unc_BTreeRecord *record;
    if (bplussearch(tree, key, &n, &record))
        return NULL;
    return record;
}

int unc0_putbtree(Unc_BTree *tree, Unc_Size key, int *created,
                  Unc_BTreeRecord **record) {
    int e;
    ASSERT(record != NULL);
    *created = 1;
    e = bplussearch(tree, key, created, record);
    return e;
}

/* you may not put values into a tree you are iterating over! */
int unc0_iterbtreerecords(Unc_BTree *tree, 
                int (*iter)(Unc_Size key, Unc_BTreeRecord *value, void *udata),
                void *udata) {
    Unc_BTreeNode *node = tree->begin;
    int k, e;
    while (node) {
        ASSERT(node->leaf);
        k = 0;
        while (k < node->numkeys) {
            if ((e = iter(node->keys[k], node->child[k], udata)))
                return e;
            ++k;
        }
        node = node->child[K - 1];
    }
    return 0;
}

static void dropnode(Unc_Allocator *alloc, Unc_BTreeNode *node) {
    if (!node) 
        return;
    else if (node->leaf) {
        unc0_mfree(alloc, node, sizeof(Unc_BTreeNode)
                    + (K - 1) * sizeof(Unc_BTreeRecord));
    } else {
        int k, n = node->numkeys;
        for (k = 0; k < n; ++k)
            dropnode(alloc, node->child[k]);
        unc0_mfree(alloc, node, sizeof(Unc_BTreeNode));
    }
}

void unc0_dropbtree(Unc_BTree *tree) {
    dropnode(tree->alloc, tree->root);
}
