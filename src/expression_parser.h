#ifndef EXPRESSION_PARSER_H
#define EXPRESSION_PARSER_H

#include "../tmp/ast.h"
#include "error.h"
#include "parser.h"
#include <stddef.h>

ErrorCode parseExpression(Parser *parser, ParseError *error, Node **result);

#endif
