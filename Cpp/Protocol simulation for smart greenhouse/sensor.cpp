#include <iostream>
#include <winsock2.h>       // WSADATA, sockaddr_in, WSAStartup(), WSACleanup(), connect(), send(), htons()
#include <ws2tcpip.h>       // inet_pton()
#include <thread>           // thread
#include <chrono>           // chrono
#include "protocolo.h"
#include "utilitarios.h"

#define SERVER_IP "127.0.0.1" // Localhost
#define PORT 5000

int main() {
    
    // Rito de incialização analogo ao gerenciador
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Falha ao iniciar o Winsock" << std::endl;
        return 1;
    }

    SOCKET SensorSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (SensorSocket == INVALID_SOCKET) {
        std::cerr << "Falha na criacao do socket" << std::endl;
        WSACleanup();
        return 1;
    }
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    //inet_pton converte um endereço IP para binário Big-Endian de acordo com o protocolo especificado
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);   


    // Estabelece conexão com o gerenciador
    std::cout << "Conectando ao Gerenciador..." << std::endl;
    if (connect(SensorSocket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Falha ao conectar. O Gerenciador esta rodando?" << std::endl;
        closesocket(SensorSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "Conectado com sucesso!" << std::endl;


    // Envia IDENTIFY (0x01) - Sensor se identificando
    uint16_t my_id = 0;
    int my_target;

    std::cout << "==== CONFIGURACAO DO SENSOR ====" << std::endl;
    std::cout << "Selecione o que medir: (1-Temp, 2-Umid, 3-CO2):" << std::endl;
    std::cin >> my_target;

    ProtocolHeader id_header;
    id_header.magic_number = MAGIC_NUMBER;
    id_header.msg_type = 0x01;                          // IDENTIFY
    id_header.device_class = 0x01;                      // Sensor
    id_header.device_id = my_id;
    id_header.payload_len = sizeof(IdentifyPayload);

    IdentifyPayload id_payload;
    id_payload.device_class = 0x01;                     // Sensor
    id_payload.target_variable = (uint8_t)my_target;
    id_payload.action_mode = 0x00;                      // Irrelevante para sensores


    send(SensorSocket, (char*)&id_header, sizeof(id_header), 0);
    send(SensorSocket, (char*)&id_payload, sizeof(id_payload), 0);
    std::cout << "[SENSOR] Mensagem IDENTIFY enviada" << std::endl;

    // Trava em loop esperando IDENTIFY_ACK (0x02) - Confirmação do gerenciador
    ProtocolHeader ack_header;
    if (!readExact(SensorSocket, (char*)&ack_header, sizeof(ack_header)) || ack_header.msg_type != 0x02) {
        std::cerr << "[SENSOR] Falha ao receber o IDENTIFY_ACK do Gerenciador" << std::endl;
        closesocket(SensorSocket);
        return 1;
    }

    my_id = ack_header.device_id;
    std::cout << "[SENSOR] O Gerenciador me atribuiu o ID: " << my_id << "\n" << std::endl;

    // Loop de envio de dados
    float data_SIM = simData(my_target,1);

    while (my_id) {
        ProtocolHeader data_header;
        data_header.magic_number = MAGIC_NUMBER;
        data_header.msg_type = 0x03; // SENSOR_DATA
        data_header.device_class = 0x01;
        data_header.device_id = my_id;
        data_header.payload_len = sizeof(SensorDataPayload);

        SensorDataPayload data_payload;
        data_payload.value = data_SIM;

        // Envia o pacote
        send(SensorSocket, (char*)&data_header, sizeof(data_header), 0);
        send(SensorSocket, (char*)&data_payload, sizeof(data_payload), 0);

        std::cout << "Dado enviado: " << data_SIM << std::endl;

        // Altera levemente a temperatura para o log não ficar monótono
        data_SIM += simData(my_target, 0); 

        // Dorme a thread por exato 1 segundo para cumprir o requisito
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    closesocket(SensorSocket);
    WSACleanup();
    return 0;
}