/**
 * @file
 * @brief Generátor cieľového kódu IFJcode26.
 * @details Projekt: Implementácia prekladača IFJ26 (IFJ/IAL 2026/2027).
 * @author Magdaléna Somorovská (xsomorm00)
 */

#include "codegen.h"
#include <stdio.h>

void gen_header(void) {
    printf(".IFJcode26\n");
}

// Práca s rámcami, volanie funkcií
void gen_move(char *symb, char *val) {
    printf("MOVE %s %s\n", symb, val);
}

void gen_defvar(char *var) {
    printf("DEFVAR %s\n", var);
}

void gen_createframe(void) {
    printf("CREATEFRAME\n");
}

void gen_pushframe(void) {
    printf("PUSHFRAME\n");
}

void gen_popframe(void) {
    printf("POPFRAME\n");
}

void gen_call(char *label) {
    printf("CALL %s\n", label);
}

void gen_return(void) {
    printf("RETURN\n");
}

// Vstupno-výstupné inštrukcie
void gen_read(char *var, char *type) {
    printf("READ %s %s\n", var, type);
}

void gen_write(char *symb) {
    printf("WRITE %s\n", symb);
}

// Aritmetické, relačné booleovské konverzné inštrukcie
void gen_add(char *var, char *symb1, char *symb2) {
    printf("ADD %s %s %s\n", var, symb1, symb2);
}

void gen_sub(char *var, char *symb1, char *symb2) {
    printf("SUB %s %s %s\n", var, symb1, symb2);
}

void gen_mul(char *var, char *symb1, char *symb2) {
    printf("MUL %s %s %s\n", var, symb1, symb2);
}

void gen_div(char *var, char *symb1, char *symb2) {
    printf("DIV %s %s %s\n", var, symb1, symb2);
}

void gen_idiv(char *var, char *symb1, char *symb2) {
    printf("IDIV %s %s %s\n", var, symb1, symb2);
}

void gen_lt(char *var, char *symb1, char *symb2) {
    printf("LT %s %s %s\n", var, symb1, symb2);
}

void gen_gt(char *var, char *symb1, char *symb2) {
    printf("GT %s %s %s\n", var, symb1, symb2);
}

void gen_eq(char *var, char *symb1, char *symb2) {
    printf("EQ %s %s %s\n", var, symb1, symb2);
}

// Inštrukcie pre riadenie toku programu
void gen_label(char *label) {
    printf("LABEL %s\n", label);
}

void gen_jumpifeq(char *label, char *symb1, char *symb2) {
    printf("JUMPIFEQ %s %s %s\n", label, symb1, symb2);
}

void gen_jumpifneq(char *label, char *symb1, char *symb2) {
    printf("JUMPIFNEQ %s %s %s\n", label, symb1, symb2);
}

void gen_jump(char *label) {
    printf("JUMP %s\n", label);
}

// Práca s reťazcami a typmi
void gen_strlen(char *var, char *symb) {
    printf("STRLEN %s %s\n", var, symb);
}

void gen_getchar(char *var, char *symb1, char *symb2) {
    printf("GETCHAR %s %s %s\n", var, symb1, symb2);
}

void gen_setchar(char *var, char *symb1, char *symb2) {
    printf("SETCHAR %s %s %s\n", var, symb1, symb2);
}

void gen_type(char *var, char *symb) {
    printf("TYPE %s %s\n", var, symb);
}

void gen_footer(void) {
}
