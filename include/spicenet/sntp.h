#include <spicenet/sntp_structures.h>

void sntp_start(int fd);
int sntp_write(int fd, int apid, void *buf, int size);
int sntp_transmit(sntp_app_t *app, void *buf, int size);
int sntp_connect(int apid, sntp_app_t **app);

int sntp_app_insert(sntp_app_t *app);
void sntp_app_remove(int apid);
sntp_app_t * sntp_app_find(int apid);