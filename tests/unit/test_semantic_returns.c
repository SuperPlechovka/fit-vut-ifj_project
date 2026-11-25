/**
 * @file
 * @brief Unit testy sémantické analýzy - return, výsledek těla, návratové typy, rekurze.
 * @author Frantisek Fiala (xfialaf00@vutbr.cz)
 */
/* AI prompt: připrav unit testy na sémantiku návratových hodnot podle specifikace IFJ26,
 * sekce 2.3 („Návratový typ a návratová hodnota“) a 3. Ověř:
 * - výsledek těla (poslední příkaz; deklarace/přiřazení/while dávají Unit) proti
 *   uvedenému návratovému typu, Int -> Double, explicitní Unit zahodí hodnotu;
 * - return s hodnotou, samostatný return jen v Unit funkci, return vyžaduje uvedený typ;
 * - odvození návratového typu (i na požádání při volání před definicí), @main musí být Unit;
 * - přímou i nepřímou rekurzi - funkce na cyklu musí mít uvedený typ;
 * - že volaná funkce nevidí proměnné volajícího;
 * - že Bool (relační výraz) nejde vrátit ani použít jako samostatný příkaz (spec 3) - kód 6.
 * Případy s if/else v těle závisí na opravě DEDENT v lexeru - jsou v test_lexer_dependent.
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

static void test_body_result(void) {
    expect_ok(PROLOG "def f(): Int = 1\n" MAIN);
    expect_ok(PROLOG "def f(): Double = 1\n" MAIN); /* Int -> Double */
    expect_ok(PROLOG "def f(): Unit = 5\n" MAIN);   /* explicitní Unit hodnotu zahodí */
    expect_ok(PROLOG "def f(): Int =\n"
                     "    val a = 1\n"
                     "    a\n" MAIN);
    expect_error(PROLOG "def f(): Int = \"s\"\n" MAIN, ERR_TYPE);
    expect_error(PROLOG "def f(): Int = 1.5\n" MAIN, ERR_TYPE); /* Double -> Int ne */
    /* blok zakončený deklarací, přiřazením nebo cyklem má výsledek () */
    expect_error(PROLOG "def f(): Int =\n"
                        "    val a = 1\n" MAIN,
                 ERR_TYPE);
    expect_error(PROLOG "def f(): Int =\n"
                        "    var a = 1\n"
                        "    a = 2\n" MAIN,
                 ERR_TYPE);
    expect_error(PROLOG "def f(): Int =\n"
                        "    var a = 1\n"
                        "    while a < 3 do\n"
                        "        a\n" MAIN,
                 ERR_TYPE);
}

static void test_inference(void) {
    expect_ok(PROLOG "def f(x: Int) = x\n" MAIN);
    expect_ok(PROLOG "def f() = ()\n" MAIN);
    expect_ok(PROLOG "def g() = 1\n"
                     "def f(): Int = g()\n" MAIN);
    /* volání před definicí - typ g se odvodí na požádání */
    expect_ok(PROLOG "def f(): Int = g()\n"
                     "def g() = 1\n" MAIN);
    expect_ok(PROLOG "def f(): Double = g()\n"
                     "def g() = 1\n" MAIN);
    expect_error(PROLOG "def f(): String = g()\n"
                        "def g() = 1\n" MAIN,
                 ERR_TYPE);
    expect_ok(PROLOG "def a() = b()\n"
                     "def b() = c()\n"
                     "def c() = \"s\"\n" MAIN);
    /* @main bez uvedeného typu se musí odvodit na Unit */
    expect_ok(PROLOG "@main def run() = ()\n");
    expect_error(PROLOG "@main def run() = 1\n", ERR_TYPE);

    /* odvozený typ se zapíše do uzlu funkce (pro generátor) */
    SemanticRun run;
    run_semantic(&run, PROLOG "def f() = g()\n"
                              "def g() = 2.5\n" MAIN);
    assert(run.code == ERR_OK);
    assert(run.program->body->type == TY_DOUBLE);
    assert(run.program->body->next->type == TY_DOUBLE);
    finish(&run);
}

static void test_return(void) {
    expect_ok(PROLOG "def f(x: Int): Int =\n"
                     "    return x\n" MAIN);
    expect_ok(PROLOG "def f(x: Int): Double =\n"
                     "    return x\n" MAIN);
    expect_ok(PROLOG "def f(): Unit =\n"
                     "    return\n" MAIN);
    expect_ok(PROLOG "def f(): Unit =\n"
                     "    return 5\n" MAIN);
    expect_error(PROLOG "def f(x: Int): String =\n"
                        "    return x\n" MAIN,
                 ERR_TYPE);
    /* return vyžaduje uvedený návratový typ */
    expect_error_at(PROLOG "def f(x: Int) =\n"
                           "    return x\n" MAIN,
                    ERR_TYPE, 3, 5);
    /* samostatný return jen ve funkci s explicitním Unit */
    expect_error(PROLOG "def f(): Int =\n"
                        "    return\n" MAIN,
                 ERR_TYPE);
    /* return uvnitř cyklu na konci těla (8 -> 0 mezer projde i s chybou DEDENT) */
    expect_error(PROLOG "def f(): Int =\n"
                        "    var a = 1\n"
                        "    while a < 3 do\n"
                        "        return \"s\"\n" MAIN,
                 ERR_TYPE);
}

static void test_recursion(void) {
    expect_ok(PROLOG "def f(n: Int): Int = f(n)\n" MAIN);
    expect_ok(PROLOG "def f(n: Int): Int = g(n)\n"
                     "def g(n: Int): Int = f(n)\n" MAIN);
    expect_error(PROLOG "def f(n: Int) = f(n)\n" MAIN, ERR_TYPE);
    /* nepřímá rekurze - chyba bez ohledu na pořadí definic */
    expect_error(PROLOG "def f(n: Int) = g(n)\n"
                        "def g(n: Int): Int = f(n)\n" MAIN,
                 ERR_TYPE);
    expect_error(PROLOG "def g(n: Int): Int = f(n)\n"
                        "def f(n: Int) = g(n)\n" MAIN,
                 ERR_TYPE);
    expect_error(PROLOG "def a(): Int = b()\n"
                        "def b() = c()\n"
                        "def c(): Int = a()\n" MAIN,
                 ERR_TYPE);
}

static void test_callee_scope(void) {
    /* g se analyzuje uprostřed f, ale proměnné f nevidí */
    expect_error(PROLOG "def f(): Int =\n"
                        "    val x = 1\n"
                        "    g()\n"
                        "def g() = x\n" MAIN,
                 ERR_UNDEFINED);
}

/* Spec 3: Bool lze použít jen přímo v podmínce if/while - nelze ho uložit, předat ani vrátit. */
static void test_bool_result(void) {
    expect_error(PROLOG "def f() = 1 < 2\n" MAIN, ERR_TYPE);
    expect_error(PROLOG "def f(): Int = 1 < 2\n" MAIN, ERR_TYPE);
    expect_error(PROLOG "def f(): Unit = 1 < 2\n" MAIN, ERR_TYPE);
    expect_error(PROLOG "def f(): Unit =\n"
                        "    return 1 < 2\n" MAIN,
                 ERR_TYPE);
    expect_error(PROLOG "@main def run(): Unit =\n"
                        "    val a = 1\n"
                        "    a == 2\n"
                        "    ()\n",
                 ERR_TYPE);
}

/* Čeká na opravu DEDENT v lexeru (návrat odsazení na nenulovou úroveň před else).
 * Do té doby v main vypnuto. */
static void test_lexer_dependent(void) {
    /* kontroly platí i pro nedosažitelný kód: return uvnitř cyklu */
    expect_error(PROLOG "def f(): Int =\n"
                        "    var a = 1\n"
                        "    while a < 3 do\n"
                        "        return \"s\"\n"
                        "    a\n" MAIN,
                 ERR_TYPE);
    expect_ok(PROLOG "def f(x: Int): Int =\n"
                     "    if x < 1 then\n"
                     "        return 1\n"
                     "    else\n"
                     "        return 2\n" MAIN);
    /* tělo končí if: jedna větev bez return */
    expect_error(PROLOG "def f(x: Int): Int =\n"
                        "    if x < 1 then\n"
                        "        return 1\n"
                        "    else\n"
                        "        2\n" MAIN,
                 ERR_TYPE);
    /* bez CONTROL není if výraz - Int větve nestačí */
    expect_error(PROLOG "def f(x: Int): Int =\n"
                        "    if x < 1 then\n"
                        "        1\n"
                        "    else\n"
                        "        2\n" MAIN,
                 ERR_TYPE);
    expect_ok(PROLOG "def f(x: Int): Unit =\n"
                     "    if x < 1 then\n"
                     "        1\n"
                     "    else\n"
                     "        2\n" MAIN);
    expect_ok(PROLOG "def f(x: Int) =\n"
                     "    if x < 1 then\n"
                     "        print(x)\n"
                     "    else\n"
                     "        ()\n" MAIN);
    expect_error(PROLOG "def f(x: Int) =\n"
                        "    if x < 1 then\n"
                        "        1\n"
                        "    else\n"
                        "        ()\n" MAIN,
                 ERR_TYPE);
    /* if uprostřed těla - jeho výsledek se nepoužije */
    expect_ok(PROLOG "def f(x: Int): Int =\n"
                     "    if x < 1 then\n"
                     "        1\n"
                     "    else\n"
                     "        2\n"
                     "    x\n" MAIN);
}

int main(void) {
    test_body_result();
    test_inference();
    test_return();
    test_recursion();
    test_callee_scope();
    test_bool_result();
    (void)test_lexer_dependent; /* zapnout po opravě DEDENT v lexeru */
    return 0;
}
