#ifndef ELF_H
#define ELF_H
#include "fs.h"

/// Generates an ELF, given a pointer to the file loaded in RAM, and allocates/sets up the correct structures in memory to make this ready to run
struct LoadedProgram instantiate_ELF(struct VNode exe, char*const *argv);

#endif