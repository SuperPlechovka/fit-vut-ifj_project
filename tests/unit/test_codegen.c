/**
 * @file
 * @brief Testovacie prípady pre generátor cieľového kódu IFJcode26.
 * @details Projekt: Implementácia prekladača IFJ26 (IFJ/IAL 2026/2027).
 * @author Magdaléna Somorovská (xsomorm00)
 */

#include "codegen.h"
#include "ilist.h"
#include <stdio.h>
#include <string.h>

#define MAX_BUF 8192

/* generujúca funkcia do samostatného súboru a načíta výstup do bufferu */
static int zachyt_program(void (*fn)(void), const char *tmp_cesta, char *buf, size_t velkost) {
    if (freopen(tmp_cesta, "w", stdout) == NULL) {
        fprintf(stderr, "CHYBA: Nepodarilo sa presmerovat stdout do %s\n", tmp_cesta);
        return 0;
    }

    fn();
    fflush(stdout);

    FILE *f = fopen(tmp_cesta, "r");
    if (f == NULL) {
        fprintf(stderr, "CHYBA: Nepodarilo sa otvorit %s na citanie\n", tmp_cesta);
        return 0;
    }

    size_t n = fread(buf, 1, velkost - 1, f);
    buf[n] = '\0';
    fclose(f);
    remove(tmp_cesta);
    return 1;
}

static int spocitaj_vyskyty(const char *text, const char *hladane) {
    int pocet = 0;
    size_t dlzka = strlen(hladane);
    const char *p = text;
    while ((p = strstr(p, hladane)) != NULL) {
        pocet++;
        p += dlzka;
    }
    return pocet;
}

static int over_obsah(const char *nazov_testu, const char *vystup, const char *ocakavane) {
    if (strstr(vystup, ocakavane) == NULL) {
        fprintf(stderr, "FAIL [%s]: Chybajuci vystup: '%s'\n", nazov_testu, ocakavane);
        return 0;
    }
    return 1;
}

/* Test aritmetických, relačných operácií a konštánt */
static void gen_program_zaklad(void) {
    gen_header();

    // Definícia premenných
    gen_defvar("GF@a");
    gen_defvar("GF@b");
    gen_defvar("GF@vstup");
    gen_defvar("GF@vysledok");
    gen_defvar("GF@porovnanie");

    // Priradenie hodnôt
    gen_move("GF@a", "int@20");
    gen_move("GF@b", "int@5");
    gen_read("GF@vstup", "int");

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

    gen_jumpifeq("preskoc_eq", "GF@a", "GF@b");
    gen_jumpifneq("za_neq", "GF@a", "GF@b");
    gen_label("preskoc_eq");
    gen_label("za_neq");
    gen_jump("koniec");
    gen_label("koniec");

    gen_footer();
}

/* Test prevodu vnútorného kódu na cieľový kód */
static void gen_program_zoznam(void) {
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

static int test_vypocty_a_vypisy(void) {
    char buf[MAX_BUF];
    const char *nazov = "test_vypocty_a_vypisy";

    if (!zachyt_program(gen_program_zaklad, "tmp_codegen_1.code", buf, sizeof(buf))) {
        return 0;
    }

    if (spocitaj_vyskyty(buf, ".IFJcode26") != 1) {
        fprintf(stderr, "FAIL [%s]: Ocaka sa prave 1 hlavicka .IFJcode26\n", nazov);
        return 0;
    }

    int ok = 1;
    ok &= over_obsah(nazov, buf, "DEFVAR GF@vstup");
    ok &= over_obsah(nazov, buf, "READ GF@vstup int");
    ok &= over_obsah(nazov, buf, "ADD GF@vysledok GF@a GF@b");
    ok &= over_obsah(nazov, buf, "IDIV GF@vysledok GF@a GF@b");
    ok &= over_obsah(nazov, buf, "WRITE int@42");
    ok &= over_obsah(nazov, buf, "WRITE float@0x");
    ok &= over_obsah(nazov, buf, "WRITE bool@true");
    ok &= over_obsah(nazov, buf, "WRITE nil@nil");
    ok &= over_obsah(nazov, buf, "WRITE string@Vysledok\\032\\0351\\032je:\\010");
    ok &= over_obsah(nazov, buf, "STRLEN GF@dlzka GF@retazec");
    ok &= over_obsah(nazov, buf, "JUMPIFEQ preskoc_eq GF@a GF@b");
    ok &= over_obsah(nazov, buf, "JUMPIFNEQ za_neq GF@a GF@b");
    ok &= over_obsah(nazov, buf, "JUMP koniec");
    return ok;
}

static int test_prevod_zoznamu(void) {
    char buf[MAX_BUF];
    const char *nazov = "test_prevod_zoznamu";

    if (!zachyt_program(gen_program_zoznam, "tmp_codegen_2.code", buf, sizeof(buf))) {
        return 0;
    }

    if (spocitaj_vyskyty(buf, ".IFJcode26") != 1) {
        fprintf(stderr, "FAIL [%s]: Ocaka sa prave 1 hlavicka .IFJcode26\n", nazov);
        return 0;
    }

    int ok = 1;
    ok &= over_obsah(nazov, buf, "DEFVAR GF@x");
    ok &= over_obsah(nazov, buf, "MOVE GF@x int@15");
    ok &= over_obsah(nazov, buf, "ADD GF@x GF@x GF@y");
    ok &= over_obsah(nazov, buf, "SUB GF@x GF@x int@2");
    ok &= over_obsah(nazov, buf, "MUL GF@x GF@x GF@y");
    ok &= over_obsah(nazov, buf, "WRITE string@Vypocet\\032x\\032=\\032");
    ok &= over_obsah(nazov, buf, "WRITE GF@x");
    return ok;
}

int main(void) {
    if (!test_vypocty_a_vypisy()) {
        return 1;
    }
    if (!test_prevod_zoznamu()) {
        return 1;
    }

    return 0;
}