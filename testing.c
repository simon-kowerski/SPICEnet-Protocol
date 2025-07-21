#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include <spicenet/config.h>
#include <spicenet/snp.h>

#if DEV_ID == 0
#define portname "/dev/ttyUSB0"
#endif

#ifndef portname
#define portname "/dev/ttyS0"
#endif

int main(int argv, char **argc)
{
    printf("[Starting SPICEnet]\n");
    int fd;

    int ret = snp_open(&fd, portname);
    if(ret) 
    {
        perror("[Failed to open serial connection]");
        return EXIT_FAILURE;
    }
    
    printf("[Opened Serial Connection] %s\n", portname);

    if((ret = snp_listen(fd)))
    {
        printf("[Serial Connection Invalid] %x\n", ret);
        return EXIT_FAILURE;
    }

    printf("[Serial Connection Confirmed]\n");

    snp_app_t *conn;
    int apid = 0x03;
    snp_connect(apid, &conn);

    printf("[Connected to apid] %d\n", apid);


    if(DEV_ID == 1) 
    {
       printf("[Wrote to port] %d\n", snp_write(conn, "Hello World!", 13));
    }

    else
    {
        char buf[13];
        printf("[Received] %d\n", snp_read(conn, buf, 13));
        printf("%s\n", buf);
    }
}