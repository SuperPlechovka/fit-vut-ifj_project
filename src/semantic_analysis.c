/**
 * @file
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
#include "semantic_analysis.h"
#include "symbol_table.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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

typedef enum { BODY_PENDING, BODY_IN_PROGRESS, BODY_DONE } BodyState;

typedef struct SemanticContext {
    Symtable *functionsTable;
    ParseError *error;
    Symtable **scopeStack; // scopeStack[0] = function body,
                           // scopeStack[scopeCount - 1] = stack top (current scope)
    size_t scopeCount;
    size_t scopeCapacity;
    Node *program;               // function list
    Node *currentFunction;       // currently analyzed function
    BodyState *states;           // state of every function body, index = order in program
    const Node **callChainStack; // functions in progress, top = currentFunction
    size_t callDepth;
} SemanticContext;

static ErrorCode pushScope(SemanticContext *context) {
    if (context->scopeCount == context->scopeCapacity) {
        size_t newCapacity = context->scopeCapacity == 0 ? 8 : context->scopeCapacity * 2;

        Symtable **newScopeStack =
            realloc(context->scopeStack, newCapacity * sizeof(*newScopeStack));
        if (newScopeStack == NULL) {
            return setParserErrorAt(context->error, ERR_INTERNAL, 0, 0, "out of memory");
        }

        context->scopeStack = newScopeStack;
        context->scopeCapacity = newCapacity;
    }

    Symtable *scope = symtable_create();

    if (scope == NULL) {
        return setParserErrorAt(context->error, ERR_INTERNAL, 0, 0, "out of memory");
    }

    context->scopeStack[context->scopeCount++] = scope;
    return ERR_OK;
}

static void popScope(SemanticContext *context) {
    assert(context->scopeCount > 0);
    context->scopeCount--;
    symtable_destroy(context->scopeStack[context->scopeCount]);
}

static void destroyScopeStack(SemanticContext *context) {
    while (context->scopeCount > 0) {
        popScope(context);
    }

    free(context->scopeStack);
    context->scopeStack = NULL;
    context->scopeCapacity = 0;
}

// starting at the highest level of nesting going up, returns first occurence
static const SymbolData *lookupVariable(const SemanticContext *context, const char *name) {
    for (size_t i = context->scopeCount; i > 0; i--) {
        const SymbolData *data = symtable_find(context->scopeStack[i - 1], name, 0);

        if (data != NULL) {
            return data;
        }
    }

    return NULL;
}

// checks for a redefinition searching down, redefining outer variables is allowed
static ErrorCode declareVariable(SemanticContext *context, const Node *node, SymbolKind kind,
                                 SymbolType type) {
    Symtable *top = context->scopeStack[context->scopeCount - 1];

    if (symtable_find(top, node->name, 0) != NULL) {
        return setParserErrorAt(context->error, ERR_REDEFINITION, node->line, node->col,
                                "variable redefinition");
    }

    SymbolData data = {.kind = kind, .type = type};

    if (symtable_insert(top, node->name, &data) != SYMTABLE_OK) {
        return setParserErrorAt(context->error, ERR_INTERNAL, 0, 0, "out of memory");
    }

    return ERR_OK;
}

// result of a block and whether it is a return statement
typedef struct {
    TypeKind type; // TY_UNKNOWN = if statement, whose branches are not Unit
    bool returns;  // last statement is return
} BlockResult;

static ErrorCode analyseStatement(SemanticContext *context, Node *node, BlockResult *result);

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
                return setParserErrorAt(context->error, ERR_REDEFINITION, func->line, func->col,
                                        "multiple @main functions");
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

static TypeKind symbolTypeToTypeKind(SymbolType type) {
    switch (type) {
    case SYMBOL_TYPE_INT:
        return TY_INT;
    case SYMBOL_TYPE_DOUBLE:
        return TY_DOUBLE;
    case SYMBOL_TYPE_STRING:
        return TY_STRING;
    case SYMBOL_TYPE_UNIT:
        return TY_UNIT;
    default:
        return TY_UNKNOWN;
    }
}

static bool isNumeric(TypeKind type) { return type == TY_INT || type == TY_DOUBLE; }

// generally types have to match with the exception of implicit conversion from Int to Double
static bool isAssignable(TypeKind target, TypeKind source) {
    return target == source || (target == TY_DOUBLE && source == TY_INT);
}

static ErrorCode typeError(SemanticContext *context, const Node *node, const char *message) {
    return setParserErrorAt(context->error, ERR_TYPE, node->line, node->col, message);
}

static ErrorCode analyseExpression(SemanticContext *context, Node *node, TypeKind *out);

static ErrorCode analyseBinary(SemanticContext *context, Node *node, TypeKind *out) {
    TypeKind left, right;
    ErrorCode code = analyseExpression(context, node->left, &left);

    if (code == ERR_OK) {
        code = analyseExpression(context, node->right, &right);
    }

    if (code != ERR_OK) {
        return code;
    }

    if (left == TY_BOOL || right == TY_BOOL) {
        return typeError(context, node, "boolean value used as operand");
    }

    bool numeric = isNumeric(left) && isNumeric(right);
    TypeKind numericResult = (left == TY_INT && right == TY_INT) ? TY_INT : TY_DOUBLE;

    switch (node->op) {
    case OP_ADD:
        if (left == TY_STRING && right == TY_STRING) {
            *out = TY_STRING;
            return ERR_OK;
        }
        // fall-through
    case OP_SUB:
    case OP_MUL:
    case OP_DIV:
        if (!numeric) {
            return typeError(context, node, "incompatible operand types");
        }
        *out = numericResult;
        return ERR_OK;

    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE:
        if (!numeric && !(left == TY_STRING && right == TY_STRING)) {
            return typeError(context, node, "incompatible operand types");
        }

        *out = TY_BOOL;
        return ERR_OK;

    case OP_EQ:
    case OP_NE:
        if (!numeric && left != right) {
            return typeError(context, node, "incompatible operand types");
        }
        *out = TY_BOOL;
        return ERR_OK;

    default:
        return setParserErrorAt(context->error, ERR_INTERNAL, node->line, node->col,
                                "unknown binary operator");
    }
}

static ErrorCode analyseUnary(SemanticContext *context, Node *node, TypeKind *out) {
    TypeKind operand;
    ErrorCode code = analyseExpression(context, node->left, &operand);

    if (code != ERR_OK) {
        return code;
    }

    if (node->op != OP_NEG || !isNumeric(operand)) {
        return typeError(context, node, "unary minus needs a number");
    }

    *out = operand;
    return ERR_OK;
}

static ErrorCode analyseCall(SemanticContext *context, Node *node, TypeKind *out);

// Determines the data type of an expression, checks its semantic rules (error 3, 6)
// and stores the type into node->type (for the code generator) and *out.
// Leaves are resolved here, compound expressions are delegated by node kind.
static ErrorCode analyseExpression(SemanticContext *context, Node *node, TypeKind *out) {
    ErrorCode code = ERR_OK;
    TypeKind type = TY_UNKNOWN;

    switch (node->kind) {
    case N_LIT_INT:
        type = TY_INT;
        break;
    case N_LIT_DOUBLE:
        type = TY_DOUBLE;
        break;
    case N_LIT_STRING:
        type = TY_STRING;
        break;
    case N_LIT_UNIT:
        type = TY_UNIT;
        break;
    case N_VAR: {
        const SymbolData *variable = lookupVariable(context, node->name);
        if (variable == NULL) {
            return setParserErrorAt(context->error, ERR_UNDEFINED, node->line, node->col,
                                    "undefined variable");
        }
        type = symbolTypeToTypeKind(variable->type);
        break;
    }
    case N_BINARY:
        code = analyseBinary(context, node, &type);
        break;
    case N_UNARY:
        code = analyseUnary(context, node, &type);
        break;
    case N_CALL:
        code = analyseCall(context, node, &type);
        break;
    default:
        return setParserErrorAt(context->error, ERR_INTERNAL, node->line, node->col,
                                "unexpected node in expression");
    }

    if (code != ERR_OK) {
        return code;
    }

    node->type = type;
    *out = type;
    return ERR_OK;
}

static ErrorCode analyseStatements(SemanticContext *context, Node *list, BlockResult *result) {
    *result = (BlockResult){TY_UNIT, false}; // block is empty (no else branch) = ()

    for (Node *statement = list; statement != NULL; statement = statement->next) {
        ErrorCode code = analyseStatement(context, statement, result);

        if (code != ERR_OK) {
            return code;
        }
    }

    return ERR_OK;
}

// if branch or while body
static ErrorCode analyseBlock(SemanticContext *context, Node *list, BlockResult *result) {
    ErrorCode code = pushScope(context);

    if (code == ERR_OK) {
        code = analyseStatements(context, list, result);
    }

    if (code == ERR_OK) {
        popScope(context);
    }

    return code;
}

// return <expr>
static ErrorCode analyseReturn(SemanticContext *context, Node *node, BlockResult *result) {
    TypeKind returnType = context->currentFunction->type;

    // return anywhere in body => return type has to be specified
    if (returnType == TY_UNKNOWN) {
        return typeError(context, node, "function containing return must declare return type");
    }

    if (node->left == NULL) {
        // return with no argument returns (), and is only allowed in
        // functions with explicitly declared return type Unit
        if (returnType != TY_UNIT) {
            return typeError(context, node, "return without value in not-Unit function");
        }
    } else {
        TypeKind valueType;
        ErrorCode code = analyseExpression(context, node->left, &valueType);

        if (code != ERR_OK) {
            return code;
        }

        // explicit return type of Unit => value is evaluated and discarded, type remains
        // unvalidated
        if (returnType != TY_UNIT && !isAssignable(returnType, valueType)) {
            return typeError(context, node, "returned value has incompatible type");
        }
    }

    *result = (BlockResult){TY_UNIT, true};
    return ERR_OK;
}

static ErrorCode analyseCondition(SemanticContext *context, Node *condition) {
    TypeKind type;
    ErrorCode code = analyseExpression(context, condition, &type);

    if (code != ERR_OK) {
        return code;
    }

    if (type != TY_BOOL) {
        return typeError(context, condition, "condition must be a relational expression");
    }

    return ERR_OK;
}

static ErrorCode analyseIf(SemanticContext *context, Node *node, BlockResult *result) {
    BlockResult thenResult;
    BlockResult elseResult;

    ErrorCode code = analyseCondition(context, node->left);

    if (code == ERR_OK) {
        code = analyseBlock(context, node->body, &thenResult);
    }

    if (code == ERR_OK) {
        code = analyseBlock(context, node->right, &elseResult);
    }

    if (code != ERR_OK) {
        return code;
    }

    bool thenUnit = thenResult.returns || thenResult.type == TY_UNIT;
    bool elseUnit = elseResult.returns || elseResult.type == TY_UNIT;

    result->returns = thenResult.returns && elseResult.returns;
    result->type = (thenUnit && elseUnit) ? TY_UNIT : TY_UNKNOWN;

    return ERR_OK;
}

// val/var id [: type] = expr
static ErrorCode analyseDeclaration(SemanticContext *context, Node *node) {
    TypeKind initType;

    ErrorCode code = analyseExpression(context, node->left, &initType);

    if (code != ERR_OK) {
        return code;
    }

    TypeKind type = node->type;

    if (type == TY_UNKNOWN) {
        if (initType == TY_UNKNOWN || initType == TY_BOOL) {
            return typeError(context, node, "cannot infer variable type");
        }

        type = initType;
        node->type = type;
    } else if (!isAssignable(type, initType)) {
        return typeError(context, node, "initializer type does not match declared type");
    }

    return declareVariable(context, node, node->is_var ? SYMBOL_VAR : SYMBOL_VAL,
                           typeKindToSymbolType(type));
}

// id = expr
static ErrorCode analyseAssignment(SemanticContext *context, Node *node) {
    const SymbolData *variable = lookupVariable(context, node->name);

    if (variable == NULL) {
        return setParserErrorAt(context->error, ERR_UNDEFINED, node->line, node->col,
                                "assignment to undefined variable");
    }

    if (variable->kind != SYMBOL_VAR) {
        return setParserErrorAt(context->error, ERR_IMMUTABLE, node->line, node->col,
                                "assignment to immutable variable");
    }

    TypeKind target = symbolTypeToTypeKind(variable->type);

    TypeKind valueType;
    ErrorCode code = analyseExpression(context, node->left, &valueType);

    if (code != ERR_OK) {
        return code;
    }

    if (!isAssignable(target, valueType)) {
        return typeError(context, node, "assigned value has incompatible type");
    }

    return ERR_OK;
}

static ErrorCode analyseStatement(SemanticContext *context, Node *node, BlockResult *result) {
    ErrorCode code;
    BlockResult ignored;

    switch (node->kind) {
    case N_DECL:
        *result = (BlockResult){TY_UNIT, false};
        return analyseDeclaration(context, node);

    case N_ASSIGN:
        *result = (BlockResult){TY_UNIT, false};
        return analyseAssignment(context, node);

    case N_IF:
        return analyseIf(context, node, result);

    case N_WHILE:
        *result = (BlockResult){TY_UNIT, false};
        code = analyseCondition(context, node->left);
        if (code == ERR_OK) {
            code = analyseBlock(context, node->body, &ignored);
        }

        return code;

    case N_RETURN:
        return analyseReturn(context, node, result);

    default:
        result->returns = false;
        return analyseExpression(context, node, &result->type);
    }
}

static ErrorCode validateFunctionResult(SemanticContext *context, Node *func,
                                        const BlockResult *body) {
    if (func->type == TY_UNIT) {
        return ERR_OK; // result is ignored
    }

    if (func->type == TY_UNKNOWN) {
        // no return => derive return value from function body
        if (body->type == TY_UNKNOWN || body->type == TY_BOOL) {
            return typeError(context, func, "cannot infer function return type");
        }

        if (func->is_main && body->type != TY_UNIT) {
            return typeError(context, func, "@main function must return Unit");
        }

        func->type = body->type; // derived type
        return ERR_OK;
    }

    if (body->returns) {
        return ERR_OK;
    }

    if (!isAssignable(func->type, body->type)) {
        return typeError(context, func, "function result does not match return type");
    }

    return ERR_OK;
}

static ErrorCode analyseFunctionBody(SemanticContext *context, Node *func) {
    BlockResult body;
    ErrorCode code = pushScope(context);

    for (const Node *param = func->args; param != NULL && code == ERR_OK; param = param->next) {
        code = declareVariable(context, param, SYMBOL_PARAMETER, typeKindToSymbolType(param->type));
    }

    if (code == ERR_OK) {
        code = analyseStatements(context, func->body, &body);
    }

    if (code == ERR_OK) {
        popScope(context);
        code = validateFunctionResult(context, func, &body);
    }

    return code;
}

static Node *findFunction(const SemanticContext *context, const char *name, size_t *index) {
    size_t i = 0;

    for (Node *func = context->program->body; func != NULL; func = func->next, i++) {
        if (strcmp(func->name, name) == 0) {
            *index = i;
            return func;
        }
    }

    return NULL;
}

static ErrorCode analyseFunction(SemanticContext *context, Node *func, size_t index) {
    // context copy: shares function table, error, states and callChainStack (pointers)
    // but has its own empty scopeStack - caller's variables are not visible
    SemanticContext calleeContext = *context;
    calleeContext.scopeStack = NULL;
    calleeContext.scopeCount = 0;
    calleeContext.scopeCapacity = 0;
    calleeContext.currentFunction = func;

    context->states[index] = BODY_IN_PROGRESS;
    calleeContext.callChainStack[calleeContext.callDepth++] = func;

    ErrorCode code = analyseFunctionBody(&calleeContext, func);

    context->states[index] = BODY_DONE;
    destroyScopeStack(&calleeContext);
    return code;
}

// return type of a user defined function where it's called
// body is potentially analysed immediately
static ErrorCode resolveReturnType(SemanticContext *context, const Node *call, Node *callee,
                                   size_t index, TypeKind *out) {
    if (context->states[index] == BODY_IN_PROGRESS) {
        for (size_t i = context->callDepth; i > 0; i--) {
            const Node *func = context->callChainStack[i - 1];

            if (func->type == TY_UNKNOWN) {
                return typeError(context, call, "recursive function must declare return type");
            }

            if (func == callee) {
                break;
            }
        }
    } else if (context->states[index] == BODY_PENDING) {
        ErrorCode code = analyseFunction(context, callee, index);

        if (code != ERR_OK) {
            return code;
        }
    }

    *out = callee->type;
    return ERR_OK;
}

// id (<args>) - function call statement
static ErrorCode analyseCall(SemanticContext *context, Node *node, TypeKind *out) {
    size_t argCount = countNodes(node->args);
    const SymbolData *function = symtable_find(context->functionsTable, node->name, argCount);

    if (function == NULL) {
        size_t ignoredIndex;

        // function is called using invalid argument count
        if (isBuiltinName(node->name) || findFunction(context, node->name, &ignoredIndex) != NULL) {
            return setParserErrorAt(context->error, ERR_CALL, node->line, node->col,
                                    "wrong number of arguments");
        }

        return setParserErrorAt(context->error, ERR_UNDEFINED, node->line, node->col,
                                "call of undefined function");
    }

    size_t i = 0;
    for (Node *arg = node->args; arg != NULL; arg = arg->next, i++) {
        TypeKind argType;
        ErrorCode code = analyseExpression(context, arg, &argType);

        if (code != ERR_OK) {
            return code;
        }

        SymbolType paramType = function->parameter_types[i];

        if (paramType != SYMBOL_TYPE_ANY &&
            !isAssignable(symbolTypeToTypeKind(paramType), argType)) {
            return setParserErrorAt(context->error, ERR_CALL, arg->line, arg->col,
                                    "argument type does not match parameter type");
        }
    }

    if (function->builtin) {
        *out = symbolTypeToTypeKind(function->type);
        return ERR_OK;
    }

    size_t index;
    Node *callee = findFunction(context, node->name, &index);
    return resolveReturnType(context, node, callee, index, out);
}

static ErrorCode analyseFunctionBodies(SemanticContext *context) {
    size_t i = 0;

    for (Node *func = context->program->body; func != NULL; func = func->next, i++) {
        if (context->states[i] == BODY_PENDING) {
            ErrorCode code = analyseFunction(context, func, i);

            if (code != ERR_OK) {
                return code;
            }
        }
    }

    return ERR_OK;
}

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

ErrorCode analyseSemantics(Node *program, ParseError *error) {
    if (program == NULL || error == NULL) {
        return setParserErrorAt(error, ERR_INTERNAL, 0, 0, "invalid analyseSemantics arguments");
    }

    size_t functionCount = countNodes(program->body);
    SemanticContext context = {.functionsTable = symtable_create(),
                               .error = error,
                               .program = program,
                               .states = calloc(functionCount + 1,
                                                sizeof(BodyState)), // +1, so calloc doesn't return
                                                                    // NULL when program is empty
                               .callChainStack = calloc(functionCount + 1, sizeof(Node *))};

    ErrorCode code = ERR_OK;
    if (context.functionsTable == NULL || context.states == NULL ||
        context.callChainStack == NULL) {
        code = setParserErrorAt(error, ERR_INTERNAL, 0, 0, "out of memory");
    }

    if (code == ERR_OK) {
        code = insertBuiltins(&context);
    }

    if (code == ERR_OK) {
        code = analyseFunctionHeaders(&context, program); // first pass
    }

    if (code == ERR_OK) {
        code = analyseFunctionBodies(&context); // second pass
    }

    symtable_destroy(context.functionsTable);
    destroyScopeStack(&context);
    free(context.states);
    free(context.callChainStack);
    return code;
}
