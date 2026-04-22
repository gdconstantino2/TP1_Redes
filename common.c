#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // Necessário para a função memcpy()
#include <arpa/inet.h>

void logexit(const char *msg){
	perror(msg);
	exit(EXIT_FAILURE);}

int addrparse(const char *addrstr, const char *portstr,
              struct sockaddr_storage *storage) {

    if (addrstr == NULL || portstr == NULL) {
        return -1;
    }

    uint16_t port = (uint16_t)atoi(portstr); // unsigned short

    if (port == 0) {
        return -1;
    }
    port = htons(port); // host to network short

    struct in_addr inaddr4; // 32-bit IP address
    if (inet_pton(AF_INET, addrstr, &inaddr4)) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_port = port;
        addr4->sin_addr = inaddr4;
        return 0;
    }

    struct in6_addr inaddr6; // 128-bit IPv6 address
    if (inet_pton(AF_INET6, addrstr, &inaddr6)) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = port;
        // addr6->sin6_addr = inaddr6;
        memcpy(&(addr6->sin6_addr), &inaddr6, sizeof(inaddr6));
        return 0;
    }

    return -1;
}

void addrtostr(const struct sockaddr *addr, char *str, size_t strsize) {
    int version;
    char addrstr[INET6_ADDRSTRLEN + 1] = "";
    uint16_t port;

    // CORREÇÃO LÓGICA: '=' trocado por '=='
    if (addr->sa_family == AF_INET) {
        version = 4;
        struct sockaddr_in *addr4 = (struct sockaddr_in *)addr;
        if (!inet_ntop(AF_INET, &(addr4->sin_addr), addrstr, INET6_ADDRSTRLEN + 1)) {
            logexit("ntop");
        }
        port = ntohs(addr4->sin_port); // network to host short

    } else if (addr->sa_family == AF_INET6) {
        version = 6;

        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)addr;

        if (!inet_ntop(AF_INET6, &(addr6->sin6_addr), addrstr, INET6_ADDRSTRLEN + 1)) {
            perror("ntop");
            exit(EXIT_FAILURE);
        }
        port = ntohs(addr6->sin6_port); // network to host short
    } else {
        fprintf(stderr, "unknown protocol family.\n");
        exit(EXIT_FAILURE);
    }
    if (str) {
        snprintf(str, strsize, "IPv%d %s %hu", version, addrstr, port);
    }
}
int server_sockaddr_init(const char *proto, const char *portstr,
                         struct sockaddr_storage *storage) {

    uint16_t port = (uint16_t)atoi(portstr); // unsigned short
    if (port == 0) {
        return -1;
    }

    port = htons(port); // host to network short

    memset(storage, 0, sizeof(*storage));

    if (0 == strcmp(proto, "v4")) {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)storage;
        addr4->sin_family = AF_INET;
        addr4->sin_addr.s_addr = INADDR_ANY;
        addr4->sin_port = port;
        return 0;
    } else if (0 == strcmp(proto, "v6")) {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)storage;
        addr6->sin6_family = AF_INET6;
        addr6->sin6_addr = in6addr_any;
        addr6->sin6_port = port;
        return 0;
    } else {
        return -1;
    }
}
void process_guide(int *senha, int *palpite, int *feedback, int *vitoria){
	int contador_vitoria = 0;
	for(int h = 0; h <=4; h++){
		feedback[h] = 0;
	}
	int senha_gasta[5] = {0};
	for(int i = 0; i <= 4; i++){ //ACERTOS EXATOS
		if(palpite[i] == senha[i]){
			feedback[i] = 2;
			senha_gasta[i] = 1;
		}
	}
	for(int j = 0; j<= 4; j++){
		if(feedback[j] == 2){}
		else{
			for(int k = 0; k <= 4; k++){
				if(palpite[j] == senha[k] && senha_gasta[k] == 0){
					feedback[j] = 1;
					senha_gasta[k] = 1;
					break;
				}
			}
		}
	}
	for(int g = 0; g <= 4; g++){
		if(feedback[g] == 2){
			contador_vitoria+=1;
		}
	}
	if(contador_vitoria == 5){
		*vitoria = 1;
	}
	else{
		*vitoria = 0;
	}
}
