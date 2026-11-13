/**
 * @file
 * @brief Rozhranie generátora cieľového kódu IFJcode26.
 * @details Projekt: Implementácia prekladača IFJ26 (IFJ/IAL 2026/2027).
 * @author Magdaléna Somorovská (xsomorm00)
 */

#ifndef CODEGEN_H
#define CODEGEN_H
#include "ilist.h"

/**
 * @brief Výpis hlavičky jazyka IFJcode26.
 */
void gen_header(void);

/**
 * @brief Skopíruje hodnotu do cieľovej premennej.
 * @param[in] symb Cieľová premenná.
 * @param[in] val Zdrojová premenná alebo konštanta.
 */
void gen_move(char *symb, char *val);

/**
 * @brief Definuje novú premennú v danom rámci.
 * @param[in] var Názov premennej vrátane rámca.
 */
void gen_defvar(char *var);

/**
 * @brief Vytvorí nový dočasný rámec.
 */
void gen_createframe(void);

/**
 * @brief Presunie dočasný rámec na zásobník rámcov.
 */
void gen_pushframe(void);

/**
 * @brief Presunie vrcholový rámec zo zásobníka do dočasného rámca.
 */
void gen_popframe(void);

/**
 * @brief Zavolá funkciu na danom návestí.
 * @param[in] label Názov návestia funkcie.
 */
void gen_call(char *label);

/**
 * @brief Návrat z funkcie.
 */
void gen_return(void);

/**
 * @brief Načíta hodnotu zo vstupu do premennej.
 * @param[in] var Cieľová premenná.
 * @param[in] type Typ načítavanej hodnoty.
 */
void gen_read(char *var, char *type);

/**
 * @brief Vypíše hodnotu na štandardný výstup.
 * @param[in] symb Premenná alebo konštanta na vypísanie.
 */
void gen_write(char *symb);

/**
 * @brief Sčíta dva symboly a uloží výsledok.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Prvý sčítanec.
 * @param[in] symb2 Druhý sčítanec.
 */
void gen_add(char *var, char *symb1, char *symb2);

/**
 * @brief Odčíta druhý symbol od prvého.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Menšenec.
 * @param[in] symb2 Menšiteľ.
 */
void gen_sub(char *var, char *symb1, char *symb2);

/**
 * @brief Vynásobí dva symboly.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Prvý činiteľ.
 * @param[in] symb2 Druhý činiteľ.
 */
void gen_mul(char *var, char *symb1, char *symb2);

/**
 * @brief Vydelí dva desatinné symboly.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Delenec.
 * @param[in] symb2 Deliteľ.
 */
void gen_div(char *var, char *symb1, char *symb2);

/**
 * @brief Celočíselne vydelí dva symboly.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Delenec.
 * @param[in] symb2 Deliteľ.
 */
void gen_idiv(char *var, char *symb1, char *symb2);

/**
 * @brief Porovná, či je prvý symbol menší ako druhý.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Prvý porovnávaný symbol.
 * @param[in] symb2 Druhý porovnávaný symbol.
 */
void gen_lt(char *var, char *symb1, char *symb2);

/**
 * @brief Porovná, či je prvý symbol väčší ako druhý.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Prvý porovnávaný symbol.
 * @param[in] symb2 Druhý porovnávaný symbol.
 */
void gen_gt(char *var, char *symb1, char *symb2);

/**
 * @brief Porovná rovnosť dvoch symbolov.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Prvý porovnávaný symbol.
 * @param[in] symb2 Druhý porovnávaný symbol.
 */
void gen_eq(char *var, char *symb1, char *symb2);

/**
 * @brief Definuje návestie v kóde.
 * @param[in] label Názov návestia.
 */
void gen_label(char *label);

/**
 * @brief Podmienený skok pri rovnosti.
 * @param[in] label Cieľové návestie.
 * @param[in] symb1 Prvý porovnávaný symbol.
 * @param[in] symb2 Druhý porovnávaný symbol.
 */
void gen_jumpifeq(char *label, char *symb1, char *symb2);

/**
 * @brief Podmienený skok pri nerovnosti.
 * @param[in] label Cieľové návestie.
 * @param[in] symb1 Prvý porovnávaný symbol.
 * @param[in] symb2 Druhý porovnávaný symbol.
 */
void gen_jumpifneq(char *label, char *symb1, char *symb2);

/**
 * @brief Nepodmienený skok na návestie.
 * @param[in] label Cieľové návestie.
 */
void gen_jump(char *label);

/**
 * @brief Zistí dĺžku reťazca.
 * @param[in] var Cieľová premenná pre uloženie dĺžky.
 * @param[in] symb Reťazcový symbol.
 */
void gen_strlen(char *var, char *symb);

/**
 * @brief Získa znak z reťazca na danom indexe.
 * @param[in] var Cieľová premenná.
 * @param[in] symb1 Zdrojový reťazec.
 * @param[in] symb2 Index znaku.
 */
void gen_getchar(char *var, char *symb1, char *symb2);

/**
 * @brief Zmení znak v reťazci na danom indexe.
 * @param[in] var Modifikovaná reťazcová premenná.
 * @param[in] symb1 Index znaku.
 * @param[in] symb2 Nový znak.
 */
void gen_setchar(char *var, char *symb1, char *symb2);

/**
 * @brief Zistí dynamický typ symbolu.
 * @param[in] var Cieľová premenná pre názov typu.
 * @param[in] symb Skúmaný symbol.
 */
void gen_type(char *var, char *symb);

/**
 * @brief Vypíše celočíselnú konštantu vo formáte int@hodnota.
 * @param[in] val Celočíselná hodnota (long long).
 */
void gen_const_int(long long val);

/**
 * @brief Vypíše desatinnú konštantu v hexadecimálnom formáte float@hodnota.
 * @param[in] val Desatinná hodnota (double).
 */
void gen_const_float(double val);

/**
 * @brief Vypíše pravdivostnú konštantu vo formáte bool@true/bool@false.
 * @param[in] val Pravdivostná hodnota (0 = false, inak true).
 */
void gen_const_bool(int val);

/**
 * @brief Vypíše konštantu reprezentujúcu hodnotu Unit (nil@nil).
 */
void gen_const_unit(void);

/**
 * @brief Vypíše reťazcovú konštantu vo formáte string@... s ošetrením escape sekvencií.
 * @param[in] str Vstupný C reťazec.
 */
void gen_const_string(const char *str);

/**
 * @brief Ukončenie generovania cieľového kódu.
 */
void gen_footer(void);

/**
 * @brief Prevedie vnútorný kód do cieľového kódu IFJcode26 na stdout.
 * @param[in] list Ukazovateľ na zoznam inštrukcií vnútorného kódu.
 */
void codegen_generate(const ListOfInstr *list);

/**
 * @brief Ukončí vykonávanie programu s daným návratovým kódom.
 * @param[in] code Návratový kód (0 až 99).
 */
void gen_exit(int code);

#endif 