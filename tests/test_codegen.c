/**
 * @file
 * @brief Testovacie prípady pre generátor cieľového kódu IFJcode26.
 * @details Projekt: Implementácia prekladača IFJ26 (IFJ/IAL 2026/2027).
 * @author Magdaléna Somorovská (xsomorm00)
 */

#include "../src/codegen.h"
#include "../src/ilist.h"
#include <stdio.h>

/* Test aritmetických, relačných operácií a konštánt */
void test_vypocty_a_vypisy(void) {
    gen_header();

    // Definícia premenných
    gen_defvar("GF@a");
    gen_defvar("GF@b");
    gen_defvar("GF@vysledok");
    gen_defvar("GF@porovnanie");

    // Priradenie hodnôt
    gen_move("GF@a", "int@20");
    gen_move("GF@b", "int@5");

    // Aritmetika 
    gen_add("GF@vysledok", "GF@a", "GF@b");
    gen_write("GF@vysledok");

    gen_sub("GF@vysledok", "GF@a", "GF@b");
    gen_write("GF@vysledok");

    gen_mul("GF@vysledok", "GF@a", "GF@b");
    gen_write("GF@vysledok");

    gen_idiv("GF@vysledok", "GF@a", "GF@b");
    gen_write("GF@vysledok");

    gen_div("GF@vysledok", "float@0x1.4p+3", "float@0x1.0p+1");
    gen_write("GF@vysledok");

    // Relačné operácie
    gen_lt("GF@porovnanie", "GF@b", "GF@a");
    gen_gt("GF@porovnanie", "GF@a", "GF@b");
    gen_eq("GF@porovnanie", "GF@a", "GF@b");
    gen_write("GF@porovnanie");

    // Výpis špeciálnych konštánt 
    printf("WRITE ");
    gen_const_int(42);
    printf("\n");

    printf("WRITE ");
    gen_const_float(3.14);
    printf("\n");

    printf("WRITE ");
    gen_const_bool(1);
    printf("\n");

    printf("WRITE ");
    gen_const_unit();
    printf("\n");

    printf("WRITE ");
    gen_const_string("Vysledok #1 je:\n");
    printf("\n");

    // Práca s reťazcami a typmi
    gen_defvar("GF@retazec");
    gen_defvar("GF@dlzka");
    gen_defvar("GF@znak");
    gen_defvar("GF@typ");

    gen_move("GF@retazec", "string@Ahoj");
    gen_strlen("GF@dlzka", "GF@retazec");
    gen_getchar("GF@znak", "GF@retazec", "int@0");
    gen_setchar("GF@retazec", "int@0", "string@B");
    gen_type("GF@typ", "GF@retazec");
    gen_write("GF@retazec");

    // Skoky, návestia a rámce
    gen_createframe();
    gen_pushframe();
    gen_popframe();

    gen_jump("koniec");
    gen_label("zaciatok");
    gen_jumpifeq("koniec", "GF@a", "GF@b");
    gen_jumpifneq("koniec", "GF@a", "GF@b");
    gen_label("koniec");

    gen_footer();
}

/* Test prevodu vnútorného kódu na cieľový kód */
void test_prevod_zoznamu(void) {
    ListOfInstr list;
    listInit(&list);

    emit(&list, I_DEFVAR, opVar(FR_GF, "x"), opNone(), opNone());
    emit(&list, I_DEFVAR, opVar(FR_GF, "y"), opNone(), opNone());
    emit(&list, I_MOVE, opVar(FR_GF, "x"), opInt(15), opNone());
    emit(&list, I_MOVE, opVar(FR_GF, "y"), opInt(3), opNone());

    emit(&list, I_ADD, opVar(FR_GF, "x"), opVar(FR_GF, "x"), opVar(FR_GF, "y"));
    emit(&list, I_SUB, opVar(FR_GF, "x"), opVar(FR_GF, "x"), opInt(2));
    emit(&list, I_MUL, opVar(FR_GF, "x"), opVar(FR_GF, "x"), opVar(FR_GF, "y"));

    emit(&list, I_WRITE, opString("Vypocet x = "), opNone(), opNone());
    emit(&list, I_WRITE, opVar(FR_GF, "x"), opNone(), opNone());
    emit(&list, I_WRITE, opString("\n"), opNone(), opNone());

    codegen_generate(&list);
    listFree(&list);
}

int main(void) {
    test_vypocty_a_vypisy();
    printf("\n");
    test_prevod_zoznamu();

    return 0;
}