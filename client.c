#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    FILE *data = fopen("/home/eason/gnuplot/data.txt", "w+");
    // char write_buf[] = "testing writing";
    char buf[512] = {0};
    int offset = 500; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }
    if (!data) {
        printf("data.txt does not open");
        exit(1);
    }
    struct timespec t1, t2;
    /* for (int i = 0; i <= offset; i++) {
         sz = write(fd, write_buf, strlen(write_buf));
         printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
     }*/
    /* for (int i = 0; i <= offset; i++) {
         lseek(fd, i, SEEK_SET);
        // clock_gettime(CLOCK_MONOTONIC, &t1);
         sz = read(fd, buf, 256);
       //  buf[sz] = 0;
        // clock_gettime(CLOCK_MONOTONIC, &t2);
        // long long ut = (long long)(t2.tv_sec * 1e9 + t2.tv_nsec) - (t1.tv_sec
     * 1e9 + t1.tv_nsec); // ns
         //fprintf(data, "%d %lld %lld %lld\n", i, sz, ut, ut - sz);
         //printf("%d %lld %lld %lld  \n", i, sz, ut, ut - sz);
         //printf("fib(%d)=%d\r\n",i,sz);
         //printf("fib(%d)=%s\r\n",i,buf);
         printf("Reading from " FIB_DEV
                " at offset %d, returned the sequence "
                "%s.\n",
                i, buf);
     }*/

    for (int i = 0; i <= offset; i++) {
        // memset(buf,0,256);memset(buf,0,256);
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        unsigned long long sz = read(fd, buf, 256);
        // buf[sz] = '\0';
        clock_gettime(CLOCK_MONOTONIC, &t2);
        unsigned long long ut =
            (unsigned long long) (t2.tv_sec * 1e9 + t2.tv_nsec) -
            (t1.tv_sec * 1e9 + t1.tv_nsec);  // ns
        fprintf(data, "%d %llu %llu %llu\n", i, sz, ut, ut - sz);
        // printf("%d %lld %lld %lld  \n", i, sz, ut, ut - sz);
        // printf("fib(%d)=%d\r\n",i,sz);
        // printf("fib(%d)=%s\r\n",i,buf);
        /* printf("Reading from " FIB_DEV
                " at offset %d, returned the sequence "
                "%s.\n",
                i, buf);*/
    }

    close(fd);
    return 0;
}
