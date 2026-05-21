#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define BUFSZ 1024

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
};

void *client_thread(void *data)
{
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);
    
    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[Cliente %d] Conectado de %s\n", cdata->client_id, caddrstr);
    
    GenericMessage msg;
    memset(&msg, 0, sizeof(GenericMessage));
    
    // Handshake inicial
    msg.type = MSG_HELLO;
    msg.status = 0;
    snprintf(msg.message, MSG_SIZE, "Bem-vindo ao servidor genérico");
    
    if (send(cdata->csock, &msg, sizeof(GenericMessage), 0) != sizeof(GenericMessage))
    {
        printf("[Cliente %d] Falha no handshake\n", cdata->client_id);
        close(cdata->csock);
        free(cdata);
        pthread_exit(EXIT_FAILURE);
    }
    
    printf("[Cliente %d] Handshake realizado\n", cdata->client_id);
    
    int sequence = 0;
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
        switch (msg.type)
        {
            case MSG_REQUEST:
                printf("[Cliente %d] Recebeu requisição: %s\n", 
                       cdata->client_id, msg.data);
                
                // Prepara resposta
                msg.type = MSG_RESPONSE;
                msg.status = 0;
                msg.sequence = sequence++;
                snprintf(msg.data, MAX_DATA_SIZE, "Eco: %s", msg.data);
                snprintf(msg.message, MSG_SIZE, "Processado com sucesso");
                
                if (send(cdata->csock, &msg, sizeof(msg), 0) != sizeof(msg))
                {
                    printf("[Cliente %d] Erro ao enviar resposta\n", cdata->client_id);
                    break;
                }
                break;
                
            case MSG_DATA:
                printf("[Cliente %d] Recebeu dados: %s\n", 
                       cdata->client_id, msg.data);
                
                msg.type = MSG_ACK;
                msg.status = 0;
                snprintf(msg.message, MSG_SIZE, "Dados recebidos");
                
                if (send(cdata->csock, &msg, sizeof(msg), 0) != sizeof(msg))
                {
                    printf("[Cliente %d] Erro ao enviar ACK\n", cdata->client_id);
                    break;
                }
                break;
                
            case MSG_EXIT:
                printf("[Cliente %d] Solicitou encerramento\n", cdata->client_id);
                msg.type = MSG_ACK;
                send(cdata->csock, &msg, sizeof(msg), 0);
                close(cdata->csock);
                free(cdata);
                pthread_exit(EXIT_SUCCESS);
                break;
                
            default:
                printf("[Cliente %d] Tipo de mensagem desconhecido: %d\n", 
                       cdata->client_id, msg.type);
                msg.type = MSG_ERROR;
                snprintf(msg.message, MSG_SIZE, "Tipo de mensagem inválido");
                send(cdata->csock, &msg, sizeof(msg), 0);
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