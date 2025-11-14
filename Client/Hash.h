#ifndef __Hash_h__
#define __Hash_h__

typedef void (*functionPointer)(char*,int);

typedef struct HashTable {
    int capacity;
    int size;
    functionPointer* functionArr;
} HashTable;

//hash function
int hash(char* key, int capacity);
HashTable* createHashTable(int capacity);
void destroyHashTable(HashTable* table);
void insert(HashTable* table, functionPointer function, char* key);
functionPointer lookup(HashTable* table, char* key);

#endif