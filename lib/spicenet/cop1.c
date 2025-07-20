//TODO mutexes to prevent more than one thread from using each state machine at a time
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <spicenet/sndlp.h>
#include <spicenet/sntp.h>
#include <stdio.h>

#include <spicenet/errors.h>
#define APID 0x001

// ************* BEGIN FOP-1 STATE MACHINE *************

//TODOS and Questions
// what is an accept response

typedef enum fop_states {place_holder, ACTIVE, RE_WO_WAIT, RE_W_WAIT, INIT_WO_BC, INIT_W_BC, INITIAL} fop_state_t;
typedef enum fop_alerts {A_SYNC, A_CLCW, A_LOCKOUT, A_LIMIT, A_NNR, A_T1, A_TERM, A_LLIF} fop_alert_t;

typedef char* fdu_t;
fop_state_t fop_state;                      // a) State
uint8_t VS;                                 // b) Transmitter Frame Sequence Number

#define K 64                                // m) FOP Sliding Window Width (1 <= k <= PW < 256)
fdu_t wait_queue;                           // c) Wait Queue                    // max size of 1 FDU
    sntp_app_t wait_app;                       // the application which sent each frame in the wait queue
    int wait_size;
fdu_t sent_queue[K];                        // d) Sent Queue                    // max size of K fdu
unsigned int to_be_retransmitted_flag[K];   // e) To Be Retransmitted Flag      // need one of these for every element in the sent queue
    sntp_app_t *sent_app[K];                   // the application which sent each frame in the sent queue
    unsigned int sq_start;                      // sent queue start
    unsigned int sq_length;                     // sent queue length
    unsigned int sq_sizes[K];
unsigned int ad_out_flag;                   // f) AD Out Flag                   //flags f g and h can be 0 or 1 
unsigned int bd_out_flag;                   // g) BD Out Flag                       // 0 - not ready
unsigned int bc_out_flag;                   // h) BC Out Flag                       // 1 - ready
uint8_t NNR;                                // i) Expected Acknowledgement Frame Sequence Number
unsigned int T1_initial;                    // j) Timer Initial Value
unsigned int trans_limit;                   // k) Transmission Limit
unsigned int trans_count;                   // l) Transmission Count
unsigned int TT;                            // n) Timeout Type
unsigned int SS;                            // o) Suspend State
unsigned int T1;                            // Timer

void fop_start_timer();
int fop_look_fdu();
void fop_alert(fop_alert_t alert);

void fop_purge_sent_queue() // DONE
{
    
    // generate signal maybe

    int i, wrap = 0;
    for(i = 0; i < sq_length; i++)
    {
        if(wrap == 0 && i >= K) wrap = K;
        free(sent_queue[sq_start + i - wrap]);
        free( sent_app[sq_start + i - wrap]);
        to_be_retransmitted_flag[sq_start + i - wrap] = 0;
    }
}

void fop_purge_wait_queue() //DONE
{
    free(wait_queue);
    wait_queue = NULL;
}

// returns bytes written 
// if pos == -1 it is a new frame to add to the sent queue
int fop_transmit_ad_frame(sntp_app_t *app, void *buf, int size, int pos) //DONE
{
    // if not a retransmit, add frame to the sent queue
    if(pos == -1) 
    {
        size += sizeof(VS);
        void *big_buf = malloc(size);

        // insert V(S) to the N(S) section of the frame
        memcpy(big_buf, &VS, sizeof(VS));
        memcpy(big_buf + 1, buf, size - sizeof(VS));
        VS += 1;    

        pos = sq_start + sq_length;
        if(pos >= K) pos -= K;
    
        sent_queue[pos] = malloc(size);
        sent_app[pos] = malloc(sizeof(sntp_app_t));
    

        memcpy(sent_queue[pos], big_buf, size);
        memcpy(sent_app[pos], app, sizeof(sntp_app_t));
        sq_sizes[pos] = size;
        free(big_buf);

        if(sq_length == 0) trans_count = 1;
        sq_length++;

        fop_purge_wait_queue();
    }

    to_be_retransmitted_flag[pos] = 0;

    fop_start_timer();

    // deliver the frame
    ad_out_flag = 0;
    int bytes_written = sntp_write(app->apid, sent_queue[pos], size);
    ad_out_flag = 1;

    int ret2;

    // respond to status of the frame
    if(bytes_written > 0) // E41 AD_Accept
    {
        switch(fop_state)
        {
            case ACTIVE:
            case RE_WO_WAIT:
                ret2 = fop_look_fdu();
            default:
                break;
        }
    }

    else // E42 AD_Reject
    {
        fop_alert(A_LLIF);
        fop_state = INITIAL;
    }

    if(ret2) return ret2;
    else return bytes_written;
}

void fop_transmit_bc_frame()
{

}

void fop_transmit_bd_frame()
{
    
}

void fop_initiate_retransmission() // DONE
{
    // pass abort to lower procedures
    trans_count++;
    fop_start_timer();
    int i, wrap = 0;
    for(i = 0; i < sq_length; i++)
    {
        if(wrap == 0 && i >= K) wrap = K;
        to_be_retransmitted_flag[sq_start + i - wrap] = 1;
    }
}

void fop_remove_ack_frames(int NR) // DONE
{
    int i, pos, wrap = 0;
    uint8_t ns;
    fdu_t current;
    for(i = 0; i < sq_length; i++)
    {
        if(wrap == 0 && i >= K) wrap = K;
        pos = sq_start + i - wrap;
        current = sent_queue[pos];
        ns = *((uint8_t *) current);

        if(ns < NR) 
        {
            sq_start = pos + 1;
            sq_length--;
            if(sq_start >= K) sq_start -= K;
            free(sent_queue[pos]);
            free(sent_app[pos]);
        }

        else break;
    }
    NNR = NR;
    trans_count = 1;
}

void fop_look_directive()
{

}

// returns bytes written 
int fop_look_fdu() // DONE
{
    if(!ad_out_flag) return ECOP_NOT_READY;

    int i, wrap = 0, pos = -1;

    for(i = 0; i < sq_length; i++)
    {
        for(i = 0; i < sq_length; i++)
        {
            if(wrap == 0 && i >= K) wrap = K;
            pos = sq_start + i - wrap;
            if(to_be_retransmitted_flag[pos]) return fop_transmit_ad_frame(sent_app[pos], sent_queue[pos], sq_sizes[pos], pos);
        }
    }

    if(wait_queue)
    {
        pos = fop_transmit_ad_frame(&wait_app, wait_queue, wait_size, -1);
        return pos;
    }

    else return 0;
}

void fop_accept()
{

}

void fop_reject()
{
    
}

void fop_confirm()
{
    
}

void fop_initalize() //DONE
{
    fop_purge_sent_queue();
    fop_purge_wait_queue();
    trans_count = 1;
    SS = 0;
}

void fop_suspend() //DONE
{
    SS = 1;
}

void fop_resume() //DONE
{
    SS = 0;
    fop_start_timer();
}

void fop_start_timer()
{

}

void fop_cancel_timer()
{
    
}

void fop_alert(fop_alert_t alert) // MOSTLY DONE
{
    fop_cancel_timer();
    fop_purge_wait_queue();
    fop_purge_sent_queue();
    //smth idk
    //send alert to SNTP somehow
    printf(" <<! %d !>>\n", alert);
}

void fop_start()
{
    //E23
    fop_accept();
    fop_initalize();
    fop_confirm();
    fop_state = ACTIVE;
    ad_out_flag = 1;
}

void fop_receive_clcw(sndlp_data_t *packet) // DONE
{
    if(packet->size != 2) // E15 Invalid CLCW
    { // TODO also make sure that there are zeros where there are supposed to be
        if(fop_state == INITIAL) return;    
        fop_alert(A_CLCW);
        fop_state = INITIAL;
        return; 
    }

    uint8_t nr = ((uint8_t *) packet->data)[1];
    int lockout = ((uint8_t *) packet->data)[0] >> 7;
    int wait = (((uint8_t *) packet->data)[0] << 1) >> 7;
    int retransmit = (((uint8_t *) packet->data)[0] << 2) >> 7;

    if(!lockout)
    {
        if(nr == VS)
        {
            if(!retransmit)
            {
                if(!wait) 
                {
                    // no lockout, nr == VS, no retransmit, no wait
                    if(nr == NNR) // E1 No new frames acknowledged
                    {
                        switch(fop_state)
                        {
                            case RE_WO_WAIT:
                            case RE_W_WAIT:
                                fop_alert(A_SYNC);
                                fop_state = INITIAL;
                                break;
                            case INIT_WO_BC:
                                // confirm
                                fop_cancel_timer();
                                fop_state = ACTIVE;
                                break;
                            case INIT_W_BC:
                                //confirm
                                //release bc frame
                                fop_cancel_timer();
                                fop_state = ACTIVE;
                                break;
                            default:
                                break;
                        }
                    }

                    // no lockout, nr == VS, no retransmit, no wait
                    else // E2 some new frames acknowledged
                    {
                        switch(fop_state)
                        {
                            case ACTIVE:
                            case RE_W_WAIT:
                            case RE_WO_WAIT:
                                fop_remove_ack_frames(nr);
                                fop_cancel_timer();
                                fop_look_fdu();
                                fop_state = ACTIVE;
                                break;
                            default:
                                break;
                        }
                    }
                } // end no wait

                else //E3 CLCW error, wait shouldn't be active if no retransmit
                {
                    // no lockout, nr == VS, no retransmit, wait
                    if(fop_state != INITIAL) fop_alert(A_CLCW);
                    fop_state = INITIAL;
                }
            } // no retransmit

            else //E4 error, retransmit while all frames positively acknowledged
            {
                // no lockout, nr == VS, retransmit
                switch(fop_state)
                {
                    case INITIAL:
                    case INIT_W_BC:
                        break;
                    default:
                        fop_alert(A_SYNC);
                        fop_state = INITIAL;
                }   
            }
        }

        else if(nr < VS && nr >= NNR) // valid N(R), some unacknowledged frames
        {
            if(!retransmit)
            {
                if(!wait)
                {
                    // no lockout, nr < VS, nr >= NN(R), (nr valid), no retransmit, no wait
                    if(nr == NNR) // E5 no new frames acknowledged
                    {
                        switch(fop_state)
                        {
                            case RE_WO_WAIT:
                            case RE_W_WAIT:
                                fop_alert(A_SYNC);
                                fop_state = INITIAL;
                                break;
                            default:
                                break;
                        }
                    } 

                    // no lockout, nr < VS, nr >= NN(R), (nr valid), no retransmit, no wait
                    else // E6 some new frames acknowledged
                    {
                        switch(fop_state)
                        {
                            case ACTIVE:
                            case RE_WO_WAIT:
                            case RE_W_WAIT:
                                fop_remove_ack_frames(nr);
                                fop_look_fdu();
                                fop_state = ACTIVE;
                                break;
                            default:
                                break;
                        }
                    }
                } // end no wait

                else // E 7
                {
                    // no lockout, nr < VS, nr >= NN(R), (nr valid), no retransmit, wait
                    switch(fop_state)
                    {
                        case ACTIVE:
                        case RE_WO_WAIT:
                        case RE_W_WAIT:
                            fop_alert(A_CLCW);
                            fop_state = INITIAL;
                            break;
                        default:
                            break;
                    }
                }
            } // end no retransmit

            else // retransmit
            {
                if(trans_limit == 1)
                {
                    // no lockout, nr < VS, nr >= NN(R), (nr valid), retransmit, transmit_limit 1
                    if(nr == NNR) // E102 No new frames acknowledged
                    {
                        switch(fop_state)
                        {
                            case ACTIVE:
                            case RE_WO_WAIT:
                            case RE_W_WAIT:
                                fop_alert(A_LIMIT);
                                fop_state = INITIAL;
                                break;
                            default:
                                break;
                        }
                    }

                    // no lockout, nr < VS, nr >= NN(R), (nr valid), retransmit, transmit_limit 1
                    else // E101 some new frames acknowledged
                    {
                        switch(fop_state)
                        {
                            case ACTIVE:
                            case RE_WO_WAIT:
                            case RE_W_WAIT:
                                fop_remove_ack_frames(nr);
                                fop_alert(A_LIMIT);
                                fop_state = INITIAL;
                                break;
                            default:
                                break;
                        }
                    }
                } // end translimit 1

                else // transmit limit > 1
                {
                    if(nr == NNR) // no new frames acknowledged
                    {
                        if(trans_count < trans_limit)
                        {
                            // no lockout, nr < VS, nr >= NN(R), (nr valid), retransmit, translimit > 1, translimit > transcount, no wait
                            if(!wait) // E10
                            {
                                switch(fop_state)
                                {
                                    case ACTIVE:
                                    case RE_W_WAIT:
                                        fop_initiate_retransmission();
                                        fop_look_fdu();
                                        fop_state = RE_WO_WAIT;
                                    default:
                                        break;
                                }
                            }

                            // no lockout, nr < VS, nr >= NN(R), (nr valid), retransmit, translimit > 1, translimit > transcount, wait
                            else // E11
                            {
                                switch (fop_state)
                                {
                                    case ACTIVE:
                                    case RE_WO_WAIT:
                                    case RE_W_WAIT:
                                        fop_state = RE_W_WAIT;
                                    default:
                                        break;
                                }
                            }
                        }

                        else // trans_count >= trans_limit
                        {
                            // no lockout, nr < VS, nr >= NN(R), (nr valid), retransmit, translimit > 1, translimit <= transcount, no wait
                            if(!wait) // E12
                            {
                                switch(fop_state)
                                {
                                    case ACTIVE:
                                    case RE_WO_WAIT:
                                    case RE_W_WAIT:
                                        fop_state = RE_WO_WAIT;
                                    default:
                                        break;
                                }
                            }

                            // no lockout, nr < VS, nr >= NN(R), (nr valid), retransmit, translimit > 1, translimit <= transcount, wait
                            else // E103
                            {
                                switch(fop_state)
                                {
                                    case ACTIVE:
                                    case RE_WO_WAIT:
                                    case RE_W_WAIT:
                                        fop_state = RE_W_WAIT;
                                    default:
                                        break;
                                }
                            }
                        } // end trans_count >= trans_limit
                    } // end no new frames acknowledged

                    else // some new frames acknowledged
                    {
                        // no lockout, nr < VS, nr >= NN(R), (nr valid), retransmit, some new frames acknoaledged, wait
                        if(!wait) // E8
                        {
                            switch(fop_state)
                            {
                                case ACTIVE:
                                case RE_WO_WAIT:
                                case RE_W_WAIT:
                                    fop_remove_ack_frames(nr);
                                    fop_initiate_retransmission();
                                    fop_look_fdu();
                                    fop_state = RE_WO_WAIT;
                                default:
                                    break;    
                            }
                        }

                        // no lockout, nr < VS, nr >= NN(R), (nr valid), retransmit, some new frames acknoaledged, no wait
                        else // E9
                        {
                            switch(fop_state)
                            {
                                case ACTIVE:
                                case RE_WO_WAIT:
                                case RE_W_WAIT:
                                    fop_remove_ack_frames(nr);
                                    fop_state = RE_W_WAIT;
                                default:
                                    break;    
                            }
                        }
                    } // end some new frames acknowledged
                } // end trans limit > 1
            } // end retransmit
        } // end valid N(R)

        else if(nr < NNR || nr > VS) // E13 invalid N(R)
        {
            // nr != VS, nr invalid
            switch(fop_state)
            {
                case INIT_W_BC:
                case INITIAL:
                    break;
                default:
                    fop_alert(A_NNR);
                    fop_state = INITIAL;
            }   
        } // end invalid N(R)
    } // end no lockout

    else // E14 Lockout flag is set
    { 
        // clcw lockout
        switch(fop_state)
        {
            case INIT_W_BC:
            case INITIAL:
                break;
            default:
                fop_alert(A_LOCKOUT);
                fop_state = INITIAL;
        }    
    }
} // end valid CLCW

int fop_request_transmit(sntp_app_t *app, void *buf, int size) // DONE
{
    if(wait_queue) return ECOP_REJECT; // E20
    switch (fop_state) //E19
    {
        case ACTIVE:
        case RE_WO_WAIT:    
            wait_queue = malloc(size);
            memcpy(wait_queue, buf, size);
            memcpy(&wait_app, app, sizeof(*app));
            wait_size = size;
            return fop_look_fdu() - sizeof(VS);
        case RE_W_WAIT:
            wait_queue = malloc(size);
            memcpy(wait_queue, buf, size);
            memcpy(&wait_app, app, sizeof(*app));
            wait_size = size;
            return ECOP_WAITING;
        default:
            return ECOP_REJECT;
    }
}

// ************* END FOP-1 STATE MACHINE *************



// ************* BEGIN FARM-1 STATE MACHINE *************
// TODO timer for transmit CLCW
// TODO BC frames
// TODO unlocking / see if wait is necessary (i dont think it is)

#define farm_accept(X) { sntp_deliver((X)->apid, (X)->data, (X)->size); free((X)); }
#define farm_discard(X) free((X))

typedef enum farm_states {OPEN, WAIT, LOCKOUT} farm_state_t;
farm_state_t farm_state;            // a) State
unsigned int lockout_flag;          // b) Lockout_Flag
unsigned int wait_flag;             // c) Wait_Flag
unsigned int retransmit_flag;       // d) Retransmit_Flag
uint8_t VR;                         // e) Receiver_Frame_Sequence_Number (usually referred to as ‘V(R)’)
unsigned int farm_b_counter;        // f) FARM-B_Counter
#define W 128                       // g) FARM_Sliding_Window_Width (also known as ‘W’)
#define PW (W/2)                    // h) FARM_Positive_Window_Width (also known as ‘PW’)
#define NW (W/2)                    // i) FARM_Negative_Window_Width (also known as ‘NW’)
pthread_mutex_t farm_lock;

void farm_start()
{
    lockout_flag = 0;
    wait_flag = 0;
    retransmit_flag = 0;
    VR = 0; 
    farm_b_counter = 0;
    farm_state = OPEN;

    pthread_mutex_init(&farm_lock, NULL);
}

// FARM-1 to receive a valid user data frame
void* farm_receive(void *void_packet)
{  
    sndlp_data_t *packet = (sndlp_data_t *) void_packet;
    pthread_mutex_lock(&farm_lock); // make sure to unlock mutex before sending the packet to SNTP or FOP 
    uint8_t NS = *((uint8_t *) packet->data);
    packet->data = &((char *) packet->data)[sizeof(NS)];
    packet->size -= sizeof(char);
    
    if(NS == VR) // E1 correct value of NS
    {
        if(farm_state == OPEN) // S0 accept the packet
        {
            VR += 1;
            retransmit_flag = 0;
            pthread_mutex_unlock(&farm_lock);
            if(packet->apid == APID) fop_receive_clcw(packet); // TODO deliver packet to FOP-1 
            else farm_accept(packet);
            return NULL;
        }

        else farm_discard(packet); 
    }

    else if(NS > VR) // E3/E5 towards positive floating window
    {
        if(NS <= VR + PW - 1) // E3 within bounds of positive window
        {
            if(farm_state == OPEN) retransmit_flag = 1;
        }

        else // E5 in lockout zone
        {
            lockout_flag = 1;
            farm_state = LOCKOUT;
        }
        
        farm_discard(packet); // all states discard for both E3 and E5
    }

    else if(NS < VR) // E4/E5 towards negative floating window
    {
        if( !(NS <= VR - NW) )// E5 in lockout zone
        {
            lockout_flag = 1;
            farm_state = LOCKOUT;
        }
        
        farm_discard(packet); // all states discard for both E4 and E5
    }

    pthread_mutex_unlock(&farm_lock);
    return NULL;
}

// ************* END FARM-1 STATE MACHINE *************