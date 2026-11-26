#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include "ast.h"
#include "error.h"
#include "parser.h"

ErrorCode analyseSemantics(Node *program, ParseError *error);

#endif
