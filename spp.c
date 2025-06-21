// *******************
//
// SPICEnet Data Link Protocol
// Space Packet Protocol
// CCSDS Standard
//
// *******************

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <math.h>

#include <spicenet/config.h>
#include <spicenet/spp.h>

struct spp_globals
{
    int seq_counts[MAX_APID];
    pthread_mutex_t mutex;
} globals;

// returns the latest unused sequence count and increments the counter
unsigned int next_seq_count(unsigned int apid)
{
    pthread_mutex_lock(&(globals.mutex));
    int ret = globals.seq_counts[apid];
    globals.seq_counts[apid]++;
    if(globals.seq_counts[apid] > 16383) globals.seq_counts[apid] = 0;
    pthread_mutex_unlock(&(globals.mutex));
    return ret;
}

// might be unnecessary
// returns the latest unused sequence count
unsigned int get_seq_count(unsigned int apid)
{
    pthread_mutex_lock(&(globals.mutex));
    int ret = globals.seq_counts[apid];
    pthread_mutex_unlock(&(globals.mutex));
    return ret;
}

void spp_init()
{
    for(int i = 0; i < 2048; i++)
    {
        globals.seq_counts[i] = 0;
    }

    pthread_mutex_init(&(globals.mutex), NULL);
}

void spp_free_packets(spp_packet_t *packet, int length)
{
    for(int i = 0; i<length; i++) free(packet[i].data);
    free(packet);
}




// Initializes the header of a space packet from an incoming packet header
// content is expected to contain exactly 48 bits (6 octets) of data
void spp_read_header(void *data, spp_packet_t *packet)
{       
    uint8_t *content = (uint8_t *) data;
    packet->version = (content[0] >> 5);
    packet->type = (content[0] >> 4) & 1;
    packet->sec_header_flag = (content [0] >> 3) & 1;
    packet->apid = ((content[0] & 7) << 8) + (content[1]);
    packet->seq_flag = (content[2] >> 6);
    packet->count_name = ((content[2] & 0x3F) << 8) + (content[3]);
    packet->length = (content[4] << 8) + content[5];
}

// Converts a packet to an octet string
// octet_string is expected to be unallocated and will end containing the string
void spp_octet_conv(spp_packet_t *packet, void **octet_string, int *size)
{
    if(!size) //to account for if size is passed as null
    {
        int s2;
        size = &s2;
    }

    *size = 6 + packet->length;
    *octet_string = malloc(*size);

// packet header - 6 bytes (octets)
    (* (uint8_t **) octet_string)[0] = (packet->version << 5) + (packet->type << 4) 
                                        + (packet->sec_header_flag << 3) + (packet->apid & 0x700);
    (* (uint8_t **) octet_string)[1] = (packet->apid) & 0x0FF;
    (* (uint8_t **) octet_string)[2] = (packet->seq_flag << 6) + (packet->count_name & 0x3F00);
    (* (uint8_t **) octet_string)[3] = packet->count_name & 0xFF;
    (* (uint8_t **) octet_string)[4] = packet->length >> 8;
    (* (uint8_t **) octet_string)[5] = packet->length & 0xFF;

    for(int i = 6; i < *size; i++)
    {
        (* (uint8_t **) octet_string)[i] = ((uint8_t *) packet->data)[i-6];
    }
}

// builds new packets from user data and given parameters
// packets is allocated and assigned
// num packs is assigned
void spp_pack_serv(void *data, int length, unsigned int apid, spp_packet_t **packets, int *num_packs)
{
    int num_packets = ceil(((double) length) / MAX_PACKET_LENGTH);
    *packets = malloc(num_packets * sizeof(spp_packet_t));

    if(num_packs) *num_packs = num_packets;

    int i, x=0;
    for(i = 0; i < num_packets; i++)
    {
        (*packets)[i].version = 0;
        (*packets)[i].type = DEV_ID;          
        (*packets)[i].sec_header_flag = 0;  //TODO: see apid table
        (*packets)[i].apid = apid;
        //TODO test if/else
        if(i == 0) (*packets)[i].seq_flag = (num_packets == 1) ? 0b11 : 0b01;
        else (*packets)[i].seq_flag = (i + 1 == num_packets) ? 0b10 : 0b00;
        (*packets)[i].count_name = next_seq_count(apid);
        if(num_packets == 1) (*packets)[i].length = length;
        else 
        {
            if(length < MAX_PACKET_LENGTH) (*packets)[i].length = length;
            else 
            {
                (*packets)[i].length = MAX_PACKET_LENGTH;
                length -= MAX_PACKET_LENGTH;
            }
        }

        (*packets)[i].data = malloc(8 * (*packets)[i].length);

        for(; x < (*packets)[i].length; x++)
        {
            ((uint8_t *) (*packets)[i].data)[x] = ((uint8_t *) data)[x];
        }
    }  
} 