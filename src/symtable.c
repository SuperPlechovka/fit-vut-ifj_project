/**
 * @file
 * @brief Tabulka symbolů implementovaná pomocí AVL stromu.
 * @details Implementace překladače IFJ26 (IFJ/IAL 2026/27).
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */

#include "symtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/** @brief Uzel stromu s vlastním jménem a daty symbolu. */
typedef struct symtable_node {
    char *name;                  /**< Vlastněné jméno symbolu. */
    SymbolData data;             /**< Data včetně vlastněné kopie typů parametrů. */
    struct symtable_node *left;  /**< Levý podstrom. */
    struct symtable_node *right; /**< Pravý podstrom. */
    int height;                  /**< Výška podstromu. */
} SymtableNode;

/** @brief Reprezentace tabulky symbolů. */
struct symtable {
    SymtableNode *root; /**< Kořen stromu. */
};

static int node_height(const SymtableNode *node);
static int max_height(int left, int right);
static void update_height(SymtableNode *node);
static SymtableNode *rotate_right(SymtableNode *node);
static SymtableNode *rotate_left(SymtableNode *node);
static SymtableNode *balance_node(SymtableNode *node);
static int compare_key(const char *name, size_t parameter_count, const SymtableNode *node);
static SymtableNode *find_node(SymtableNode *root, const char *name, size_t parameter_count);
static int valid_data(const SymbolData *data);
static int copy_data(SymbolData *target, const SymbolData *source);
static SymtableNode *create_node(const char *name, const SymbolData *data);
static SymtableNode *insert_node(SymtableNode *root, const char *name, const SymbolData *data,
                                 SymtableResult *result);
static SymtableNode *remove_node(SymtableNode *root, const char *name, size_t parameter_count,
                                 SymtableResult *result);
static SymtableNode *find_min(SymtableNode *root);
static void free_node(SymtableNode *node);
static void destroy_node(SymtableNode *node);

Symtable *symtable_create(void) {
    Symtable *table = malloc(sizeof(*table));
    if (table == NULL) {
        return NULL;
    }
    table->root = NULL;
    return table;
}

void symtable_clear(Symtable *table) {
    if (table == NULL) {
        return;
    }
    destroy_node(table->root);
    table->root = NULL;
}

void symtable_destroy(Symtable *table) {
    symtable_clear(table);
    free(table);
}

SymtableResult symtable_insert(Symtable *table, const char *name, const SymbolData *data) {
    if (table == NULL || name == NULL || name[0] == '\0' || !valid_data(data)) {
        return SYMTABLE_INTERNAL_ERROR;
    }
    if (find_node(table->root, name, data->parameter_count) != NULL) {
        return SYMTABLE_DUPLICATE;
    }
    SymtableResult result = SYMTABLE_OK;
    table->root = insert_node(table->root, name, data, &result);
    return result;
}

const SymbolData *symtable_find(const Symtable *table, const char *name, size_t parameter_count) {
    if (table == NULL || name == NULL || name[0] == '\0') {
        return NULL;
    }
    const SymtableNode *node = find_node(table->root, name, parameter_count);
    return node == NULL ? NULL : &node->data;
}

SymtableResult symtable_update(Symtable *table, const char *name, const SymbolData *data) {
    if (table == NULL || name == NULL || name[0] == '\0' || !valid_data(data)) {
        return SYMTABLE_INTERNAL_ERROR;
    }
    const SymtableNode *node = find_node(table->root, name, data->parameter_count);
    if (node == NULL) {
        return SYMTABLE_NOT_FOUND;
    }
    if (node->data.kind != data->kind) {
        return SYMTABLE_INTERNAL_ERROR;
    }

    SymtableResult result = SYMTABLE_OK;
    table->root = insert_node(table->root, name, data, &result);
    return result;
}

SymtableResult symtable_remove(Symtable *table, const char *name, size_t parameter_count) {
    if (table == NULL || name == NULL || name[0] == '\0') {
        return SYMTABLE_INTERNAL_ERROR;
    }
    SymtableResult result = SYMTABLE_NOT_FOUND;
    table->root = remove_node(table->root, name, parameter_count, &result);
    return result;
}

/** @brief Vrátí uloženou výšku. */
static int node_height(const SymtableNode *node) { return node == NULL ? 0 : node->height; }

/** @brief Vrátí větší výšku */
static int max_height(const int left, const int right) {
    if (left > right) {
        return left;
    }
    return right;
}

/** @brief Přepočítá výšku neprázdného uzlu podle jeho potomků. */
static void update_height(SymtableNode *node) {
    node->height = max_height(node_height(node->left), node_height(node->right)) + 1;
}

/** @brief Pravá rotace. */
static SymtableNode *rotate_right(SymtableNode *node) {
    SymtableNode *root = node->left;
    node->left = root->right;
    root->right = node;
    node->height = max_height(node_height(node->left), node_height(node->right)) + 1;
    root->height = max_height(node_height(root->left), node_height(root->right)) + 1;
    return root;
}

/** @brief Levá rotace. */
static SymtableNode *rotate_left(SymtableNode *node) {
    SymtableNode *root = node->right;
    node->right = root->left;
    root->left = node;
    node->height = max_height(node_height(node->left), node_height(node->right)) + 1;
    root->height = max_height(node_height(root->left), node_height(root->right)) + 1;
    return root;
}

/** @brief Po vložení nebo odstranění obnoví výšku a vyváženost podstromu */
static SymtableNode *balance_node(SymtableNode *node) {
    update_height(node);
    const int balance = node_height(node->right) - node_height(node->left);
    if (balance < -1) {
        if (node_height(node->left->left) < node_height(node->left->right)) {
            node->left = rotate_left(node->left);
        }
        return rotate_right(node);
    }
    if (balance > 1) {
        if (node_height(node->right->right) < node_height(node->right->left)) {
            node->right = rotate_right(node->right);
        }
        return rotate_left(node);
    }
    return node;
}

/** @brief Porovná klíč s uzlem podle jména a podle počtu parametrů. */
static int compare_key(const char *name, size_t parameter_count, const SymtableNode *node) {
    const int comparison = strcmp(name, node->name);
    if (comparison != 0) {
        return comparison;
    }
    if (parameter_count < node->data.parameter_count) {
        return -1;
    }
    if (parameter_count > node->data.parameter_count) {
        return 1;
    }
    return 0;
}

/** @brief Rekurzivně najde uzel podle klíče */
static SymtableNode *find_node(SymtableNode *root, const char *name, size_t parameter_count) {
    if (root == NULL) {
        return NULL;
    }
    const int comparison = compare_key(name, parameter_count, root);
    if (comparison == 0) {
        return root;
    }
    if (comparison < 0) {
        return find_node(root->left, name, parameter_count);
    }
    return find_node(root->right, name, parameter_count);
}

/** @brief Ověří data před kopírováním. */
static int valid_data(const SymbolData *data) {
    if (data == NULL || data->kind < SYMBOL_VAR || data->kind > SYMBOL_FUNCTION ||
        data->type < SYMBOL_TYPE_UNKNOWN || data->type > SYMBOL_TYPE_ANY) {
        return 0;
    }
    if (data->kind != SYMBOL_FUNCTION) {
        return data->parameter_count == 0;
    }
    if (data->parameter_count > SIZE_MAX / sizeof(SymbolType)) {
        return 0;
    }
    if (data->parameter_count != 0 && data->parameter_types == NULL) {
        return 0;
    }
    for (size_t i = 0; i < data->parameter_count; i++) {
        if (data->parameter_types[i] < SYMBOL_TYPE_UNKNOWN ||
            data->parameter_types[i] > SYMBOL_TYPE_ANY) {
            return 0;
        }
    }
    return 1;
}

/** @brief Zkopíruje data. */
static int copy_data(SymbolData *target, const SymbolData *source) {
    *target = *source;
    target->parameter_types = NULL;
    if (source->kind != SYMBOL_FUNCTION) {
        target->defined = 0;
        target->builtin = 0;
        target->entry_point = 0;
    } else if (source->parameter_count != 0) {
        const size_t size = source->parameter_count * sizeof(SymbolType);
        SymbolType *types = malloc(size);
        if (types == NULL) {
            return 0;
        }
        memcpy(types, source->parameter_types, size);
        target->parameter_types = types;
    }
    return 1;
}

/** @brief Vytvoří uzel */
static SymtableNode *create_node(const char *name, const SymbolData *data) {
    SymtableNode *node = malloc(sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->name = malloc(strlen(name) + 1);
    if (node->name == NULL) {
        free(node);
        return NULL;
    }
    strcpy(node->name, name);
    if (!copy_data(&node->data, data)) {
        free(node->name);
        free(node);
        return NULL;
    }
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    return node;
}

/** @brief Vloží nebo aktualizuje uzel. */
static SymtableNode *insert_node(SymtableNode *root, const char *name, const SymbolData *data,
                                 SymtableResult *result) {
    if (root == NULL) {
        SymtableNode *node = create_node(name, data);
        if (node == NULL) {
            *result = SYMTABLE_INTERNAL_ERROR;
        }
        return node;
    }
    const int comparison = compare_key(name, data->parameter_count, root);
    if (comparison < 0) {
        root->left = insert_node(root->left, name, data, result);
    } else if (comparison > 0) {
        root->right = insert_node(root->right, name, data, result);
    } else {
        SymbolData copy;
        if (!copy_data(&copy, data)) {
            *result = SYMTABLE_INTERNAL_ERROR;
            return root;
        }
        free((SymbolType *)root->data.parameter_types);
        root->data = copy;
    }
    return *result == SYMTABLE_OK ? balance_node(root) : root;
}

/** @brief Najde nejmenší uzel neprázdného podstromu. */
static SymtableNode *find_min(SymtableNode *root) {
    if (root->left == NULL) {
        return root;
    }
    return find_min(root->left);
}

/** @brief Odstraní uzel podle klíče. */
static SymtableNode *remove_node(SymtableNode *root, const char *name, size_t parameter_count,
                                 SymtableResult *result) {
    if (root == NULL) {
        return NULL;
    }
    const int comparison = compare_key(name, parameter_count, root);
    if (comparison < 0) {
        root->left = remove_node(root->left, name, parameter_count, result);
        return *result == SYMTABLE_OK ? balance_node(root) : root;
    }
    if (comparison > 0) {
        root->right = remove_node(root->right, name, parameter_count, result);
        return *result == SYMTABLE_OK ? balance_node(root) : root;
    }

    *result = SYMTABLE_OK;
    if (root->left == NULL && root->right == NULL) {
        free_node(root);
        return NULL;
    }
    if (root->left != NULL && root->right != NULL) {
        SymtableNode *successor = find_min(root->right);
        char *old_name = root->name;
        SymbolData old_data = root->data;
        root->name = successor->name;
        root->data = successor->data;
        successor->name = old_name;
        successor->data = old_data;
        root->right =
            remove_node(root->right, successor->name, successor->data.parameter_count, result);
        return balance_node(root);
    }
    SymtableNode *only_child;
    if (root->left == NULL) {
        only_child = root->right;
    } else {
        only_child = root->left;
    }
    free_node(root);
    return only_child;
}

/** @brief Uvolní jeden uzel a jeho data. */
static void free_node(SymtableNode *node) {
    free(node->name);
    free((SymbolType *)node->data.parameter_types);
    free(node);
}

/** @brief Zruší podstrom postorder průchodem. */
static void destroy_node(SymtableNode *node) {
    if (node == NULL) {
        return;
    }
    destroy_node(node->left);
    destroy_node(node->right);
    free_node(node);
}
