#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include "data_structs.h"
#include "const_vars.h"

// Define the root variable here
extern MemoryBlock *root;

void initTree();
MemoryBlock *createMemoryBlock(int size, int address, BlockStatus status, MemoryBlock *parent);
bool splitBlock(MemoryBlock *block);
MemoryBlock *findAllocatedAddress(MemoryBlock *node, int memory);
MemoryBlock *allocate(MemoryBlock *root, int memory);
void updateParent(MemoryBlock *node, int memory, int add);
bool deallocate(MemoryBlock *block);
bool mergeBlock(MemoryBlock *block);

#endif
