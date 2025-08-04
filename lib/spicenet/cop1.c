// *******************
//
// SPICEnet Transport Protocol
// Communications Operation Procedure 1 (COP-1)
// CCSDS Standard 232.1-B-2
//
// Implemented by: Simon Kowerski
//
// *******************

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <spicenet/sndlp.h>
#include <spicenet/sntp.h>
#include <stdio.h>

#include <spicenet/errors.h>
#define CLCW_APID 0x001
#define BC_APID 0x002

// ************* BEGIN FOP-1 STATE MACHINE *************

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
int T1_active;
pthread_t timer_thread;

sntp_app_t *fop_app;
pthread_mutex_t fop_lock;
int init;

void fop_start_timer();
int fop_look_fdu();
void fop_alert(fop_alert_t alert, int event);

void fop_purge_sent_queue() // DONE WORKS
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

void fop_purge_wait_queue() // DONE WORKS
{
    free(wait_queue);
    wait_queue = NULL;
}

// returns bytes written 
// if pos == -1 it is a new frame to add to the sent queue
int fop_transmit_ad_frame(sntp_app_t *app, void *buf, int size, int pos) // DONE WORKS
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
        fop_alert(A_LLIF, 42);
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
int fop_look_fdu() // DONE WORKS
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

void fop_initalize() // DONE WORKS
{
    fop_purge_sent_queue();
    fop_purge_wait_queue();
    trans_count = 1;
    trans_limit = 10;
    SS = 0;
    T1 = 0;
    T1_initial = 4;
    TT = 0;
    ad_out_flag = 1;
    bc_out_flag = 1;
    bd_out_flag = 1;
    fop_state = INITIAL;
    VS = 0;
    NNR = 0; // TODO check nnr
}

void fop_suspend() 
{
    // notify SNTP that this is suspended
}

void fop_resume() // DONE
{
    SS = 0;
    fop_start_timer();
}

void * timer_run(void * none)
{
    while(T1 > 0)
    {
        T1--;
        sleep(1);
    }

    pthread_mutex_lock(&fop_lock);
    T1_active = 0;

    if(trans_count < trans_limit)
    {
        if(TT == 0) // E16
        {
            switch(fop_state)
            {
                case ACTIVE:
                case RE_WO_WAIT:
                    fop_initiate_retransmission();
                    fop_look_fdu();
                    break;
                case INIT_WO_BC:
                    fop_alert(A_T1, 16);
                    fop_state = INITIAL;
                    break;
                case INIT_W_BC:
                    // TODO initiate bc retrans
                    fop_look_directive();
                default:
                    break;
            }
        }

        else // E104
        {
            switch(fop_state)
            {
                case ACTIVE:
                case RE_WO_WAIT:
                    fop_initiate_retransmission();
                    fop_look_fdu();
                    break;
                case INIT_WO_BC:
                    SS=4;
                    fop_suspend();
                    fop_state = INITIAL;
                    break;
                case INIT_W_BC:
                    // TODO initiate bc retrans
                    fop_look_directive();
                default:
                    break;
            }
        }
    }

    else
    {
        if(TT == 0) // E17
        {
            fop_alert(A_T1, 17);
            fop_state = INITIAL;
        }
        
        else // E18
        {
            if(fop_state == INIT_W_BC) fop_alert(A_T1, 18);
            else 
            {
                SS = fop_state;
                fop_suspend();
            }
            fop_state = INITIAL;
        }
    }
    pthread_mutex_unlock(&fop_lock);
    return NULL;
}

void fop_start_timer()
{
    T1 = T1_initial;
    if(!T1_active)
    {
        T1_active = 1;
        pthread_create(&timer_thread, NULL, timer_run, NULL);
        pthread_detach(timer_thread);
    }
}

void fop_cancel_timer()
{
    pthread_cancel(timer_thread);
    T1_active = 0;
}

void fop_alert(fop_alert_t alert, int event) // DONEALMOST
{
    printf(" <<! ALERT %d - EVENT %d!>>\n", alert, event);
    fop_cancel_timer();
    fop_purge_wait_queue();
    fop_purge_sent_queue();

    uint8_t lockvar;
    if(alert == A_LIMIT || alert == A_T1); //TODO procedure to close the connection due to timeout
    else if(alert == A_LOCKOUT) { lockvar = 0x01; sntp_write(fop_app->apid, &lockvar, 1); }
    else{} // TODO do smth when CLCW error occurs (should never happen but just in case)
    fop_start();
    //smth idk
    //send alert to SNTP somehow
}

void fop_start()
{
    //E23
    fop_accept();
    fop_initalize();
    fop_confirm();
    fop_state = ACTIVE;
    ad_out_flag = 1;
    if(!init) 
    {
        sntp_connect(CLCW_APID, &fop_app);
        pthread_mutex_init(&fop_lock, NULL);
        init = 1;
    }
}

void fop_receive_clcw(sndlp_data_t *packet) // DONE
{
    if(packet->size != 2 && ( *((uint8_t *) packet->data) & 0b00011111) != 0) // E15 Invalid CLCW
    { 
        if(fop_state == INITIAL) return;    
        fop_alert(A_CLCW, 15);
        fop_state = INITIAL;
        return; 
    }


    uint8_t nr = ((uint8_t *) packet->data)[1];
    int lockout = ((uint8_t *) packet->data)[0] >> 7;
    int wait = (((uint8_t *) packet->data)[0] << 1) >> 7;
    int retransmit = (((uint8_t *) packet->data)[0] << 2) >> 7;

    pthread_mutex_lock(&fop_lock);

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
                                fop_alert(A_SYNC, 1);
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
                    if(fop_state != INITIAL) fop_alert(A_CLCW, 3);
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
                        fop_alert(A_SYNC, 4);
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
                                fop_alert(A_SYNC, 5);
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
                            fop_alert(A_CLCW, 7);
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
                                fop_alert(A_LIMIT, 102);
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
                                fop_alert(A_LIMIT, 101);
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
                    fop_alert(A_NNR, 13);
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
                fop_alert(A_LOCKOUT, 14);
                fop_state = INITIAL;
        }    
    }

    pthread_mutex_unlock(&fop_lock);

} // end valid CLCW



// FOP-1 DIRECTIVES

int fop_request_transmit(sntp_app_t *app, void *buf, int size) // DONE WORKS
{
    if(wait_queue) return ECOP_REJECT; // E20

    pthread_mutex_lock(&fop_lock);

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
    
    pthread_mutex_unlock(&fop_lock);
}

// ************* END FOP-1 STATE MACHINE *************



// ************* BEGIN FARM-1 STATE MACHINE *************
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

int farm_timer;
pthread_mutex_t farm_lock;
void* clcw_service(void *app_t);

void farm_start(int fd) // DONE WORKS
{
    lockout_flag = 0;
    wait_flag = 0;
    retransmit_flag = 0;
    VR = 0; 
    farm_b_counter = 0;
    farm_state = OPEN;

    farm_timer = .1;

    pthread_t thread;
    sntp_app_t *app;
    sntp_connect(CLCW_APID, &app);
    pthread_create(&thread, NULL, clcw_service, app);
    pthread_detach(thread);
    pthread_mutex_init(&farm_lock, NULL);
}

void farm_unlock()
{
    switch(farm_state) // E7 valid unlock frame received
    {
        case LOCKOUT:
            lockout_flag = 0;
        case WAIT:
            wait_flag = 0;
        case OPEN:
            farm_b_counter++;
            retransmit_flag = 0;
    }
}

void farm_set_vr(uint8_t new_vr)
{
    switch(farm_state) // E8 valid ser V(R) to V*(R) frame received
    {
        case WAIT:
            wait_flag = 0;
        case OPEN:
            retransmit_flag = 0;
            VR = new_vr;
        case LOCKOUT:
            farm_b_counter++;
    }
}

// FARM-1 to receive a valid user data frame
void* farm_receive(void *void_packet) // DONE WORKS
{  
    sndlp_data_t *packet = (sndlp_data_t *) void_packet;
    
    if(packet->apid == CLCW_APID) { fop_receive_clcw(packet); free(packet); return NULL; } // deliver clcw packets to FOP-1 
    if(packet->apid == BC_APID) // valid BC frame arrived
    {
        pthread_mutex_lock(&farm_lock);
        if(((uint8_t *)packet->data)[0] == 0x00) farm_unlock();
        else if(((uint8_t *)packet->data)[0] == 0x01) farm_set_vr(((uint8_t *)packet->data)[1]);
        pthread_mutex_unlock(&farm_lock);
        return NULL;
    } 

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
            farm_accept(packet);
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

void* clcw_service(void *app_t)
{

    sntp_app_t *app = (sntp_app_t *) app_t; 

    uint16_t packet;
    while(1) // E11
    {
        packet = 0;
        packet += lockout_flag;
        packet << 1;
        packet += wait_flag;
        packet << 1;
        packet += retransmit_flag;
        packet << 5;
        packet += VR;
        sntp_write(app->apid, &packet, 2);
        sleep(farm_timer);
    }
}

// ************* END FARM-1 STATE MACHINE *************

void cop_1_start(int fd)
{
    fop_start();
    farm_start(fd);
}