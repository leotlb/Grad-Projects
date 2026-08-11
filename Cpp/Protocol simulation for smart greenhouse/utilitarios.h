#include <iostream>
#include <winsock2.h>

// Verifica validade do conexão e lê a quantiade exata de dados
inline bool readExact(SOCKET hSocket, char* buffer, int tam_esperado) {
    int total_recebido = 0;
    while (total_recebido < tam_esperado) {

        // recv recupera os dados de um socket conectado
        int res = recv(hSocket, buffer + total_recebido, tam_esperado - total_recebido, 0);
        
        if (res == SOCKET_ERROR || res == 0) {
            return false;   // Perda de conexão ou o cliente fechou
        }
        total_recebido += res;
    }
    return true;
}

// Simula valores para leitura dos sensores
inline float simData(int my_target, bool initial){

    if(initial){
        switch(my_target){
            case(1):
                return 22.0f;   // Temperatura inicial
            case(2):
                return 50.0f;   // % Humidade inicial
            case(3):
                return 900.0f;  // Concentração de CO2 inicial
            default:
                break;
        }
    }
    else {
        switch(my_target){
            case(1):
                return 0.5f;    // Variação de tempetura
            case(2):
                return 5.0f;    // Variação de % de humidade
            case(3):
                return 50.0f;   // Variação de concentração de CO2
            default:
                break;
        }
    }
    return 0.0f;
}