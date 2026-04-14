#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#define BUFSZ 1024

// 1. Corrigida a assinatura: agora recebe apenas o nome do programa
void usage(int argc, char **argv){
        printf("Usage: %s <server IP> <server port>\n", argv[0]);
        printf("Example: %s 127.0.0.1 51511\n", argv[0]);
        exit(EXIT_FAILURE);
}


// 2. Alterado de void main para int main (padrão C)
int main(int argc, char **argv){
        if (argc < 3) {
                usage(argc, argv); // 3. Enviando o argumento corretamente
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
                logexit("connect"); // 4. Corrigido de logiexit para logexit
        }

        char addrstr[BUFSZ];
        addrtostr(addr, addrstr, BUFSZ);
        printf("connected to %s\n", addrstr);


        char buf[BUFSZ];
        memset(buf, 0, BUFSZ);
        printf("<mensagem> ");
        fgets(buf, BUFSZ-1, stdin);

        int count = send(s, buf, strlen(buf)+1, 0);
        if (count != strlen(buf)+1){
                logexit("send");
        }

        memset(buf, 0, BUFSZ);
        unsigned total = 0;
        while(1){
                count = recv(s, buf + total, BUFSZ - total, 0);
                if (count == 0){
                        break;
                }
                total += count;
        }

        close(s); // 5. Adicionado o ponto e vírgula ausente

        printf("received %d bytes\n", total);
        puts(buf);

        exit(EXIT_SUCCESS);
} // 6. Removida a chave dupla que existia aqui embaixo
