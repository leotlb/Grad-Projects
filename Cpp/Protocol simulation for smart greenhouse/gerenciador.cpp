#include <iostream>
#include <winsock2.h>       // WSADATA, sockaddr_in, WSAStartup(), WSACleanup(), connect(), send(), htons()
#include <map>              // map
#include <mutex>            // mutex, lock()
#include <atomic>           // atomic
#include <thread>           // thread
#include "protocolo.h"
#include "utilitarios.h"


#define PORT 5000

// Estrutura de Limites para variáveis alvo
struct EnviromentLimits {
    bool configured = false;                            // Começa bloqueado para evitar sensores ativos sem configuração
    float min_value = 0.0f;
    float max_value = 0.0f;
    float hysteresis = 0.0f;
    SOCKET increase_actuator_socket = INVALID_SOCKET;   // Socket do Atuador que opera para aumentar essa variável
    SOCKET decrease_actuator_socket = INVALID_SOCKET;   // Socket do Atuador que opera para diminuir essa variável
    uint8_t increase_state = 0x00;                      // Estado atual do Atuador que opera para aumentar essa variável
    uint8_t decrease_state = 0x00;                      // Estado atual do Atuador que opera para diminuir essa variável
};

std::atomic<uint16_t> id_generator(100);            // IDs dos dispositivos começarão em 100
std::map<uint16_t, float> sensors_db;               // Mapa Sensor -> Leitura
std::map<uint16_t, uint8_t> device_target;          // Mapa Sensor/Atuador -> Variável Alvo
std::map<uint8_t, EnviromentLimits> global_limits;  // Mapa Variável Alvo -> Limites Configurados
std::mutex mtx_control;                             // Protege os mapas contra conflitos de Threads

// Funcão de tratamento das conexões generalista
// Monitora mensagens, executa a lógica de tratamento e envia respostas
void clientHandler(SOCKET ClientSocket) {

    std::cout << "Escutando novo dispositivo" << std::endl;

    uint16_t my_id = 0;
    uint8_t my_target = 0x00;

    // Loop principal de operação
    while (true) {
        
        // Leitura do header
        ProtocolHeader header;
        if (!readExact(ClientSocket, (char*)&header, sizeof(header))) {
            std::cout << "[GERENCIADOR] Erro: Dispositivo desconectado ou falha de rede" << std::endl;
            break;  // Crítico -> sai do loop e fecha o socket
        }

        // Validação
        if (header.magic_number != MAGIC_NUMBER) {
            std::cerr << "[GERENCIADOR] Erro: Pacote ignorado - Magic Number invalido" << std::endl;
            continue;   // Não Crítico -> continua tentando
        }
        if (header.msg_type == 0x00) {
            std::cerr << "[GERENCIADOR] Erro: Pacote ignorado - MSG_TYPE reservado/invalido" << std::endl;
            continue;   // Não Crítico -> continua tentando
        }

        // Leitura do payload
        char* payload_buffer = nullptr;
        if (header.payload_len > 0) {
            payload_buffer = new char[header.payload_len];
            if (!readExact(ClientSocket, payload_buffer, header.payload_len)) {
                std::cerr << "[GERENCIADOR] Erro: corpo da mensagem invalido" << std::endl;
                delete[] payload_buffer;
                break;
            }
        }

    
        std::cout << "\n[GERENCIADOR] Nova mensagem recebida" << std::endl;
        std::cout << "Tipo MSG : 0x0" << (int)header.msg_type << std::endl;
        std::cout << "ID Origem: " << header.device_id << std::endl;

        // Recebe IDENTIFY (0x01) - Sensor/Atuador se identificando
        if (header.msg_type == 0x01) {
            IdentifyPayload* id_payload = (IdentifyPayload*)payload_buffer;
            
            if (id_payload->target_variable < 0x01 || id_payload->target_variable > 0x03) {
                std::cerr << "[GERENCIADOR] IDENTIFY incorreto. Variavel alvo inexistente" << std::endl;
                break;
            }

            // Checa se variavel alvo do Sensor/Atuador se identificando está configurada
            mtx_control.lock();
            bool temp_config = global_limits[id_payload->target_variable].configured;
            mtx_control.unlock();

            if (!temp_config) {
                std::cerr << "[GERENCIADOR] IDENTIFY rejeitado. A Variavel " << (int)id_payload->target_variable << " ainda nao foi configurada" << std::endl;
                break;
            }

            my_target = id_payload->target_variable;
            my_id = id_generator++;

            std::cout << "[GERENCIADOR] Novo dispositivo inserido" << std::endl;
            std::cout << "Classe: " << (int)id_payload->device_class << std::endl;
            std::cout << "ID Atribuido: " << (int)my_id << std::endl;

            // Caso atuador
            std::lock_guard<std::mutex> lock(mtx_control);
            if (id_payload->device_class == 0x01) { 
                device_target[my_id] = my_target;
            } else if (id_payload->device_class == 0x02) {
                // Avalia o modo de ação do Atuador (0x01 = Aumenta, 0x02 = Diminui)
                if (id_payload->action_mode == 0x01) {
                    global_limits[my_target].increase_actuator_socket = ClientSocket;
                    std::cout << "[GERENCIADOR] Atuador de AUMENTO registrado para Variavel: " << my_target << std::endl;
                } else if (id_payload->action_mode == 0x02) {
                    global_limits[my_target].decrease_actuator_socket = ClientSocket;
                    std::cout << "[GERENCIADOR] Atuador de REDUCAO registrado para Variavel: " << my_target << std::endl;
                } else {
                    std::cerr << "[GERENCIADOR] Erro: Modo de acao do Atuador desconhecido (" << (int)id_payload->action_mode << ")." << std::endl;
                }
            }

            // Envia IDENTIFY_ACK (0x02)
            // Informa o ID designado no cabeçalho
            ProtocolHeader ack_header;
            ack_header.magic_number = MAGIC_NUMBER;
            ack_header.msg_type = 0x02;      // IDENTIFY_ACK
            ack_header.device_class = 0x00;  // Gerenciador
            ack_header.device_id = my_id;    // ID designado
            ack_header.payload_len = 0;

            send(ClientSocket, (char*)&ack_header, sizeof(ack_header), 0);
            std::cout << "Enviou ACK " << std::endl;
        }

        // Recebe SENSOR_DATA (0x03) - Sensor enviando dado
        else if (header.msg_type == 0x03) {
            SensorDataPayload* data_payload = (SensorDataPayload*)payload_buffer;
            
            // O sensor agora usa o ID oficial (header.device_id) que recebeu no ACK
            std::cout << "[GERENCIADOR] Sensor " << header.device_id << " Leitura: " << data_payload->value << std::endl;

            // Salva no "banco de dados" e grava uma copia temporaria dos limites da variavel atualizada
            // A cópia é feita para evitar ficar acessando variável compartilhada
            mtx_control.lock();
            sensors_db[header.device_id] = data_payload->value;
            uint8_t var_medida = device_target[header.device_id];
            EnviromentLimits temp_limits = global_limits[var_medida];
            mtx_control.unlock();

            // Tratamento da saída da variavel dos limites
            
            // Caso: Atuador de redução
            if (temp_limits.decrease_actuator_socket != INVALID_SOCKET) {
                uint8_t new_state = temp_limits.decrease_state; // Por padrão, mantém como está

                if (data_payload->value >= temp_limits.max_value) {
                    new_state = 0x01; // ON
                }
                else if (data_payload->value <= (temp_limits.max_value - temp_limits.hysteresis)) {
                    new_state = 0x00; // OFF
                }

                // Trigger o envio de ACTUATOR_CMD (0x04) - Ordem de ativação para o Atuador
                // Feito para evitar constantes mensagens ACTUATOR_CMD enquanto variável se mantém fora dos limites
                if (new_state != temp_limits.decrease_state) {
                    ProtocolHeader cmd_header;
                    cmd_header.magic_number = MAGIC_NUMBER;
                    cmd_header.msg_type = 0x04;                             // ACTUATOR_CMD
                    cmd_header.device_class = 0x00;                         // Gerenciador
                    cmd_header.device_id = my_id;                           // Irrelevante
                    cmd_header.payload_len = sizeof(ActuatorCmdPayload);

                    ActuatorCmdPayload cmd_payload;
                    cmd_payload.state = new_state;

                    send(temp_limits.decrease_actuator_socket, (char*)&cmd_header, sizeof(cmd_header), 0);
                    send(temp_limits.decrease_actuator_socket, (char*)&cmd_payload, sizeof(cmd_payload), 0);

                    // Atualiza o estado da variável compartilhada
                    mtx_control.lock();
                    global_limits[my_target].decrease_state = new_state;
                    mtx_control.unlock();

                    std::cout << "[GERENCIADOR] Enviado comando: 0x0" << (int)header.msg_type << " para o Atuador " << cmd_header.device_id << std::endl;
                }
            }

            // Caso: Atuador de aumento
            if (temp_limits.increase_actuator_socket != INVALID_SOCKET) {
                uint8_t new_state = temp_limits.increase_state;

                if (data_payload->value <= temp_limits.min_value) {
                    new_state = 0x01;   // ON
                } 
                else if (data_payload->value >= (temp_limits.min_value + temp_limits.hysteresis)) {
                    new_state = 0x00;   // OFF
                }

                // Trigger o envio de ACTUATOR_CMD (0x04) - Ordem de ativação para o Atuador
                // Feito para evitar constantes mensagens ACTUATOR_CMD enquanto variável se mantém fora dos limites
                if (new_state != temp_limits.increase_state) {
                    ProtocolHeader cmd_header;
                    cmd_header.magic_number = MAGIC_NUMBER;
                    cmd_header.msg_type = 0x04;                             // ACTUATOR_CMD
                    cmd_header.device_class = 0x00;                         // Gerenciador
                    cmd_header.device_id = my_id;                           // Irrelevante
                    cmd_header.payload_len = sizeof(ActuatorCmdPayload);

                    ActuatorCmdPayload cmd_payload;
                    cmd_payload.state = new_state;

                    send(temp_limits.increase_actuator_socket, (char*)&cmd_header, sizeof(cmd_header), 0);
                    send(temp_limits.increase_actuator_socket, (char*)&cmd_payload, sizeof(cmd_payload), 0);

                    // Atualiza o estado da variável compartilhada
                    mtx_control.lock();
                    global_limits[my_target].increase_state = new_state;
                    mtx_control.unlock();

                    std::cout << "[GERENCIADOR] Enviado comando: 0x0" << (int)header.msg_type << " para o Atuador " << cmd_header.device_id << std::endl;
                }
            }

        }

        // Recebe CLIENT_SET_CONFIG (0x05) - Cliente configura limites
        else if (header.msg_type == 0x05) {
            ClientSetConfigPayload* config_payload = (ClientSetConfigPayload*)payload_buffer;
            
            std::cout << "\n[GERENCIADOR] Cliente " << header.device_id << " Configurou limites da estufa:" << std::endl;
            std::cout << "Var Alvo: " << (int)config_payload->target_variable << std::endl;
            std::cout << "Max: " << config_payload->max_value << " | Min: " << config_payload->min_value << " | Hist: " << config_payload->hysteresis << "\n" << std::endl;
            
            std::lock_guard<std::mutex> lock(mtx_control);
            global_limits[config_payload->target_variable].min_value = config_payload->min_value;
            global_limits[config_payload->target_variable].max_value = config_payload->max_value;
            global_limits[config_payload->target_variable].hysteresis = config_payload->hysteresis;

            // Libera o cadastro de Sensore/Atuadores
            global_limits[config_payload->target_variable].configured = true;

            std::cout << "[GERENCIADOR] Limites salvos e cadastro de dispositivos relacionados liberado" << std::endl;
        }

        // Recebe CLIENT_REQ_DATA (0x06) -  Cliente requista leitura de Sensor
        // Envia CLIENT_RES_DATA (0x07) -  Gerenciador envia dado do Sensor
        else if (header.msg_type == 0x06) {
            uint16_t alvo_id = header.device_id;
            
            ProtocolHeader res_header;
            res_header.magic_number = MAGIC_NUMBER;
            res_header.msg_type = 0x07; // CLIENT_RES_DATA
            res_header.device_class = 0x00; 
            res_header.device_id = 0; 
            res_header.payload_len = sizeof(ClientResDataPayload);

            ClientResDataPayload res_payload;
            res_payload.sensor_id = alvo_id;

            // Busca a última leitura travando escrita
            mtx_control.lock();
            if (sensors_db.count(alvo_id) > 0) {
                res_payload.status = 0x00; // OK
                res_payload.value = sensors_db[alvo_id];
            } else {
                res_payload.status = 0x01; // Erro - Offline 
                res_payload.value = 0.0f;
            }
            mtx_control.unlock();

            send(ClientSocket, (char*)&res_header, sizeof(res_header), 0);
            send(ClientSocket, (char*)&res_payload, sizeof(res_payload), 0);
        }

        // Libera memoria do ciclo atual do loop
        if (payload_buffer != nullptr) {
            delete[] payload_buffer;
        }
    }

    closesocket(ClientSocket);
}

int main() {

    // Inicialização
    // Chama a API do Winsock, passando uma struct recipiente e especifica versão com o MAKEWORD(2, 2)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Falha ao iniciar o Winsock" << std::endl;
        return 1;
    }


    // Incialização do Socket
    // ListenSocket é o handle do socket que especifica seu funcionamento
    // AF_INET (2) especifica o uso de IPv4
    // SOCK_STREAM (1) especifica o tipo de comunicação como TCP (SOCK_DGRAM para UDP)
    // IPPROTO_TCP (6) define de fato o protocolo como TCP (poderia ser 0 também pois IPPROTO_TCP é o default quando SOCK_STREAM é usado)
    SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        std::cerr << "Falha na criacao do socket" << std::endl;
        WSACleanup();   // Limpa a memória do Winsock
        return 1;
    }


    // Configuração de Porta e Endereço
    // sockaddr_in guarda as características de endereços IPv4 (familia, porta, endereço)
    sockaddr_in address;
    address.sin_family = AF_INET;

    // s_addr é um long embutido in_addr embutida em sockaddr_in, que define o IP destino dos pacotes que devem ser aceitos
    // INADDR_ANY (eq 0.0.0.0) implica que no bind conseguinte o socket pacotes destinados a qualquer placa de rede do computador
    address.sin_addr.s_addr = INADDR_ANY;   // Aceita conexões de qualquer IP (localhost ou Wi-Fi)
    
    // Host TO Network Short é necessário toda vez que há passagem de porta ou IP para conversão de Endianess
    // Maioria dos protocolos de rede usam Big-Endian e x86_64 usa Litte-Endian
    address.sin_port = htons(PORT); 

    // Atrela o socket aberto à porta 5000
    if (bind(ListenSocket, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Falha no bind. A porta pode ja estar em uso" << std::endl;
        closesocket(ListenSocket); // Libera o socket
        WSACleanup();
        return 1;
    }

    // Escuta
    // Configura a escuta do socket limitando a 10 dispositivos no backlog (tentando se conectar ao mesmo tempo)
    if (listen(ListenSocket, 10) == SOCKET_ERROR) {
        std::cerr << "Falha no listen" << std::endl;
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    std::cout << "Gerenciador da Estufa Iniciado" << std::endl;
    std::cout << "[GERENCIADOR] Escutando na porta " << PORT << std::endl;


    // Loop principal de operação
    while (true) {
        sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        // Trava até que alguém se conecte
        SOCKET ClientSocket = accept(ListenSocket, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (ClientSocket == INVALID_SOCKET) {
            std::cerr << "[GERENCIADOR] Falha ao aceitar conexao de um cliente" << std::endl;
            continue;
        }

        std::cout << "[GERENCIADOR] Novo dispositivo conectado" << std::endl;

        std::thread(clientHandler, ClientSocket).detach();
    }

    closesocket(ListenSocket);
    WSACleanup();
    return 0;
}