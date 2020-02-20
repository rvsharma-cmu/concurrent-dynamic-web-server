/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */

#include "csapp.h"


/* Adds two numbers and writes them out */
void adder(int fd, char* args) {
    char *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1=0, n2=0;

    p = strchr(args, '&');
    *p = '\0';
    strcpy(arg1, args);
    strcpy(arg2, p+1);
    n1 = atoi(arg1);
    n2 = atoi(arg2);

    /* Make the response body */
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>",
            content, n1, n2, n1+n2);
    sprintf(content, "%sThanks for visiting!\r\n", content);

    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %lu\r\n", strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s\n", content);
//    printf("n1 + n2 = %d\n", n1+n2);
    fflush(stdout);

    rio_writen(fd, content, strlen(content));
    return;
}

