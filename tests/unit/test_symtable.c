/**
 * @file
 * @brief Ověření tabulky symbolů a parametrů typu ANY.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 */
/* AI prompt: Vytvoř unit testy tabulky symbolů. Ověř ANY u print a toStr,
 * vlastní kopie jmen a parametrů, duplicity, samostatné rozsahy, hledání podle arity,
 * aktualizaci včetně použití vlastních dat, nezávislost kopie parametrů po aktualizaci,
 * neplatné vstupy a zachování dat při odmítnutí operace, metadata všech druhů symbolů,
 * vložení seřazených klíčů v obou směrech, zachování záznamů při rotacích LL/RR/LR/RL,
 * mazání podle arity, zachování ostatních záznamů a opakované vyprázdnění tabulky.
 * Model: GPT-6, Codex.
 */
#include "../../src/symtable.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_builtin_parameters(void) {
    Symtable *table = symtable_create();
    assert(table != NULL);
    SymbolType types[] = {SYMBOL_TYPE_ANY};
    SymbolData data = {.kind = SYMBOL_FUNCTION,
                       .type = SYMBOL_TYPE_UNIT,
                       .parameter_count = 1,
                       .parameter_types = types,
                       .defined = 1,
                       .builtin = 1};
    char name[] = "print";
    assert(symtable_insert(table, name, &data) == SYMTABLE_OK);
    data.type = SYMBOL_TYPE_STRING;
    assert(symtable_insert(table, "toStr", &data) == SYMTABLE_OK);
    name[0] = 'x';
    types[0] = SYMBOL_TYPE_INT;

    const SymbolData *found = symtable_find(table, "print", 1);
    assert(found != NULL);
    assert(found->type == SYMBOL_TYPE_UNIT);
    assert(found->parameter_types[0] == SYMBOL_TYPE_ANY);
    assert(found->parameter_types != types);
    assert(found->defined && found->builtin);
    assert(symtable_find(table, "print", 0) == NULL);
    assert(symtable_find(table, "print", 2) == NULL);
    assert(symtable_find(table, "toStr", 1)->type == SYMBOL_TYPE_STRING);
    assert(symtable_insert(table, "print", &data) == SYMTABLE_DUPLICATE);
    symtable_destroy(table);
}

static void test_updates(void) {
    Symtable *table = symtable_create();
    assert(table != NULL);
    SymbolType types[] = {SYMBOL_TYPE_UNKNOWN};
    SymbolData data = {.kind = SYMBOL_FUNCTION,
                       .type = SYMBOL_TYPE_UNKNOWN,
                       .parameter_count = 1,
                       .parameter_types = types};
    assert(symtable_insert(table, "f", &data) == SYMTABLE_OK);
    types[0] = SYMBOL_TYPE_ANY;
    data.type = SYMBOL_TYPE_UNIT;
    assert(symtable_update(table, "f", &data) == SYMTABLE_OK);
    const SymbolData *found = symtable_find(table, "f", 1);
    assert(symtable_update(table, "f", found) == SYMTABLE_OK);
    assert(symtable_find(table, "f", 1)->parameter_types[0] == SYMBOL_TYPE_ANY);

    types[0] = (SymbolType)(SYMBOL_TYPE_ANY + 1);
    assert(symtable_update(table, "f", &data) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_find(table, "f", 1)->parameter_types[0] == SYMBOL_TYPE_ANY);
    assert(symtable_insert(table, "invalid", &data) == SYMTABLE_INTERNAL_ERROR);
    data.type = (SymbolType)(SYMBOL_TYPE_ANY + 1);
    assert(symtable_insert(table, "invalid", &data) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_find(table, "invalid", 1) == NULL);
    symtable_destroy(table);
}

static void test_update_copies(void) {
    Symtable *table = symtable_create();
    assert(table != NULL);
    SymbolType original_types[] = {SYMBOL_TYPE_INT, SYMBOL_TYPE_STRING};
    SymbolData data = {.kind = SYMBOL_FUNCTION,
                       .type = SYMBOL_TYPE_UNKNOWN,
                       .parameter_count = 2,
                       .parameter_types = original_types};
    assert(symtable_insert(table, "f", &data) == SYMTABLE_OK);

    SymbolType new_types[] = {SYMBOL_TYPE_DOUBLE, SYMBOL_TYPE_ANY};
    data.type = SYMBOL_TYPE_UNIT;
    data.parameter_types = new_types;
    data.defined = 1;
    data.builtin = 1;
    assert(symtable_update(table, "f", &data) == SYMTABLE_OK);
    new_types[0] = SYMBOL_TYPE_STRING;
    new_types[1] = SYMBOL_TYPE_INT;

    const SymbolData *found = symtable_find(table, "f", 2);
    assert(found != NULL && found->type == SYMBOL_TYPE_UNIT);
    assert(found->defined && found->builtin);
    assert(found->parameter_types != new_types);
    assert(found->parameter_types[0] == SYMBOL_TYPE_DOUBLE);
    assert(found->parameter_types[1] == SYMBOL_TYPE_ANY);

    data.parameter_count = 1;
    assert(symtable_update(table, "f", &data) == SYMTABLE_NOT_FOUND);
    assert(symtable_find(table, "f", 1) == NULL);
    found = symtable_find(table, "f", 2);
    assert(found != NULL && found->parameter_count == 2);
    assert(found->parameter_types[0] == SYMBOL_TYPE_DOUBLE);
    symtable_destroy(table);
}

static void test_scopes(void) {
    Symtable *outer = symtable_create();
    Symtable *inner = symtable_create();
    assert(outer != NULL && inner != NULL);
    SymbolData data = {.kind = SYMBOL_PARAMETER, .type = SYMBOL_TYPE_INT};
    assert(symtable_insert(outer, "x", &data) == SYMTABLE_OK);
    data.kind = SYMBOL_VAR;
    data.type = SYMBOL_TYPE_STRING;
    assert(symtable_insert(inner, "x", &data) == SYMTABLE_OK);
    assert(symtable_find(inner, "x", 0)->type == SYMBOL_TYPE_STRING);
    symtable_destroy(inner);
    assert(symtable_find(outer, "x", 0)->kind == SYMBOL_PARAMETER);
    assert(symtable_find(outer, "x", 0)->type == SYMBOL_TYPE_INT);
    symtable_destroy(outer);
    symtable_destroy(NULL);
}

static void test_ordered_keys(void) {
    SymbolData data = {.kind = SYMBOL_VAL, .type = SYMBOL_TYPE_INT};
    for (int reverse = 0; reverse < 2; reverse++) {
        Symtable *table = symtable_create();
        assert(table != NULL);
        char name[32];
        for (int i = 0; i < 1024; i++) {
            snprintf(name, sizeof(name), "x%04d", reverse ? 1023 - i : i);
            assert(symtable_insert(table, name, &data) == SYMTABLE_OK);
        }
        for (int i = 0; i < 1024; i++) {
            snprintf(name, sizeof(name), "x%04d", i);
            const SymbolData *found = symtable_find(table, name, 0);
            assert(found != NULL && found->type == SYMBOL_TYPE_INT);
        }
        symtable_destroy(table);
    }
}

static void test_rotations(void) {
    const char *orders[][3] = {{"c", "b", "a"}, {"a", "b", "c"}, {"c", "a", "b"}, {"a", "c", "b"}};
    for (size_t i = 0; i < sizeof(orders) / sizeof(orders[0]); i++) {
        Symtable *table = symtable_create();
        assert(table != NULL);
        for (size_t j = 0; j < 3; j++) {
            SymbolData data = {.kind = SYMBOL_VAR, .type = (SymbolType)(SYMBOL_TYPE_INT + j)};
            assert(symtable_insert(table, orders[i][j], &data) == SYMTABLE_OK);
        }
        for (size_t j = 0; j < 3; j++) {
            const SymbolData *data = symtable_find(table, orders[i][j], 0);
            assert(data != NULL);
            assert(data->type == (SymbolType)(SYMBOL_TYPE_INT + j));
        }
        symtable_destroy(table);
    }
}

static void test_symbol_data(void) {
    Symtable *table = symtable_create();
    assert(table != NULL);
    SymbolData data = {
        .kind = SYMBOL_FUNCTION, .type = SYMBOL_TYPE_UNKNOWN, .defined = 1, .entry_point = 1};
    assert(symtable_insert(table, "run", &data) == SYMTABLE_OK);
    data.type = SYMBOL_TYPE_UNIT;
    assert(symtable_update(table, "run", &data) == SYMTABLE_OK);
    const SymbolData *found = symtable_find(table, "run", 0);
    assert(found->type == SYMBOL_TYPE_UNIT && found->defined && found->entry_point);
    assert(found->parameter_count == 0 && found->parameter_types == NULL);
    data.type = SYMBOL_TYPE_INT;
    assert(symtable_insert(table, "run", &data) == SYMTABLE_DUPLICATE);
    assert(symtable_find(table, "run", 0)->type == SYMBOL_TYPE_UNIT);

    SymbolKind kinds[] = {SYMBOL_VAR, SYMBOL_VAL, SYMBOL_PARAMETER};
    const char *names[] = {"variable", "constant", "parameter"};
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
        data = (SymbolData){.kind = kinds[i], .type = SYMBOL_TYPE_INT};
        assert(symtable_insert(table, names[i], &data) == SYMTABLE_OK);
        data.type = SYMBOL_TYPE_DOUBLE;
        assert(symtable_update(table, names[i], &data) == SYMTABLE_OK);
        found = symtable_find(table, names[i], 0);
        assert(found->kind == kinds[i] && found->type == SYMBOL_TYPE_DOUBLE);
        data.kind = SYMBOL_FUNCTION;
        assert(symtable_update(table, names[i], &data) == SYMTABLE_INTERNAL_ERROR);
        assert(symtable_find(table, names[i], 0)->kind == kinds[i]);
    }
    symtable_destroy(table);
}

static void test_invalid_inputs(void) {
    Symtable *table = symtable_create();
    assert(table != NULL);
    SymbolData data = {.kind = SYMBOL_VAR, .type = SYMBOL_TYPE_INT};
    assert(symtable_find(NULL, "x", 0) == NULL);
    assert(symtable_find(table, NULL, 0) == NULL);
    assert(symtable_find(table, "", 0) == NULL);
    assert(symtable_insert(NULL, "x", &data) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_insert(table, NULL, &data) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_insert(table, "", &data) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_insert(table, "x", NULL) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_update(NULL, "x", &data) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_update(table, NULL, &data) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_update(table, "", &data) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_update(table, "x", NULL) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_update(table, "missing", &data) == SYMTABLE_NOT_FOUND);

    SymbolType type = SYMBOL_TYPE_INT;
    SymbolData invalid[] = {
        {.kind = (SymbolKind)-1, .type = SYMBOL_TYPE_INT},
        {.kind = (SymbolKind)(SYMBOL_FUNCTION + 1), .type = SYMBOL_TYPE_INT},
        {.kind = SYMBOL_VAR, .type = (SymbolType)-1},
        {.kind = SYMBOL_VAR, .type = (SymbolType)(SYMBOL_TYPE_ANY + 1)},
        {.kind = SYMBOL_VAR, .type = SYMBOL_TYPE_INT, .parameter_count = 1},
        {.kind = SYMBOL_FUNCTION, .type = SYMBOL_TYPE_INT, .parameter_count = 1},
        {.kind = SYMBOL_FUNCTION,
         .type = SYMBOL_TYPE_INT,
         .parameter_count = SIZE_MAX,
         .parameter_types = &type}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert(symtable_insert(table, "invalid", &invalid[i]) == SYMTABLE_INTERNAL_ERROR);
    }
    assert(symtable_find(table, "invalid", 0) == NULL);
    symtable_destroy(table);
}

static void test_remove_functions(void) {
    Symtable *table = symtable_create();
    assert(table != NULL);
    SymbolType types[] = {SYMBOL_TYPE_ANY};
    SymbolData data = {.kind = SYMBOL_FUNCTION, .type = SYMBOL_TYPE_UNIT};
    assert(symtable_remove(NULL, "f", 0) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_remove(table, NULL, 0) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_remove(table, "", 0) == SYMTABLE_INTERNAL_ERROR);
    assert(symtable_remove(table, "f", 0) == SYMTABLE_NOT_FOUND);
    assert(symtable_insert(table, "f", &data) == SYMTABLE_OK);
    data.parameter_count = 1;
    data.parameter_types = types;
    assert(symtable_insert(table, "f", &data) == SYMTABLE_OK);
    assert(symtable_remove(table, "f", 2) == SYMTABLE_NOT_FOUND);
    assert(symtable_remove(table, "f", 0) == SYMTABLE_OK);
    assert(symtable_find(table, "f", 0) == NULL);
    assert(symtable_find(table, "f", 1)->parameter_types[0] == SYMBOL_TYPE_ANY);
    assert(symtable_remove(table, "f", 1) == SYMTABLE_OK);
    assert(symtable_remove(table, "f", 1) == SYMTABLE_NOT_FOUND);
    assert(symtable_insert(table, "f", &data) == SYMTABLE_OK);
    symtable_destroy(table);
}

static void test_remove_keys(void) {
    for (int order = 0; order < 3; order++) {
        Symtable *table = symtable_create();
        assert(table != NULL);
        int present[512] = {0};
        char name[32];
        for (int i = 0; i < 512; i++) {
            snprintf(name, sizeof(name), "x%04d", i);
            SymbolData data = {.kind = SYMBOL_FUNCTION,
                               .type = (SymbolType)(SYMBOL_TYPE_INT + i % 4),
                               .parameter_count = 1};
            SymbolType type = (SymbolType)(SYMBOL_TYPE_INT + i % 4);
            data.parameter_types = &type;
            assert(symtable_insert(table, name, &data) == SYMTABLE_OK);
            present[i] = 1;
        }
        for (int i = 0; i < 512; i++) {
            const int key = order == 0 ? i : (order == 1 ? 511 - i : (i * 197) % 512);
            snprintf(name, sizeof(name), "x%04d", key);
            assert(symtable_remove(table, name, 1) == SYMTABLE_OK);
            present[key] = 0;
            for (int j = 0; j < 512; j++) {
                snprintf(name, sizeof(name), "x%04d", j);
                const SymbolData *data = symtable_find(table, name, 1);
                if (present[j]) {
                    assert(data != NULL);
                    assert(data->type == (SymbolType)(SYMBOL_TYPE_INT + j % 4));
                    assert(data->parameter_types[0] == data->type);
                } else {
                    assert(data == NULL);
                }
            }
        }
        symtable_destroy(table);
    }
}

static void test_clear(void) {
    Symtable *table = symtable_create();
    assert(table != NULL);
    symtable_clear(NULL);
    symtable_clear(table);
    for (int i = 0; i < 3; i++) {
        SymbolType types[] = {SYMBOL_TYPE_ANY};
        SymbolData data = {.kind = SYMBOL_FUNCTION,
                           .type = SYMBOL_TYPE_UNIT,
                           .parameter_count = 1,
                           .parameter_types = types};
        assert(symtable_insert(table, "print", &data) == SYMTABLE_OK);
        symtable_clear(table);
        assert(symtable_find(table, "print", 1) == NULL);
        assert(symtable_remove(table, "print", 1) == SYMTABLE_NOT_FOUND);
        symtable_clear(table);
    }
    symtable_destroy(table);
}

int main(void) {
    test_builtin_parameters();
    test_updates();
    test_update_copies();
    test_scopes();
    test_ordered_keys();
    test_rotations();
    test_symbol_data();
    test_invalid_inputs();
    test_remove_functions();
    test_remove_keys();
    test_clear();
    return 0;
}
