/**
 * @file
 * @brief Načte program ze stdin, zkontroluje ho a vypíše cílový kód na stdout.
 * @author Jan Křepinský (xkrepij00@stud.fit.vutbr.cz)
 */

#include "ast.h"
#include "codegen.h"
#include "error.h"
#include "ilist.h"
#include "insidecodegene.h"
#include "lexer.h"
#include "parser.h"
#include "semantic_analysis.h"
#include <stdio.h>

int main(void) {
    Lexer lexer = {0};
    Parser parser = {0};
    ParseError error = {0};
    Node *program = NULL;
    ListOfInstr instructions;
    int code = ERR_OK;
    const char *code_phase = "lexer initialization";

    listInit(&instructions);

    const LexerResult lexer_result = lexer_init(&lexer, stdin);
    if (lexer_result != LEXER_OK) {
        code = ERR_INTERNAL;
    }

    if (code == ERR_OK) {
        code_phase = "parsing";
        code = parserInit(&parser, &lexer, &error);
        if (code == ERR_OK) {
            code = parseProgram(&parser, &error, &program);
        }
        if (ferror(stdin)) {
            code = ERR_INTERNAL;
            error = (ParseError){.code = ERR_INTERNAL, .message = "cannot read standard input"};
        }
    }

    if (code == ERR_OK) {
        code_phase = "semantic analysis";
        code = analyseSemantics(program, &error);
    }

    if (code == ERR_OK) {
        code_phase = "intermediate code generation";
        if (codegenProgram(program, &instructions) != 0) {
            code = ERR_INTERNAL;
        }
    }

    if (code == ERR_OK) {
        code_phase = "target code output";
        codegen_generate(&instructions);
        if (fflush(stdout) == EOF || ferror(stdout)) {
            code = ERR_INTERNAL;
        }
    }

    listFree(&instructions);
    nodeFree(program);
    parserDestroy(&parser);
    lexer_destroy(&lexer);

    if (code != ERR_OK) {
        if (error.line != 0) {
            fprintf(stderr, "%zu:%zu: ", error.line, error.column);
        }
        fprintf(stderr, "error %d (%s): %s\n", code, code_phase,
                error.message != NULL ? error.message : "internal compiler error");
    }
    return code;
}
