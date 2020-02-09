/* Cache Header file 
 * Name: Ragahv Sharma
 * Andrew.ID = rvsharma
 */

/* 
 * The cache is implemented as a singly linked list; 
 * fully associative cache, with LRU policy for eviction
 * The eviction can be multiple eviction of blocks if the block 
 * being evicted does not satisfy the storage requirement of the 
 * entering block. 
 */

#include "csapp.h"
#include <stdio.h>

/* 
 *  Cache structure - Fully associative 
 *  _________      ___________________     __________________       _________
 * |         |    |                   |   |                  |     |         |
 * |cachehead|--->|urlkey - webobject |-->|urlkey - webobject|<----|cachetail|
 * |_________|    |___________________|   |__________________|     |_________|
 * 
 */

/* Max cache and object sizes */
/* Max object size = 100 KiB
 * Max cache size = 1 MiB
 */
#define MAX_CACHE_SIZE (1024*1024)
#define MAX_OBJECT_SIZE (100*1024)

/* 
 * Structure for cache 
 * Key - URI of the web object 
 * Web-Object - Response sent by the server which the proxy will cache
 * size - size of the web object 
 * age - age of the cache element 
 * next - pointer to the next block of the cache 
 */
typedef struct cacheline cacheline_t;
typedef struct cacheq cacheq_t;

typedef struct cacheline{
    char *key;
    char *web_object;
    size_t size;
    int age;
    cacheline_t *next;
}cacheline_t;

typedef struct cacheq
{
    size_t cache_size;
    cacheline_t *head;
    cacheline_t *tail;
}cacheq_t;

/* helper function declarations */

/* 
 * creates a cache at the begining of the connection and returns a pointer to 
 * the cache. When the connection stops(proxy exit); we will call free cache
 * function to free the cache
 */
cacheq_t *create_cache(void);

/* 
 * This function adds a new webobject to the cache 
 * at the tail of the queue.
 * It checks the size of the response, if it is more than 100 KiB 
 * it will discard the response. Else it will store the webobject in the cache
 */
bool add_to_cache(cacheq_t *cache, char *key, char *wobjct, size_t rspns_ln);

/* 
 * remove_from_cacheline() removes a web object from cache 
 * when provided the pointer to evict the block.
 * It returns whether the function was successfully able to 
 * remove the object from the cache or not. 
 */
bool remove_from_cache(cacheq_t *cache, cacheline_t *line);

/* 
 * eviction logic for cache: 
 * In while loop, checking the age for each of the line 
 * elements and keeping a pointer to the oldest element found in 
 * the queue. on reaching the tail pointer, return the oldest element 
 * in the queue to the caller which then evicts that element from 
 * the queue by calling remove_from_cache()
 */
size_t eviction(cacheq_t *cache);

/* 
 * search_cache() searches for a key in the entire cache 
 * and returns the pointer to the block containing the key 
 * and its web object to the caller function
 */
cacheline_t *search_cache(cacheq_t *cache, char *key);

/* 
 * is_cache_full() returns true if the cache is full and false otherwise 
 * arguements are pointer to a cache, and the length of the new block 
 * being added. 
 */
bool is_cache_full(cacheq_t* cache, size_t response_length);

/*
 * print_cache() goes through each element and prints out the 
 * value of the cache lines; key, web object, size, and age. 
 */
void print_cache(cacheq_t *cache);

/* end helper declarations */