#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#define BUFSZ 1024

void usage(int argc, char **argv) {
    printf("usage: %s <protocolo> <porta> <senha>\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    int senha_secreta[5];
    if (argc < 4) {
        usage(argc, argv);
    }
    if(strlen(argv[3]) != 5){
	exit(EXIT_FAILURE);
    }
    for(int i = 0; i <= 4; i++){
	if(!isdigit(argv[3][i])){
		exit(EXIT_FAILURE);
	}
	senha_secreta[i] = argv[3][i] - '0';
    }

    struct sockaddr_storage storage;
    if (0 != server_sockaddr_init(argv[1], argv[2], &storage)) {
        usage(argc, argv);
    }

    int s;
    s = socket(storage.ss_family, SOCK_STREAM, 0);
    if (s == -1) {
        logexit("socket");
    }

    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if (0 != bind(s, addr, sizeof(storage))) {
        logexit("bind");
    }

    if (0 != listen(s, 10)) {
        logexit("listen");
    }

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    const char *ip_versão;
    if(strcmp(argv[1], "v4") == 0){
	ip_versão = "4";
    }else if(strcmp(argv[1], "v6") == 0){

	ip_versão = "6";
    }
	else{
		exit(EXIT_FAILURE);
	}
    printf("Servidor iniciado em modo IPv%s na porta %s\n", ip_versão, argv[2]);

    while (1) {
        struct sockaddr_storage cstorage;
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
	socklen_t caddrlen = sizeof(cstorage);
        int csock = accept(s, caddr, &caddrlen);
        if (csock == -1) {
            logexit("accept");
        }

        char caddrstr[BUFSZ];
        addrtostr(caddr, caddrstr, BUFSZ);
        printf("Cliente conectado\n");

	HackerMessage msg;
	memset(&msg, 0, sizeof(HackerMessage));
	msg.type = MSG_START;
	msg.win_status = 0;
	ssize_t bytes_sent = send(csock, &msg, sizeof(HackerMessage), 0);
	if(bytes_sent!= sizeof(HackerMessage)){logexit("send start");}


	int tentativas = 0;
ssize_t bytes_received;

while (1) {
    bytes_received = recv(csock, &msg, sizeof(msg), 0);
    if (bytes_received <= 0) {
        break;
    }

    if (msg.type == MSG_GUESS) {
        int valido = 1;
        for (int i = 0; i <= 4; i++) {
            if (msg.guess[i] < 0 || msg.guess[i] > 9) {
                valido = 0;
                break;
            }
        }

        if (valido == 0) {
            msg.type = MSG_ERROR;
            msg.win_status = -1;
            send(csock, &msg, sizeof(msg), 0);
            continue;
        }

        tentativas++;
        msg.attempts = tentativas;

        process_guide(senha_secreta, msg.guess, msg.feedback, &msg.win_status);

        if (msg.win_status == 1) {
            msg.type = MSG_WIN;
            send(csock, &msg, sizeof(msg), 0);
            break;
        } else {
            msg.type = MSG_FEEDBACK;
            send(csock, &msg, sizeof(msg), 0);
        }
    }
}

close(csock);
printf("Cliente desconectado\n");
}
}
