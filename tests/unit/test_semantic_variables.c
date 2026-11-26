/**
 * @file
 * @brief Unit testy sémantické analýzy - proměnné, rozsahy platnosti a typy výrazů.
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: připrav unit testy na proměnné pro průchod 2 analyseSemantics podle
 * specifikace IFJ26 (sekce 1.3, 2.2, 2.4, 2.5, 3). Ověř:
 * - definici val/var s uvedeným i odvozeným typem, typ Unit, implicitní Int -> Double;
 * - použití nedefinované proměnné, použití před definicí, mimo rozsah - kód 3;
 * - redefinici ve stejném bloku - kód 4; překrytí v podbloku je v pořádku,
 *   inicializace vidí vnější proměnnou stejného jména (val x = x + 1);
 * - přiřazení do val a do parametru - kód 7;
 * - typové chyby - kód 6: nekompatibilní inicializace a přiřazení, Double -> Int,
 *   operátory nad nekompatibilními typy, Bool mimo podmínku, podmínka jiného typu než Bool;
 * - parametry funkce jako proměnné;
 * - pozici chyby u několika případů.
 * Funkce v testech mají explicitní návratový typ Unit, aby testy nezávisely
 * na odvozování návratových typů. Vnořené bloky (if/while v těle funkce) jsou
 * zvlášť v test_nested_blocks, protože lexer zatím při návratu o úroveň odsazení
 * vyrábí DEDENT navíc; až to opraví, stačí test zapnout v main.
 * Model: Claude Opus 5.5, Claude Code.
 */
#include "../../src/parser.h"
#include "../../src/semantic_analysis.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *input;
    Lexer lexer;
    Parser parser;
    ParseError error;
    Node *program;
    ErrorCode code;
} SemanticRun;

#define PROLOG "import ifj26.*\n"
/* Tělo vstupní funkce; řádky těla musí být odsazené čtyřmi mezerami. */
#define MAIN PROLOG "@main def run(): Unit =\n"

/* Zparsuje text (musí projít bez chyby) a spustí na něm sémantickou analýzu. */
static void run_semantic(SemanticRun *run, const char *text) {
    memset(run, 0, sizeof(*run));
    run->input = tmpfile();
    assert(run->input != NULL);
    assert(fputs(text, run->input) >= 0);
    rewind(run->input);

    assert(lexer_init(&run->lexer, run->input) == LEXER_OK);
    assert(parserInit(&run->parser, &run->lexer, &run->error) == ERR_OK);

    ErrorCode parse_code = parseProgram(&run->parser, &run->error, &run->program);
    if (parse_code != ERR_OK) {
        fprintf(stderr, "test je špatně - program se nezparsoval (chyba %d, %zu:%zu %s):\n%s\n",
                parse_code, run->error.line, run->error.column,
                run->error.message != NULL ? run->error.message : "", text);
    }
    assert(parse_code == ERR_OK);

    run->code = analyseSemantics(run->program, &run->error);
}

static void finish(SemanticRun *run) {
    nodeFree(run->program);
    parserDestroy(&run->parser);
    lexer_destroy(&run->lexer);
    fclose(run->input);
}

static void expect_ok(const char *text) {
    SemanticRun run;
    run_semantic(&run, text);
    if (run.code != ERR_OK) {
        fprintf(stderr, "očekáváno OK, chyba %d (%zu:%zu %s):\n%s\n", run.code, run.error.line,
                run.error.column, run.error.message != NULL ? run.error.message : "", text);
    }
    assert(run.code == ERR_OK);
    assert(run.error.code == ERR_OK);
    finish(&run);
}

static void expect_error(const char *text, ErrorCode expected) {
    SemanticRun run;
    run_semantic(&run, text);
    if (run.code != expected) {
        fprintf(stderr, "očekávána chyba %d, dostal %d (%s):\n%s\n", expected, run.code,
                run.error.message != NULL ? run.error.message : "", text);
    }
    assert(run.code == expected);
    assert((int)run.error.code == (int)expected);
    finish(&run);
}

/* Navíc ověří pozici chyby (řádek, sloupec). */
static void expect_error_at(const char *text, ErrorCode expected, size_t line, size_t column) {
    SemanticRun run;
    run_semantic(&run, text);
    if (run.code != expected || run.error.line != line || run.error.column != column) {
        fprintf(stderr, "očekávána chyba %d na %zu:%zu, dostal %d na %zu:%zu (%s):\n%s\n", expected,
                line, column, run.code, run.error.line, run.error.column,
                run.error.message != NULL ? run.error.message : "", text);
    }
    assert(run.code == expected);
    assert(run.error.line == line && run.error.column == column);
    finish(&run);
}

static void test_definitions(void) {
    expect_ok(MAIN "    val a = 1\n"
                   "    var b = 2.5\n"
                   "    val c = \"s\"\n"
                   "    ()\n");
    /* uvedený typ */
    expect_ok(MAIN "    val a: Int = 1\n"
                   "    var b: Double = 2.5\n"
                   "    val c: String = \"s\"\n"
                   "    ()\n");
    /* Unit lze použít jako typ proměnné */
    expect_ok(MAIN "    val u = ()\n"
                   "    val v: Unit = ()\n"
                   "    ()\n");
    /* jediná implicitní konverze: Int -> Double */
    expect_ok(MAIN "    val a: Double = 1\n"
                   "    var b: Double = 2.5\n"
                   "    b = 3\n"
                   "    ()\n");
    /* proměnná v inicializaci další proměnné, odvozený typ se pamatuje */
    expect_ok(MAIN "    val a = 1\n"
                   "    val b: Int = a\n"
                   "    var c = b + a\n"
                   "    c = a\n"
                   "    ()\n");
    /* stejné jméno v různých funkcích */
    expect_ok(PROLOG "def f(): Unit =\n"
                     "    val a = 1\n"
                     "    ()\n"
                     "@main def run(): Unit =\n"
                     "    val a = \"s\"\n"
                     "    ()\n");
}

static void test_parameters(void) {
    expect_ok(PROLOG "def f(a: Int, b: Double): Unit =\n"
                     "    val c: Double = a + b\n"
                     "    var d = a\n"
                     "    d = a * 2\n"
                     "    ()\n"
                     "@main def run(): Unit = ()\n");
    /* parametr je nemodifikovatelný - 7 */
    expect_error_at(PROLOG "def f(a: Int): Unit =\n"
                           "    a = 2\n"
                           "    ()\n"
                           "@main def run(): Unit = ()\n",
                    ERR_IMMUTABLE, 3, 5);
    /* parametr jiné funkce není vidět - 3 */
    expect_error(PROLOG "def f(a: Int): Unit = ()\n"
                        "@main def run(): Unit =\n"
                        "    val b = a\n"
                        "    ()\n",
                 ERR_UNDEFINED);
}

static void test_undefined(void) {
    expect_error_at(MAIN "    val a = b\n"
                         "    ()\n",
                    ERR_UNDEFINED, 3, 13);
    /* použití před definicí */
    expect_error(MAIN "    val a = b + 1\n"
                      "    val b = 2\n"
                      "    ()\n",
                 ERR_UNDEFINED);
    /* proměnná nesmí být vidět ve vlastní inicializaci */
    expect_error(MAIN "    val a = a\n"
                      "    ()\n",
                 ERR_UNDEFINED);
    /* přiřazení do nedefinované proměnné */
    expect_error(MAIN "    x = 1\n"
                      "    ()\n",
                 ERR_UNDEFINED);
    /* nedefinovaná proměnná jako samostatný výraz */
    expect_error(MAIN "    x\n", ERR_UNDEFINED);
    /* funkce není proměnná */
    expect_error(PROLOG "def f(): Int = 1\n"
                        "@main def run(): Unit =\n"
                        "    val a = f\n"
                        "    ()\n",
                 ERR_UNDEFINED);
}

static void test_redefinition(void) {
    expect_error_at(MAIN "    val a = 1\n"
                         "    val a = 2\n"
                         "    ()\n",
                    ERR_REDEFINITION, 4, 5);
    expect_error(MAIN "    val a = 1\n"
                      "    var a = 2\n"
                      "    ()\n",
                 ERR_REDEFINITION);
    expect_error(MAIN "    var a = 1\n"
                      "    val a = \"s\"\n"
                      "    ()\n",
                 ERR_REDEFINITION);
}

static void test_immutable(void) {
    expect_error_at(MAIN "    val a = 1\n"
                         "    a = 2\n"
                         "    ()\n",
                    ERR_IMMUTABLE, 4, 5);
}

static void test_type_errors(void) {
    /* inicializace nekompatibilní s uvedeným typem */
    expect_error(MAIN "    val a: Int = \"s\"\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    val a: String = 1\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    val a: Unit = 1\n"
                      "    ()\n",
                 ERR_TYPE);
    /* Double -> Int implicitně nelze */
    expect_error(MAIN "    val a: Int = 1.5\n"
                      "    ()\n",
                 ERR_TYPE);
    /* přiřazení jiného typu */
    expect_error(MAIN "    var a = 1\n"
                      "    a = \"s\"\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    var a = 1\n"
                      "    a = 2.5\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    var a = \"s\"\n"
                      "    a = ()\n"
                      "    ()\n",
                 ERR_TYPE);
}

static void test_expressions(void) {
    /* aritmetika: Int op Int -> Int, s Double -> Double */
    expect_ok(MAIN "    val a: Int = 1 + 2 * 3 - 4 / 2\n"
                   "    val b: Double = 1 + 2.5\n"
                   "    val c: Double = 2.5 * 2\n"
                   "    val d: Double = 1 / 2.0\n"
                   "    val e: Int = -a\n"
                   "    val f: Double = -b\n"
                   "    ()\n");
    /* výsledek Int + Double je Double, do Int ho uložit nelze */
    expect_error(MAIN "    val a: Int = 1 + 2.5\n"
                      "    ()\n",
                 ERR_TYPE);
    /* konkatenace */
    expect_ok(MAIN "    val s: String = \"a\" + \"b\"\n"
                   "    ()\n");
    /* bez automatické konverze na String */
    expect_error(MAIN "    val s = \"a\" + 1\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    val s = 1.5 + \"a\"\n"
                      "    ()\n",
                 ERR_TYPE);
    /* String podporuje jen + */
    expect_error(MAIN "    val s = \"a\" - \"b\"\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    val s = \"a\" * 2\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    val s = \"a\" / \"b\"\n"
                      "    ()\n",
                 ERR_TYPE);
    /* unární minus jen na čísla */
    expect_error(MAIN "    val s = -\"a\"\n"
                      "    ()\n",
                 ERR_TYPE);
    /* Unit není číslo */
    expect_error(MAIN "    val a = () + 1\n"
                      "    ()\n",
                 ERR_TYPE);
    /* proměnná se typuje podle své definice */
    expect_error(MAIN "    val a = \"s\"\n"
                      "    val b = a * 2\n"
                      "    ()\n",
                 ERR_TYPE);
}

static void test_bool_outside_condition(void) {
    /* Boolean nelze uložit do proměnné */
    expect_error(MAIN "    val b = 1 < 2\n"
                      "    ()\n",
                 ERR_TYPE);
    /* ani přiřadit */
    expect_error(MAIN "    var b = 1\n"
                      "    b = 1 == 1\n"
                      "    ()\n",
                 ERR_TYPE);
    /* ani použít v aritmetice */
    expect_error(MAIN "    val a = (1 < 2) + 1\n"
                      "    ()\n",
                 ERR_TYPE);
}

/* Vnořené bloky: překrytí, rozsahy větví if a těla while. */
static void test_nested_blocks(void) {
    /* překrytí v podbloku, inicializace vidí vnější proměnnou */
    expect_ok(MAIN "    val x = 1\n"
                   "    if x < 2 then\n"
                   "        val x = x + 1\n"
                   "        val y = \"s\"\n"
                   "        ()\n"
                   "    else\n"
                   "        val x = \"jiný typ\"\n"
                   "        val y = 2.5\n"
                   "        ()\n"
                   "    ()\n");
    /* po konci podbloku zase platí vnější x (Int) */
    expect_ok(MAIN "    val x = 1\n"
                   "    while x < 2 do\n"
                   "        val x = \"s\"\n"
                   "        ()\n"
                   "    val y: Int = x\n"
                   "    ()\n");
    /* proměnná z podbloku po jeho konci neexistuje - 3 */
    expect_error(MAIN "    if 1 < 2 then\n"
                      "        val a = 1\n"
                      "        ()\n"
                      "    else\n"
                      "        ()\n"
                      "    val b = a\n"
                      "    ()\n",
                 ERR_UNDEFINED);
    /* proměnná z THEN větve není vidět v ELSE větvi */
    expect_error(MAIN "    if 1 < 2 then\n"
                      "        val a = 1\n"
                      "        ()\n"
                      "    else\n"
                      "        val b = a\n"
                      "        ()\n"
                      "    ()\n",
                 ERR_UNDEFINED);
    /* v podbloku lze přiřadit do vnější var */
    expect_ok(MAIN "    var i = 0\n"
                   "    while i < 10 do\n"
                   "        i = i + 1\n"
                   "    ()\n");
    /* ... ale ne do vnější val */
    expect_error(MAIN "    val i = 0\n"
                      "    while i < 10 do\n"
                      "        i = i + 1\n"
                      "    ()\n",
                 ERR_IMMUTABLE);
    /* podmínky: relační výrazy nad čísly i řetězci, == nad stejnými typy */
    expect_ok(MAIN "    val a = 1\n"
                   "    val s = \"x\"\n"
                   "    if a <= 2.5 then\n"
                   "        ()\n"
                   "    else\n"
                   "        ()\n"
                   "    if s < \"y\" then\n"
                   "        ()\n"
                   "    else\n"
                   "        ()\n"
                   "    while s != \"y\" do\n"
                   "        ()\n"
                   "    ()\n");
    /* podmínka musí být Boolean - 6 */
    expect_error(MAIN "    val a = 1\n"
                      "    while a do\n"
                      "        ()\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    if \"s\" then\n"
                      "        ()\n"
                      "    else\n"
                      "        ()\n"
                      "    ()\n",
                 ERR_TYPE);
    /* relační operátor nad nekompatibilními typy - 6 */
    expect_error(MAIN "    if 1 < \"s\" then\n"
                      "        ()\n"
                      "    else\n"
                      "        ()\n"
                      "    ()\n",
                 ERR_TYPE);
    expect_error(MAIN "    if \"s\" == 1 then\n"
                      "        ()\n"
                      "    else\n"
                      "        ()\n"
                      "    ()\n",
                 ERR_TYPE);
}

int main(void) {
    test_definitions();
    test_parameters();
    test_undefined();
    test_redefinition();
    test_immutable();
    test_type_errors();
    test_expressions();
    test_bool_outside_condition();
    test_nested_blocks();
    return 0;
}
