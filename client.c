#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#define BUFSZ 1024


void usage(int argc, char **argv){
        printf("Usage: %s <server IP> <server port>\n", argv[0]);
        printf("Example: %s 127.0.0.1 51511\n", argv[0]);
        exit(EXIT_FAILURE);
}

int main(int argc, char **argv){
	if (argc < 3) { //<IP> <porta>
                usage(argc, argv);
        }

        int s;
        s = socket(AF_INET, SOCK_STREAM, 0);
        if(s == -1){
                logexit("socket");
        }
	struct sockaddr_storage storage;
	if( 0 != addrparse(argv[1], argv[2], &storage)){
		usage(argc, argv);}
	struct sockaddr *addr = (struct sockaddr *)(&storage);

        if (0 != connect(s, addr, sizeof(storage))){
                logexit("connect");
        }
	//Mensagem do servidor
	HackerMessage msg;
	ssize_t bytes = recv(s, &msg, sizeof(msg), 0);
	printf("Erro no protocolo: esperado MSG_START.\n");
	close(s);
	exit(EXIT_FAILURE);}
	printf("Cliente Conectado!\n");


	char input[BUFSZ];
	int palpite[5];
	int tentativas = 0;

        while (1) {
        	printf("Insira seu palpite:\n> ");
        	fflush(stdout);

        	if (fgets(input, BUFSZ, stdin) == NULL) {
            	break;
        	}
        	input[strcspn(input, "\n")] = '\0';
        	if (strlen(input) != 5) {
            		printf("Insira uma sequência válida!\n");
            		continue;
		}

        	int valido = 1;
        	for (int i = 0; i < 5; i++) {
            		if (!isdigit(input[i])) {
                		printf("Insira uma sequência válida!\n");
                		valido = 0;
                		break;
            		}
            	palpite[i] = input[i] - '0';
        	}

        	if (!valido) {
            		continue;
        	}
        	memset(&msg, 0, sizeof(msg));
        	msg.type = MSG_GUESS;
        	memcpy(msg.guess, guess, sizeof(guess));
        	if (send(s, &msg, sizeof(msg), 0) != sizeof(msg)) {
            	logexit("send guess");
        	}

        	// Recebe resposta do servidor
        	bytes = recv(s, &msg, sizeof(msg), 0);
        	if (bytes <= 0) {
            		printf("Conexão encerrada pelo servidor.\n");
            		break;
        	}

        	// Processa a resposta
        	if (msg.type == MSG_WIN) {
            		printf("Acesso concedido! Thaísa recuperou o sistema!\n");
            		break;
        	} else if (msg.type == MSG_ERROR) {
            		printf("Insira uma sequência válida!\n");
        	} else if (msg.type == MSG_FEEDBACK) {
            		tentativas = msg.attempts;

            // Monta string da dica conforme especificação
            char dica[6];
            for (int i = 0; i < 5; i++) {
                if (msg.feedback[i] == 2) {
                    dica[i] = '0' + msg.guess[i];   // dígito correto
                } else if (msg.feedback[i] == 1) {
                    dica[i] = '*';                  // presente mas posição errada
                } else {
                    dica[i] = '-';                  // ausente
                }
            }
            dica[5] = '\0';

            printf("Dica: %s\n", dica);
            printf("Tentativas realizadas: %d\n", attempts);
        }
    }

    close(s);
    return 0;
}
