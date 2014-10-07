#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main()
{


    int file;
    char *filename = "/dev/atsha0";
    static char buf[] = {0x03, 0x07, 0x1B, 0x01, 0x00, 0x00, 0x27, 0x47};
    static char recv_buf[32];


    if ((file = open(filename, O_RDWR)) < 0) {
        /* ERROR HANDLING: you can check errno to see what went wrong */
        perror("Failed to open the i2c bus");
        exit(1);
    }

    if(sizeof(buf) != write(file,buf,sizeof(buf))){
        perror("Write failed\n");
        exit(1);
    }

    if (sizeof(recv_buf) != read(file,buf,sizeof(recv_buf))){
        perror("Read failed\n");
        exit(1);
    }

    close(file);

    return 0;
}
