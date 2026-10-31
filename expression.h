#ifndef ARDUINT_EXPRESSION_H
#define ARDUINT_EXPRESSION_H

#include "node.h"
#include "token.h"
#include "value.h"

typedef struct PowerNode PowerNode;
typedef struct UnaryNode UnaryNode;
typedef struct FactorNode FactorNode;
typedef struct SumNode SumNode;
typedef struct ComparisonNode ComparisonNode;
typedef struct AssignmentNode AssignmentNode;
typedef struct ExpressionNode ExpressionNode; // Forward declaration

// A-> V | NAME
typedef struct {
  AtomType type;
  union {
    Value *value;
    Token *name;
  } as;
} AtomNode;

// G -> A | ( E )
typedef struct {
  NodeType type;
  union {
    ExpressionNode *expression;
    AtomNode *atom;
  } as;
} GroupingNode;

// P -> G | G ^ P
typedef struct {
  NodeType type;
  union {
    GroupingNode *grouping;
    struct {
      GroupingNode *lhs;
      // Token *op; //unneccesary, it'll always be '^'
      PowerNode *rhs;
    } power;
  } as;
} PowerNode;

// U -> P | -U | !U
typedef struct {
  NodeType type;
  union {
    PowerNode *power;
    struct {
      Token *op;
      UnaryNode *rhs;
    } unary;
  } as;
} UnaryNode;

// F -> U | F * U
typedef struct {
  NodeType type;
  union {
    UnaryNode *unary;
    struct {
      FactorNode *lhs;
      Token *op;
      UnaryNode *rhs;
    } factor;
  } as;
} FactorNode;

// M -> F | M + F
typedef struct {
  NodeType type;
  union {
    FactorNode *factor;
    struct {
      SumNode *lhs;
      Token *op;
      FactorNode *rhs;
    } sum;
  } as;
} SumNode;

// C -> M | C == M
typedef struct {
  NodeType type;
  union {
    SumNode *sum;
    struct {
      ComparisonNode *lhs;
      Token *op;
      SumNode *rhs;
    } comparison;
  } as;
} ComparisonNode;

// A -> C | NAME = E
typedef struct {
  NodeType type;
  union {
    ComparisonNode *sum;
    struct {
      Token *name;
      ExpressionNode *rhs;
    } assignment;
  } as;
} AssignmentNode;

// E -> A
typedef struct {
  AssignmentNode *assignment;
} ExpressionNode;

// S -> E
typedef struct {
  ExpressionNode *expression;
} ProgramNode;

#endif