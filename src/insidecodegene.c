#include "insidecodegene.h"
#include "ast.h"
#include "ilist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ScopeBinding {
    char *orig_name;
    char *frame_name;
    TypeKind type;
    int depth;
    struct ScopeBinding *next;
} ScopeBinding;

typedef struct NameNode {
    char *name;
    struct NameNode *next;
} NameNode;

typedef struct {
    ListOfInstr decls;
    ListOfInstr body;
    int tmp;
    int lbl_seq;
    int err;
    Frame frame;
    TypeKind ret_type;
    const Node *funcs;
    ScopeBinding *bindings;
    NameNode *used_names;
    int scope_depth;
    int rename_seq;
} Gen;

static void fail(Gen *g, const char *msg) {
    if (!g->err)
        fprintf(stderr, "Chyba generování kódu: %s\n", msg);
    g->err = 99;
}

static void *xcalloc(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (p == NULL) {
        fprintf(stderr, "Interní chyba: nedostatek paměti\n");
        exit(99);
    }
    return p;
}

static char *xstrdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = xcalloc(len, 1);
    memcpy(p, s, len);
    return p;
}

static void dropOp(Operand *o) {
    free(o->name);
    o->name = NULL;
    o->kind = OPD_NONE;
}

static void appendList(ListOfInstr *dst, ListOfInstr *src) {
    if (src->first == NULL)
        return;
    if (dst->first == NULL) {
        *dst = *src;
    } else {
        dst->last->next = src->first;
        dst->last = src->last;
    }
    src->first = NULL;
    src->last = NULL;
}

static int isNameUsed(const Gen *g, const char *name) {
    const NameNode *it;
    for (it = g->used_names; it != NULL; it = it->next) {
        if (strcmp(it->name, name) == 0)
            return 1;
    }
    return 0;
}

static void markNameUsed(Gen *g, const char *name) {
    NameNode *node = xcalloc(1, sizeof *node);
    node->name = xstrdup(name);
    node->next = g->used_names;
    g->used_names = node;
}

static void scopeEnter(Gen *g) { g->scope_depth++; }

static void scopeLeave(Gen *g) {
    while (g->bindings != NULL && g->bindings->depth == g->scope_depth) {
        ScopeBinding *next = g->bindings->next;
        free(g->bindings->orig_name);
        free(g->bindings->frame_name);
        free(g->bindings);
        g->bindings = next;
    }
    g->scope_depth--;
}

static void scopeFreeAll(Gen *g) {
    while (g->bindings != NULL) {
        ScopeBinding *next = g->bindings->next;
        free(g->bindings->orig_name);
        free(g->bindings->frame_name);
        free(g->bindings);
        g->bindings = next;
    }
    while (g->used_names != NULL) {
        NameNode *next = g->used_names->next;
        free(g->used_names->name);
        free(g->used_names);
        g->used_names = next;
    }
}

static const char *scopeDeclare(Gen *g, const char *orig, TypeKind type) {
    ScopeBinding *b = xcalloc(1, sizeof *b);
    b->orig_name = xstrdup(orig);
    b->type = type;
    b->depth = g->scope_depth;

    if (!isNameUsed(g, orig)) {
        b->frame_name = xstrdup(orig);
    } else {
        char buf[128];
        do {
            g->rename_seq++;
            snprintf(buf, sizeof buf, "%s$%d", orig, g->rename_seq);
        } while (isNameUsed(g, buf));
        b->frame_name = xstrdup(buf);
    }

    markNameUsed(g, b->frame_name);
    b->next = g->bindings;
    g->bindings = b;
    return b->frame_name;
}

static const char *scopeResolve(const Gen *g, const char *orig) {
    const ScopeBinding *b;
    for (b = g->bindings; b != NULL; b = b->next) {
        if (strcmp(b->orig_name, orig) == 0)
            return b->frame_name;
    }
    return orig;
}

static TypeKind scopeResolveType(const Gen *g, const char *orig) {
    const ScopeBinding *b;
    for (b = g->bindings; b != NULL; b = b->next) {
        if (strcmp(b->orig_name, orig) == 0)
            return b->type;
    }
    return TY_UNKNOWN;
}

static const Node *findFunc(const Node *funcs, const char *name) {
    const Node *f;
    for (f = funcs; f != NULL; f = f->next) {
        if (f->kind == N_FUNC && f->name != NULL && strcmp(f->name, name) == 0)
            return f;
    }
    return NULL;
}

static int isExprKind(NodeKind kind) {
    switch (kind) {
    case N_LIT_INT:
    case N_LIT_UNIT:
    case N_LIT_DOUBLE:
    case N_LIT_STRING:
    case N_VAR:
    case N_BINARY:
    case N_UNARY:
    case N_CALL:
        return 1;
    default:
        return 0;
    }
}

static Operand tmpOp(const Gen *g, int idx) {
    char buf[32];
    snprintf(buf, sizeof buf, "$t%d", idx);
    return opVar(g->frame, buf);
}

static int newTemp(Gen *g) {
    int idx = g->tmp++;
    emit(&g->decls, I_DEFVAR, tmpOp(g, idx), opNone(), opNone());
    return idx;
}

static Operand funcLabel(const char *name) {
    size_t len = strlen(name) + 4;
    char *buf = xcalloc(len, 1);
    Operand o;
    snprintf(buf, len, "$f_%s", name);
    o = opLabel(buf);
    free(buf);
    return o;
}

static Operand coerce(Gen *g, Operand val, TypeKind src, TypeKind dst) {
    if (src == TY_INT && dst == TY_DOUBLE) {
        int idx = newTemp(g);
        emit(&g->body, I_INT2FLOAT, tmpOp(g, idx), val, opNone());
        return tmpOp(g, idx);
    }
    return val;
}

static Operand genExpr(Gen *g, const Node *n);
static void genStmt(Gen *g, const Node *n);
static void genList(Gen *g, const Node *list);

static Operand genLogic(Gen *g, const Node *n) {
    int is_and = (n->op == OP_AND);
    int id = g->lbl_seq++;
    char s_end[48];
    Operand l, r;
    int idx;

    snprintf(s_end, sizeof s_end, "$%s_end_%d", is_and ? "and" : "or", id);

    l = genExpr(g, n->left);
    if (g->err) {
        dropOp(&l);
        return opNone();
    }

    idx = newTemp(g);
    emit(&g->body, I_MOVE, tmpOp(g, idx), l, opNone());
    emit(&g->body, I_JUMPIFEQ, opLabel(s_end), tmpOp(g, idx), opBool(is_and ? 0 : 1));

    r = genExpr(g, n->right);
    if (g->err) {
        dropOp(&r);
        return opNone();
    }
    emit(&g->body, I_MOVE, tmpOp(g, idx), r, opNone());
    emit(&g->body, I_LABEL, opLabel(s_end), opNone(), opNone());
    return tmpOp(g, idx);
}

static Operand genBinary(Gen *g, const Node *n) {
    Operand l, r;
    TypeKind target_type;
    int idx;

    if (n->op == OP_AND || n->op == OP_OR)
        return genLogic(g, n);

    l = genExpr(g, n->left);
    r = genExpr(g, n->right);
    if (g->err) {
        dropOp(&l);
        dropOp(&r);
        return opNone();
    }

    if (n->op >= OP_LT && n->op <= OP_NE) {
        if (n->left->type == TY_DOUBLE || n->right->type == TY_DOUBLE)
            target_type = TY_DOUBLE;
        else
            target_type = n->left->type;
    } else {
        target_type = n->type;
    }

    l = coerce(g, l, n->left->type, target_type);
    r = coerce(g, r, n->right->type, target_type);

    switch (n->op) {
    case OP_ADD:
        idx = newTemp(g);
        emit(&g->body, (n->type == TY_STRING) ? I_CONCAT : I_ADD, tmpOp(g, idx), l, r);
        return tmpOp(g, idx);
    case OP_SUB:
        idx = newTemp(g);
        emit(&g->body, I_SUB, tmpOp(g, idx), l, r);
        return tmpOp(g, idx);
    case OP_MUL:
        idx = newTemp(g);
        emit(&g->body, I_MUL, tmpOp(g, idx), l, r);
        return tmpOp(g, idx);
    case OP_DIV:
        idx = newTemp(g);
        emit(&g->body, (target_type == TY_DOUBLE) ? I_DIV : I_IDIV, tmpOp(g, idx), l, r);
        return tmpOp(g, idx);
    case OP_LT:
        idx = newTemp(g);
        emit(&g->body, I_LT, tmpOp(g, idx), l, r);
        return tmpOp(g, idx);
    case OP_GT:
        idx = newTemp(g);
        emit(&g->body, I_GT, tmpOp(g, idx), l, r);
        return tmpOp(g, idx);
    case OP_EQ:
        idx = newTemp(g);
        emit(&g->body, I_EQ, tmpOp(g, idx), l, r);
        return tmpOp(g, idx);
    case OP_NE: {
        int eq_idx = newTemp(g);
        idx = newTemp(g);
        emit(&g->body, I_EQ, tmpOp(g, eq_idx), l, r);
        emit(&g->body, I_NOT, tmpOp(g, idx), tmpOp(g, eq_idx), opNone());
        return tmpOp(g, idx);
    }
    case OP_LE: {
        int gt_idx = newTemp(g);
        idx = newTemp(g);
        emit(&g->body, I_GT, tmpOp(g, gt_idx), l, r);
        emit(&g->body, I_NOT, tmpOp(g, idx), tmpOp(g, gt_idx), opNone());
        return tmpOp(g, idx);
    }
    case OP_GE: {
        int lt_idx = newTemp(g);
        idx = newTemp(g);
        emit(&g->body, I_LT, tmpOp(g, lt_idx), l, r);
        emit(&g->body, I_NOT, tmpOp(g, idx), tmpOp(g, lt_idx), opNone());
        return tmpOp(g, idx);
    }
    default:
        dropOp(&l);
        dropOp(&r);
        fail(g, "Nepodporovaná binární operace");
        return opNone();
    }
}

static Operand genUnary(Gen *g, const Node *n) {
    Operand val = genExpr(g, n->left);
    int idx;

    if (g->err) {
        dropOp(&val);
        return opNone();
    }
    if (n->op != OP_NEG) {
        dropOp(&val);
        fail(g, "Nepodporovaná unární operace");
        return opNone();
    }

    idx = newTemp(g);
    emit(&g->body, I_SUB, tmpOp(g, idx), (n->left->type == TY_DOUBLE) ? opFloat(0.0) : opInt(0),
         val);
    return tmpOp(g, idx);
}

static int genBuiltinCall(Gen *g, const Node *n, const Operand *vals, int argc, int want_result,
                          Operand *out) {
    if (strcmp(n->name, "print") == 0) {
        if (argc != 1) {
            fail(g, "print vyžaduje 1 argument");
            return 1;
        }
        if (n->args->type == TY_UNIT) {
            emit(&g->body, I_WRITE, opString("()"), opNone(), opNone());
        } else {
            emit(&g->body, I_WRITE, opDup(vals[0]), opNone(), opNone());
        }
        if (want_result)
            *out = opNil();
        return 1;
    }

    if (strcmp(n->name, "readInt") == 0) {
        int idx;
        if (argc != 0) {
            fail(g, "readInt nevyžaduje žádné argumenty");
            return 1;
        }
        idx = newTemp(g);
        emit(&g->body, I_READ, tmpOp(g, idx), opType("int"), opNone());
        if (want_result)
            *out = tmpOp(g, idx);
        return 1;
    }

    if (strcmp(n->name, "readDouble") == 0) {
        int idx;
        if (argc != 0) {
            fail(g, "readDouble nevyžaduje žádné argumenty");
            return 1;
        }
        idx = newTemp(g);
        emit(&g->body, I_READ, tmpOp(g, idx), opType("float"), opNone());
        if (want_result)
            *out = tmpOp(g, idx);
        return 1;
    }

    if (strcmp(n->name, "readString") == 0) {
        int idx;
        if (argc != 0) {
            fail(g, "readString nevyžaduje žádné argumenty");
            return 1;
        }
        idx = newTemp(g);
        emit(&g->body, I_READ, tmpOp(g, idx), opType("string"), opNone());
        if (want_result)
            *out = tmpOp(g, idx);
        return 1;
    }

    if (strcmp(n->name, "toStr") == 0) {
        int idx;
        TypeKind arg_ty;
        if (argc != 1) {
            fail(g, "toStr vyžaduje 1 argument");
            return 1;
        }
        idx = newTemp(g);
        arg_ty = n->args->type;
        if (arg_ty == TY_STRING) {
            emit(&g->body, I_MOVE, tmpOp(g, idx), opDup(vals[0]), opNone());
        } else if (arg_ty == TY_INT) {
            emit(&g->body, I_INT2STR, tmpOp(g, idx), opDup(vals[0]), opNone());
        } else if (arg_ty == TY_DOUBLE) {
            emit(&g->body, I_FLOAT2STR, tmpOp(g, idx), opDup(vals[0]), opNone());
        } else if (arg_ty == TY_UNIT) {
            emit(&g->body, I_MOVE, tmpOp(g, idx), opString("()"), opNone());
        } else {
            int t_ty = newTemp(g);
            int id = g->lbl_seq++;
            char l_int[48], l_flt[48], l_str[48], l_end[48];
            snprintf(l_int, sizeof l_int, "$tostr_int_%d", id);
            snprintf(l_flt, sizeof l_flt, "$tostr_flt_%d", id);
            snprintf(l_str, sizeof l_str, "$tostr_str_%d", id);
            snprintf(l_end, sizeof l_end, "$tostr_end_%d", id);

            emit(&g->body, I_TYPE, tmpOp(g, t_ty), opDup(vals[0]), opNone());
            emit(&g->body, I_JUMPIFEQ, opLabel(l_int), tmpOp(g, t_ty), opString("int"));
            emit(&g->body, I_JUMPIFEQ, opLabel(l_flt), tmpOp(g, t_ty), opString("float"));
            emit(&g->body, I_JUMPIFEQ, opLabel(l_str), tmpOp(g, t_ty), opString("string"));
            emit(&g->body, I_MOVE, tmpOp(g, idx), opString("()"), opNone());
            emit(&g->body, I_JUMP, opLabel(l_end), opNone(), opNone());

            emit(&g->body, I_LABEL, opLabel(l_int), opNone(), opNone());
            emit(&g->body, I_INT2STR, tmpOp(g, idx), opDup(vals[0]), opNone());
            emit(&g->body, I_JUMP, opLabel(l_end), opNone(), opNone());

            emit(&g->body, I_LABEL, opLabel(l_flt), opNone(), opNone());
            emit(&g->body, I_FLOAT2STR, tmpOp(g, idx), opDup(vals[0]), opNone());
            emit(&g->body, I_JUMP, opLabel(l_end), opNone(), opNone());

            emit(&g->body, I_LABEL, opLabel(l_str), opNone(), opNone());
            emit(&g->body, I_MOVE, tmpOp(g, idx), opDup(vals[0]), opNone());
            emit(&g->body, I_LABEL, opLabel(l_end), opNone(), opNone());
        }
        if (want_result)
            *out = tmpOp(g, idx);
        return 1;
    }

    if (strcmp(n->name, "strLength") == 0) {
        int idx;
        if (argc != 1) {
            fail(g, "strLength vyžaduje 1 argument");
            return 1;
        }
        idx = newTemp(g);
        emit(&g->body, I_STRLEN, tmpOp(g, idx), opDup(vals[0]), opNone());
        if (want_result)
            *out = tmpOp(g, idx);
        return 1;
    }

    if (strcmp(n->name, "subStr") == 0) {
        int res, len, cond, idx, cnt, ch, id;
        char l_loop[48], l_end[48];
        if (argc != 3) {
            fail(g, "subStr vyžaduje 3 argumenty");
            return 1;
        }
        id = g->lbl_seq++;
        snprintf(l_loop, sizeof l_loop, "$substr_loop_%d", id);
        snprintf(l_end, sizeof l_end, "$substr_end_%d", id);

        res = newTemp(g);
        len = newTemp(g);
        cond = newTemp(g);
        idx = newTemp(g);
        cnt = newTemp(g);
        ch = newTemp(g);

        emit(&g->body, I_MOVE, tmpOp(g, res), opString(""), opNone());
        emit(&g->body, I_STRLEN, tmpOp(g, len), opDup(vals[0]), opNone());
        emit(&g->body, I_MOVE, tmpOp(g, idx), opDup(vals[1]), opNone());
        emit(&g->body, I_MOVE, tmpOp(g, cnt), opInt(0), opNone());

        emit(&g->body, I_LT, tmpOp(g, cond), tmpOp(g, idx), opInt(0));
        emit(&g->body, I_JUMPIFEQ, opLabel(l_end), tmpOp(g, cond), opBool(1));
        emit(&g->body, I_GT, tmpOp(g, cond), opDup(vals[2]), opInt(0));
        emit(&g->body, I_JUMPIFEQ, opLabel(l_end), tmpOp(g, cond), opBool(0));

        emit(&g->body, I_LABEL, opLabel(l_loop), opNone(), opNone());
        emit(&g->body, I_LT, tmpOp(g, cond), tmpOp(g, idx), tmpOp(g, len));
        emit(&g->body, I_JUMPIFEQ, opLabel(l_end), tmpOp(g, cond), opBool(0));
        emit(&g->body, I_LT, tmpOp(g, cond), tmpOp(g, cnt), opDup(vals[2]));
        emit(&g->body, I_JUMPIFEQ, opLabel(l_end), tmpOp(g, cond), opBool(0));

        emit(&g->body, I_GETCHAR, tmpOp(g, ch), opDup(vals[0]), tmpOp(g, idx));
        emit(&g->body, I_CONCAT, tmpOp(g, res), tmpOp(g, res), tmpOp(g, ch));
        emit(&g->body, I_ADD, tmpOp(g, idx), tmpOp(g, idx), opInt(1));
        emit(&g->body, I_ADD, tmpOp(g, cnt), tmpOp(g, cnt), opInt(1));
        emit(&g->body, I_JUMP, opLabel(l_loop), opNone(), opNone());

        emit(&g->body, I_LABEL, opLabel(l_end), opNone(), opNone());
        if (want_result)
            *out = tmpOp(g, res);
        return 1;
    }

    if (strcmp(n->name, "strcmp") == 0) {
        int res, cond, id;
        char l_lt[48], l_gt[48], l_end[48];
        if (argc != 2) {
            fail(g, "strcmp vyžaduje 2 argumenty");
            return 1;
        }
        id = g->lbl_seq++;
        snprintf(l_lt, sizeof l_lt, "$strcmp_lt_%d", id);
        snprintf(l_gt, sizeof l_gt, "$strcmp_gt_%d", id);
        snprintf(l_end, sizeof l_end, "$strcmp_end_%d", id);

        res = newTemp(g);
        cond = newTemp(g);

        emit(&g->body, I_LT, tmpOp(g, cond), opDup(vals[0]), opDup(vals[1]));
        emit(&g->body, I_JUMPIFEQ, opLabel(l_lt), tmpOp(g, cond), opBool(1));
        emit(&g->body, I_GT, tmpOp(g, cond), opDup(vals[0]), opDup(vals[1]));
        emit(&g->body, I_JUMPIFEQ, opLabel(l_gt), tmpOp(g, cond), opBool(1));
        emit(&g->body, I_MOVE, tmpOp(g, res), opInt(0), opNone());
        emit(&g->body, I_JUMP, opLabel(l_end), opNone(), opNone());

        emit(&g->body, I_LABEL, opLabel(l_lt), opNone(), opNone());
        emit(&g->body, I_MOVE, tmpOp(g, res), opInt(-1), opNone());
        emit(&g->body, I_JUMP, opLabel(l_end), opNone(), opNone());

        emit(&g->body, I_LABEL, opLabel(l_gt), opNone(), opNone());
        emit(&g->body, I_MOVE, tmpOp(g, res), opInt(1), opNone());

        emit(&g->body, I_LABEL, opLabel(l_end), opNone(), opNone());
        if (want_result)
            *out = tmpOp(g, res);
        return 1;
    }

    if (strcmp(n->name, "ord") == 0) {
        int res, len, cond, id;
        char l_end[48];
        if (argc != 2) {
            fail(g, "ord vyžaduje 2 argumenty");
            return 1;
        }
        id = g->lbl_seq++;
        snprintf(l_end, sizeof l_end, "$ord_end_%d", id);

        res = newTemp(g);
        len = newTemp(g);
        cond = newTemp(g);

        emit(&g->body, I_MOVE, tmpOp(g, res), opInt(0), opNone());
        emit(&g->body, I_STRLEN, tmpOp(g, len), opDup(vals[0]), opNone());
        emit(&g->body, I_LT, tmpOp(g, cond), opDup(vals[1]), opInt(0));
        emit(&g->body, I_JUMPIFEQ, opLabel(l_end), tmpOp(g, cond), opBool(1));
        emit(&g->body, I_LT, tmpOp(g, cond), opDup(vals[1]), tmpOp(g, len));
        emit(&g->body, I_JUMPIFEQ, opLabel(l_end), tmpOp(g, cond), opBool(0));
        emit(&g->body, I_STRI2INT, tmpOp(g, res), opDup(vals[0]), opDup(vals[1]));
        emit(&g->body, I_LABEL, opLabel(l_end), opNone(), opNone());
        if (want_result)
            *out = tmpOp(g, res);
        return 1;
    }

    if (strcmp(n->name, "chr") == 0) {
        int res;
        if (argc != 1) {
            fail(g, "chr vyžaduje 1 argument");
            return 1;
        }
        res = newTemp(g);
        emit(&g->body, I_INT2CHAR, tmpOp(g, res), opDup(vals[0]), opNone());
        if (want_result)
            *out = tmpOp(g, res);
        return 1;
    }

    return 0;
}

static Operand genCall(Gen *g, const Node *n, int want_result) {
    int argc = 0;
    int i = 0;
    Operand *vals = NULL;
    Operand builtin_out = opNone();
    const Node *a;
    const Node *fn;
    const Node *param;

    for (a = n->args; a != NULL; a = a->next)
        argc++;
    if (argc > 0)
        vals = xcalloc((size_t)argc, sizeof *vals);

    /* nejdřív všechny argumenty zleva doprava, aby vnořená volání nepřepsala TF */
    for (a = n->args; a != NULL && !g->err; a = a->next, i++)
        vals[i] = genExpr(g, a);

    if (g->err) {
        for (i = 0; i < argc; i++)
            dropOp(&vals[i]);
        free(vals);
        return opNone();
    }

    if (genBuiltinCall(g, n, vals, argc, want_result, &builtin_out)) {
        for (i = 0; i < argc; i++)
            dropOp(&vals[i]);
        free(vals);
        return builtin_out;
    }

    /* konverze argumentů Int -> Double podle signatury volané funkce */
    fn = findFunc(g->funcs, n->name);
    param = (fn != NULL) ? fn->args : NULL;
    for (a = n->args, i = 0; a != NULL && param != NULL; a = a->next, param = param->next, i++) {
        vals[i] = coerce(g, vals[i], a->type, param->type);
    }

    emit(&g->body, I_CREATEFRAME, opNone(), opNone(), opNone());
    for (i = 0; i < argc; i++) {
        char pbuf[32];
        snprintf(pbuf, sizeof pbuf, "$%d", i + 1);
        emit(&g->body, I_DEFVAR, opVar(FR_TF, pbuf), opNone(), opNone());
        emit(&g->body, I_MOVE, opVar(FR_TF, pbuf), vals[i], opNone());
    }
    free(vals);

    emit(&g->body, I_CALL, funcLabel(n->name), opNone(), opNone());

    if (want_result) {
        int idx = newTemp(g);
        emit(&g->body, I_MOVE, tmpOp(g, idx), opVar(FR_GF, "$retval"), opNone());
        return tmpOp(g, idx);
    }
    return opNone();
}

static Operand genExpr(Gen *g, const Node *n) {
    if (n == NULL)
        return opNone();

    switch (n->kind) {
    case N_LIT_INT:
        return opInt(n->ival);
    case N_LIT_UNIT:
        return opNil();
    case N_LIT_DOUBLE:
        return opFloat(n->dval);
    case N_LIT_STRING:
        return opString(n->name);
    case N_VAR:
        return opVar(g->frame, scopeResolve(g, n->name));
    case N_BINARY:
        return genBinary(g, n);
    case N_UNARY:
        return genUnary(g, n);
    case N_CALL:
        return genCall(g, n, 1);
    default:
        fail(g, "Neznámý druh výrazu");
        return opNone();
    }
}

static void genIf(Gen *g, const Node *n) {
    int id = g->lbl_seq++;
    char s_else[48], s_end[48];
    Operand cond;

    snprintf(s_else, sizeof s_else, "$if_else_%d", id);
    snprintf(s_end, sizeof s_end, "$if_end_%d", id);

    cond = genExpr(g, n->left);
    if (g->err) {
        dropOp(&cond);
        return;
    }

    emit(&g->body, I_JUMPIFEQ, opLabel(n->right != NULL ? s_else : s_end), cond, opBool(0));
    genList(g, n->body);

    if (n->right != NULL) {
        emit(&g->body, I_JUMP, opLabel(s_end), opNone(), opNone());
        emit(&g->body, I_LABEL, opLabel(s_else), opNone(), opNone());
        genList(g, n->right);
    }
    emit(&g->body, I_LABEL, opLabel(s_end), opNone(), opNone());
}

static void genWhile(Gen *g, const Node *n) {
    int id = g->lbl_seq++;
    char s_cond[48], s_end[48];
    Operand cond;

    snprintf(s_cond, sizeof s_cond, "$while_cond_%d", id);
    snprintf(s_end, sizeof s_end, "$while_end_%d", id);

    emit(&g->body, I_LABEL, opLabel(s_cond), opNone(), opNone());

    cond = genExpr(g, n->left);
    if (g->err) {
        dropOp(&cond);
        return;
    }

    emit(&g->body, I_JUMPIFEQ, opLabel(s_end), cond, opBool(0));
    genList(g, n->body);
    emit(&g->body, I_JUMP, opLabel(s_cond), opNone(), opNone());
    emit(&g->body, I_LABEL, opLabel(s_end), opNone(), opNone());
}

static void genStmt(Gen *g, const Node *n) {
    Operand val;

    if (n == NULL || g->err)
        return;

    switch (n->kind) {
    case N_DECL: {
        const char *var_name;
        val = opNone();
        if (n->left != NULL) {
            val = genExpr(g, n->left);
            if (g->err) {
                dropOp(&val);
                break;
            }
            val = coerce(g, val, n->left->type, n->type);
        }
        var_name = scopeDeclare(g, n->name, n->type);
        emit(&g->decls, I_DEFVAR, opVar(g->frame, var_name), opNone(), opNone());
        if (n->left != NULL) {
            emit(&g->body, I_MOVE, opVar(g->frame, var_name), val, opNone());
        }
        break;
    }

    case N_ASSIGN: {
        TypeKind dst_type = (n->type != TY_UNKNOWN) ? n->type : scopeResolveType(g, n->name);
        val = genExpr(g, n->left);
        if (g->err) {
            dropOp(&val);
            break;
        }
        val = coerce(g, val, n->left->type, dst_type);
        emit(&g->body, I_MOVE, opVar(g->frame, scopeResolve(g, n->name)), val, opNone());
        break;
    }

    case N_WRITE:
        if (n->left != NULL && n->left->type == TY_UNIT) {
            emit(&g->body, I_WRITE, opString("()"), opNone(), opNone());
            break;
        }
        val = genExpr(g, n->left);
        if (g->err) {
            dropOp(&val);
            break;
        }
        emit(&g->body, I_WRITE, val, opNone(), opNone());
        break;

    case N_RETURN:
        if (n->left != NULL) {
            val = genExpr(g, n->left);
            if (g->err) {
                dropOp(&val);
                break;
            }
            if (n->left->type == TY_UNIT || g->ret_type == TY_UNIT) {
                dropOp(&val);
                emit(&g->body, I_MOVE, opVar(FR_GF, "$retval"), opNil(), opNone());
            } else {
                TypeKind target_ret = (n->type != TY_UNKNOWN) ? n->type : g->ret_type;
                val = coerce(g, val, n->left->type, target_ret);
                emit(&g->body, I_MOVE, opVar(FR_GF, "$retval"), val, opNone());
            }
        } else {
            emit(&g->body, I_MOVE, opVar(FR_GF, "$retval"), opNil(), opNone());
        }
        if (g->frame == FR_LF) {
            emit(&g->body, I_POPFRAME, opNone(), opNone(), opNone());
            emit(&g->body, I_RETURN, opNone(), opNone(), opNone());
        } else {
            emit(&g->body, I_EXIT, opInt(0), opNone(), opNone());
        }
        break;

    case N_IF:
        genIf(g, n);
        break;

    case N_WHILE:
        genWhile(g, n);
        break;

    case N_CALL:
        val = genCall(g, n, 0);
        dropOp(&val);
        break;

    case N_LIT_UNIT:
        break;

    case N_LIT_INT:
    case N_LIT_DOUBLE:
    case N_LIT_STRING:
    case N_VAR:
    case N_BINARY:
    case N_UNARY:
        val = genExpr(g, n);
        dropOp(&val);
        break;

    default:
        fail(g, "Neznámý druh příkazu");
        break;
    }
}

static void genList(Gen *g, const Node *list) {
    const Node *curr;
    scopeEnter(g);
    for (curr = list; curr != NULL && !g->err; curr = curr->next)
        genStmt(g, curr);
    scopeLeave(g);
}

static int genFunc(const Node *f, const Node *funcs, ListOfInstr *out, int *lbl_seq) {
    Gen g;
    ListOfInstr head;

    listInit(&g.decls);
    listInit(&g.body);
    listInit(&head);
    g.tmp = 0;
    g.lbl_seq = *lbl_seq;
    g.err = 0;
    g.frame = f->is_main ? FR_GF : FR_LF;
    g.ret_type = f->type;
    g.funcs = funcs;
    g.bindings = NULL;
    g.used_names = NULL;
    g.scope_depth = 0;
    g.rename_seq = 0;

    if (f->is_main) {
        emit(&head, I_LABEL, opLabel("$main"), opNone(), opNone());
    } else {
        const Node *p;
        int param_idx = 1;

        emit(&head, I_LABEL, funcLabel(f->name), opNone(), opNone());
        emit(&head, I_PUSHFRAME, opNone(), opNone(), opNone());
        for (p = f->args; p != NULL; p = p->next, param_idx++) {
            char pbuf[32];
            const char *pname = scopeDeclare(&g, p->name, p->type); // <-- tady doplnit p->type
            snprintf(pbuf, sizeof pbuf, "$%d", param_idx);
            emit(&head, I_DEFVAR, opVar(FR_LF, pname), opNone(), opNone());
            emit(&head, I_MOVE, opVar(FR_LF, pname), opVar(FR_LF, pbuf), opNone());
        }
    }

    if (!f->is_main && f->body != NULL && f->body->next == NULL && isExprKind(f->body->kind)) {
        /* jednořádková funkce s implicitním návratem výrazu: def answer(): Int = 42 */
        if (f->type == TY_UNIT || f->body->type == TY_UNIT) {
            genStmt(&g, f->body);
            if (!g.err) {
                emit(&g.body, I_MOVE, opVar(FR_GF, "$retval"), opNil(), opNone());
            }
        } else {
            Operand val = genExpr(&g, f->body);
            if (!g.err) {
                TypeKind dst_ty = (f->type != TY_UNKNOWN) ? f->type : f->body->type;
                val = coerce(&g, val, f->body->type, dst_ty);
                emit(&g.body, I_MOVE, opVar(FR_GF, "$retval"), val, opNone());
            } else {
                dropOp(&val);
            }
        }
        if (!g.err) {
            emit(&g.body, I_POPFRAME, opNone(), opNone(), opNone());
            emit(&g.body, I_RETURN, opNone(), opNone(), opNone());
        }
    } else {
        genList(&g, f->body);
        if (!g.err) {
            if (f->is_main) {
                emit(&g.body, I_EXIT, opInt(0), opNone(), opNone());
            } else {
                emit(&g.body, I_MOVE, opVar(FR_GF, "$retval"), opNil(), opNone());
                emit(&g.body, I_POPFRAME, opNone(), opNone(), opNone());
                emit(&g.body, I_RETURN, opNone(), opNone(), opNone());
            }
        }
    }

    scopeFreeAll(&g);
    *lbl_seq = g.lbl_seq;

    if (g.err) {
        listFree(&head);
        listFree(&g.decls);
        listFree(&g.body);
        return g.err;
    }

    appendList(out, &head);
    appendList(out, &g.decls);
    appendList(out, &g.body);
    return 0;
}

int codegenProgram(const Node *prog, ListOfInstr *out) {
    const Node *f;
    int lbl_seq = 0;
    int main_count = 0;

    listInit(out);
    if (prog == NULL || prog->kind != N_PROGRAM)
        return 99;

    emit(out, I_DEFVAR, opVar(FR_GF, "$retval"), opNone(), opNone());
    emit(out, I_MOVE, opVar(FR_GF, "$retval"), opNil(), opNone());
    emit(out, I_JUMP, opLabel("$main"), opNone(), opNone());

    for (f = prog->body; f != NULL; f = f->next) {
        int err;

        if (f->kind != N_FUNC) {
            listFree(out);
            return 99;
        }
        if (f->is_main)
            main_count++;

        err = genFunc(f, prog->body, out, &lbl_seq);
        if (err != 0) {
            listFree(out);
            return err;
        }
    }

    if (main_count != 1) {
        listFree(out);
        return 99;
    }
    return 0;
}
