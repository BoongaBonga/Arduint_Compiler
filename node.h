#ifndef ARDUINT_NODE_H
#define ARDUINT_NODE_H

typedef enum {
  NODE_PROGRAM,    // S -> E
  NODE_EXPRESSION, // E -> A
  NODE_ASSIGN,     // A -> C | NAME = E
  NODE_COMPARISON, // C -> M | C == M
  NODE_SUM,        // M -> F | M + F
  NODE_FACTOR,     // F -> U | F * U
  NODE_UNARY,      // U -> P | -U | !U
  NODE_POWER,      // P -> G | G ^ P
  NODE_GROUPING,   // G -> A | ( E )
  NODE_ATOM,       // A -> V | NAME
} NodeType;

typedef enum { NAME, VALUE } AtomType;

#endif