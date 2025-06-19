typedef struct serial_recieve
{
    int type : 1;
    int apid : 11;
    int pkt_num : 14;
    int size : 16;

    void *data;
} sndlp_data_t;

int sndlp_open(int *fd, char *portname);
int sndlp_read(int fd, sndlp_data_t *buf);
int sndlp_write(int fd, int apid, void *buf, int size);
int sndlp_connect(int fd);
