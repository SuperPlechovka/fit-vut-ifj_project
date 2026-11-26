/**
 * @file
 * @brief Generátor cieľového kódu IFJcode26.
 * @details Projekt: Implementácia prekladača IFJ26 (IFJ/IAL 2026/2027).
 * @author Magdaléna Somorovská (xsomorm00)
 */

#include "codegen.h"
#include <stdio.h>

void gen_header(void) { printf(".IFJcode26\n"); }

// Práca s rámcami, volanie funkcií
void gen_move(const char *symb, const char *val) { printf("MOVE %s %s\n", symb, val); }

void gen_defvar(const char *var) { printf("DEFVAR %s\n", var); }

void gen_createframe(void) { printf("CREATEFRAME\n"); }

void gen_pushframe(void) { printf("PUSHFRAME\n"); }

void gen_popframe(void) { printf("POPFRAME\n"); }

void gen_call(const char *label) { printf("CALL %s\n", label); }

void gen_return(void) { printf("RETURN\n"); }

// Vstupno-výstupné inštrukcie
void gen_read(const char *var, const char *type) { printf("READ %s %s\n", var, type); }

void gen_write(const char *symb) { printf("WRITE %s\n", symb); }

// Aritmetické, relačné booleovské konverzné inštrukcie
void gen_add(const char *var, const char *symb1, const char *symb2) {
    printf("ADD %s %s %s\n", var, symb1, symb2);
}

void gen_sub(const char *var, const char *symb1, const char *symb2) {
    printf("SUB %s %s %s\n", var, symb1, symb2);
}

void gen_mul(const char *var, const char *symb1, const char *symb2) {
    printf("MUL %s %s %s\n", var, symb1, symb2);
}

void gen_div(const char *var, const char *symb1, const char *symb2) {
    printf("DIV %s %s %s\n", var, symb1, symb2);
}

void gen_idiv(const char *var, const char *symb1, const char *symb2) {
    printf("IDIV %s %s %s\n", var, symb1, symb2);
}

void gen_lt(const char *var, const char *symb1, const char *symb2) {
    printf("LT %s %s %s\n", var, symb1, symb2);
}

void gen_gt(const char *var, const char *symb1, const char *symb2) {
    printf("GT %s %s %s\n", var, symb1, symb2);
}

void gen_eq(const char *var, const char *symb1, const char *symb2) {
    printf("EQ %s %s %s\n", var, symb1, symb2);
}

// Inštrukcie pre riadenie toku programu
void gen_label(const char *label) { printf("LABEL %s\n", label); }

void gen_jumpifeq(const char *label, const char *symb1, const char *symb2) {
    printf("JUMPIFEQ %s %s %s\n", label, symb1, symb2);
}

void gen_jumpifneq(const char *label, const char *symb1, const char *symb2) {
    printf("JUMPIFNEQ %s %s %s\n", label, symb1, symb2);
}

void gen_jump(const char *label) { printf("JUMP %s\n", label); }

// Práca s reťazcami a typmi
void gen_strlen(const char *var, const char *symb) { printf("STRLEN %s %s\n", var, symb); }

void gen_getchar(const char *var, const char *symb1, const char *symb2) {
    printf("GETCHAR %s %s %s\n", var, symb1, symb2);
}

void gen_setchar(const char *var, const char *symb1, const char *symb2) {
    printf("SETCHAR %s %s %s\n", var, symb1, symb2);
}

void gen_type(const char *var, const char *symb) { printf("TYPE %s %s\n", var, symb); }

// Pomocné funkcie pre správny zápis konštánt, escape sekvencií a hodnoty Unit
void gen_const_int(long long val) { printf("int@%lld", val); }

void gen_const_float(double val) { printf("float@%a", val); }

void gen_const_bool(int val) { printf("bool@%s", val ? "true" : "false"); }

void gen_const_unit(void) { printf("nil@nil"); }

void gen_const_string(const char *str) {
    printf("string@");
    if (str == NULL) {
        return;
    }
    for (int i = 0; str[i] != '\0'; i++) {
        unsigned char c = (unsigned char)str[i];
        if (c <= 32 || c == '#' || c == '\\') {
            printf("\\%03d", c);
        } else {
            putchar(c);
        }
    }
}

// Prevod vnútorného kódu (ListOfInstr) na cieľový kód IFJcode26
static void gen_operand(const Operand *op) {
    if (op == NULL) {
        return;
    }
    if (op->kind == OPD_INT) {
        gen_const_int(op->val.i);
    } else if (op->kind == OPD_FLOAT) {
        gen_const_float(op->val.d);
    } else if (op->kind == OPD_STRING) {
        gen_const_string(op->name);
    } else if (op->kind == OPD_BOOL) {
        gen_const_bool(op->val.b);
    } else if (op->kind == OPD_NIL) {
        gen_const_unit();
    } else if (op->kind == OPD_VAR) {
        if (op->frame == FR_GF) {
            printf("GF@%s", op->name);
        } else if (op->frame == FR_LF) {
            printf("LF@%s", op->name);
        } else if (op->frame == FR_TF) {
            printf("TF@%s", op->name);
        }
    } else if (op->kind == OPD_LABEL || op->kind == OPD_TYPE) {
        printf("%s", op->name);
    }
}

void codegen_generate(const ListOfInstr *list) {
    static const char *instr_names[I_COUNT] = {[I_MOVE] = "MOVE",
                                               [I_CREATEFRAME] = "CREATEFRAME",
                                               [I_PUSHFRAME] = "PUSHFRAME",
                                               [I_POPFRAME] = "POPFRAME",
                                               [I_DEFVAR] = "DEFVAR",
                                               [I_CALL] = "CALL",
                                               [I_RETURN] = "RETURN",
                                               [I_PUSHS] = "PUSHS",
                                               [I_POPS] = "POPS",
                                               [I_ADD] = "ADD",
                                               [I_SUB] = "SUB",
                                               [I_MUL] = "MUL",
                                               [I_DIV] = "DIV",
                                               [I_IDIV] = "IDIV",
                                               [I_LT] = "LT",
                                               [I_GT] = "GT",
                                               [I_EQ] = "EQ",
                                               [I_AND] = "AND",
                                               [I_OR] = "OR",
                                               [I_NOT] = "NOT",
                                               [I_INT2FLOAT] = "INT2FLOAT",
                                               [I_FLOAT2INT] = "FLOAT2INT",
                                               [I_INT2CHAR] = "INT2CHAR",
                                               [I_STRI2INT] = "STRI2INT",
                                               [I_INT2STR] = "INT2STR",
                                               [I_FLOAT2STR] = "FLOAT2STR",
                                               [I_READ] = "READ",
                                               [I_WRITE] = "WRITE",
                                               [I_CONCAT] = "CONCAT",
                                               [I_STRLEN] = "STRLEN",
                                               [I_GETCHAR] = "GETCHAR",
                                               [I_SETCHAR] = "SETCHAR",
                                               [I_TYPE] = "TYPE",
                                               [I_LABEL] = "LABEL",
                                               [I_JUMP] = "JUMP",
                                               [I_JUMPIFEQ] = "JUMPIFEQ",
                                               [I_JUMPIFNEQ] = "JUMPIFNEQ",
                                               [I_EXIT] = "EXIT",
                                               [I_BREAK] = "BREAK",
                                               [I_DPRINT] = "DPRINT"};

    gen_header();

    if (list == NULL) {
        gen_exit(0);
        return;
    }

    int exit = 0;
    const ListItem *it = list->first;
    while (it != NULL) {
        printf("%s", instr_names[it->instr.type]);

        if (it->instr.a1.kind != OPD_NONE) {
            printf(" ");
            gen_operand(&it->instr.a1);
        }
        if (it->instr.a2.kind != OPD_NONE) {
            printf(" ");
            gen_operand(&it->instr.a2);
        }
        if (it->instr.a3.kind != OPD_NONE) {
            printf(" ");
            gen_operand(&it->instr.a3);
        }
        printf("\n");
        if (it->instr.type == I_EXIT) {
            exit = 1;
        }

        it = it->next;
    }
    if (!exit) {
        gen_exit(0);
    }
}

void gen_exit(int code) { printf("EXIT int@%d\n", code); }

void gen_footer(void) { gen_exit(0); }
