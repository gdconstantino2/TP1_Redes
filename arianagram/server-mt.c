#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common-mt.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFSZ 1024
#define FEED_SIZE 5

typedef struct ClientNode {
    int socket;
    char username[USER_SIZE];
    struct ClientNode *next;
} ClientNode;

static ClientNode *clients = NULL;
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
    fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data
{
    int csock;
    struct sockaddr_storage storage;
    int client_id;
    char username[USER_SIZE];
};

void remove_client(int socket)
{
    pthread_mutex_lock(&clients_mutex);
    ClientNode *prev = NULL;
    ClientNode *curr = clients;
    while (curr != NULL) {
        if (curr->socket == socket) {
            if (prev == NULL) {
                clients = curr->next;
            } else {
                prev->next = curr->next;
            }
            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *client_thread(void *data)
{
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);

    Message msg;
    memset(&msg, 0, sizeof(Message));
    ssize_t bytes_received;

    while (1) {
        bytes_received = recv(cdata->csock, &msg, sizeof(msg), 0);
        if (bytes_received <= 0) {
            printf("[DISC] %s desconectou.\n", cdata->username);
            remove_client(cdata->csock);
            break;
        }

        // Converte endianness dos campos recebidos
        msg.type = ntohs(msg.type);
        msg.msg_id = ntohl(msg.msg_id);

        switch (msg.type) {
            case MSG_CONNECT:
                // Remove cliente antigo com mesmo username
                pthread_mutex_lock(&clients_mutex);
                ClientNode *prev = NULL;
                ClientNode *curr = clients;
                while (curr != NULL) {
                    if (strcmp(curr->username, msg.username) == 0) {
                        if (prev == NULL) {
                            clients = curr->next;
                        } else {
                            prev->next = curr->next;
                        }
                        close(curr->socket);
                        free(curr);
                        break;
                    }
                    prev = curr;
                    curr = curr->next;
                }
                pthread_mutex_unlock(&clients_mutex);

                // Adiciona novo cliente
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
                printf("[CONN] %s conectou.\n", msg.username);
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
                if (feed_count < FEED_SIZE) {
                    feed_count++;
                }

                printf("[LOG] @%s posted (ID %u): \"%s\"\n", msg.username, id, msg.content);

                pthread_mutex_lock(&follows_mutex);
                FollowNode *f = follows;
                while (f != NULL) {
                    if (strcmp(f->followed, msg.username) == 0) {
                        pthread_mutex_lock(&clients_mutex);
                        ClientNode *c = clients;
                        while (c != NULL) {
                            if (strcmp(c->username, f->follower) == 0 &&
                                strcmp(c->username, msg.username) != 0) {
                                int pos = (feed_next - 1 + FEED_SIZE) % FEED_SIZE;

                                Message push_msg;
                                memset(&push_msg, 0, sizeof(push_msg));
                                push_msg.type = htons(MSG_PUSH);
                                strncpy(push_msg.username, feed[pos].username, USER_SIZE);
                                strncpy(push_msg.content, feed[pos].content, CONTENT_SIZE);
                                push_msg.msg_id = htonl(feed[pos].id);

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
                if (strcmp(msg.username, msg.content) == 0) {
                    break;
                }
                pthread_mutex_lock(&follows_mutex);
                FollowNode *existing = follows;
                int already_follows = 0;
                while (existing != NULL) {
                    if (strcmp(existing->follower, msg.username) == 0 &&
                        strcmp(existing->followed, msg.content) == 0) {
                        already_follows = 1;
                        break;
                    }
                    existing = existing->next;
                }
                if (!already_follows) {
                    FollowNode *follow = malloc(sizeof(FollowNode));
                    if (follow != NULL) {
                        strncpy(follow->follower, msg.username, USER_SIZE);
                        strncpy(follow->followed, msg.content, USER_SIZE);
                        follow->next = follows;
                        follows = follow;
                    }
                }
                pthread_mutex_unlock(&follows_mutex);
                break;

            case MSG_READ:
                pthread_mutex_lock(&feed_mutex);
                int pos = (feed_next - 1 + FEED_SIZE) % FEED_SIZE;
                for (int i = 0; i < feed_count; i++) {
                    Message push_msg;
                    memset(&push_msg, 0, sizeof(push_msg));
                    push_msg.type = htons(MSG_PUSH);
                    strncpy(push_msg.username, feed[pos].username, USER_SIZE);
                    strncpy(push_msg.content, feed[pos].content, CONTENT_SIZE);
                    push_msg.msg_id = htonl(feed[pos].id);
                    send(cdata->csock, &push_msg, sizeof(push_msg), 0);
                    pos = (pos - 1 + FEED_SIZE) % FEED_SIZE;
                }
                pthread_mutex_unlock(&feed_mutex);
                break;

            case MSG_END:
                printf("[DISC] %s desconectou.\n", cdata->username);
                remove_client(cdata->csock);
                close(cdata->csock);
                free(cdata);
                pthread_exit(EXIT_SUCCESS);
                break;

            default:
                break;
        }
    }

    close(cdata->csock);
    free(cdata);
    pthread_exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        usage(argc, argv);
    }

    int port = atoi(argv[1]);
    if (port == 0) {
        usage(argc, argv);
    }

    struct addrinfo hints, *res, *p;
    int s = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(NULL, port_str, &hints, &res) != 0) {
        logexit("getaddrinfo");
    }

    for (p = res; p != NULL; p = p->ai_next) {
        s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (s == -1) {
            continue;
        }

        int opt = 1;
        if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            close(s);
            continue;
        }

        if (bind(s, p->ai_addr, p->ai_addrlen) == -1) {
            close(s);
            continue;
        }

        break;
    }

    freeaddrinfo(res);

    if (s == -1) {
        logexit("socket/bind");
    }

    if (listen(s, 128) != 0) {
        logexit("listen");
    }

    printf("Aguardando conexoes na porta %d.\n", port);

    int next_client_id = 1;
    while (1) {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage);

        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("accept");
        }

        struct client_data *cdata = malloc(sizeof(*cdata));
        if (!cdata) {
            logexit("malloc");
        }

        cdata->csock = csock;
        cdata->client_id = next_client_id++;
        memcpy(&(cdata->storage), &cstorage, sizeof(cstorage));

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread, cdata) != 0) {
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
