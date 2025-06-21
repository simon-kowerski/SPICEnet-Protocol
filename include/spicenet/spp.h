typedef struct space_packet
{
    // packet header contents - 6 bytes
    unsigned int version : 3;
    unsigned int type : 1;
    unsigned int sec_header_flag: 1;
    unsigned int apid : 11;
    unsigned int seq_flag : 2;
    unsigned int count_name : 14;
    unsigned int length : 16;

    // packet data
    void *data;
} spp_packet_t;


void spp_init();

void spp_free_packets(spp_packet_t *packet, int length);
void spp_read_header(void *header_content, spp_packet_t *packet);
void spp_octet_conv(spp_packet_t *packet, void **octet_string, int *size);
void spp_pack_serv(void *data, int length, unsigned int apid, spp_packet_t **packets, int *num_packs);
