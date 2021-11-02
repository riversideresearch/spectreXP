#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "libmicroarchi.h"

#define ACCES_TIME_NB_TRIES 10000

/* Source:
 * http://fivelinesofcode.blogspot.com/2014/03/how-to-translate-virtual-to-physical.html
 */
/* ************************************ */
#define PAGEMAP_ENTRY 8
#define GET_BIT(X, Y) (X & ((uint64_t) 1 << Y)) >> Y
#define GET_PFN(X) X & 0x7FFFFFFFFFFFFF

const int __endian_bit = 1;
#define is_bigendian() ((*(char*) &__endian_bit) == 0)

int i, c, pid, status;
unsigned long virt_addr;
uint64_t read_val, file_offset;
char path_buf[0x100] = {};
FILE* f;
char* end;

int libmicro_print_physical (unsigned long virt_addr)
{
    /* This function is only available to root users */
    if (geteuid() != 0) {
        ERR ("libmicro_print_physical: please call me as root !");
        return -1;
    }

    LOG ("Big endian? %d", is_bigendian ());
    f = fopen ("/proc/self/pagemap", "rb");
    if (!f) {
        LOG ("Error! Cannot open %s\n", "/proc/self/pagemap");
        return -1;
    }

    // Shifting by virt-addr-offset number of bytes
    // and multiplying by the size of an address (the size of an entry in
    // pagemap file)
    file_offset = virt_addr / getpagesize () * PAGEMAP_ENTRY;
    LOG ("Vaddr: 0x%lx, Page_size: %d, Entry_size: %d",
         virt_addr,
         getpagesize (),
         PAGEMAP_ENTRY);
    LOG ("Reading %s at 0x%llx",
         "/proc/self/pagemap",
         (unsigned long long) file_offset);
    status = fseek (f, file_offset, SEEK_SET);
    if (status) {
        perror ("Failed to do fseek!");
        return -1;
    }
    errno = 0;
    read_val = 0;
    unsigned char c_buf[PAGEMAP_ENTRY];
    for (i = 0; i < PAGEMAP_ENTRY; i++) {
        c = getc (f);
        if (c == EOF) {
            //LOG ("%s", "\nReached end of the file\n");
            return 0;
        }
        if (is_bigendian ())
            c_buf[i] = c;
        else
            c_buf[PAGEMAP_ENTRY - i - 1] = c;
    }
    for (i = 0; i < PAGEMAP_ENTRY; i++) {
        // LOG("%d ",c_buf[i]);
        read_val = (read_val << 8) + c_buf[i];
    }
    LOG ("Result: 0x%llx", (unsigned long long) read_val);
    // if(GET_BIT(read_val, 63))
    if (GET_BIT (read_val, 63)) {
        LOG ("PFN: 0x%llx", (unsigned long long) GET_PFN (read_val));
    }
    else {
        LOG ("%s", "Page not present\n");
    }
    if (GET_BIT (read_val, 62))
        LOG ("%s", "Page swapped\n");
    fclose (f);
    return 0;
}
/* ******************************** */

// extern __inline__ unsigned long long libmicro_rdtsc(void);

__inline__ unsigned long libmicro_probe (char* adrs);

void* libmicro_get_file_handle (const char* exe_name)
{
    int fd = open (exe_name, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }

    struct stat st;
    fstat (fd, &st);

    void* mapped_addr =
      mmap (NULL, st.st_size, PROT_READ, MAP_FILE | MAP_SHARED, fd, 0);
    if (mapped_addr == MAP_FAILED) {
        close (fd);
        return NULL;
    }
    close (fd);

    return mapped_addr;
}

int libmicro_put_on_cpu_zero (void)
{
    cpu_set_t my_set;
    CPU_ZERO (&my_set);
    CPU_SET (0, &my_set);
    return sched_setaffinity (0, sizeof (cpu_set_t), &my_set);
}

int libmicro_put_on_cpu (unsigned int cpu_num)
{
    cpu_set_t my_set;
    CPU_ZERO (&my_set);
    CPU_SET (cpu_num, &my_set);
    return sched_setaffinity (0, sizeof (cpu_set_t), &my_set);
}

size_t libmicro_get_ram_hit_threshold (void)
{
    static char probe[256 * PAGE_SIZE];
    unsigned long long time1;
    unsigned long long time2;
    char dummy;
    unsigned long long ram_median = 0;

    srandom (time (NULL) - getpid ());

    /* Get the average access time from RAM */
    for (int i = 0; i < 256; i++) {
        libmicro_flush (&probe[i]);
    }

    for (int i = 0; i < ACCES_TIME_NB_TRIES; i++) {
        int index = random () % 256;
        time1 = libmicro_rdtsc ();
        dummy = probe[index];
        __asm__("mfence\nlfence\n");
        time2 = libmicro_rdtsc () - time1;
        libmicro_flush (&probe[index]);
        ram_median += time2;
    }
    ram_median /= ACCES_TIME_NB_TRIES;
    // LOG ("ram time: %llu", ram_median);
    return ram_median;
}

size_t libmicro_get_cache_hit_threshold (void)
{
    static char probe[256 * PAGE_SIZE];
    unsigned long long time1;
    unsigned long long time2;
    char dummy;

    size_t probed_index;
    unsigned long long cache_median = 0;
    for (int i = 0; i < ACCES_TIME_NB_TRIES; i++) {
        /* Put in cache */
        for (int j = 0; j < 256; j++) {
            dummy = probe[j];
        }
        /* Measure access */
        for (int j = 0; j < 256; j++) {
            /* Disables stride prediction */
            probed_index = ((j * 167) + 13) & 255;
            time1 = libmicro_rdtsc ();
            dummy = probe[probed_index];
            __asm__("mfence\nlfence\n");
            time2 = libmicro_rdtsc () - time1;
            cache_median += time2;
        }
        cache_median /= 256;
    }
    // LOG ("cache time: %llu", cache_median);

    return cache_median;
}

int libmicro_get_highest_value_index (int* array, size_t size)
{
    if (!array || size == 0) {
        return -1;
    }

    int max = array[0];
    int index = -1;
    int all_same = 1;
    for (int k = 1; k < size; k++) {
        if (array[k] > max) {
            max = array[k];
            index = k;
        }
        all_same = all_same && (array[k] == array[k - 1]);
    }
    if (all_same) {
        return -1;
    }
    /* Limit case where the highest value is located in the first index */
    if (index == -1 && array[0] >= array[size - 1]) {
        index = 0;
    }

    return index;
}