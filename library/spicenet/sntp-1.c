// *******************
//
// SPICEnet Transport Protocol
//
// *******************

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>

#include <spicenet/sntp.h>
#include <spicenet/sndlp.h>
#include <spicenet/config.h>

#define APID 0x001
#define HEAD_SIZE 4
#define TAIL_SIZE 1

//TODO don't forget to wait for child processes
//TODO have a way to get rid of inactive apps, maybe see if the fd has closed

// how you read to and write from the port
int fd;
pthread_t receive_tid;
pthread_mutex_t fd_mutex;  //TODO figure out if we need this i think we might not

// for user programs to write to
int write_fds[2];
pthread_mutex_t write_mutex;

// list of running apps
sntp_app_t *running_apps;

unsigned char crc8(void *buf, int length);

// ** FUNCTION DEFINITIONS BEGIN ** 


/* handlers if i want
void reap(int signum)
{
     pid_t pid = 1;
     while(pid > 0)
     {
          pid = waitpid(-1, NULL, WNOHANG);
          if(pid > 0)
          {
               //process ended
          }
     }
}

void install_handlers(void)
{
    struct sigaction act;
    act.sa_handler = reap;
    act.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &act, NULL);
}*/

// gives client apps a sntp_app_t struct required to use SNTP
int sntp_connect(int apid, sntp_app_t **app)
{
    *app = malloc(sizeof(sntp_app_t));
    int ret = pipe((*app)->read);
    if(ret) return ret;
    (*app)->apid = apid;
    ret = sntp_app_insert(*app);
    if(ret) return ret;
    pthread_mutex_init(&((*app)->mutex), NULL);
    pthread_cond_init(&((*app)->read_ready), NULL);
    return 0;
}

// close
// does free the app structure
int sntp_close(sntp_app_t *app)
{
    sntp_app_remove(app->apid);
    free(app);
    return 0;
}

// allows client apps to send data to SNTP
int sntp_transmit(sntp_app_t *app, void *buf, int size)
{
    pthread_mutex_lock(&write_mutex);
    int ret = sntp_write(write_fds[0], app->apid, buf, size);
    pthread_mutex_unlock(&write_mutex);
    return ret;
}

// listens for incoming traffic on the connection
// in its own thread 
void * sntp_receive_client(void *arg)
{
    struct pollfd pollfds[1];
    pollfds[0].fd = fd;
    pollfds[0].events = POLLIN;

    sndlp_data_t received;
    sntp_app_t *app;

    int last_packet[MAX_APID];


    while(1)
    {
        poll(pollfds, 1, -1);

        int size = sndlp_read(fd, &received);
        if(size <= 0) continue;

        int header = * ((int *) received.data);
        void *data = &(received.data[HEAD_SIZE]);
        char crc = *((char *) &(received.data[HEAD_SIZE + size]));

        if(crc8(data, received.size - HEAD_SIZE - TAIL_SIZE) != crc) // packet invalid
        {
            continue; // skip this packet
        }

        if(last_packet[received.apid]+1 != received.pkt_num) // packet out of order
        {
            // TODO handle out of order packet
        }


        // TODO request retransmission if necessary

        // TODO increment last received packet number
            //need an if statement here to ensure that the packets are truly in the correct order
        last_packet[received.apid] = received.pkt_num;

        // send to appropriate apid
        // TODO maybe start another thread for the below
        app = sntp_app_find(received.apid);
        pthread_mutex_lock(&(app->mutex));
        write(app->read[1], data, received.size - HEAD_SIZE - TAIL_SIZE);
        pthread_cond_signal(&(app->read_ready));
        pthread_mutex_unlock(&(app->mutex));
    }
}

// forwards the SNTP packet to SNDLP
int sntp_write(int fd, int apid, void *buf, int size)
{
    int write_size = size + HEAD_SIZE + TAIL_SIZE;
    char *write_buf = malloc(write_size);

    char crc = crc8(buf, size);
    
    //TODO write value at the beginning
    memcpy(&write_buf[HEAD_SIZE], buf, size);
    memcpy(&write_buf[HEAD_SIZE + size], &crc, 1);

    return sndlp_write(fd, apid, write_buf, write_size) - (HEAD_SIZE + TAIL_SIZE);
}

// initlizes global mutexes and starts threads
// TODO int return
void sntp_start(int fd)
{
    pthread_mutex_init(&fd_mutex, NULL);
    pthread_mutex_init(&write_mutex, NULL);
    pipe(write_fds);
    int error = pthread_create(&receive_tid, NULL, sntp_receive_client, NULL);
    if (error); //TODO error message maybe
    //install_handlers();
}

#define POLYNOMIAL (0x1070U << 3)
// code provided by BCT
unsigned char crc8(void *buf, int length)
{
    unsigned char *inData = &buf[0];
    unsigned short data;
    unsigned char crc = 0xFF;
    
    int i, j;
    for(j = 0; j < length ; j++)
    {
        data = crc ^ *inData++;
        data <<= 8;
        for(i = 0; i < 8; i++)
        {
            if ((data & 0x8000) != 0)
            data = data^POLYNOMIAL;

            data <<= 1;
        }
    
        crc = (unsigned char)(data >> 8);
    }

    return crc;
}


