#include <spicenet/sndlp.h>
#include <spicenet/sntp_structures.h>

void cop_1_start(int fd);

int fop_request_transmit(sntp_app_t *app, void *buf, int size);
void fop_receive_clcw(sndlp_data_t *packet);

void* farm_receive(void *void_packet); // packet should be sndlp_data_t
