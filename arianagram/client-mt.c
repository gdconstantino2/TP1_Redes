#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common-mt.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

#define BUFSZ 1024

void usage(int argc, char **argv)
{
    fprintf(stderr, "Uso: %s <host> <porta> <username>\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        usage(argc, argv);
    }

    struct sockaddr_storage storage;
    if (addrparse(argv[1], argv[2], &storage) != 0) {
        usage(argc, argv);
    }

    int s = socket(storage.ss_family, SOCK_STREAM, 0);
    ssize_t bytes;
    if (s == -1) {
        logexit("socket");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (connect(s, addr, sizeof(storage)) != 0) {
        logexit("connect");
    }

    printf("Conectado ao servidor Arianagram!\n");

    Message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_CONNECT;
    strncpy(msg.username, argv[3], USER_SIZE);

    if (send(s, &msg, sizeof(msg), 0) != sizeof(msg)) {
        logexit("send connect");
    }

    printf("Identificado como %s\n", argv[3]);

    printf("\nComandos disponíveis:\n");
    printf("  POST <texto> - Publica uma mensagem\n");
    printf("  FOLLOW @user - Segue um usuário\n");
    printf("  READ - Lê o feed histórico\n");
    printf("  exit - Encerra conexão\n");
    printf("  help - Mostra ajuda\n\n");

    char input[BUFSZ];
    char command[BUFSZ];
    char argument[BUFSZ];

    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);

    fd_set readfds;
    int max_fd;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(s, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        max_fd = (s > STDIN_FILENO) ? s : STDIN_FILENO;

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(s, &readfds)) {
            Message push_msg;
            bytes = recv(s, &push_msg, sizeof(push_msg), MSG_DONTWAIT);
            while (bytes > 0) {
                if (push_msg.type == MSG_PUSH) {
                    printf("\n[NOTIFICATION] @%s: \"%s\"\n",
                           push_msg.username, push_msg.content);
                    fflush(stdout);
                    printf("> ");
                    fflush(stdout);
                }
                bytes = recv(s, &push_msg, sizeof(push_msg), MSG_DONTWAIT);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            printf("> ");
            fflush(stdout);

            if (fgets(input, BUFSZ, stdin) == NULL) {
                break;
            }

            input[strcspn(input, "\n")] = '\0';
            if (strlen(input) == 0) {
                continue;
            }

            if (sscanf(input, "%s %[^\n]", command, argument) < 1) {
                continue;
            }

            memset(&msg, 0, sizeof(msg));

            if (strcmp(command, "exit") == 0) {
                msg.type = MSG_END;
                send(s, &msg, sizeof(msg), 0);
                break;
            } else if (strcmp(command, "POST") == 0) {
                msg.type = MSG_POST;
                strncpy(msg.username, argv[3], USER_SIZE);
                strncpy(msg.content, argument, CONTENT_SIZE);
                send(s, &msg, sizeof(msg), 0);
            } else if (strcmp(command, "FOLLOW") == 0) {
                msg.type = MSG_FOLLOW;
                strncpy(msg.username, argv[3], USER_SIZE);
                strncpy(msg.content, argument, CONTENT_SIZE);
                send(s, &msg, sizeof(msg), 0);
            } else if (strcmp(command, "READ") == 0) {
                msg.type = MSG_READ;
                send(s, &msg, sizeof(msg), 0);

                usleep(100000);

                int count = 0;
                Message feed_msg;
                bytes = recv(s, &feed_msg, sizeof(feed_msg), MSG_DONTWAIT);
                while (bytes > 0) {
                    if (feed_msg.type == MSG_PUSH) {
                        printf("[FEED] ID %u | @%s: \"%s\"\n",
                               feed_msg.msg_id, feed_msg.username, feed_msg.content);
                        count++;
                    }
                    bytes = recv(s, &feed_msg, sizeof(feed_msg), MSG_DONTWAIT);
                }

                if (count == 0) {
                    printf("[FEED] Nenhuma mensagem no feed.\n");
                }
                fflush(stdout);
            } else if (strcmp(command, "help") == 0) {
                printf("\nComandos:\n");
                printf("  POST <texto> - Publica uma mensagem\n");
                printf("  FOLLOW @user - Segue um usuário\n");
                printf("  READ - Lê o feed histórico\n");
                printf("  exit - Encerra conexão\n\n");
            } else {
                printf("Comando desconhecido. Use 'help'\n");
            }
        }
    }

    close(s);
    printf("Cliente encerrado.\n");
    return 0;
}
