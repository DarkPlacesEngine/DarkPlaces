#ifndef INTOVERFLOW_H
#define INTOVERFLOW_H

// simple safe library to handle integer overflows when doing buffer size calculations
// Usage:
//   - calculate data size using INTOVERFLOW_??? macros
//   - compare: calculated-size <= INTOVERFLOW_NORMALIZE(buffersize)
// Functionality:
//   - all overflows (values > INTOVERFLOW_MAX) and errors are mapped to INTOVERFLOW_MAX
//   - if any input of an operation is INTOVERFLOW_MAX, INTOVERFLOW_MAX will be returned
//   - otherwise, regular arithmetics apply

#define INTOVERFLOW_MAX 2147483647

#define INTOVERFLOW_ADD(a,b) (((a) < INTOVERFLOW_MAX && (b) < INTOVERFLOW_MAX && (a) < INTOVERFLOW_MAX - (b)) ? ((a) + (b)) : INTOVERFLOW_MAX)
#define INTOVERFLOW_SUB(a,b) (((a) < INTOVERFLOW_MAX && (b) < INTOVERFLOW_MAX && (b) <= (a))                  ? ((a) - (b)) : INTOVERFLOW_MAX)
#define INTOVERFLOW_MUL(a,b) (((a) < INTOVERFLOW_MAX && (b) < INTOVERFLOW_MAX && (a) < INTOVERFLOW_MAX / (b)) ? ((a) * (b)) : INTOVERFLOW_MAX)
#define INTOVERFLOW_DIV(a,b) (((a) < INTOVERFLOW_MAX && (b) < INTOVERFLOW_MAX && (b) > 0)                     ? ((a) / (b)) : INTOVERFLOW_MAX)

#define INTOVERFLOW_NORMALIZE(a) (((a) < INTOVERFLOW_MAX) ? (a) : (INTOVERFLOW_MAX - 1))

#endif
