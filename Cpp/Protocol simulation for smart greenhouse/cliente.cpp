#include <iostream>
#include <winsock2.h>       // WSADATA, sockaddr_in, WSAStartup(), WSACleanup(), connect(), send(), htons()
#include <ws2tcpip.h>       // inet_pton()
#include "protocolo.h"
#include "utilitarios.h"

#define SERVER_IP "127.0.0.1"
#define PORT 5000
#define CLIENT_ID 999

int main() {

    // Rito de incialização analogo ao gerenciador
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Falha ao iniciar o Winsock" << std::endl;
        return 1;
    }

    SOCKET ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ClientSocket == INVALID_SOCKET) {
        std::cerr << "Falha na criacao do socket" << std::endl;
        WSACleanup();
        return 1;
    }
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    //inet_pton converte um endereço IP para binário Big-Endian de acordo com o protocolo especificado
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    std::cout << "[CLIENTE] Conectando ao Gerenciador..." << std::endl;
    if (connect(ClientSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "[CLIENTE] Falha ao conectar. O Gerenciador esta rodando?" << std::endl;
        return 1;
    }


    bool is_running = true;
    while (is_running) {
        int opcao;
        std::cout << "\n=== MENU CLIENTE ===" << std::endl;
        std::cout << "1 - Enviar Nova Configuracao (CLIENT_SET_CONFIG - 0x05)" << std::endl;
        std::cout << "2 - Requisitar Dado Atual (CLIENT_REQ_DATA - 0x06)" << std::endl;
        std::cout << "3 - Encerrar Cliente (Sair)" << std::endl;
        std::cout << "Escolha uma opcao: ";
        std::cin >> opcao;

        // Envia CLIENT_SET_CONFIG (0x05) - Cliente configura limites
        if (opcao == 1) {

            // Configuração (0x05)
            int my_target;
            float my_min, my_max, my_hist;
            
            std::cout << "Variavel (1-Temp, 2-Umid, 3-CO2): "; std::cin >> my_target;
            std::cout << "Minimo: "; std::cin >> my_min;
            std::cout << "Maximo: "; std::cin >> my_max;
            std::cout << "Histerese: "; std::cin >> my_hist;

            ProtocolHeader config_header;
            config_header.magic_number = MAGIC_NUMBER;
            config_header.msg_type = 0x05; 
            config_header.device_class = 0x03; // Cliente
            config_header.device_id = CLIENT_ID;
            config_header.payload_len = sizeof(ClientSetConfigPayload);

            ClientSetConfigPayload config_payload;
            config_payload.target_variable = (uint8_t)my_target;
            config_payload.min_value = my_min;
            config_payload.max_value = my_max;
            config_payload.hysteresis = my_hist;

            send(ClientSocket, (char*)&config_header, sizeof(config_header), 0);
            send(ClientSocket, (char*)&config_payload, sizeof(config_payload), 0);
            std::cout << "[CLIENTE] Configuracao enviada com sucesso!" << std::endl;
        }

        // Envia CLIENT_REQ_DATA (0x06) - Cliente requisita dado de sensor
        else if (opcao == 2) {
            uint16_t id_sensor_alvo;
            std::cout << "[CLIENTE] Digite o ID do sensor que deseja consultar: ";
            std::cin >> id_sensor_alvo;

            ProtocolHeader req_header;
            req_header.magic_number = MAGIC_NUMBER;
            req_header.msg_type = 0x06;             // CLIENT_REQ_DATA
            req_header.device_class = 0x03;
            req_header.device_id = id_sensor_alvo;  // ID do sensor vai no cabeçalho
            req_header.payload_len = 0;

            send(ClientSocket, (char*)&req_header, sizeof(req_header), 0);
            std::cout << "[CLIENTE] Requisicao enviada. Aguardando resposta do Gerenciador..." << std::endl;

            // Trava em loop esperando CLIENT_RES_DATA (0x07) - Gerenciador envia dado do Sensor
            ProtocolHeader res_header;
            if (readExact(ClientSocket, (char*)&res_header, sizeof(res_header))) {
                if (res_header.magic_number == MAGIC_NUMBER && res_header.msg_type == 0x07) {
                    
                    // Aloca e lê o payload de resposta
                    ClientResDataPayload res_payload;
                    if (readExact(ClientSocket, (char*)&res_payload, sizeof(res_payload))) {
                        
                        std::cout << "\n[CLIENTE] Dado recebido do gerenciador:" << std::endl;
                        std::cout << "Sensor ID: " << res_payload.sensor_id << std::endl;
                        
                        if (res_payload.status == 0x00) {
                            std::cout << "Status: ONLINE" << std::endl;
                            std::cout << "Valor: " << res_payload.value << std::endl;
                        } else {
                            std::cout << "Status   : OFFLINE (Dado desatualizado)" << std::endl;
                            std::cout << "Ultimo   : " << res_payload.value << std::endl;
                        }
                    }
                } else {
                    std::cerr << "Recebeu um pacote inesperado da rede" << std::endl;
                }
            } else {
                std::cerr << "Erro de conexao ao tentar ler a resposta" << std::endl;
                is_running = false;
            }
        }
        else if (opcao == 3) {
            std::cout << "Encerrando o cliente..." << std::endl;
            is_running = false;
        }
        else {
            std::cout << "[-] Opcao invalida." << std::endl;
        }
    }

    closesocket(ClientSocket);
    WSACleanup();
    
    return 0;
}