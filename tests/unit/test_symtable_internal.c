/**
 * @file
 * @brief Kontrola AVL invariantů a úklidu paměti při selhání alokací.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 */
/* AI prompt: Oveř uspořádání klíčů, uložené výšky a AVL vyváženost po vložení
 * a odstranění, rotace LL/RR/LR/RL, selhání jednotlivých alokací, zachování dat
 * při neúspěšné aktualizaci a úklid paměti při mazání a vyprázdnění tabulky.
 * Model: GPT-6, Codex.
 */
#include "../../src/symtable.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail_after = -1;
static size_t live_allocations = 0;

static void *test_malloc(size_t size) {
    if (fail_after == 0) {
        return NULL;
    }
    if (fail_after > 0) {
        fail_after--;
    }
    void *ptr = malloc(size);
    if (ptr != NULL) {
        live_allocations++;
    }
    return ptr;
}

static void test_free(void *ptr) {
    if (ptr != NULL) {
        assert(live_allocations > 0);
        live_allocations--;
    }
    free(ptr);
}

#define symtable_create checked_symtable_create
#define symtable_clear checked_symtable_clear
#define symtable_destroy checked_symtable_destroy
#define symtable_insert checked_symtable_insert
#define symtable_find checked_symtable_find
#define symtable_update checked_symtable_update
#define symtable_remove checked_symtable_remove
#define malloc test_malloc
#define free test_free
#include "../../src/symtable.c"
#undef malloc
#undef free

static int check_tree(const SymtableNode *node, const SymtableNode *lower,
                      const SymtableNode *upper) {
    if (node == NULL) {
        return 0;
    }
    if (lower != NULL) {
        int comparison = strcmp(node->name, lower->name);
        assert(comparison > 0 ||
               (comparison == 0 && node->data.parameter_count > lower->data.parameter_count));
    }
    if (upper != NULL) {
        int comparison = strcmp(node->name, upper->name);
        assert(comparison < 0 ||
               (comparison == 0 && node->data.parameter_count < upper->data.parameter_count));
    }
    int left = check_tree(node->left, lower, node);
    int right = check_tree(node->right, node, upper);
    assert(abs(left - right) <= 1);
    int height = 1 + (left > right ? left : right);
    assert(node->height == height);
    return height;
}

static void test_avl_invariants(void) {
    const char *orders[][3] = {{"c", "b", "a"}, {"a", "b", "c"}, {"c", "a", "b"}, {"a", "c", "b"}};
    SymbolData data = {.kind = SYMBOL_VAR, .type = SYMBOL_TYPE_INT};
    for (size_t i = 0; i < sizeof(orders) / sizeof(orders[0]); i++) {
        Symtable *table = symtable_create();
        assert(table != NULL);
        for (size_t j = 0; j < 3; j++) {
            assert(symtable_insert(table, orders[i][j], &data) == SYMTABLE_OK);
            check_tree(table->root, NULL, NULL);
        }
        assert(strcmp(table->root->name, "b") == 0);
        symtable_destroy(table);
    }

    for (int order = 0; order < 3; order++) {
        Symtable *table = symtable_create();
        assert(table != NULL);
        char name[32];
        for (int i = 0; i < 1024; i++) {
            int key = order == 0 ? i : (order == 1 ? 1023 - i : (i * 197) % 1024);
            snprintf(name, sizeof(name), "x%04d", key);
            assert(symtable_insert(table, name, &data) == SYMTABLE_OK);
            check_tree(table->root, NULL, NULL);
        }
        for (int i = 0; i < 1024; i++) {
            int key = order == 0 ? 1023 - i : (order == 1 ? i : (i * 389) % 1024);
            snprintf(name, sizeof(name), "x%04d", key);
            assert(symtable_remove(table, name, 0) == SYMTABLE_OK);
            check_tree(table->root, NULL, NULL);
        }
        assert(table->root == NULL);
        symtable_destroy(table);
    }
    assert(live_allocations == 0);
}

static void test_allocation_failures(void) {
    fail_after = 0;
    assert(symtable_create() == NULL);
    assert(live_allocations == 0);
    fail_after = -1;

    Symtable *table = symtable_create();
    assert(table != NULL);
    SymbolType types[] = {SYMBOL_TYPE_ANY};
    SymbolData data = {.kind = SYMBOL_FUNCTION,
                       .type = SYMBOL_TYPE_UNIT,
                       .parameter_count = 1,
                       .parameter_types = types};
    assert(symtable_insert(table, "m", &data) == SYMTABLE_OK);
    SymtableNode *original_root = table->root;
    size_t before = live_allocations;

    const char *names[] = {"a", "z"};
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        for (int allocation = 0; allocation < 3; allocation++) {
            fail_after = allocation;
            assert(symtable_insert(table, names[i], &data) == SYMTABLE_INTERNAL_ERROR);
            assert(symtable_find(table, names[i], 1) == NULL);
            assert(table->root == original_root);
            assert(live_allocations == before);
            check_tree(table->root, NULL, NULL);
        }
    }
    fail_after = -1;
    assert(symtable_insert(table, "a", &data) == SYMTABLE_OK);
    assert(symtable_insert(table, "z", &data) == SYMTABLE_OK);
    before = live_allocations;

    const SymbolData *original = symtable_find(table, "a", 1);
    const SymbolType *original_types = original->parameter_types;
    SymbolType new_types[] = {SYMBOL_TYPE_INT};
    SymbolData updated = data;
    updated.type = SYMBOL_TYPE_STRING;
    updated.parameter_types = new_types;
    updated.defined = 1;
    fail_after = 0;
    assert(symtable_update(table, "a", &updated) == SYMTABLE_INTERNAL_ERROR);
    const SymbolData *found = symtable_find(table, "a", 1);
    assert(found == original && found->parameter_types == original_types);
    assert(found->type == SYMBOL_TYPE_UNIT && !found->defined);
    assert(found->parameter_types[0] == SYMBOL_TYPE_ANY);
    assert(symtable_update(table, "a", found) == SYMTABLE_INTERNAL_ERROR);
    assert(live_allocations == before);
    check_tree(table->root, NULL, NULL);

    assert(symtable_remove(table, "m", 1) == SYMTABLE_OK);
    check_tree(table->root, NULL, NULL);
    symtable_clear(table);
    assert(table->root == NULL);
    assert(live_allocations == 1);
    symtable_clear(table);
    symtable_destroy(table);
    fail_after = -1;
    assert(live_allocations == 0);
}

int main(void) {
    test_avl_invariants();
    test_allocation_failures();
    return 0;
}
