#ifndef MATH_H
#define MATH_H

#define INFINITY 1e504f
#define HUGE_VAL 1e500
//GNU extension
#define	NAN (0.0f / 0.0f)
// #define NAN ((union { uint32_t u; float f; }){ .u = 0x7FC00000 }.f)

//TCC doesn't like complex.h
// #include "_openlibm/openlibm_complex.h"
#include "_openlibm/openlibm_math.h"

#endif /* !MATH_H */
