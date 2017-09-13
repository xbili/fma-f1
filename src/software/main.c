#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#define IN_BUF_SIZE 16
#define OUT_BUF_SIZE 32

// Memory address offsets
#define INPUT_1_OFFSET 0x00010000
#define INPUT_2_OFFSET 0x00001000
#define OUTPUT_OFFSET 0x00002000

int main(int argc, char *argv[])
{
    char *queue;
    char *p;
    int fd;
    int ret;

    short* in1;
    short* in2;
    int* out;

    queue = argv[1];

    in1 = malloc(sizeof(short));
    in2 = malloc(sizeof(short));
    out = malloc(sizeof(int));

    *in1 = (short) strtol(argv[2], &p, 10);
    *in2 = (short) strtol(argv[3], &p, 10);

    printf("On queue: %s\n", queue);
    printf("Evaluating %d * %d with hardware multiplier.\n", *in1, *in2);

    // Open an EDMA queue for read/write
    fd = open(queue, O_RDWR);
    if (fd == -1) {
        perror("open failed with errno");
    }

    // Writes inputs to their respective offsets
    ret = pwrite(fd, in1, IN_BUF_SIZE, INPUT_1_OFFSET);
    if (ret < 0) {
        perror("write failed with errno");
    }

    ret = pwrite(fd, in2, IN_BUF_SIZE, INPUT_2_OFFSET);
    if (ret < 0) {
        perror("write failed with errno");
    }

    // Ensure the write made it to the CL
    fsync(fd);

    // Read output from the address offset
    ret = pread(fd, out, OUT_BUF_SIZE, OUTPUT_OFFSET);
    if (ret < 0) {
        perror("read failed with errno");
    }

    if (close(fd) < 0) {
        perror("close failed with errno");
    }

    printf("Expected result is %d\n", (*in1) * (*in2));
    printf("Data read is %d\n", *out);

    free(in1);
    free(in2);
    free(out);

    return 0;
}
