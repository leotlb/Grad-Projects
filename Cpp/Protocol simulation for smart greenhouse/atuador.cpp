#include <iostream>
#include <winsock2.h>       // WSADATA, sockaddr_in, WSAStartup(), WSACleanup(), connect(), send(), htons()
#include <ws2tcpip.h>       // inet_pton()
#include "protocolo.h"
#include "utilitarios.h"

#define SERVER_IP "127.0.0.1"
#define PORT 5000

int main() {

    // Rito de incialização analogo ao gerenciador
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Falha ao iniciar o Winsock" << std::endl;
        return 1;
    }

    SOCKET ActuatorSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ActuatorSocket == INVALID_SOCKET) {
        std::cerr << "Falha na criacao do socket" << std::endl;
        WSACleanup();
        return 1;
    }
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    //inet_pton converte um endereço IP para binário Big-Endian de acordo com o protocolo especificado
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr); 

    std::cout << "[ATUADOR] Conectando ao Gerenciador..." << std::endl;
    if (connect(ActuatorSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR){
        std::cerr << "[ATUADOR] Falha ao conectar. O Gerenciador esta rodando?" << std::endl;
        closesocket(ActuatorSocket);
        WSACleanup();
        return 1;
    }

    uint16_t my_id = 0;
    int my_target,my_action;

    std::cout << "==== CONFIGURACAO DO ATUADOR ====" << std::endl;
    std::cout << "Em qual variavel devo agir? (1-Temp, 2-Umid, 3-CO2): " << std::endl;
    std::cin >> my_target;
    std::cout << "Eu aumento ou diminuo a variavel? (1-Aumento, 2-Diminuo): " << std::endl;
    std::cin >> my_action;

    

    // Envia IDENTIFY (0x01) - Atuador se identificando
    ProtocolHeader id_header;
    id_header.magic_number = MAGIC_NUMBER;
    id_header.msg_type = 0x01;      // IDENTIFY
    id_header.device_class = 0x02;  // Atuador
    id_header.device_id = 0;        // Zerado
    id_header.payload_len = sizeof(IdentifyPayload);

    IdentifyPayload id_payload;
    id_payload.device_class = 0x02;                     // Atuador
    id_payload.target_variable = (uint8_t)my_target;
    id_payload.action_mode = (uint8_t)my_action;

    send(ActuatorSocket, (char*)&id_header, sizeof(id_header), 0);
    send(ActuatorSocket, (char*)&id_payload, sizeof(id_payload), 0);
    
    // Trava em loop esperando IDENTIFY_ACK (0x02) - Confirmação do gerenciador
    ProtocolHeader ack_header;
    if (!readExact(ActuatorSocket, (char*)&ack_header, sizeof(ack_header)) || ack_header.msg_type != 0x02) {
        std::cerr << "[ATUADOR] Falha ao receber o IDENTIFY_ACK." << std::endl;
        return 1;
    }
    
    my_id = ack_header.device_id;
    std::cout << "[ATUADOR] O Gerenciador me atribuiu o ID: " << my_id << std::endl;
    std::cout << "[ATUADOR] Aguardando comando\n" << std::endl;

    // Espera pelo recebimento ACTUATOR_CMD (0x04) - Ordem de ativação do gerenciador
    while (true) {
        ProtocolHeader cmd_header;
        
        // Trava em loop esperando ACTUATOR_CMD (0x04)
        if (!readExact(ActuatorSocket, (char*)&cmd_header, sizeof(cmd_header))) {
            std::cout << "[ATUADOR] Conexao com o Gerenciador foi encerrada." << std::endl;
            break;
        }

        if (cmd_header.magic_number == MAGIC_NUMBER && cmd_header.msg_type == 0x04) {
            ActuatorCmdPayload cmd_payload;
            
            if (readExact(ActuatorSocket, (char*)&cmd_payload, sizeof(cmd_payload))) {
                if (cmd_payload.state == 0x01) {
                    std::cout << "[ATUADOR] Ordem recebida - Ligando equipamento (ON)" << std::endl;
                } else if (cmd_payload.state == 0x00) {
                    std::cout << "[ATUADOR] Ordem recebida - Desligando equipamento (OFF)" << std::endl;
                }
            }
        }
    }

    closesocket(ActuatorSocket);
    WSACleanup();
    return 0;
}