
#include "stdio.h"
#include "stdlib.h"

#define crash {printf("%s not implemented on line %d, file %s", __func__, __LINE__, __FILE__);abort();}

void __mulxc3() crash
void __mulsc3() crash
void __muldc3() crash