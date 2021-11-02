#ifndef LIBMICROARCHI_H
#define LIBMICROARCHI_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef CACHE_HIT_THRESHOLD
#define CACHE_HIT_THRESHOLD 100UL
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/**
 * Reads the CPU's timestamp counter and returns its value
 */
__attribute__ ((always_inline)) static inline unsigned long long
libmicro_rdtsc (void)
{
    volatile unsigned long long int x;
    __asm__ volatile("rdtsc\n" : "=a"(x)::"rdx", "rcx");
    return x;
}

/**
 * @brief      Accesses the memory at 'addrs' and return the time
 *             it took the CPU to read it
 */
static inline unsigned long libmicro_probe (char* adrs)
{
    volatile unsigned long timestamp;

    __asm__ __volatile__("    mfence             \n"
                         //"    lfence             \n"
                         "    rdtsc              \n"
                         "    mfence             \n"
                         //"    lfence             \n"

                         /* Save the time counter */
                         "    movl %%eax, %%esi  \n"

                         /* Memory access */
                         "    movl (%1), %%eax   \n"

                         /* Measure time */
                         //"    lfence             \n"
                         "    mfence             \n"
                         "    rdtsc              \n"
                         "    mfence             \n"
                         "    subl %%esi, %%eax  \n"

                         /* Evict the address from the cache */
                         //"   clflush 0(%1)       \n"
                         : "=a"(timestamp)
                         : "c"(adrs)
                         : "%esi", "%edx");
    return timestamp;
}

/**
 * @brief      Flushes the given address from the cache hierarchy.
 */
__attribute__ ((always_inline)) static inline void libmicro_flush (
  const void* addr)
{
    __asm__ volatile("clflush (%0)\n"
                     //"mfence\n"
                     ::"r"(addr)
                     :);
}

/**
 * @brief      Puts the calling thread on CPU 1
 *
 * @return     0 if everything is okay, -1 if error and sets errno
 */
int libmicro_put_on_cpu_zero (void);

/* Prints the physical address located at the virtual address 'virt_addr'
 * The calling process must be root, as physical translations are only
 * available to privileged users on linux kernels 
 * (see https://lwn.net/Articles/642074/) 
 * Returns -1 if the user is unprivileged, and the physical map otherwise */
int libmicro_print_physical (unsigned long virt_addr);

/**
 * mmap() an executable in the caller's address space.
 * The executable is mapped as read-only.
 * Returns a pointer to the mapped executable.
 * If it fails, returns NULL and sets errno
 */
void* libmicro_get_file_handle (const char* exe_name);

/* Puts the program on the specified CPU.
 * Returns -1 if an error occured and sets errno */
int libmicro_put_on_cpu (unsigned int cpu_num);

/* Returns the number of cycles required by the CPU to access a variable stored
 * in cache */
size_t libmicro_get_cache_hit_threshold (void);

/* Returns the number of cycles required by the CPU to access a variable stored
 * in RAM */
size_t libmicro_get_ram_hit_threshold (void);

/* Simple access to a variable to put it in cache */
__attribute__ ((always_inline)) static inline void libmicro_access_memory (
  void* addr)
{
    /* Taken from https://github.com/IAIK/transientfail */
    __asm__ volatile("movq (%0), %%r11\n" : : "c"(addr) : "r11");
}

/* Returns the number of clock cycles elapsed while accessing the memory zone
 * pointed by addr */
__attribute__ ((always_inline)) static inline unsigned long
libmicro_measure_access_time (void* addr)
{
    register unsigned long long time1;
    unsigned long timestamp;

    time1 = libmicro_rdtsc ();
    libmicro_access_memory (addr);
    __asm__ volatile("mfence\n");
    timestamp = libmicro_rdtsc () - time1;

    return timestamp;
}

/* Returns the index of the highest value found in the array
 * Returns -1 if an error occured or if all values are equal */
int libmicro_get_highest_value_index (int* array, size_t size);

/*********************************************************************/
/* Log functions */
/* Every output is made to STDERR */
/*********************************************************************/
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define LOG(str, ...)                                                          \
    fprintf (                                                                  \
      stderr, ANSI_COLOR_BLUE "[*] " ANSI_COLOR_RESET str "\n", __VA_ARGS__);
#define LOG_NOARG(str)                                                         \
    fprintf (stderr, ANSI_COLOR_BLUE "[*] " ANSI_COLOR_RESET str "\n");

#define OK(str, ...)                                                           \
    fprintf (                                                                  \
      stderr, ANSI_COLOR_GREEN "[+] " ANSI_COLOR_RESET str "\n", __VA_ARGS__);
#define OK_NOARG(str)                                                          \
    fprintf (stderr, ANSI_COLOR_GREEN "[*] " ANSI_COLOR_RESET str "\n");

#define ERR(str)                                                               \
    fprintf (stderr, ANSI_COLOR_RED "[-] " ANSI_COLOR_RESET str "\n");

#define ERREXIT(str, ...)                                                      \
    do {                                                                       \
        fprintf (stderr,                                                       \
                 ANSI_COLOR_RED "[-] " ANSI_COLOR_RESET str "\n",              \
                 __VA_ARGS__);                                                 \
        exit (EXIT_FAILURE);                                                   \
    } while (0);
#define ERREXIT_NOARG(str)                                                     \
    do {                                                                       \
        fprintf (stderr, ANSI_COLOR_RED "[-] " ANSI_COLOR_RESET str "\n");     \
        exit (EXIT_FAILURE);                                                   \
    } while (0);

#define PERREXIT(str)                                                          \
    do {                                                                       \
        perror (ANSI_COLOR_RED "[-] " ANSI_COLOR_RESET str);                   \
        exit (EXIT_FAILURE);                                                   \
    } while (0);
/*********************************************************************/

#endif /* LIBMICROARCHI_H */
