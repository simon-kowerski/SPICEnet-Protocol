#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <stdint.h>
#include <stdio.h>

#include <spicenet/spp.h>
#include <spicenet/sndlp.h>
#include <spicenet/config.h>


int main()
{
    int fd;
    uint8_t buf[] = {0x12, 0x34, 0x56};

    if(sndlp_open(&fd, "/dev/ttyUSB0")) 
    {
        perror("failed to open");
        return EXIT_FAILURE;
    }
    
    printf("Serial Connection Open\n");
    int x =sndlp_connect(fd); 
    if(x) 
    {
        printf("Didnt connect: %d\n", x);
        return EXIT_FAILURE;
    }

    printf("Verified Connection");
    
    printf("Sent %d bytes\n", sndlp_write(fd, 0x000, buf, sizeof(buf)));
    
    return 0;
}