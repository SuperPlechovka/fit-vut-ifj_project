#include "ast.h"
#include <stdlib.h>
#include <string.h>

static char *dupName(const char *s) {
    char *p;
    if (s == NULL)
        return NULL;
    p = malloc(strlen(s) + 1);
    if (p != NULL)
        strcpy(p, s);
    return p;
}

Node *nodeNew(NodeKind kind) {
    Node *n = calloc(1, sizeof *n);
    if (n != NULL)
        n->kind = kind;
    return n;
}

void nodeFree(Node *n) {
    Node *next;
    while (n != NULL) {
        next = n->next;
        nodeFree(n->left);
        nodeFree(n->right);
        nodeFree(n->args);
        nodeFree(n->body);
        free(n->name);
        free(n);
        n = next;
    }
}

Node *nodeLitInt(long long v) {
    Node *n = nodeNew(N_LIT_INT);
    if (n != NULL) {
        n->ival = v;
        n->type = TY_INT;
    }
    return n;
}

Node *nodeLitDouble(double v) {
    Node *n = nodeNew(N_LIT_DOUBLE);
    if (n != NULL) {
        n->dval = v;
        n->type = TY_DOUBLE;
    }
    return n;
}

Node *nodeLitString(const char *s) {
    Node *n = nodeNew(N_LIT_STRING);
    if (n == NULL)
        return NULL;
    n->name = dupName(s);
    if (n->name == NULL) {
        free(n);
        return NULL;
    }
    n->type = TY_STRING;
    return n;
}

Node *nodeVar(const char *name) {
    Node *n = nodeNew(N_VAR);
    if (n == NULL)
        return NULL;
    n->name = dupName(name);
    if (n->name == NULL) {
        free(n);
        return NULL;
    }
    return n;
}

Node *nodeBinary(OpKind op, Node *l, Node *r) {
    Node *n = nodeNew(N_BINARY);
    if (n == NULL) {
        nodeFree(l);
        nodeFree(r);
        return NULL;
    }
    n->op = op;
    n->left = l;
    n->right = r;
    return n;
}

Node *nodeUnary(OpKind op, Node *operand) {
    Node *n = nodeNew(N_UNARY);
    if (n == NULL) {
        nodeFree(operand);
        return NULL;
    }
    n->op = op;
    n->left = operand;
    return n;
}

Node *nodeCall(const char *name, Node *args) {
    Node *n = nodeNew(N_CALL);
    if (n == NULL) {
        nodeFree(args);
        return NULL;
    }
    n->name = dupName(name);
    if (n->name == NULL) {
        nodeFree(args);
        free(n);
        return NULL;
    }
    n->args = args;
    return n;
}

Node *nodeDecl(const char *name, int is_var, TypeKind declared, Node *init) {
    Node *n = nodeNew(N_DECL);
    if (n == NULL) {
        nodeFree(init);
        return NULL;
    }
    n->name = dupName(name);
    if (n->name == NULL) {
        nodeFree(init);
        free(n);
        return NULL;
    }
    n->is_var = is_var;
    n->type = declared;
    n->left = init;
    return n;
}

Node *nodeAssign(const char *name, Node *expr) {
    Node *n = nodeNew(N_ASSIGN);
    if (n == NULL) {
        nodeFree(expr);
        return NULL;
    }
    n->name = dupName(name);
    if (n->name == NULL) {
        nodeFree(expr);
        free(n);
        return NULL;
    }
    n->left = expr;
    return n;
}

Node *nodeWrite(Node *expr) {
    Node *n = nodeNew(N_WRITE);
    if (n == NULL) {
        nodeFree(expr);
        return NULL;
    }
    n->left = expr;
    return n;
}

Node *nodeReturn(Node *expr) {
    Node *n = nodeNew(N_RETURN);
    if (n == NULL) {
        nodeFree(expr);
        return NULL;
    }
    n->left = expr;
    return n;
}

Node *nodeIf(Node *cond, Node *then_body, Node *else_body) {
    Node *n = nodeNew(N_IF);
    if (n == NULL) {
        nodeFree(cond);
        nodeFree(then_body);
        nodeFree(else_body);
        return NULL;
    }
    n->left = cond;
    n->body = then_body;
    n->right = else_body;
    return n;
}

Node *nodeWhile(Node *cond, Node *body) {
    Node *n = nodeNew(N_WHILE);
    if (n == NULL) {
        nodeFree(cond);
        nodeFree(body);
        return NULL;
    }
    n->left = cond;
    n->body = body;
    return n;
}

Node *nodeFunc(const char *name, int is_main, TypeKind ret, Node *body) {
    Node *n = nodeNew(N_FUNC);
    if (n == NULL) {
        nodeFree(body);
        return NULL;
    }
    n->name = dupName(name);
    if (n->name == NULL) {
        nodeFree(body);
        free(n);
        return NULL;
    }
    n->is_main = is_main;
    n->type = ret;
    n->body = body;
    return n;
}

Node *nodeProgram(Node *funcs) {
    Node *n = nodeNew(N_PROGRAM);
    if (n == NULL) {
        nodeFree(funcs);
        return NULL;
    }
    n->body = funcs;
    return n;
}

void nodeSetPos(Node *n, int line, int col) {
    if (n != NULL) {
        n->line = line;
        n->col = col;
    }
}
Node *nodeLitUnit(void) {
    Node *n = nodeNew(N_LIT_UNIT);
    if (n == NULL) {
        return NULL;
    }
    n->type = TY_UNIT;
    return n;
}
void nodeAppend(Node **list, Node *node) {
    if (list == NULL || node == NULL) {
        return;
    }

    if (*list == NULL) {
        *list = node;
        return;
    }

    Node *current = *list;
    while (current->next != NULL) {
        current = current->next;
    }
    current->next = node;
}
Node *nodeParam(const char *name, TypeKind type) {
    Node *n = nodeNew(N_PARAM);
    if (n == NULL) {
        return NULL;
    }
    n->name = dupName(name);
    if (n->name == NULL) {
        free(n);
        return NULL;
    }
    n->type = type;
    return n;
}
