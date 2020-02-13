//
// Created by Raghav Sharma on 12/02/20.
//

/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"
#include <dlfcn.h>



void* handle_request(void* arg);
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *function_name, char *cgiargs);
void serve_static(int fd, char *function_name, int filesize);
void get_filetype(char *function_name, char *filetype);
void serve_dynamic(int fd, char *function_name, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg);
void cleanup(int fd);



int main(int argc, char **argv)
{
    int listenfd, port, clientlen;
    int* connfd_ptr;
    struct sockaddr_in clientaddr;
    pthread_t tid;


    /* Check command line args */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd_ptr = (int*) Malloc(sizeof(int));
        *connfd_ptr = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        printf("Connection made.\n");
        Pthread_create(&tid, NULL, handle_request, (void*) connfd_ptr);
    }
}
/* $end tinymain */

/*Small wrapper to parse/forward the request that fits Pthread specs*/
void* handle_request(void* connfd_ptr) {
    int fd = *((int*) connfd_ptr);
    Free(connfd_ptr);
    doit(fd);
    return NULL;
}

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char function_name[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* This thread doesn't have to be joined */
    Pthread_detach(Pthread_self());

    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);

    printf("Scanned input. %s\n", buf);
    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implemented",
                    "Tiny does not implement this method");
        cleanup(fd);
        return;
    }
    read_requesthdrs(&rio);

    /* Parse URI from GET request */
    is_static = parse_uri(uri, function_name, cgiargs);
    if (stat(function_name, &sbuf) < 0 && is_static) {
        clienterror(fd, function_name, "404", "Not found",
                    "Tiny couldn't find this file");
        cleanup(fd);
        return;
    }
    if (is_static) { /* Serve static content */
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, function_name, "403", "Forbidden",
                        "Tiny couldn't read the file");
            cleanup(fd);
            return;
        }
        serve_static(fd, function_name, sbuf.st_size);
    }
    else { /* Serve dynamic content */
#ifdef OLD
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, function_name, "403", "Forbidden",
                    "Tiny couldn't run the CGI program");
            cleanup(fd);
            return;
        }
#endif
        serve_dynamic(fd, function_name, cgiargs);
    }
    cleanup(fd);
}
/* $end doit */

/*
 * read_requesthdrs - read and parse HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];

    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
    while(strcmp(buf, "\r\n")) {
        Rio_readlineb(rp, buf, MAXLINE);
        printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into function_name and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
int parse_uri(char *uri, char *function_name, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */
        strcpy(cgiargs, "");
        strcpy(function_name, ".");
        strcat(function_name, uri);
        if (uri[strlen(uri)-1] == '/')
            strcat(function_name, "home.html");
        return 1;
    }
    else {  /* Dynamic content */
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr+1);
            *ptr = '\0';
        }
        else
            strcpy(cgiargs, "");
        //strcpy(function_name, ".");

        // Skip the "/cgi-bin/" portion and jump to the program
        strcat(function_name, uri+9);
        return 0;
    }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client
 */
/* $begin serve_static */
void serve_static(int fd, char *function_name, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    /* Send response headers to client */
    get_filetype(function_name, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));

    /* Send response body to client */
    srcfd = Open(function_name, O_RDONLY, 0);
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *function_name, char *filetype)
{
    if (strstr(function_name, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(function_name, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(function_name, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else
        strcpy(filetype, "text/plain");
}
/* $end serve_static */

size_t getfilesize(char* filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *function_name, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    void *handle;
    void (*function)(int, char*);
    char *error;
    size_t size;

    sprintf(buf, "Hello\n");
    write(fd, buf, strlen(buf));

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    write(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    write(fd, buf, strlen(buf));

    //#ifdef OLD

    sprintf(buf, "./lib/%s.so", function_name);
    size = getfilesize(buf);
    if ((handle = dlopen(buf, RTLD_LAZY)) == NULL) {
        sprintf(buf, "%s\n", dlerror());
        write(fd, buf, strlen(buf));
        return;
    }

    printf("Opened file and got handle to function\n");
    /* Get the function (from dlysm) and add to cache */

    function = dlsym(handle, function_name);

    if ((error = dlerror()) != NULL) {
        printf("Invalid function error: %s %s\n", function_name, error);
        sprintf(buf, "Invalid function %s\n", function_name);
        Rio_writen(fd, buf, strlen(buf));
        return;
    }

    /* Execute the function */
    if (function != NULL) {
        function(fd, cgiargs);
    }

    //#endif
}
/* $end serve_dynamic */

/* cleanup -- Frees up descriptors in use and ends thread */
void cleanup(int fd) {
    Close(fd);
    Pthread_exit(NULL);
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
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

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



