#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <spicenet/config.h>
#include <spicenet/snp.h>

int main(int argv, char **argc)
{
    printf("[Starting SPICEnet]\n");
    int fd;
    char *portname = "/dev/ttyUSB0";

    int ret = snp_open(&fd, portname);
    if(ret) 
    {
        perror("[Failed to open serial connection]");
        return EXIT_FAILURE;
    }
    
    printf("[Opened Serial Connection] %s\n", portname);

    if((ret = snp_listen(fd)))
    {
        printf("[Serial Connection Invalid] %d\n", ret);
        return EXIT_FAILURE;
    }

    printf("[Serial Connection Confirmed]\n");

    snp_app_t *conn;
    int apid = 0x03;
    snp_connect(apid, &conn);

    printf("[Connected to apid] %d\n", apid);
}