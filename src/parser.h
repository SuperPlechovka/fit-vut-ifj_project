#ifndef PARSER_H
#define PARSER_H

#include <stddef.h>
#include "../tmp/ast.h"
#include "scanner.h"

typedef struct {
  int code;
  size_t line;
  size_t column;
  const char *message;
} ParseError;

typedef struct {
  Scanner *scanner;
  Token current;
  Token lookahead;
} Parser;

int parserInit(Parser *parser, Scanner *scanner);
void parserDestroy(Parser *parser);
int parserAdvance(Parser *parser);
int parseExpression(Parser *parser, ParseError *error, Node *result);

#endif