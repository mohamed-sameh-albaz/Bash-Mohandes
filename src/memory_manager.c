#include "memory_manager.h"
#include <string.h>

void initTree()
{

    // Initialize root if it's NULL
    if (root == NULL)
    {
        root = (MemoryBlock *)malloc(sizeof(MemoryBlock));
        if (root == NULL)
        {
            fprintf(stderr, "Failed to allocate memory for root block\n");
            return;
        }
    }

    root->left = NULL;
    root->right = NULL;
    root->parent = NULL;
    root->ocuupiedmemory = 0;
    root->status = FREE;
    root->address = 0;
    root->size = MEM_SIZE;
}

MemoryBlock *createMemoryBlock(int size, int address, BlockStatus status, MemoryBlock *parent)
{
    MemoryBlock *block = (MemoryBlock *)malloc(sizeof(MemoryBlock));
    if (block == NULL)
    {
        return NULL;
    }
    block->size = size;
    block->address = address;
    block->status = status;

    block->ocuupiedmemory = 0;
    block->parent = parent;
    block->left = NULL;
    block->right = NULL;
    return block;
}

bool splitBlock(MemoryBlock *block)
{
    if (block == NULL || block->status != FREE || block->size < 2)
    {
        return false;
    }

    int childSize = block->size / 2;
    block->left = createMemoryBlock(childSize, block->address, FREE, block);
    if (block->left == NULL)
    {
        return false;
    }

    block->right = createMemoryBlock(childSize, block->address + childSize, FREE, block);
    if (block->right == NULL)
    {

        free(block->left);
        block->left = NULL;
        return false;
    }

    block->status = SPLIT;
    return true;
}

MemoryBlock *findAllocatedAddress(MemoryBlock *node, int memory) // start with root
{
    if (node == NULL || node->status == RESERVED || node->size < memory)
    {
        return NULL;
    }
    if (memory > node->size / 2 && node->status == FREE)
    {
        return node;
    }

    if (node->left == NULL && node->right == NULL)
    {
        splitBlock(node); // incase of free node
    }
    // become split or he was split
    if (node->size - node->ocuupiedmemory < memory)
    {
        return NULL;
    }
    MemoryBlock *left = findAllocatedAddress(node->left, memory);
    if (left != NULL)
    {
        return left;
    }
    return findAllocatedAddress(node->right, memory);
}

MemoryBlock *allocate(MemoryBlock *root, int memory) // to take address and size (block size)
{
    if (root == NULL || root->size < memory)
    {
        return NULL; // can not allocate go to waiting list
    }
    MemoryBlock *foundBlock = findAllocatedAddress(root, memory);

    if (foundBlock)
    {
        foundBlock->ocuupiedmemory = memory;
        foundBlock->status = RESERVED;
        updateParent(foundBlock, memory, 1);
        return foundBlock;
    }

    return NULL;
}

void updateParent(MemoryBlock *node, int memory, int add)
{
    if (node->parent == NULL || node->parent->status != SPLIT) // for safe side the check of SPLIT
        return;

    if (add)
        node->parent->ocuupiedmemory += memory;
    else
        node->parent->ocuupiedmemory -= memory;

    updateParent(node->parent, memory, add);
}

bool mergeBlock(MemoryBlock *block)
{
    if (block == NULL || block->parent == NULL || block->status != FREE)
    {
        return false;
    }

    MemoryBlock *parent = block->parent;
    MemoryBlock *otherCh = (parent->left == block) ? parent->right : parent->left;

    if (otherCh == NULL || otherCh->status != FREE)
    {
        return false;
    }

    free(parent->left);
    free(parent->right);
    parent->left = NULL;
    parent->right = NULL;
    parent->status = FREE;
    return true;
}

bool deallocate(MemoryBlock *block)
{

    if (block == NULL)
        return false;

    if (block->parent == NULL) // root
    {

        block->ocuupiedmemory = 0;
        block->status = FREE;
        block->left = NULL;  // for safety
        block->right = NULL; // for safety
        return true;
    }
    updateParent(block, block->ocuupiedmemory, 0);
    block->status = FREE;

    if (!mergeBlock(block))
    {
        block->ocuupiedmemory = 0;
    }
    return true;
}

// Function to print the binary tree in a visual tree-like format
void printTree(MemoryBlock *node, int level, char *prefix, bool isLeft)
{
    if (node == NULL)
        return;

    // Create the prefix for the current node and its children
    char newPrefix[256];
    printf("%s", prefix);

    if (level > 0)
    {
        // Print the connector and the node info
        printf("%s", isLeft ? "├── " : "└── ");

        // Create new prefix for children
        sprintf(newPrefix, "%s%s", prefix, isLeft ? "│   " : "    ");
    }
    else
    {
        // Root node
        sprintf(newPrefix, "%s", "    ");
    }

    // Print node information
    char statusStr[10];
    if (node->status == FREE)
        strcpy(statusStr, "FREE");
    else if (node->status == RESERVED)
        strcpy(statusStr, "RESERVED");
    else
        strcpy(statusStr, "SPLIT");

    printf("[Addr: %d, Size: %d, Occupied Memory: %d, Status: %s]\n", node->address, node->size, node->ocuupiedmemory, statusStr);
    // Recursively print the children
    printTree(node->left, level + 1, newPrefix, true);
    printTree(node->right, level + 1, newPrefix, false);
}

// int main()
// {
//     // Initialize the memory tree
//     root = (MemoryBlock *)malloc(sizeof(MemoryBlock));
//     initTree();

//     printf("Initial Tree:\n");
//     printTree(root, 0, "", false);

//     // Test case 1: Allocate memory
//     printf("\nAllocating 256 bytes...\n");
//     MemoryBlock *block1 = allocate(root, 256);
//     printTree(root, 0, "", false);

//     // Test case 2: Allocate another memory block
//     printf("\nAllocating 128 bytes...\n");
//     MemoryBlock *block2 = allocate(root, 128);
//     printTree(root, 0, "", false);

//     // Test case 3: Deallocate a block
//     printf("\nDeallocating 256 bytes...\n");
//     deallocate(block1);
//     printTree(root, 0, "", false);

//     // Test case 4: Allocate a smaller block
//     printf("\nAllocating 64 bytes...\n");
//     MemoryBlock *block3 = allocate(root, 64);
//     printTree(root, 0, "", false);

//     // Test case 5: Deallocate another block
//     printf("\nDeallocating 128 bytes...\n");
//     deallocate(block2);
//     printTree(root, 0, "", false);

//     // Test case 6: Allocate a larger block to demonstrate fragmentation
//     printf("\nAllocating 384 bytes...\n");
//     MemoryBlock *block4 = allocate(root, 384);
//     printTree(root, 0, "", false);

//     // Test case 7: Allocate several small blocks in sequence
//     printf("\nAllocating multiple small blocks...\n");
//     MemoryBlock *block5 = allocate(root, 32);
//     MemoryBlock *block6 = allocate(root, 32);
//     MemoryBlock *block7 = allocate(root, 32);
//     printTree(root, 0, "", false);

//     // Test case 8: Deallocate all blocks to test merging
//     printf("\nDeallocating all blocks to test merging...\n");
//     if (block3)
//         deallocate(block3);
//     if (block4)
//         deallocate(block4);
//     if (block5)
//         deallocate(block5);
//     if (block6)
//         deallocate(block6);
//     if (block7)
//         deallocate(block7);
//     printTree(root, 0, "", false);

//     // Cleanup
//     free(root);
//     return 0;
// }
