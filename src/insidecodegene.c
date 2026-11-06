#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "ilist.h"
#include "insidecodegene.h"

typedef struct {
  ListOfInstr decls;
  ListOfInstr body;
  int tmp;
  int lbl_seq;
  int err;
  Frame frame; 
} Gen;


static void genIf(Gen *g, Node *n);
static void genWhile(Gen *g, Node *n);
static void fail(Gen *g, const char *what)
{
  if (!g->err)
    fprintf(stderr, "Chyba generování kódu: %s\n", what);
  g->err = 99;
}

static const char *typeToString(TypeKind t)
{
  switch (t) {
    case TY_INT:    return "int";
    case TY_DOUBLE: return "float";
    case TY_STRING: return "string";
    case TY_BOOL:   return "bool";
    default:        return "int";
  }
}

static Operand tmpOp(Gen *g, int idx)
{
  char buf[32];
  snprintf(buf, sizeof buf, "$t%d", idx);
  return opVar(g->frame, buf);
}

static int newTemp(Gen *g)
{
  int idx = g->tmp++;
  emit(&g->decls, I_DEFVAR, tmpOp(g, idx), opNone(), opNone());
  return idx;
}

static Operand newLabel(Gen *g, const char *prefix)
{
  char buf[64];
  snprintf(buf, sizeof buf, "%s$%d", prefix, g->lbl_seq++);
  

  char *label_str = strdup(buf);
  return opLabel(label_str);
}

static Operand coerce(Gen *g, Operand v, TypeKind from, TypeKind to)
{
  if (from == TY_INT && to == TY_DOUBLE) {
    int idx = newTemp(g);
    Operand t = tmpOp(g, idx);
    emit(&g->body, I_INT2FLOAT, t, v, opNone());
    return t;
  }
  return v;
}

static Operand genExpr(Gen *g, Node *n);
static void genList(Gen *g, Node *stmt);

static Operand genBinary(Gen *g, Node *n)
{
  Operand l = genExpr(g, n->left);
  Operand r = genExpr(g, n->right);
  int idx;

  /* Určení společného typu operandů */
  TypeKind target_type = n->type;
  if (n->op >= OP_LT && n->op <= OP_NE) {
    if (n->left->type == TY_DOUBLE || n->right->type == TY_DOUBLE) {
      target_type = TY_DOUBLE;
    } else {
      target_type = n->left->type;
    }
  }

  l = coerce(g, l, n->left->type, target_type);
  r = coerce(g, r, n->right->type, target_type);

  switch (n->op) {
    case OP_ADD:
      idx = newTemp(g);
      emit(&g->body, I_ADD, tmpOp(g, idx), l, r);
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
      fail(g, "Nepodporovaná binární operace");
      return opNone();
  }
}

static Operand genUnary(Gen *g, Node *n)
{
  Operand v = genExpr(g, n->left);
  int idx;

  if (n->op != OP_NEG) {
    fail(g, "Nepodporovaná unární operace");
    return opNone();
  }
  v = coerce(g, v, n->left->type, n->type);
  idx = newTemp(g);
  if (n->type == TY_DOUBLE)
    emit(&g->body, I_SUB, tmpOp(g, idx), opFloat(0.0), v);
  else
    emit(&g->body, I_SUB, tmpOp(g, idx), opInt(0), v);
  return tmpOp(g, idx);
}

static Operand genCall(Gen *g, Node *n)
{
  emit(&g->body, I_CREATEFRAME, opNone(), opNone(), opNone());

  int param_idx = 1;
  for (Node *arg = n->args; arg != NULL; arg = arg->next, param_idx++) {
    char pbuf[32];
    snprintf(pbuf, sizeof pbuf, "$%d", param_idx);
    Operand param_op = opVar(FR_TF, pbuf);
    emit(&g->body, I_DEFVAR, param_op, opNone(), opNone());

    Operand val = genExpr(g, arg);
    emit(&g->body, I_MOVE, param_op, val, opNone());
  }

  emit(&g->body, I_CALL, opLabel(n->name), opNone(), opNone());

if (n->type != TY_UNIT && n->type != TY_UNKNOWN) {
  int idx = newTemp(g);
  emit(&g->body, I_MOVE, tmpOp(g, idx), opVar(FR_GF, "$retval"), opNone());
  return tmpOp(g, idx);
}
  return opNone();
}

static Operand genExpr(Gen *g, Node *n)
{
  if (!n) return opNone();

  switch (n->kind) {
    case N_LIT_INT:    return opInt(n->ival);
    case N_LIT_DOUBLE: return opFloat(n->dval);
    case N_LIT_STRING: return opString(n->name);
    case N_VAR:        return opVar(g->frame, n->name);
    case N_BINARY:     return genBinary(g, n);
    case N_UNARY:      return genUnary(g, n);
    case N_CALL:       return genCall(g, n);
    default:
      fail(g, "Výraz tohoto druhu zatím není podporován");
      return opNone();
  }
}

static void genStmt(Gen *g, Node *n)
{
  if (!n || g->err) return;
  Operand v;

  switch (n->kind) {
    case N_DECL:
      emit(&g->decls, I_DEFVAR, opVar(g->frame, n->name), opNone(), opNone());
      if (n->left != NULL) {
        v = genExpr(g, n->left);
        v = coerce(g, v, n->left->type, n->type);
        emit(&g->body, I_MOVE, opVar(g->frame, n->name), v, opNone());
      }
      break;

    case N_ASSIGN:
      v = genExpr(g, n->left);
      v = coerce(g, v, n->left->type, n->type);
      emit(&g->body, I_MOVE, opVar(g->frame, n->name), v, opNone());
      break;

    case N_WRITE:
      v = genExpr(g, n->left);
      emit(&g->body, I_WRITE, v, opNone(), opNone());
      break;

  case N_RETURN:
      if (n->left != NULL) {
        v = genExpr(g, n->left);
        v = coerce(g, v, n->left->type, n->type);
        emit(&g->body, I_MOVE, opVar(FR_GF, "$retval"), v, opNone());
      }
      emit(&g->body, I_POPFRAME, opNone(), opNone(), opNone());
      emit(&g->body, I_RETURN, opNone(), opNone(), opNone());
      break;
  case N_IF:
    genIf(g, n);
    break;
  case N_WHILE:
  genWhile(g, n);
  break;

    default:
      fail(g, "Příkaz tohoto druhu není podporován");
      break;
  }
}

static void genList(Gen *g, Node *stmt)
{
  for (; stmt != NULL && !g->err; stmt = stmt->next)
    genStmt(g, stmt);
}



static int genFunc(Node *f, ListOfInstr *out, int *lbl_seq)
{
  Gen g;
  listInit(&g.decls);
  listInit(&g.body);
  g.tmp = 0;
  g.lbl_seq = *lbl_seq;
  g.err = 0;
  g.frame = f->is_main ? FR_GF : FR_LF;

  ListOfInstr head;
  listInit(&head);

  if (f->is_main) {
    emit(&head, I_LABEL, opLabel("main"), opNone(), opNone());

    
  } else {
    emit(&head, I_LABEL, opLabel(f->name), opNone(), opNone());
    emit(&head, I_PUSHFRAME, opNone(), opNone(), opNone());
  }

  genList(&g, f->body);

  if (!f->is_main) {
    emit(&g.body, I_POPFRAME, opNone(), opNone(), opNone());
    emit(&g.body, I_RETURN, opNone(), opNone(), opNone());
  }

  *lbl_seq = g.lbl_seq;

  ListOfInstr tmp;
  listInit(&tmp);
  joinLists(&tmp, &head, &g.decls);
  joinLists(out, &tmp, &g.body);

  return g.err;
}

int codegenProgram(Node *prog, ListOfInstr *out)
{
  listInit(out);
  if (prog == NULL || prog->kind != N_PROGRAM)
    return 99;

  ListOfInstr header;
  listInit(&header);

  /* Globální deklarace pro návratové hodnoty funkcí */
  emit(&header, I_DEFVAR, opVar(FR_GF, "$retval"), opNone(), opNone());
  /* Úvodní skok na hlavní funkci */
  emit(&header, I_JUMP, opLabel("main"), opNone(), opNone());

  ListOfInstr funcs_code;
  listInit(&funcs_code);

  int lbl_seq = 0;
  int main_found = 0;

  for (Node *f = prog->body; f != NULL; f = f->next) {
    if (f->kind != N_FUNC) {
      listFree(&header);
      listFree(&funcs_code);
      return 99;
    }

    if (f->is_main) {
      main_found = 1;
  
    }

    ListOfInstr f_code;
    listInit(&f_code);

    int err = genFunc(f, &f_code, &lbl_seq);
    if (err != 0) {
      listFree(&header);
      listFree(&funcs_code);
      listFree(&f_code);
      return err;
    }

    ListOfInstr merged;
    listInit(&merged);
    joinLists(&merged, &funcs_code, &f_code);
    funcs_code = merged;
  }

  if (!main_found) {
    listFree(&header);
    listFree(&funcs_code);
    return 99;
  }

  joinLists(out, &header, &funcs_code);
  return 0;
}

static void genIf(Gen *g, Node *n)
{
  Operand cond = genExpr(g, n->left);

  Operand lbl_else = newLabel(g, "if_else");
  Operand lbl_end  = newLabel(g, "if_end");

  Operand target_else = (n->right != NULL) ? lbl_else : lbl_end;

  emit(&g->body, I_JUMPIFEQ, opDup(target_else), cond, opBool(0));

  genList(g, n->body);

  if (n->right != NULL) {
    emit(&g->body, I_JUMP, opDup(lbl_end), opNone(), opNone());
    emit(&g->body, I_LABEL, lbl_else, opNone(), opNone());
    genList(g, n->right);
  }

  emit(&g->body, I_LABEL, lbl_end, opNone(), opNone());
}

static void genWhile(Gen *g, Node *n)
{
  Operand lbl_cond = newLabel(g, "while_cond");
  Operand lbl_end  = newLabel(g, "while_end");

  emit(&g->body, I_LABEL, opDup(lbl_cond), opNone(), opNone());

  Operand cond = genExpr(g, n->left);

  emit(&g->body, I_JUMPIFEQ, opDup(lbl_end), cond, opBool(0));

  genList(g, n->body);

  emit(&g->body, I_JUMP, lbl_cond, opNone(), opNone());
  emit(&g->body, I_LABEL, lbl_end, opNone(), opNone());
}