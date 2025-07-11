#include <spicenet/sntp_structures.h>

#define TABLE_SIZE 20

sntp_app_t *hash_table[TABLE_SIZE];

int sntp_app_insert(sntp_app_t *app)
{
    int hash = (app->apid % TABLE_SIZE);

    sntp_app_t *head = hash_table[hash];
    sntp_app_t *prev = NULL;
    
    while(head)
    {
        if(head->apid == app->apid) return -1;
        prev = head;
        head = head->next;
    }
    
    if(prev) prev->next = app; // add to end of list
    else hash_table[hash] = app; // make head if list empty

    return 0;
}

void sntp_app_remove(int apid)
{
    int hash = (apid % TABLE_SIZE);

    sntp_app_t *head = hash_table[hash];
    sntp_app_t *prev = NULL;
    
    while(head && head->apid != apid)
    {
        prev = head;
        head = head->next;
    }

    if(!head) return;
    else
    {
        prev->next = head->next;
    }
}

sntp_app_t * sntp_app_find(int apid)
{
    int hash = (apid % TABLE_SIZE);

    sntp_app_t *head = hash_table[hash];
    sntp_app_t *prev = NULL;
    
    while(head && head->apid != apid)
    {
        prev = head;
        head = head->next;
    }

    if(!head) return NULL;
    else return head;
}