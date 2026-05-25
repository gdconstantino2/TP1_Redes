#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common-mt.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <arpa/inet.h>

#define BUFSZ 1024

void usage(int argc, char **argv)
{
    fprintf(stderr, "Uso: %s <host> <porta> <username>\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (addrparse(argv[1], argv[2], &storage) != 0)
    {
        usage(argc, argv);
    }

    int s = socket(storage.ss_family, SOCK_STREAM, 0);
    ssize_t bytes;
    if (s == -1)
    {
        logexit("socket");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (connect(s, addr, sizeof(storage)) != 0)
    {
        logexit("connect");
    }

    printf("Conectado ao servidor como %s.\n", argv[3]);

    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = htons(MSG_CONNECT);
    strncpy(msg.username, argv[3], USER_SIZE);

    if (send(s, &msg, sizeof(msg), 0) != sizeof(msg))
    {
        logexit("send connect");
    }

    printf("\nComandos disponíveis:\n");
    printf("  POST <texto> - Publica uma mensagem\n");
    printf("  FOLLOW @user - Segue um usuário\n");
    printf("  READ - Lê o feed histórico\n");
    printf("  exit - Encerra conexão\n");
    printf("  help - Mostra ajuda\n\n");

    char input[BUFSZ];
    char command[BUFSZ];
    char argument[BUFSZ];
    
    fd_set readfds;
    int max_fd;
    int stdin_ready = 1;

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        if (stdin_ready) {
            FD_SET(STDIN_FILENO, &readfds);
        }
        
        max_fd = (s > STDIN_FILENO) ? s : STDIN_FILENO;
        
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }
        
        // Verifica notificações do servidor
        if (FD_ISSET(s, &readfds)) {
            Message push_msg;
            bytes = recv(s, &push_msg, sizeof(push_msg), MSG_DONTWAIT);
            while (bytes > 0) {
                uint16_t type = ntohs(push_msg.type);
                if (type == MSG_PUSH) {
                    printf("\n[NOTIFICATION] @%s: \"%s\"\n", 
                           push_msg.username, push_msg.content);
                    fflush(stdout);
                    printf("> ");
                    fflush(stdout);
                }
                bytes = recv(s, &push_msg, sizeof(push_msg), MSG_DONTWAIT);
            }
        }
        
        // Verifica comando do usuário
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            printf("> ");
            fflush(stdout);
            
            if (fgets(input, BUFSZ, stdin) == NULL)
                break;

            input[strcspn(input, "\n")] = '\0';
            if (strlen(input) == 0)
                continue;

            if (sscanf(input, "%s %[^\n]", command, argument) < 1)
                continue;

            memset(&msg, 0, sizeof(msg));

            if (strcmp(command, "exit") == 0)
            {
                msg.type = htons(MSG_END);
                send(s, &msg, sizeof(msg), 0);
                break;
            }
            else if (strcmp(command, "POST") == 0)
            {
                msg.type = htons(MSG_POST);
                strncpy(msg.username, argv[3], USER_SIZE);
                strncpy(msg.content, argument, CONTENT_SIZE);
                send(s, &msg, sizeof(msg), 0);
            }
            else if (strcmp(command, "FOLLOW") == 0)
            {
                msg.type = htons(MSG_FOLLOW);
                strncpy(msg.username, argv[3], USER_SIZE);
                strncpy(msg.content, argument, CONTENT_SIZE);
                send(s, &msg, sizeof(msg), 0);
            }
            else if (strcmp(command, "READ") == 0)
            {
                msg.type = htons(MSG_READ);
                send(s, &msg, sizeof(msg), 0);
                
                // Recebe mensagens até MSG_END
                int count = 0;
                while (1) {
                    Message feed_msg;
                    bytes = recv(s, &feed_msg, sizeof(feed_msg), 0);
                    if (bytes <= 0) break;
                    
                    uint16_t type = ntohs(feed_msg.type);
                    if (type == MSG_PUSH) {
                        uint32_t msg_id = ntohl(feed_msg.msg_id);
                        printf("[FEED] ID %u | @%s: \"%s\"\n",
                               msg_id, feed_msg.username, feed_msg.content);
                        count++;
                    }
                    else if (type == MSG_END) {
                        break;
                    }
                }
                fflush(stdout);
            }
            else if (strcmp(command, "help") == 0)
            {
                printf("\nComandos:\n");
                printf("  POST <texto> - Publica uma mensagem\n");
                printf("  FOLLOW @user - Segue um usuário\n");
                printf("  READ - Lê o feed histórico\n");
                printf("  exit - Encerra conexão\n\n");
            }
            else
            {
                printf("Comando desconhecido. Use 'help'\n");
            }
        }
    }

    close(s);
    printf("Cliente encerrado.\n");
    return 0;
}
