#ifndef COMMON_H
#define COMMON_H
#pragma once
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#define USER_SIZE 16
#define CONTENT_SIZE 140

// Cabeçalho referencia
typedef enum {
    MSG_CONNECT = 0,       
    MSG_POST = 1,    
    MSG_FOLLOW = 2,   
    MSG_READ = 3,        
    MSG_PUSH = 4,      
    MSG_END = 5       
} MessageType;


typedef struct {
    uint16_t type;                
    char username[USER_SIZE];       
    char message[CONTENT_SIZE];    
    uint32_t msg_id;     
}Message;

//Funções auxiliares
void logexit(const char *msg);
int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage);
void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);
int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage *storage);

#endif