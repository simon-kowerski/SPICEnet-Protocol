// *******************
//
// SPICEnet Data Link Protocol
// Driver for serial over RS422
//
// *******************

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdint.h>

#include <stdio.h>

#include <spicenet/spp.h>
#include <spicenet/sndlp.h>
#include <spicenet/config.h>

//TODO error checking and stuff

int APID = 0x000;
uint8_t SYNC[] = {0x1D, 0xEC, 0x0AF, 0x00};

// Opens the serial port for reading and writing
// returns 0 on success or -1 and updates errno on failure
int sndlp_open(int *fd, char *portname)
{
    struct termios tty;

    // open the serial port
    *fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (*fd == -1) {
        // perror("Error opening serial port");
        return -1;
    }

    // configure the serial port
    if (tcgetattr(*fd, &tty) != 0) 
    {
        //perror("Error getting serial port attributes");
        close(*fd);
        return -1;
    }

    // set baud rate to 115200
    cfsetospeed(&tty, B115200);  // Set output speed
    cfsetispeed(&tty, B115200);  // Set input speed

    // set 8N1: 8 data bits, no parity, 1 stop bit
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8 data bits
    tty.c_cflag &= ~PARENB;                         // No parity
    tty.c_cflag &= ~CSTOPB;                         // 1 stop bit
    tty.c_cflag |= CREAD | CLOCAL;                  // Enable receiver and ignore modem control lines

    // set raw input mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         // No software flow control
    tty.c_oflag &= ~OPOST;                          // Raw output

    // apply the settings
    if (tcsetattr(*fd, TCSANOW, &tty) != 0) 
    {
        perror("Error setting serial port attributes");
        close(*fd);
        return -1;
    }

    return 0;
}

// verfies a connection to the other device
// returns 0 on success or an error code on failure

// TODO ensure config settings match
int sndlp_connect(int fd)
{
    int ret = 0;
    sndlp_write(fd, APID, SYNC, sizeof(SYNC));
    sndlp_data_t buf;
    sndlp_read(fd, &buf);
    SYNC[sizeof(SYNC) - 1] = 0xff;
    if(buf.apid != APID) ret = -1;
    else if(buf.type == DEV_ID) ret = -2;
    else if(buf.size != sizeof(SYNC)) ret = -3;
    else 
    {
        for(int i = 0; i < sizeof(SYNC) - 1; i++)
        {
            if(((uint8_t *)buf.data)[i] != SYNC[i]) 
            {
                ret = -4;
                break;
            }
        }
    }
    if(((uint8_t *)buf.data)[sizeof(SYNC) - 1] == 0x00) sndlp_write(fd, APID, SYNC, sizeof(SYNC));
    free(buf.data);
    
    if(ret) return ret;
    
    return 0;
}

// writes to the open file
// returns the number of user data bytes written (not including spp header), or -1 on error
int sndlp_write(int fd, int apid, void *buf, int size)
{
    spp_packet_t *packets;
    int num_packs;
    int written = 0, to_write;
    void *data;
    
    spp_pack_serv(buf, size, apid, &packets, &num_packs);

    for(int i = 0; i < num_packs; i++)
    {
        spp_octet_conv(&(packets[i]), &data, &to_write);
        size = write(fd, data, to_write);
        free(data);
        if(size < 0) 
        {
            spp_free_packets(packets, num_packs);
            return size;
        }
        written += size;
    }

    spp_free_packets(packets, num_packs);

    return written - 6;
}

// buf assumed to be already initalized
int sndlp_read(int fd, sndlp_data_t *buf)
{
    int total = 0, bytes;

    buf->data = malloc(6);

    while(total < 6)
    {
        bytes = read(fd, buf->data + total, 6 - total);

        if(bytes < 0) 
        {
            free(buf->data);
            buf->data = NULL;
            return bytes;
        }

        total += bytes;
    }

    spp_packet_t packet;
    spp_read_header(buf->data, &packet);

    buf->type = packet.type;
    buf->apid = packet.apid;
    buf->pkt_num = packet.count_name;
    buf->size = packet.length;

    total = 0;

    buf->data = realloc(buf->data, buf->size);

    while(total < buf->size)
    {
        bytes = read(fd, buf->data + total, buf->size - total);

        if(bytes < 0) 
        {
            free(buf->data);
            buf->data = NULL;
            return bytes;
        }

        total += bytes;
    }
            
    return total;
}
