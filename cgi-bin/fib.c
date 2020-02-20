//
// Created by Raghav Sharma on 19/02/20.
//

#include "csapp.h"

int fibonacci(int n) {
    if (n <= 2) {
        return 1;
    } else {
        return fibonacci(n-1) + fibonacci(n-2);
    }
}

/* fibonacci generator*/
void fib(int fd, char* args) {

    char arg1[MAXLINE], content[MAXLINE];
    int n1=0;

    strcpy(arg1, args);

    n1 = atoi(arg1);

    /* Make the response body */
    sprintf(content, "Welcome to add.com: ");
    sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    sprintf(content, "%sThe answer is: fib(%d) = %d\r\n<p>",
            content, n1, fibonacci(n1));
    sprintf(content, "%sThanks for visiting!\r\n", content);

    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %lu\r\n", strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s\n", content);
    fflush(stdout);

    rio_writen(fd, content, strlen(content));
    return;
}
