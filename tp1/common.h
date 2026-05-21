#ifndef COMMON_H
#define COMMON_H
#pragma once
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>

#define MSG_SIZE 128

// Definição dos tipos de mensagens do protocolo
typedef enum {
    MSG_START,    // Servidor solicita a senha de acesso
    MSG_GUESS,    // Cliente envia o palpite de 5 dígitos
    MSG_FEEDBACK, // Servidor retorna as dicas (0, 1 ou 2)
    MSG_WIN,      // Servidor informa que o código foi quebrado
    MSG_ERROR,    // Servidor reporta erro de formato/tamanho
    MSG_EXIT      // Encerramento da conexão
} MessageType;

// Estrutura principal de comunicação (Contrato do Protocolo)
typedef struct {
    int type;               // Tipo da mensagem (MessageType)
    int guess[5];           // Vetor com os 5 dígitos enviados pelo cliente
    int feedback[5];        // Dicas retornadas pelo servidor (2, 1 ou 0)
    int attempts;           // Contador de tentativas realizadas
    int win_status;         // 1 para vitória, 0 para em jogo, -1 para erro
    char message[MSG_SIZE]; // Mensagem de texto para logs do terminal
} HackerMessage;

// Protótipos das funções auxiliares que você já possui
void logexit(const char *msg);
int addrparse(const char *addrstr, const char *portstr, struct sockaddr_storage *storage);
void addrtostr(const struct sockaddr *addr, char *str, size_t strsize);
int server_sockaddr_init(const char *proto, const char *portstr, struct sockaddr_storage *storage);
void process_guide(int *senha, int *palpite, int *feedback, int *vitoria);
#endif

