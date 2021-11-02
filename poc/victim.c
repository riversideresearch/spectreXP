#define _XOPEN_SOURCE 500
#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define PAGE_SIZE 512

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#define MAX 80
#define PORT 4444
#define SA struct sockaddr

#include "libmicroarchi.h"

#define SECRET "Hello there ! General Kenobi !\0"
 
//Initialize probe_array[] to non-zero value such that it can be used as a side channel

const char probe_array[] = { [0 ... 256 * PAGE_SIZE] = 1 };

__attribute__ ((section (".rodata.array1_size"))) size_t array1_size = 16;

static char secret_value[64];

static char array1[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};

int victim_function (size_t x)
{
    int y = 0;

    if (x < array1_size) {
        y = probe_array[array1[x] * PAGE_SIZE];
    }
    return y;
}

int main (void)
{

//beginning of socket creation

    int sockfd, connfd, len;
    struct sockaddr_in servaddr, cli;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully bound..\n");

    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    else
        printf("Server listening..\n");
    len = sizeof(cli);

    connfd = accept(sockfd, (SA*)&cli, &len);
    if (connfd < 0) {
        printf("server acccept failed...\n");
        exit(0);
    }
    else
        printf("server acccept the client...\n");

//end of socket creation

    for (size_t i = 0; i < array1_size; i++) {
        array1[i] = i;
    }

    //Create private copy of secret_value

    strncpy (secret_value, SECRET, strlen (SECRET) + 1);
    
/*
Unused log functions for debugging/verbosity
    LOG ("VICTIM - victim_function is at %p", victim_function);
    LOG ("VICTIM - array1_size is at %p", &array1_size);
    LOG ("VICTIM - probe_array is at %p", probe_array);
    LOG ("VICTIM - secret_value is at %p", secret_value);
*/
  
    //print offset between shared array1 and secret_value, in a real attack this would have to be guessed/bruteforced
    
    LOG ("VICTIM - secret_value offset is %ld", (size_t) (secret_value - array1));

    char buffer[64];
    size_t x;
    int i;
    int nb_read;
    
    //Pull secret_value to cache so attack will work

    __asm__ volatile("lea %0, %%rax\n"
                     "movb (%%rax), %%al\n" ::"m"(secret_value)
                     :);
                     
    //Loop waits for user/attacker input to pass to victim_function    
    
    while (1) {
        memset (buffer, '\0', 64 * sizeof (char));

        i = 0;
        while (1) {
            nb_read = read(connfd, buffer, sizeof(buffer));
            if (nb_read == -1) {
                PERREXIT ("read");
            }
            if (nb_read == 0) {
                return EXIT_SUCCESS;
            }
            if (buffer[i] == '\n') {
                break;
            }
            i++;
            if (i >= 64) {
                break;
            }
        }

        x = (size_t) strtoul (buffer, NULL, 10);
        x = victim_function (x);
    }
    //close socket
    close(sockfd);

    return EXIT_SUCCESS;
}
