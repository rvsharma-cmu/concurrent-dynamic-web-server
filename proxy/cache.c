/* Cache file 
 * Name: Raghav Sharma
 * Andrew.ID = rvsharma
 */
#include "csapp.h"
#include "cache.h"
#include <stdbool.h>

//#define DEBUG // uncomment this line to enable debugging

#ifdef DEBUG

/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

int lru = 1;
/* cache helper function definitions */

/* 
 * creates a cache at the begining of the connection and returns a pointer to 
 * the cache. When the connection stops(proxy exit); we will call free cache
 * function to free the cache
 */
cacheq_t *create_cache(void)
{
    cacheq_t *cache = malloc(sizeof(cacheq_t));
    if(cache == NULL)
    {
        unix_error("malloc error\n");
        return false; 
    }
    cache->head = NULL;
    cache->tail = NULL;
    cache->cache_size = 0;
    return cache;
}

/* 
 * This function adds a new webobject to the cache 
 * at the tail of the queue.
 * It checks the size of the response, if it is more than 100 KiB 
 * it will discard the response. Else it will store the webobject in the cache
 */
bool add_to_cache(cacheq_t *cache, char *key, char *web_object, size_t response_length)
{
    // if the cache is empty; return false 
    size_t object_len, key_len; 
    if(cache == NULL)
        return false; 
    cacheline_t *newline;
    newline = (cacheline_t *)malloc(sizeof(cacheline_t));
    if(newline == NULL)
        return false; 

    object_len = response_length;

    // if object size is bigger than the allowed object size
    if(!(object_len < MAX_OBJECT_SIZE))
    {   
        free(newline);
        return false; 
    }

    newline->web_object = malloc(response_length);

    key_len = strlen(key) + 1;
    
    newline->key = (char *)malloc(key_len);

    if((newline->web_object == NULL)||(newline->key == NULL))
    {
        free(newline);
        unix_error("malloc error\n");
        return false;
    }

    // inserting the first element in cache
    if(cache->head == NULL && cache->tail == NULL)
    {
        memcpy(newline->web_object, web_object, object_len);
        memcpy(newline->key, key, key_len);
        newline->age = lru++;
        newline->size = object_len;
        newline->next = NULL;

        cache->head = newline;
        cache->tail = newline; 
        cache->cache_size += object_len;
        return true;
    }

    // else insert at tail 
    memcpy(newline->web_object, web_object, object_len);
    memcpy(newline->key, key, key_len);
    newline->age = lru++;
    newline->next = NULL;
    newline->size = object_len;
    cache->tail->next = newline;
    cache->tail = newline;
    cache->cache_size += object_len;
    return true;
}

/* 
 * remove_from_cacheline() removes a web object from cache 
 * when provided the pointer to evict the block.
 * It returns whether the function was successfully able to 
 * remove the object from the cache or not. 
 */
bool remove_from_cache(cacheq_t *cache, cacheline_t *line)
{
    if(cache == NULL || ((cache->head == NULL) && (cache->tail == NULL)))
        return false;

    cacheline_t *temp, *prev; 

    temp = line;
    prev = cache->head; 

    // if head is to be removed
    if(cache->head == temp)
    {
        cache->head = temp->next; 
        cache->cache_size -= temp->size;
        free(temp->web_object);
        free(temp->key);
        free(temp);
        return true;
    }

    // if it is tail to be removed
    if(cache->tail == temp)
    {
        cache->tail->next = NULL;
        cache->cache_size -= temp->size;
        free(temp->web_object);
        free(temp->key);
        free(temp);
        return true;
    }

    // if the element is in the middle
    while((prev != NULL) && (prev->next != temp))
        prev = prev->next; 
        
    prev->next = temp->next;
    temp->next = NULL;
    cache->cache_size -= temp->size;
    free(temp->web_object);
    free(temp->key);
    free(temp);
    return true;
}

/* 
 * eviction logic for cache: 
 * In while loop, checking the age for each of the line 
 * elements and keeping a pointer to the oldest element found in 
 * the queue. on reaching the tail pointer, return the oldest element 
 * in the queue to the caller which then evicts that element from 
 * the queue by calling remove_from_cache()
 */
size_t eviction(cacheq_t *cache)
{
    cacheline_t *traverse, *target; 
    size_t evict_size = 0;
    bool status = false; 

    // return 0 if nothing to evict
    if(cache == NULL)
        return evict_size; 

    traverse = cache->head; 
    target = cache->head; 
    while(traverse != NULL)
    {
        if(traverse->age < target->age)
            target = traverse; 

        traverse = traverse->next; 
    }
    
    if(target)
    {
        evict_size = target->size;
        status = remove_from_cache(cache, target); 
    }

    if(status)
        return (evict_size);
    else
        return evict_size;

}

/* 
 * search_cache() searches for a key in the entire cache 
 * and returns the pointer to the block containing the key 
 * and its web object to the caller function
 */
cacheline_t *search_cache(cacheq_t *cache, char *key)
{   
    cacheline_t *target; 

    // return NULL if there is nothing to search
    if(cache == NULL || ((cache->head == NULL) && (cache->tail == NULL)))
        return NULL;
    target = cache->head; 
    fflush(stdout);
    while((target != NULL) && (strcmp(key, target->key)))
        target = target->next; 

    return target; 
}

/* 
 * is_cache_full() returns true if the cache is full and false otherwise 
 * arguements are pointer to a cache, and the length of the new block 
 * being added. 
 */
bool is_cache_full(cacheq_t *cache, size_t response_length)
{

    if(cache->cache_size + response_length > MAX_CACHE_SIZE)
        return true; 
    else 
        return false;
        
}

/*
 * print_cache() goes through each element and prints out the 
 * value of the cache lines; key, web object, size, and age. 
 */
void print_cache(cacheq_t *cache)
{
    cacheline_t *temp; 

    temp = cache->head; 

    while(temp != NULL)
    {
        dbg_printf("key = %s\n", temp->key);
        dbg_printf("web_object = %s\n", temp->web_object);
        dbg_printf("size = %lu\n", temp->size);
        dbg_printf("age = %d\n", temp->age);
        temp = temp->next;
    }
}