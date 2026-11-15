#include "insidecodegene.h"
#include "ast.h"
#include "ilist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    ListOfInstr decls;
    ListOfInstr body;
    int tmp;
    int lbl_seq;
    int err;
    Frame frame;
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

static Operand genExpr(Gen *g, Node *n);
static void genStmt(Gen *g, Node *n);
static void genList(Gen *g, Node *list);

static Operand genLogic(Gen *g, Node *n) {
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

static Operand genBinary(Gen *g, Node *n) {
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

    target_type = n->type;
    if (n->op >= OP_LT && n->op <= OP_NE) {
        if (n->left->type == TY_DOUBLE || n->right->type == TY_DOUBLE)
            target_type = TY_DOUBLE;
        else
            target_type = n->left->type;
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

static Operand genUnary(Gen *g, Node *n) {
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

static Operand genCall(Gen *g, Node *n, int want_result) {
    int argc = 0;
    int i = 0;
    Operand *vals = NULL;
    Node *a;

    for (a = n->args; a != NULL; a = a->next)
        argc++;
    if (argc > 0)
        vals = xcalloc((size_t)argc, sizeof *vals);

    /* nejdřív všechny argumenty, aby vnořená volání nepřepsala TF */
    for (a = n->args; a != NULL && !g->err; a = a->next, i++)
        vals[i] = genExpr(g, a);

    if (g->err) {
        for (i = 0; i < argc; i++)
            dropOp(&vals[i]);
        free(vals);
        return opNone();
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

static Operand genExpr(Gen *g, Node *n) {
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
        return opVar(g->frame, n->name);
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

static void genIf(Gen *g, Node *n) {
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

static void genWhile(Gen *g, Node *n) {
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

static void genStmt(Gen *g, Node *n) {
    Operand val;

    if (n == NULL || g->err)
        return;

    switch (n->kind) {
    case N_DECL:
        emit(&g->decls, I_DEFVAR, opVar(g->frame, n->name), opNone(), opNone());
        if (n->left != NULL) {
            val = genExpr(g, n->left);
            if (g->err) {
                dropOp(&val);
                break;
            }
            val = coerce(g, val, n->left->type, n->type);
            emit(&g->body, I_MOVE, opVar(g->frame, n->name), val, opNone());
        }
        break;

    case N_ASSIGN:
        val = genExpr(g, n->left);
        if (g->err) {
            dropOp(&val);
            break;
        }
        val = coerce(g, val, n->left->type, n->type);
        emit(&g->body, I_MOVE, opVar(g->frame, n->name), val, opNone());
        break;

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
            val = coerce(g, val, n->left->type, n->type);
            emit(&g->body, I_MOVE, opVar(FR_GF, "$retval"), val, opNone());
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

    default:
        fail(g, "Neznámý druh příkazu");
        break;
    }
}

static void genList(Gen *g, Node *list) {
    Node *curr;
    for (curr = list; curr != NULL && !g->err; curr = curr->next)
        genStmt(g, curr);
}

static int genFunc(Node *f, ListOfInstr *out, int *lbl_seq) {
    Gen g;
    ListOfInstr head;

    listInit(&g.decls);
    listInit(&g.body);
    listInit(&head);
    g.tmp = 0;
    g.lbl_seq = *lbl_seq;
    g.err = 0;
    g.frame = f->is_main ? FR_GF : FR_LF;

    if (f->is_main) {
        emit(&head, I_LABEL, opLabel("$main"), opNone(), opNone());
    } else {
        Node *p;
        int param_idx = 1;

        emit(&head, I_LABEL, funcLabel(f->name), opNone(), opNone());
        emit(&head, I_PUSHFRAME, opNone(), opNone(), opNone());
        for (p = f->args; p != NULL; p = p->next, param_idx++) {
            char pbuf[32];
            snprintf(pbuf, sizeof pbuf, "$%d", param_idx);
            emit(&head, I_DEFVAR, opVar(FR_LF, p->name), opNone(), opNone());
            emit(&head, I_MOVE, opVar(FR_LF, p->name), opVar(FR_LF, pbuf), opNone());
        }
    }

    genList(&g, f->body);

    if (!g.err) {
        if (f->is_main) {
            emit(&g.body, I_EXIT, opInt(0), opNone(), opNone());
        } else {
            emit(&g.body, I_POPFRAME, opNone(), opNone(), opNone());
            emit(&g.body, I_RETURN, opNone(), opNone(), opNone());
        }
    }

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

int codegenProgram(Node *prog, ListOfInstr *out) {
    Node *f;
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

        err = genFunc(f, out, &lbl_seq);
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