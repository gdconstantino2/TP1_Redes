#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFSZ 1024

void usage(int argc, char **argv)
{
    fprintf(stderr, "Uso: %s <endereco> <porta>\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        usage(argc, argv);
    }
    
    struct sockaddr_storage storage;
    if (addrparse(argv[1], argv[2], &storage) != 0)
    {
        usage(argc, argv);
    }
    
    int s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1)
    {
        logexit("socket");
    }
    
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (connect(s, addr, sizeof(storage)) != 0)
    {
        logexit("connect");
    }
    
    printf("Conectado ao servidor genérico!\n");
    
    // Recebe handshake inicial
    GenericMessage msg;
    ssize_t bytes = recv(s, &msg, sizeof(msg), 0);
    if (bytes != sizeof(msg) || msg.type != MSG_HELLO)
    {
        fprintf(stderr, "Erro no protocolo: handshake falhou\n");
        close(s);
        exit(EXIT_FAILURE);
    }
    
    printf("Servidor: %s\n", msg.message);
    printf("\nComandos disponíveis:\n");
    printf("  req <texto> - Envia requisição\n");
    printf("  data <dados> - Envia dados\n");
    printf("  exit - Encerra conexão\n");
    printf("  help - Mostra ajuda\n\n");
    
    char input[BUFSZ];
    char command[BUFSZ];
    char argument[BUFSZ];
    
    while (1)
    {
        printf("> ");
        fflush(stdout);
        
        if (fgets(input, BUFSZ, stdin) == NULL)
        {
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        
        if (strlen(input) == 0)
        {
            continue;
        }
        
        // Parse comando
        if (sscanf(input, "%s %[^\n]", command, argument) < 1)
        {
            continue;
        }
        
        memset(&msg, 0, sizeof(msg));
        
        if (strcmp(command, "exit") == 0)
        {
            msg.type = MSG_EXIT;
            send(s, &msg, sizeof(msg), 0);
            
            // Aguarda confirmação
            bytes = recv(s, &msg, sizeof(msg), 0);
            printf("Encerrando conexão...\n");
            break;
        }
        else if (strcmp(command, "req") == 0)
        {
            msg.type = MSG_REQUEST;
            snprintf(msg.data, MAX_DATA_SIZE, "%s", argument);
            send(s, &msg, sizeof(msg), 0);
            
            bytes = recv(s, &msg, sizeof(msg), 0);
            if (bytes <= 0)
            {
                printf("Conexão perdida\n");
                break;
            }
            
            if (msg.type == MSG_RESPONSE)
            {
                printf("Resposta: %s\n", msg.data);
                printf("Mensagem: %s\n", msg.message);
            }
        }
        else if (strcmp(command, "data") == 0)
        {
            msg.type = MSG_DATA;
            snprintf(msg.data, MAX_DATA_SIZE, "%s", argument);
            send(s, &msg, sizeof(msg), 0);
            
            bytes = recv(s, &msg, sizeof(msg), 0);
            if (bytes <= 0)
            {
                printf("Conexão perdida\n");
                break;
            }
            
            if (msg.type == MSG_ACK)
            {
                printf("ACK: %s\n", msg.message);
            }
        }
        else if (strcmp(command, "help") == 0)
        {
            printf("\nComandos:\n");
            printf("  req <texto> - Envia requisição, servidor ecoa\n");
            printf("  data <dados> - Envia dados, servidor confirma\n");
            printf("  exit - Encerra conexão\n\n");
        }
        else
        {
            printf("Comando desconhecido. Use 'help'\n");
        }
    }
    
    close(s);
    printf("Cliente encerrado.\n");
    return 0;
}