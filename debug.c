#ifndef ARDUINT_DEBUG_C
#define ARDUINT_DEBUG_C

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "common.h"
#include "errorHandling.h"
#include "expression.h"
#include "lexer.h"
#include "node.h"
#include "token.h"

const char *lexemes[68] = {
    "IDENTIFIER",
    "NUMBER",
    "STRING",

    // Control flow
    "IF",
    "ELSE",
    "WHILE",
    "FOR",
    "LOOP",
    "RETURN",
    "FUNCTION",
    "FALSE",
    "TRUE",

    // Arduino operations
    "SETPIN",
    "ANALOG",
    "READPIN",
    "DELAY",
    "DELAY_MICROS",
    "PRINT",
    "MICROS",
    "MS",
    "S",
    "M",
    "H",

    // Functions & keywords
    "DEFINE",
    "AS",
    "TO",

    // Operators
    "PLUS",
    "MINUS",
    "SLASH",
    "STAR",
    "POWER",
    "AND",
    "NOT",
    "OR",

    // One-character tokens
    "BACKSLASH",
    "COMMA",
    "POINT_COMMA",
    "COLON",
    "DOT",
    "EQUALS",
    "PERCENT",
    "LEFT_SQUARE_BRACKET",
    "RIGHT_SQUARE_BRACKET",
    "LEFT_ROUND_BRACKET",
    "RIGHT_ROUND_BRACKET",
    "LEFT_SQUIGGLY_BRACKET",
    "RIGHT_SQUIGGLY_BRACKET",
    "LESS",
    "GREATER",

    // Two-character tokens
    "EQUALS_EQUALS",
    "GREATER_EQUALS",
    "LESS_EQUALS",
    "NOT_EQUALS",
    "MINUS_EQUALS",
    "PLUS_EQUALS",
    "SLASH_EQUALS",
    "STAR_EQUALS",
    "POWER_EQUALS",
    "AND_EQUALS",
    "OR_EQUALS",
    "PERCENT_EQUALS",

    "PLUS_PLUS",
    "MINUS_MINUS",
    "AND_AND",
    "OR_OR",

    "NEWLINE",
    "ERROR",
    "EOF",
};

void printToken(Token *token) {
  printf("[%s", lexemes[token->type]);

  bool sourceNeeded = token->type == TOKEN_NUMBER ||
                      token->type == TOKEN_IDENTIFIER ||
                      token->type == TOKEN_ERROR || token->type == TOKEN_STRING;

  if (!sourceNeeded) {
    printf("] ");
    if (token->type == TOKEN_NEWLINE) printf("\n");
    return;
  }

  char *characters = malloc(sizeof(char) * token->length + 1);
  if (characters == NULL) {
    defaultError("Ran out of memory.");
    exit(1);
  }
  memcpy(characters, token->start, token->length);
  characters[token->length] = '\0';

  printf(": %s] ", characters);

  free(characters);
}

void printTokens(Vector *tokens) {
  for (int i = 0; i < tokens->count; i++) {
    printToken(vecGet(tokens, i));
  }
  printf("\n");
}
/*
static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int printInstruction(Chunk *chunk, int offset) {
  printf("%04d ", offset);

  uint8_t instruction = *(uint8_t *)vecGet(&chunk->byteCode, offset);
  switch (instruction) {
  case OP_RETURN: return simpleInstruction("OP_RETURN", offset);
  default: defaultError("Unknown opCode %d.", instruction); return offset + 1;
  }
}

void printChunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->byteCode.count;) {
    offset = printInstruction(chunk, offset);
  }
}*/

void printExpression(ExpressionNode *expr);

static void printLexeme(Token *token) {
  char *str = copySourceStr(token->start, token->length);
  printf("%s", str);
  free(str);
}

static void printValue(Value *value) {
  switch (value->type) {
  case NULL_VAL:   printf("NULL"); break;
  case NUMBER_VAL: printf("%lf", value->as.number); break;
  default:         printf("ErrorVal"); break;
  }
}

static void printAtom(AtomNode *a) {
  switch (a->type) {
  case ATOM_NAME:  printLexeme(a->as.name); break;
  case ATOM_VALUE: printValue(a->as.value); break;
  }
}

static void printGrouping(GroupingNode *g) {
  if (g == NULL) return;

  switch (g->type) {
  case NODE_ATOM:     printAtom(g->as.atom);
  case NODE_GROUPING: {
    printf("(");
    printf(" ");
    printExpression(g->as.expression);
    printf(" )");
    break;
  }
  }
}

static void printPower(PowerNode *p) {
  if (p == NULL) return;

  switch (p->type) {
  case NODE_GROUPING: printGrouping(p->as.grouping); break;
  case NODE_POWER:    {
    printf("(^ ");
    printGrouping(p->as.power.lhs);
    printf(" ");
    printPower(p->as.power.rhs);
    printf(")");
    break;
  }
  }
}

static void printUnary(UnaryNode *u) {
  if (u == NULL) return;

  switch (u->type) {
  case NODE_POWER: printPower(u->as.power); break;
  case NODE_UNARY: {
    printf("(");
    printLexeme(u->as.unary.op);
    printf(" ");
    printUnary(u->as.unary.rhs);
    printf(")");
    break;
  }
  }
}

static void printFactor(FactorNode *f) {
  if (f == NULL) return;

  switch (f->type) {
  case NODE_UNARY:  printUnary(f->as.unary); break;
  case NODE_FACTOR: {
    printf("(");
    printLexeme(f->as.factor.op);
    printf(" ");
    printFactor(f->as.factor.lhs);
    printf(" ");
    printUnary(f->as.factor.rhs);
    printf(")");
    break;
  }
  }
}

static void printSum(SumNode *sum) {
  if (sum == NULL) return;

  switch (sum->type) {
  case NODE_FACTOR: printFactor(sum->as.factor); break;
  case NODE_SUM:    {
    printf("(");
    printLexeme(sum->as.sum.op);
    printf(" ");
    printSum(sum->as.sum.lhs);
    printf(" ");
    printFactor(sum->as.sum.rhs);
    printf(")");
    break;
  }
  }
}

static void printComparison(ComparisonNode *comp) {
  if (comp == NULL) return;

  switch (comp->type) {
  case NODE_SUM:        printSum(comp->as.sum); break;
  case NODE_COMPARISON: {
    printf("(");
    printLexeme(comp->as.comparison.op);
    printf(" ");
    printComparison(comp->as.comparison.lhs);
    printf(" ");
    printSum(comp->as.comparison.rhs);
    printf(")");
    break;
  }
  }
}

static void printAssignment(AssignmentNode *assign) {
  if (assign == NULL) return;

  switch (assign->type) {
  case NODE_COMPARISON: printComparison(assign->as.comparison); break;
  case NODE_ASSIGN:     {
    printf("(");
    printLexeme(assign->as.assignment.name);
    printf(" = ");
    printExpression(assign->as.assignment.rhs);
    printf(")");
    break;
  }
  }
}

void printExpression(ExpressionNode *expr) {
  if (expr == NULL) return;

  printf("(Expr ");
  printAssignment(expr->assignment);
  printf(")");
}

void printProgramTree(ProgramNode *program) {
  if (program == NULL) return;

  printf("Program: (");
  printExpression(program->expression);
  printf(")\n");
}

#endif