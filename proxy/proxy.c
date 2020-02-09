/* 
 * Name: Raghav Sharma
 * Andrew ID: rvsharma
 */

/* Implementation details: 
 * This is a concurrent proxy with cache mechanism + LRU eviction policy. 
 * The proxy will run indefinitely till user hits ctrl+c and serve the 
 * web client(browser). The proxy will forward this request to the server 
 * by calling open_clientfd and retrieve the response by rio_readnb function.
 * The proxy will then serve this same response to the web client on the 
 * open file descriptor. 
 * If the response is found in the cache, the proxy will retrieve that 
 * element from the cache and serve it to the web client. If the response 
 * is not found in the cache I am adding it to the cache using the 
 * URI as the key and the web object as the data.
 * 
 * Eviction Policy: I am maintaining a global LRU counter which holds the 
 * age of the cache block. During the eviction, I am checking the age of 
 * blocks and evicting the least recently used block. Multiple evictions 
 * become necessary if the size of the incoming block does not match the 
 * available space in the cache. 
 * 
 * Synchronization: The synchronization of the cache buffer is done using 
 * mutex in the pthread library. I am using reader-preffered policies, 
 * and acquiring write locks during the cache search and increasing the 
 * lru counter. During writing in the cache buffer I am using writer locks 
 * that this ensures only one writer is able to write during the cache buffer 
 * write.
 */

#include "csapp.h"
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

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

/*
 * Internal helper routines
 */
void doit(int fd);
void parse_requesthdrs(rio_t *rp, char *addn_hdrs);
int parse_uri(char *uri, char *host, char *port, char *filename);
void clienterror(int fd, char *cause, char *errnum, char *sms, char *lngmsg);
void post_request(int clientfd, rio_t *rp);
void *thread(void *vargp);
void build_headers(rio_t *rio, char *host, char *filename, 
                              char *clientbuf, char *buf, char *addn_hdrs);
void write_to_cache(char *server_response, size_t write_len, char *uri);
bool read_from_cache(char *uri, int fd);

/* End internal helper routines */ 

/* Global variables */ 

cacheq_t *cache; 
extern int lru;
static int readcnt = 0; // variable for maintaining the reader count 
sem_t mutex, w;

/* End global variables */

/*
  String to use for the User-Agent header.  
  Don't forget to terminate with \r\n
*/
static char *hdr_useragent_value = "User-Agent: Mozilla/5.0"
                                    " (X11; Linux x86_64; rv:3.10.0)"
                                    " Gecko/20181101 Firefox/61.0.1\r\n";
static char *hdr_connection_value = "Connection: close\r\n";
static char *hdr_proxyconn_value = "Proxy-Connection: close\r\n";

static char *hdr_useragent_key = "User-Agent:";
static char *hdr_connection_key = "Connection:";
static char *hdr_proxyconn_key = "Proxy-Connection";

/* 
 * inputs the argument for the proxy server and gets the port on which 
 * to listen. It will then open the port on that port number
 * and wait for a connection from its web client 
 */
int main(int argc, char** argv) 
{
  int listenfd, *connfdp; 
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  Sem_init(&mutex, 0, 1);
  Sem_init(&w, 0, 1);
  pthread_t tid;

  if(argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  
  // initializing the cache
  cache = create_cache();

  // handler for SIGPIPE signals 
  Signal(SIGPIPE, SIG_IGN);

  /* listen as a proxy server and use the connfd for each connection */
  listenfd = Open_listenfd(argv[1]);
  while(1)
  {
    clientlen = sizeof(struct sockaddr_storage);
    connfdp = Malloc(sizeof(int));
    *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Pthread_create(&tid, NULL, thread, connfdp);
    
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, 
                              MAXLINE, port, MAXLINE, 0);
    
    printf("accepted connection from (%s, %s) via thread"
                    "%d\n", hostname, port, (int)tid);

  }
  return 0;
}

/* 
 * thread function for concurrent serving 
 * concurrently spawned proxies. 
 */
void *thread(void *vargp)
{
  int connfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(connfd);
  Close(connfd);
  return NULL;
}

/* 
 * doit() function reads and parses the passed command line argument 
 * from the web client. It will only accept GET requests and print 
 * error if requested for any other type of content 
 * handles query/response per transaction 
 */
void doit(int fd)
{
  char buf[MAXBUF], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], host[MAXLINE], port[MAXLINE], clientbuf[MAXBUF];
  char addn_hdrs[MAXBUF];
  char server_response[MAX_OBJECT_SIZE] = "";
  rio_t rio, clientrio;
  int valid_uri, offset = 0;
  int clientfd;
  size_t n, response_length = 0;
  bool is_in_cache;

  /* Read request line and headers */
  rio_readinitb(&rio, fd);

  if(rio_readlineb(&rio, buf, MAXLINE)<=0)
    return; 

  sscanf(buf, "%s %s %s", method, uri, version);

  if(strncasecmp(method, "GET", MAXLINE))
  {
    clienterror(fd, method, "501", "Not Implemented",
                    "Proxy server does not implement this method");
    return;
  }
  
  // search the uri in the cache, return if true;
  is_in_cache = read_from_cache(uri, fd);

  if(is_in_cache)
    return;

  // continue normal workflow of serving response from server;
  // add it to the cache at the end 
  if(!(valid_uri = parse_uri(uri, host, port, filename)))
  {
    clienterror(fd, uri, "400", "Bad request", 
                      "request could not be understood by the server");
    
    return;
  }
  
  dbg_printf("Host : %s, Port : %s, Filename : %s\n", host, port, filename);

  if((clientfd = open_clientfd(host, port)) < 0)
  {
    fprintf(stderr, "Could not open the connection, all connects failed\n");
    return;
  }

  build_headers(&rio, host, filename, clientbuf, buf, addn_hdrs);

  // write the buffer to the client connection fd 
  Rio_writen(clientfd, clientbuf, strlen(clientbuf));

  // read the response from the server 
  rio_readinitb(&clientrio, clientfd);
  while((n = rio_readnb(&clientrio,clientbuf,MAXLINE)) != 0)
  {
    rio_writen(fd,clientbuf,n);
    response_length += n;
    if( (response_length) <= MAX_OBJECT_SIZE)
      memcpy(server_response + offset, clientbuf, n);
    offset = response_length;
  }

  Close(clientfd);

  // insert the element in the cache if the object size is less
  // than the MAX_OBJECT_SIZE
  write_to_cache(server_response, response_length, uri);

  dbg_printf("Exiting doit()\n");
  return;
}

/* 
 * read the request header into the buffer 
 * using the Rio_readlineb command 
 */
void parse_requesthdrs(rio_t *rp, char *addn_hdrs)
{
  char hdr_buf[MAXBUF];
  rio_readlineb(rp, hdr_buf, MAXLINE);
  dbg_printf("%s", hdr_buf);
  while(strcmp(hdr_buf, "\r\n"))
  {
    if((strcasestr(hdr_buf, hdr_useragent_key)) || 
        (strcasestr(hdr_buf, hdr_connection_key)) ||
        (strcasestr(hdr_buf, hdr_proxyconn_key)))
        {
          rio_readlineb(rp, hdr_buf, MAXLINE);
          continue; 
        }
    else  
      strcat(addn_hdrs, hdr_buf);
    
    rio_readlineb(rp, hdr_buf, MAXLINE);
    dbg_printf("%s", hdr_buf);

  }

  return;
}

/*
 * Parse the URI content into host, port and filename
 * return 0 if invalid request;
 * return 1 if valid request
 */
int parse_uri(char *uri, char *hostname, char *port, char *filename)
{
  int n = 0, h, i;
  char *ptr; 
  char *temp, *p, *f;

  if((ptr = strstr(uri, "http")))
  {
    temp = ptr + 7;
        
    while((temp[n] != '/') && (temp[n] != ':') && (n < strlen(uri)))
    {
      n++;
    }
    
    // if the uri consist of the port number 
    if(temp[n] == ':')
    {
      strncpy(hostname, temp, n);

      hostname[n] = '\0';

      h = 0;
      while((temp[n+h] != '/') && (h < strlen(uri)))
          h++;
            
      p = temp+n+1;
      strncpy(port, p, h-1);
      port[h-1] = '\0';
            
      i = 0;
      while((temp[n+h+i] != '\0') && (i < strlen(uri)-h-n))
      {
        i++;
      }

      f = temp+n+h;
      strncpy(filename, f, i);
      filename[i] = '\0';

      if((!strcmp(filename, "\0")) || (!strcmp(filename, "/")))
      {
          strcpy(filename, "/index.html");
      }
      }

      // if the uri does not consist of the port number 
      else
      {

        strncpy(hostname, temp, n);
        hostname[n] = '\0';
        i = 0;
        while((temp[n+i] != '\0') && (i < strlen(uri)-n))
        {
          i++;
        }

        f = temp+n;
        strncpy(filename, f, i);
        filename[i] = '\0';
        if((!strcmp(filename, "\0")) || (!strcmp(filename, "/")))
        {
            strcpy(filename, "/index.html");
        }            
        strcpy(port, "80");
      }
    return 1;
  }
  else
  {
    dbg_printf("does not contains http\n");
    return 0;
  }
}

/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>proxy Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Proxy server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

/* build_headers() builds the headers for the proxy to send 
 * to the server, it takes arguments such as hostname, path, buffer, 
 * and the rio internal buffer and parses them into an HTTP response
 * and returns it to proxy. 
 */
void build_headers(rio_t *rio, char *host, char *filename, 
              char *clientbuf, char *buf, char *addn_hdrs)
{
  // build the request headers 
  sprintf(clientbuf, "GET %s HTTP/1.0\r\n", filename);
  sprintf(buf, "Host: %s\r\n", host);
  strcat(clientbuf, buf);
  strcat(clientbuf,hdr_useragent_value);
  strcat(clientbuf,hdr_connection_value);
  strcat(clientbuf,hdr_proxyconn_value);

  // parse additional request headers   
  parse_requesthdrs(rio, addn_hdrs);
  strcat(clientbuf, addn_hdrs);
  strcat(clientbuf, "\r\n");
  return;
}

/* 
 * write_to_cache() will take arguments like server response, 
 * write length and uri and write the key object pair to the cache buffer.
 * It searches the cache whether the object already exists in the cache. 
 * It will add to thhe cache if the object is not found in the 
 * cache. Eviction is done on basis of lru count and evicted till the 
 * response length is available in the cache. 
 */
void write_to_cache(char *server_response, size_t write_len, char *uri)
{
  size_t evict_size = 0;
  sem_wait(&w);
  if( write_len <= MAX_OBJECT_SIZE )
  {
    
    dbg_printf("response size smaller than max_cache_object\n");
    if(is_cache_full(cache, write_len))
    {
      
      dbg_printf("Cache full, evicting\n");
      while(evict_size < write_len && !search_cache(cache, uri))
      {
        // Take writer lock to evict from cache
        evict_size += eviction(cache);
      }
      
    }
    
    dbg_printf("adding to cache\n");
    // Take writer lock to add to cache
    if(!add_to_cache(cache, uri, server_response, write_len) 
          && !search_cache(cache, uri))
      printf("Failed to add to cache\n"); 
  }
  sem_post(&w);
}

/*
 * searches the cache for the string uri and 
 * serves it to the open file descriptor for the client 
 * 
 * It will return true if the element was found in the cache and served 
 * Return false if the element was not found in the cache
 */
bool read_from_cache(char *uri, int fd)
{
  cacheline_t *response_object;
  // Initailize the reader lock
  sem_wait(&mutex);
  readcnt++;
  if(readcnt == 1)
    sem_wait(&w);
  sem_post(&mutex);
  
  response_object = search_cache(cache, uri);
  
  if(response_object)
  {
    dbg_printf("Cache Hit, replying to browser query\n"); fflush(stdout);
  
    Rio_writen(fd, response_object->web_object, response_object->size);
    response_object->age = lru++;
  
    sem_wait(&mutex);
    readcnt--;
    if(readcnt == 0)
      sem_post(&w);
  // remove reader lock
    sem_post(&mutex);

    dbg_printf("Cache_hit!\n");
    return true; 
  }

  sem_wait(&mutex);
  readcnt--;
  if(readcnt == 0)
    sem_post(&w);
  // remove reader lock
  sem_post(&mutex);

  // if found a response in cache, serve to client now
  // Take a writer lock to update the lru
  
  dbg_printf("Cache Miss\n");
  return false;
}