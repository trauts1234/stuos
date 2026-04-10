#ifndef assert_H
#define assert_H

#ifdef NDEBUG
#define assert(ignore)((void) 0)
#else
void __assert(bool condition, int line, char* file);

#define assert(value)(__assert(value, __LINE__, __FILE__))
#endif

#endif