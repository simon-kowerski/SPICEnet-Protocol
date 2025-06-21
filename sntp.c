// *******************
//
// SPICEnet Transport Protocol
//
// *******************

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>

#include <spicenet/sndlp.h>
#include <spicenet/config.h>

#define HEAD_SIZE 4
#define TAIL_SIZE 1

typedef struct threadsafe_fd //TODO maybe take this out of the struct, make multiple global variables could be easier
{
    int fd;
    pthread_mutex_t *mutex;
} sntp_fd_t;

//TODO have a way to get rid of inactive apps, maybe see if the fd has closed
// snp_write and snp_read will take this struct as an argument
typedef struct app
{
    int read[2];
    int write; // will be the write end of 
    pthread_mutex_t *mutex;
} sntp_app_t; //can be defined as something different for the user

sntp_fd_t fd;
int writefds[2]; // TODO give a mutex
sntp_app_t *running_apps;

void sntp_init()
{
    //initialzie all mutexs and such
    //create thread for out_client
    //create thread for in_client
}

//in a thread
void sntp_transmit_client(void *arg)
{
    struct pollfd pollfds[1];
    pollfds[0].fd = writefds[0];
    pollfds[0].events = POLLIN;

    sndlp_data_t recieved;

    while(1)
    {
        poll(pollfds, 1, -1);

        pthread_mutex_lock(fd.mutex);
        //sntp_write();
        pthread_mutex_unlock(fd.mutex);

        // check validity of packet
        // request retransmission if necessary
        // send to appropriate apid
    }
}

//in a thread
void sntp_recieve_client(void *arg)
{
    struct pollfd pollfds[1];
    pollfds[0].fd = fd.fd;
    pollfds[0].events = POLLIN;

    sndlp_data_t recieved;

    while(1)
    {
        poll(pollfds, 1, -1);

        sndlp_read(fd.fd, &recieved);
        // check validity of packet
        // request retransmission if necessary
        // send to appropriate apid
    }
}

int sntp_write(int fd, int apid, void *buf, int size)
{
    int write_size = size + HEAD_SIZE + TAIL_SIZE;
    char *write_buf = malloc(write_size);

    char crc = crc8(buf, size);
    
    //TODO write value at the beginning
    memcpy(write_buf[4], buf, size);
    memcpy(write_buf[4+size], crc, 1);

    return sndlp_write(fd, apid, write_buf, write_size) - (HEAD_SIZE + TAIL_SIZE);
}

#define POLYNOMIAL (0x1070U << 3)
// code provided by BCT
unsigned char crc8(void *buf, int length)
{
    // unsigned char crcData[DATA_LENGTH] = {0x1A, 0xCF, 0x10, 0x00, 0x00, 0x04, 1, 2, 3, 4};
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
}


