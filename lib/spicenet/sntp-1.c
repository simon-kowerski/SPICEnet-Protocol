// *******************
//
// SPICEnet Transport Protocol
// With COP-1 
//
// Written by: Simon Kowerski
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
#include <stdint.h>

#include <spicenet/sntp.h>
#include <spicenet/sndlp.h>
#include <spicenet/config.h>
#include <spicenet/cop1.h>

#define HEAD_SIZE 0
#define TAIL_SIZE 1

//TODO have a way to get rid of inactive apps, maybe see if the fd has closed
//TODO COP-1 Alert and Suspend Handler - automatically close the connections when it detects the timeout alert from COP-1

// how you read to and write from the port
int fd;
pthread_t receive_tid;

// for user programs to write to
pthread_mutex_t write_mutex;

// list of running apps
sntp_app_t *running_apps;

unsigned char crc8(void *buf, int length);

// ** FUNCTION DEFINITIONS BEGIN ** 

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
    (*app)->unread = 0;
    cop_1_start(fd);
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
    return fop_request_transmit(app, buf, size); 
}

// listens for incoming traffic on the connection
// in its own thread 
void * sntp_receive_client(void *arg)
{
    struct pollfd pollfds[1];
    pollfds[0].fd = fd;
    pollfds[0].events = POLLIN;

    sndlp_data_t *received;

    while(1)
    {
        poll(pollfds, 1, -1);

        received = malloc(sizeof(sndlp_data_t));

        int size = sndlp_read(fd, received);
        if(size <= 0) continue;

        //int header = * ((int *) received->data);
        void *data = &(((char *) (received->data))[HEAD_SIZE]);

        char crc = *((char *) &(((char *) (received->data))[HEAD_SIZE + size - TAIL_SIZE]));

        received->size -= HEAD_SIZE + TAIL_SIZE;
        received->data = data;

        if(crc8(data, received->size) != crc) // packet invalid
        {
            free(received->data);
            free(received);
            continue; // skip this packet
        }

        // send packet to FARM-1 in new thread
        sndlp_data_t *received2 = malloc(sizeof(sndlp_data_t));
        memcpy(received2, received, sizeof(sndlp_data_t));
        free(received);
        
        pthread_t thread;
        pthread_create(&thread, NULL, farm_receive, received2);
        pthread_detach(thread);
    }
}

// send an accepted packet to its respective application
void sntp_deliver(int apid, void *data, int size)
{
    sntp_app_t *app = sntp_app_find(apid);
    pthread_mutex_lock(&(app->mutex));
    int written = write(app->read[1], data, size);
    if (written > 0) app->unread += written;
    pthread_cond_signal(&(app->read_ready));
    pthread_mutex_unlock(&(app->mutex));
}

// forwards the SNTP packet to SNDLP
int sntp_write(int apid, void *buf, int size)
{
    int write_size = size + HEAD_SIZE + TAIL_SIZE;
    char *write_buf = malloc(write_size);

    char crc = crc8(buf, size);
    
    // write value at the beginning if you want or need
    memcpy(&write_buf[HEAD_SIZE], buf, size);
    memcpy(&write_buf[HEAD_SIZE + size], &crc, 1);

    pthread_mutex_lock(&write_mutex);
    int bytes = sndlp_write(fd, apid, write_buf, write_size) - (HEAD_SIZE + TAIL_SIZE);
    pthread_mutex_unlock(&write_mutex);

    free(write_buf);

    return bytes;
}

// initlizes global mutexes and starts threads
int sntp_start(int file_desc)
{
    pthread_mutex_init(&write_mutex, NULL);
    fd = file_desc;
    int error = pthread_create(&receive_tid, NULL, sntp_receive_client, NULL);
    if (error) return error;
    return 0;
}

#define POLYNOMIAL (0x1070U << 3)
// code provided by BCT
unsigned char crc8(void *buf, int length)
{
    unsigned char *inData = &((unsigned char *) buf)[0];
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


