#ifndef DLFCN_H
#define DLFCN_H

void  *dlopen(const char *, int);
void  *dlsym(void *, const char *);
int    dlclose(void *);
char  *dlerror(void);

#endif