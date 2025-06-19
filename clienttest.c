#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <inttypes.h>

#include <stdint.h>

#include <spicenet/sndlp.h>
int main()
{
    int fd;
    sndlp_data_t buf;
    uint8_t buf2[256];
    sndlp_open(&fd, "/dev/ttyS0");
    while(1)
    {
        printf("----\nTOP\n");
        struct pollfd fds[2];

        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;
        fds[1].fd = fd;
        fds[1].events = POLLIN;

        poll(fds, 2, -1);

        int i = 0;
        if(fds[0].revents)
        {
            while(scanf("%2hhx", &(buf2[i])))
            {
                printf("%02hhx", buf2[i]);
                i++;
            }
            printf("\n");
            sndlp_write(fd, 0, buf2, i);
            continue;
        }

        printf("bytes read: %d\n", sndlp_read(fd, &buf));
        printf("apid %d: ", buf.apid);
        printf("data: ");
        for(int i = 0; i < buf.size; i++)
        {
            printf("%02x", ((uint8_t*) buf.data)[i]);
        }
        printf("\n");
    }

    return EXIT_SUCCESS;
}