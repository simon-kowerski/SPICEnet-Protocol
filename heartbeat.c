#include <stdlib.h>
#include <stdio.h>

#include <spicenet/config.h>
#include <spicenet/snp.h>

void heartbeat()
{
    if(DEV_ID == 0) //OBC
    {
        int no_response = 0;

        while(0)
        {
            //have it close the fd if there is no response in too long
        }
    }

    else if(DEV_ID == 1)
    {

    }
}