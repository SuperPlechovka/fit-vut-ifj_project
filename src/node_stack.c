/**
 * @file
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
#include "ast.h"
#include <node_stack.h>
#include <stdlib.h>

#define INIT_STACK_CAPACITY 128

int nodeStackInit(NodeStack *stack) {
    Node **arr = malloc(sizeof(*arr) * INIT_STACK_CAPACITY);
    if (arr == NULL) {
        return EXIT_FAILURE;
    }

    stack->items = arr;
    stack->capacity = INIT_STACK_CAPACITY;
    stack->size = 0;

    return EXIT_SUCCESS;
}

int nodeStackPush(NodeStack *stack, Node *node) {
    if (stack->size >= stack->capacity) {
        size_t new_capacity = stack->capacity * 2;
        void *result = realloc(stack->items, new_capacity * sizeof *stack->items);
        if (result == NULL) {
            return EXIT_FAILURE;
        }
        stack->items = result;
        stack->capacity = new_capacity;
    }

    if (stack->size < stack->capacity) {
        stack->items[stack->size] = node;
        stack->size++;
    }
    return EXIT_SUCCESS;
}

int nodeStackPop(NodeStack *stack, Node **result) {
    if (stack == NULL || result == NULL || stack->size == 0) {
        return EXIT_FAILURE;
    }

    stack->size--;
    *result = stack->items[stack->size];

    stack->items[stack->size] = NULL;
    return EXIT_SUCCESS;
}

int nodeStackPeek(const NodeStack *stack, Node **result) {
    if (stack == NULL || result == NULL || stack->size == 0) {
        return EXIT_FAILURE;
    }

    *result = stack->items[stack->size - 1];
    return EXIT_SUCCESS;
}

bool nodeStackIsEmpty(const NodeStack *stack) { return stack->size == 0; }

void nodeStackDestroy(NodeStack *stack) {
    for (size_t i = 0; i < stack->size; i++) {
        nodeFree(stack->items[i]);
    }

    free(stack->items);
}
