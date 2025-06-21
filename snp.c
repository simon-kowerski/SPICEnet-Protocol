#include <spicenet/sndlp.h>
#include <spicenet/spp.h>
#include <spicenet/sntp.h>

// calls sndlp_open and gets/stores the fd
// future potential to support various drivers for different physical layers
int snp_open(int *fd, char *portname)
{
    return sndlp_open(fd, portname);
}

// calls sndlp_connect, then does the following
    // only to happen once per SYSTEM
    // will initalize spp and sntp
    // will start listening for incoming data on the port
    // will start listening for send request from processes 
    // any data coming in with no matching apid will get ignored 
int snp_listen(int *fd)
{
    sndlp_connect(fd);
    spp_init();
    sntp_init();
}

// returns a struct of some kind for the individual app to use
int snp_connect(int apid);

// sends requests to the sntp protocols
int snp_read();
int snp_write();
