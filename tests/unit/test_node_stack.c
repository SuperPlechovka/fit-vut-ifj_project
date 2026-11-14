/**
 * @file
 * @brief Unit testy zásobníku uzlů AST (NodeStack).
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: převeď starý test src/parser/tests/node_stack_test.c do tests/unit/
 * podle aktuálního src/node_stack.h. Ověř:
 * - prázdný zásobník - pop a peek selžou;
 * - LIFO pořadí a peek, který nemění velikost;
 * - pop vrátí uzel a vynuluje jeho slot (destroy ho pak neuvolní);
 * - růst kapacity při překročení počáteční velikosti;
 * - destroy uvolní uzly, které na zásobníku zůstaly (ověřitelné ASan (kontrola práce s pamětí)).
 * Model: Claude Opus 5.5, Claude Code.
 */
#include "../../src/node_stack.h"

#include <assert.h>
#include <stdlib.h>

static void test_empty_stack(void) {
    NodeStack stack;
    Node *value = NULL;

    assert(nodeStackInit(&stack) == EXIT_SUCCESS);
    assert(stack.size == 0);
    assert(nodeStackIsEmpty(&stack));
    assert(nodeStackPop(&stack, &value) != EXIT_SUCCESS);
    assert(nodeStackPeek(&stack, &value) != EXIT_SUCCESS);
    nodeStackDestroy(&stack);
}

static void test_lifo_and_peek(void) {
    NodeStack stack;
    Node *first = nodeLitInt(1);
    Node *second = nodeLitInt(2);
    Node *value = NULL;

    assert(first != NULL && second != NULL);
    assert(nodeStackInit(&stack) == EXIT_SUCCESS);
    assert(nodeStackPush(&stack, first) == EXIT_SUCCESS);
    assert(nodeStackPush(&stack, second) == EXIT_SUCCESS);
    assert(stack.size == 2);

    assert(nodeStackPeek(&stack, &value) == EXIT_SUCCESS);
    assert(value == second);
    assert(stack.size == 2);

    assert(nodeStackPop(&stack, &value) == EXIT_SUCCESS);
    assert(value == second);
    assert(stack.items[1] == NULL);
    nodeFree(value);

    assert(nodeStackPop(&stack, &value) == EXIT_SUCCESS);
    assert(value == first);
    nodeFree(value);

    assert(nodeStackIsEmpty(&stack));
    nodeStackDestroy(&stack);
}

static void test_growth_and_destroy_frees_rest(void) {
    NodeStack stack;

    assert(nodeStackInit(&stack) == EXIT_SUCCESS);
    size_t initial_capacity = stack.capacity;

    for (size_t i = 0; i < initial_capacity + 1; i++) {
        Node *node = nodeLitInt((long long)i);
        assert(node != NULL);
        assert(nodeStackPush(&stack, node) == EXIT_SUCCESS);
    }
    assert(stack.size == initial_capacity + 1);
    assert(stack.capacity > initial_capacity);

    Node *top = NULL;
    assert(nodeStackPeek(&stack, &top) == EXIT_SUCCESS);
    assert(top->ival == (long long)initial_capacity);

    /* zbylé uzly uvolní destroy - pod ASan se případný únik projeví */
    nodeStackDestroy(&stack);
}

int main(void) {
    test_empty_stack();
    test_lifo_and_peek();
    test_growth_and_destroy_frees_rest();
    return 0;
}
