/**
 * @file
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
#ifndef NODE_STACK_H
#define NODE_STACK_H

#include "ast.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    Node **items;
    size_t size;
    size_t capacity;
} NodeStack;

int nodeStackInit(NodeStack *stack);
int nodeStackPush(NodeStack *stack, Node *node);
int nodeStackPop(NodeStack *stack, Node **result);
int nodeStackPeek(const NodeStack *stack, Node **result);
bool nodeStackIsEmpty(NodeStack *stack);
void nodeStackDestroy(NodeStack *stack);

#endif
