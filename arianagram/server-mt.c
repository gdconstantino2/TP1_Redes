#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common-mt.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define BUFSZ 1024
#define FEED_SIZE 5

typedef struct ClientNode {
    int socket;
    char username[USER_SIZE];
    struct ClientNode *next;
} ClientNode;

static ClientNode *clients = NULL;  // head da lista
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    uint32_t id;
    char username[USER_SIZE];
    char content[CONTENT_SIZE];
} FeedEntry;

typedef struct FollowNode {
    char follower[USER_SIZE];
    char followed[USER_SIZE];
    struct FollowNode *next;
} FollowNode;

static FollowNode *follows = NULL;
static pthread_mutex_t follows_mutex = PTHREAD_MUTEX_INITIALIZER;

static FeedEntry feed[FEED_SIZE];
static uint32_t next_id = 1;
static int feed_count = 0;      
static int feed_next = 0;       

static pthread_mutex_t feed_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t id_mutex = PTHREAD_MUTEX_INITIALIZER;


void usage(int argc, char **argv)
{
    fprintf(stderr, "Uso: %s <v4|v6> <porta>\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data
{
    int csock;
    struct sockaddr_storage storage;
    int client_id;
    char username[USER_SIZE];
};

void *client_thread(void *data)
{
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);
    
    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[Cliente %d] Conectado de %s\n", cdata->client_id, caddrstr);
    
    Message msg;
    memset(&msg, 0, sizeof(Message));
    
    
    printf("[Cliente %d] Handshake realizado\n", cdata->client_id);
    
    //int sequence = 0;
    ssize_t bytes_received;

    
    while (1)
    {
        bytes_received = recv(cdata->csock, &msg, sizeof(msg), 0);
        if (bytes_received <= 0)
        {
            printf("[Cliente %d] Desconectou\n", cdata->client_id);
            break;
        }
        
        // Processa mensagem baseado no tipo
        switch (msg.type) {
                case MSG_CONNECT:
                    strncpy(cdata->username, msg.username, USER_SIZE);
                    pthread_mutex_lock(&clients_mutex);
                    ClientNode *client = malloc(sizeof(ClientNode));
                    if (client != NULL) {
                        client->socket = cdata->csock;
                        strncpy(client->username, msg.username, USER_SIZE);
                        client->next = clients; 
                        clients = client;          
                    }
                    pthread_mutex_unlock(&clients_mutex);
                    printf("[CONN] @%s conectou.\n", msg.username);
                    break;
                case MSG_POST:
                    pthread_mutex_lock(&id_mutex);
                    uint32_t id = next_id++;
                    pthread_mutex_unlock(&id_mutex);
                    pthread_mutex_lock(&feed_mutex);

                    feed[feed_next].id = id;
                    strncpy(feed[feed_next].username, msg.username, USER_SIZE);
                    strncpy(feed[feed_next].content, msg.content, CONTENT_SIZE);

                    feed_next = (feed_next + 1) % FEED_SIZE;
                    if (feed_count < FEED_SIZE){ 
                        feed_count++;}

                    printf("[LOG] @%s posted (ID %u): \"%s\"\n", msg.username, id, msg.content);
                    pthread_mutex_lock(&follows_mutex);
                    FollowNode *f = follows;
                    while (f != NULL) {
                        if (strcmp(f->followed, msg.username) == 0) {
        
                            pthread_mutex_lock(&clients_mutex);
                            ClientNode *c = clients;
                            while (c != NULL) {
                                if (strcmp(c->username, f->follower) == 0) {
                                    int pos = (feed_next - 1 + FEED_SIZE) % FEED_SIZE;
    
                                    Message push_msg;
                                    push_msg.type = MSG_PUSH;
                                    strncpy(push_msg.username, feed[pos].username, USER_SIZE);
                                    strncpy(push_msg.content, feed[pos].content, CONTENT_SIZE);
                                    push_msg.msg_id = feed[pos].id;
    
                                    send(c->socket, &push_msg, sizeof(push_msg), 0);
                                }
                                c = c->next;
                            }   
                            pthread_mutex_unlock(&clients_mutex);
        
                        }
                        f = f->next;
                        }
                    pthread_mutex_unlock(&follows_mutex);
                    pthread_mutex_unlock(&feed_mutex);
                    break;
        
                case MSG_FOLLOW:
                    
                    pthread_mutex_lock(&follows_mutex);
                    FollowNode *follow = malloc(sizeof(FollowNode));
                    if (follow != NULL) {  // sempre verifique se malloc não falhou
                        strncpy(follow->follower, msg.username, USER_SIZE);
                        strncpy(follow->followed, msg.content, USER_SIZE);
                        follow->next = follows;   // insere no início da lista
                        follows = follow;          // atualiza a cabeça
                    }
                    pthread_mutex_unlock(&follows_mutex);
                    break;
        
                 case MSG_READ:
                    pthread_mutex_lock(&feed_mutex);

                    int pos = (feed_next - 1 + FEED_SIZE) % FEED_SIZE;  // começa pela mais recente

                    for (int i = 0; i < feed_count; i++) {
                        Message push_msg;
                        push_msg.type = MSG_PUSH;
                        strncpy(push_msg.username, feed[pos].username, USER_SIZE);
                        strncpy(push_msg.content, feed[pos].content, CONTENT_SIZE);
                        push_msg.msg_id = feed[pos].id;
                        send(cdata->csock, &push_msg, sizeof(push_msg), 0);
                        pos = (pos - 1 + FEED_SIZE) % FEED_SIZE;  // vai para a anterior
                        
                    }
                    
                    pthread_mutex_unlock(&feed_mutex);
                    break;
        
                case MSG_END:
                        // Cliente quer sair
                    close(cdata->csock);
                    free(cdata);
                    pthread_exit(EXIT_SUCCESS);
                    break;
        
                default:
                    // Tipo desconhecido - apenas ignore ou log
                    printf("[Cliente %d] Tipo inválido: %d\n", cdata->client_id, msg.type);
                    break;
}
    }
    
    close(cdata->csock);
    free(cdata);
    printf("Cliente %d desconectado\n", cdata->client_id);
    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        usage(argc, argv);
    }
    
    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage))
    {
        usage(argc, argv);
    }
    
    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1)
    {
        logexit("socket");
    }
    
    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        logexit("setsockopt");
    }
    
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage)))
    {
        logexit("bind");
    }
    
    // Backlog 128 é um valor razoável para fila de conexões pendentes
    // Não limita o número de clientes simultâneos
    if (0 != listen(s, 128))
    {
        logexit("listen");
    }
    
    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    
    const char *ip_versao;
    if (strcmp(argv[1], "v4") == 0)
        ip_versao = "4";
    else if (strcmp(argv[1], "v6") == 0)
        ip_versao = "6";
    else
        exit(EXIT_FAILURE);
    
    printf("=== SERVIDOR MULTITHREAD GENÉRICO ===\n");
    printf("Modo: IPv%s\n", ip_versao);
    printf("Porta: %s\n", argv[2]);
    printf("Aguardando conexões... (sem limite de clientes)\n");
    printf("=====================================\n\n");
    
    int next_client_id = 1;
    while (1)
    {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);
        
        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1)
        {
            logexit("accept");
        }
        
        struct client_data *cdata = malloc(sizeof(*cdata));
        if (!cdata)
        {
            logexit("malloc");
        }
        
        cdata->csock = csock;
        cdata->client_id = next_client_id++;
        memcpy(&(cdata->storage), &cstorage, sizeof(storage));
        
        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, cdata) != 0)
        {
            perror("pthread_create");
            close(csock);
            free(cdata);
            continue;
        }
        
        pthread_detach(tid);
    }
    
    close(s);
    exit(EXIT_SUCCESS);
}
