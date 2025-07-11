//TODO HEADER

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>

#include <spicenet/snp.h>
#include <spicenet/sntp.h>
#include <spicenet/sndlp.h>
#include <spicenet/spp.h>

#include <poll.h>

// called by main to start service, universal stuff
    // snp_open - call sndlp_open to get the fd
    // snp_listen - sndlp_connect to ensure someone is on the other end, then start all services

// called by apps to connect to and communicate with their respective ports
    // snp_connect - gives a struct for apps to use to read and write
    // read/write etc



// calls sndlp_open and gets/stores the fd
// future potential to support various drivers for different physical layers
int snp_open(int *fd, char *portname)
{
    return sndlp_open(fd, portname);
}

// calls sndlp_connect, then does the following
    // only to happen once per SYSTEM
    // will initalize spp and sntp
    // any data coming in with no matching apid will get ignored 
    // once returned, you know that the port is open an someone is listening
//TODO return value among others
int snp_listen(int fd)
{
    int ret;
    if((ret = sndlp_connect(fd))) return ret;
    spp_init();
    //TODO if((ret = sntp_start(fd))) return ret;
    sntp_start(fd);
    return 0;
}

// returns a struct for the individual app to use
int snp_connect(int apid, snp_app_t **app)
{
    return sntp_connect(apid, app);
}

// 
int snp_write(snp_app_t *app, void *buf, int size)
{
    return sntp_transmit(app, buf, size);
}

//
int snp_read(snp_app_t *app, void *buf, int size)
{
    pthread_mutex_lock(&(app->mutex));
    pthread_cond_wait(&(app->read_ready), &(app->mutex));
    int ret = read(app->read[0], buf, size);
    pthread_mutex_unlock(&(app->mutex));
    return ret;
}

// TODO diff return value if timeout
int snp_poll(snp_app_t *fds, int num_fds, int timeout)
{
    struct pollfd *pollfds = malloc(num_fds * sizeof(struct pollfd));

    int i;
    for(i = 0; i < num_fds; i++)
    {
        pollfds[0].fd = fds[i].read[0];
        pollfds[0].events = POLLIN;
    }

    int ret = poll(pollfds, num_fds, timeout);
    free(pollfds);
    return ret;
}
