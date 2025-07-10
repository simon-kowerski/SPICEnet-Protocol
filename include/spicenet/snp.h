#include <spicenet/sntp_structures.h>

typedef sntp_app_t snp_app_t;

int snp_open(int *fd, char *portname);
int snp_listen(int fd);
int snp_connect(int apid, snp_app_t **app);

int snp_write(snp_app_t *app, void *buf, int size);
int snp_read(snp_app_t *app, void *buf, int size);
int snp_poll(snp_app_t *fds, int num_fds, int timeout);
