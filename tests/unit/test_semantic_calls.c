/**
 * @file
 * @brief Unit testy sémantické analýzy - volání funkcí (analyseCall).
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: připrav unit testy na sémantiku volání funkcí podle specifikace IFJ26,
 * sekce 2.3 („Příkaz volání funkce“), 4 a 6. Ověř:
 * - vestavěné funkce (počet a typy parametrů, Any u print/toStr, návratový typ);
 * - uživatelské funkce včetně volání před definicí a odvozeného návratového typu;
 * - implicitní konverzi Int -> Double u argumentu (opačně chyba);
 * - nedefinovanou funkci - kód 3; špatný počet argumentů - kód 5; špatný typ argumentu -
 *   kód 5 (pozice argumentu); nedefinovanou proměnnou v argumentu - kód 3;
 * - typ výsledku volání při inicializaci a přiřazení - kód 6;
 * - že se typ zapíše do node->type uzlu N_CALL (pro generátor).
 * Těla jsou bez if/else, aby testy nezávisely na chybě lexeru s DEDENT.
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
        fprintf(stderr, "očekávána chyba %d na %zu:%zu, dostal %d na %zu:%zu:\n%s\n", expected,
                line, column, run.code, run.error.line, run.error.column, text);
    }
    assert(run.code == expected);
    assert(run.error.line == line && run.error.column == column);
    finish(&run);
}

#define PROLOG "import ifj26.*\n"
#define MAIN "@main def run(): Unit = ()\n"
/* program, kde je testovaný kód tělem @main */
#define IN_MAIN(body) PROLOG "@main def run(): Unit =\n" body

static void test_builtins(void) {
    expect_ok(IN_MAIN("    print(1)\n"
                      "    print(2.5)\n"
                      "    print(\"s\")\n"
                      "    print(())\n"));
    expect_ok(IN_MAIN("    val s = readLine()\n"
                      "    val i: Int = readInt()\n"
                      "    val d: Double = readDouble()\n"
                      "    val t: String = toStr(i)\n"
                      "    val n: Int = strLength(s)\n"
                      "    val c: String = subStr(s, 0, n)\n"
                      "    val o: Int = ord(s, 0)\n"
                      "    val h: String = chr(o)\n"
                      "    val k: Int = toInt(d)\n"));
    /* odvozený typ proměnné z návratového typu vestavěné funkce */
    expect_ok(IN_MAIN("    val n = readInt()\n"
                      "    val m: Int = n\n"));
    /* Int -> Double u argumentu (toInt bere Double) */
    expect_ok(IN_MAIN("    val k = toInt(1)\n"));
    /* Int výsledek do Double proměnné */
    expect_ok(IN_MAIN("    val d: Double = readInt()\n"));
}

static void test_user_functions(void) {
    expect_ok(PROLOG "def f(x: Double): Double = x\n"
                     "@main def run(): Unit =\n"
                     "    val a = f(1)\n"
                     "    val b: Double = f(2.5)\n");
    /* volání před definicí, návratový typ se odvodí na požádání */
    expect_ok(PROLOG "def f(): Int = g(2)\n"
                     "def g(x: Int) = x\n" MAIN);
    expect_ok(PROLOG "@main def run(): Unit =\n"
                     "    val s: String = name()\n"
                     "def name() = \"x\"\n");
    /* volání jako samostatný příkaz - výsledek se zahodí */
    expect_ok(PROLOG "def f(): Int = 1\n"
                     "@main def run(): Unit =\n"
                     "    f()\n"
                     "    ()\n");
}

static void test_undefined(void) {
    expect_error(IN_MAIN("    foo()\n"), ERR_UNDEFINED);
    expect_error(IN_MAIN("    val a = foo(1)\n"), ERR_UNDEFINED);
    /* nedefinovaná proměnná jako argument */
    expect_error(IN_MAIN("    print(x)\n"), ERR_UNDEFINED);
    /* parametr jiné funkce není vidět */
    expect_error(PROLOG "def f(a: Int): Int = a\n"
                        "@main def run(): Unit = print(a)\n",
                 ERR_UNDEFINED);
}

static void test_argument_count(void) {
    expect_error_at(IN_MAIN("    print()\n"), ERR_CALL, 3, 5);
    expect_error(IN_MAIN("    print(1, 2)\n"), ERR_CALL);
    expect_error(IN_MAIN("    readInt(1)\n"), ERR_CALL);
    expect_error(IN_MAIN("    subStr(\"a\", 1)\n"), ERR_CALL);
    expect_error(PROLOG "def f(x: Int): Int = x\n"
                        "@main def run(): Unit =\n"
                        "    f()\n",
                 ERR_CALL);
    expect_error(PROLOG "def f(): Int = 1\n"
                        "@main def run(): Unit =\n"
                        "    f(1)\n",
                 ERR_CALL);
}

static void test_argument_types(void) {
    /* chyba se hlásí na pozici argumentu */
    expect_error_at(IN_MAIN("    strLength(5)\n"), ERR_CALL, 3, 15);
    expect_error(IN_MAIN("    toInt(\"s\")\n"), ERR_CALL);
    expect_error(IN_MAIN("    chr(1.5)\n"), ERR_CALL);  /* Double -> Int neexistuje */
    expect_error(IN_MAIN("    subStr(\"a\", 0, \"b\")\n"), ERR_CALL);
    expect_error(PROLOG "def f(x: Int): Int = x\n"
                        "@main def run(): Unit =\n"
                        "    f(1.5)\n",
                 ERR_CALL);
    expect_error(PROLOG "def f(x: String): Int = 1\n"
                        "@main def run(): Unit =\n"
                        "    val n = 1\n"
                        "    f(n)\n",
                 ERR_CALL);
}

static void test_result_type(void) {
    expect_error(IN_MAIN("    val i: Int = readDouble()\n"), ERR_TYPE);
    expect_error(IN_MAIN("    val r: String = toInt(1.5)\n"), ERR_TYPE);
    expect_error(IN_MAIN("    var s = \"a\"\n"
                         "    s = readInt()\n"),
                 ERR_TYPE);
    /* volání funkce vracející Unit nejde uložit do proměnné typu Int */
    expect_error(IN_MAIN("    val x: Int = print(1)\n"), ERR_TYPE);
}

/* Generátor podle node->type volání vkládá INT2FLOAT / volí ADD vs CONCAT. */
static void test_call_node_type(void) {
    SemanticRun run;
    run_semantic(&run, PROLOG "def f() = 2.5\n"
                              "@main def run(): Unit =\n"
                              "    val a = readInt()\n"
                              "    val b = f()\n");
    assert(run.code == ERR_OK);

    const Node *main = run.program->body->next;
    const Node *declA = main->body;
    const Node *declB = declA->next;
    assert(declA->left->kind == N_CALL && declA->left->type == TY_INT);
    assert(declB->left->kind == N_CALL && declB->left->type == TY_DOUBLE);
    /* odvozené typy proměnných i funkce */
    assert(declA->type == TY_INT && declB->type == TY_DOUBLE);
    assert(run.program->body->type == TY_DOUBLE);
    finish(&run);
}

int main(void) {
    test_builtins();
    test_user_functions();
    test_undefined();
    test_argument_count();
    test_argument_types();
    test_result_type();
    test_call_node_type();
    return 0;
}
