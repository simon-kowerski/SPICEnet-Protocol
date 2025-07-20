#ifndef SERIAL_RECV
#define SERIAL_RECV
typedef struct serial_recieve
{
    unsigned int type : 1;
    unsigned int apid : 11;
    unsigned int pkt_num : 14;
    unsigned int size : 16;

    void *data;
} sndlp_data_t;
#endif

int sndlp_open(int *fd, char *portname);
int sndlp_read(int fd, sndlp_data_t *buf);
int sndlp_write(int fd, int apid, void *buf, int size);
int sndlp_connect(int fd);
