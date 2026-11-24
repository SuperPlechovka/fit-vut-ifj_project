#include "semantic_analysis.h"
#include "symbol_table.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct SemanticContext {
    Symtable *functionsTable;
    ParseError *error;
} SemanticContext;

static SymbolType typeKindToSymbolType(const TypeKind kind) {
    switch (kind) {
    case TY_DOUBLE:
        return SYMBOL_TYPE_DOUBLE;
    case TY_INT:
        return SYMBOL_TYPE_INT;
    case TY_STRING:
        return SYMBOL_TYPE_STRING;
    case TY_UNIT:
        return SYMBOL_TYPE_UNIT;
    default:
        return SYMBOL_TYPE_UNKNOWN;
    }
}

typedef struct {
    const char *name;
    SymbolType return_type;
    size_t param_count;
    SymbolType params[3];
} BuiltinDef;

static const BuiltinDef BUILTINS[] = {
    {"print", SYMBOL_TYPE_UNIT, 1, {SYMBOL_TYPE_ANY}},
    {"readLine", SYMBOL_TYPE_STRING, 0, {0}},
    {"readInt", SYMBOL_TYPE_INT, 0, {0}},
    {"readDouble", SYMBOL_TYPE_DOUBLE, 0, {0}},
    {"toStr", SYMBOL_TYPE_STRING, 1, {SYMBOL_TYPE_ANY}},
    {"toInt", SYMBOL_TYPE_INT, 1, {SYMBOL_TYPE_DOUBLE}},
    {"strLength", SYMBOL_TYPE_INT, 1, {SYMBOL_TYPE_STRING}},
    {"subStr", SYMBOL_TYPE_STRING, 3, {SYMBOL_TYPE_STRING, SYMBOL_TYPE_INT, SYMBOL_TYPE_INT}},
    {"ord", SYMBOL_TYPE_INT, 2, {SYMBOL_TYPE_STRING, SYMBOL_TYPE_INT}},
    {"chr", SYMBOL_TYPE_STRING, 1, {SYMBOL_TYPE_INT}},
};

static ErrorCode insertBuiltins(SemanticContext *context) {

    size_t builtinCount = sizeof(BUILTINS) / sizeof(BUILTINS[0]);
    for (size_t i = 0; i < builtinCount; i++) {
        SymbolData data = {.kind = SYMBOL_FUNCTION,
                           .type = BUILTINS[i].return_type,
                           .parameter_count = BUILTINS[i].param_count,
                           .parameter_types = BUILTINS[i].params,
                           .defined = true,
                           .builtin = true};

        SymtableResult result = symtable_insert(context->functionsTable, BUILTINS[i].name, &data);

        if (result != SYMTABLE_OK) {
            return setParserErrorAt(context->error, ERR_INTERNAL, 0, 0, "out of memory");
        }
    }

    return ERR_OK;
}

static bool isNameUsedBefore(const Node *first, const Node *stop, const char *name) {
    for (const Node *node = first; node != NULL && node != stop; node = node->next) {
        if (strcmp(node->name, name) == 0) {
            return true;
        }
    }

    return false;
}

static bool isBuiltinName(const char *name) {
    size_t builtinCount = sizeof(BUILTINS) / sizeof(BUILTINS[0]);

    for (size_t i = 0; i < builtinCount; i++) {
        if (strcmp(BUILTINS[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

static size_t countNodes(const Node *first) {
    size_t count = 0;

    for (const Node *node = first; node != NULL; node = node->next) {
        count++;
    }

    return count;
}

static ErrorCode declareFunction(SemanticContext *context, const Node *func) {
    size_t paramCount = countNodes(func->args);

    SymbolType *paramTypes = NULL;
    if (paramCount > 0) {
        paramTypes = malloc(paramCount * sizeof(SymbolType));
        if (paramTypes == NULL) {
            return setParserErrorAt(context->error, ERR_INTERNAL, 0, 0, "out of memory");
        }
    }

    ErrorCode code = ERR_OK;
    size_t i = 0;

    for (const Node *param = func->args; param != NULL; param = param->next, i++) {
        if (isNameUsedBefore(func->args, param, param->name)) {
            code = setParserErrorAt(context->error, ERR_REDEFINITION, param->line, param->col,
                                    "duplicate parameter name");
            break;
        }
        paramTypes[i] = typeKindToSymbolType(param->type);
    }

    if (code == ERR_OK) {
        SymbolData data = {.kind = SYMBOL_FUNCTION,
                           .type = typeKindToSymbolType(func->type),
                           .parameter_count = paramCount,
                           .parameter_types = paramTypes,
                           .defined = true,
                           .entry_point = func->is_main};

        SymtableResult result = symtable_insert(context->functionsTable, func->name, &data);

        if (result == SYMTABLE_DUPLICATE) {
            code = setParserErrorAt(context->error, ERR_REDEFINITION, func->line, func->col,
                                    "function redefinition");
        } else if (result != SYMTABLE_OK) {
            code = setParserErrorAt(context->error, ERR_INTERNAL, 0, 0, "out of memory");
        }
    }

    free(paramTypes);
    return code;
}

static ErrorCode validateMainSignature(SemanticContext *context, const Node *func) {
    if (func->args != NULL) {
        return setParserErrorAt(context->error, ERR_CALL, func->line, func->col,
                                "@main function must not have parameters");
    }

    if (func->type != TY_UNKNOWN && func->type != TY_UNIT) {
        return setParserErrorAt(context->error, ERR_TYPE, func->line, func->col,
                                "@main function must return Unit");
    }

    return ERR_OK;
}

static ErrorCode analyseFunctionHeaders(SemanticContext *context, const Node *program) {
    size_t mainCount = 0;

    for (const Node *func = program->body; func != NULL; func = func->next) {
        if (func->kind != N_FUNC) {
            continue;
        }

        if (isBuiltinName(func->name) || isNameUsedBefore(program->body, func, func->name)) {
            return setParserErrorAt(context->error, ERR_REDEFINITION, func->line, func->col,
                                    "function redefinition");
        }

        if (func->is_main) {
            mainCount++;
            if (mainCount > 1) {
                return setParserErrorAt(context->error, ERR_REDEFINITION, func->line, func->col, "multiple @main functions");
            }

            ErrorCode code = validateMainSignature(context, func);
            if (code != ERR_OK) {
                return code;
            }
        }

        ErrorCode code = declareFunction(context, func);

        if (code != ERR_OK) {
            return code;
        }
    }

    if (mainCount == 0) {
        return setParserErrorAt(context->error, ERR_UNDEFINED, 0, 0, "missing @main function");
    }

    return ERR_OK;
}

ErrorCode analyseSemantics(Node *program, ParseError *error) {
    if (program == NULL || error == NULL) {
        return setParserErrorAt(error, ERR_INTERNAL, 0, 0, "invalid analyseSemantics arguments");
    }

    SemanticContext context = {.error = error, .functionsTable = symtable_create()};

    if (context.functionsTable == NULL) {
        return setParserErrorAt(error, ERR_INTERNAL, 0, 0, "out of memory");
    }

    ErrorCode code = insertBuiltins(&context);

    if (code == ERR_OK) {
        code = analyseFunctionHeaders(&context, program);
    }

    symtable_destroy(context.functionsTable);
    return code;
}
