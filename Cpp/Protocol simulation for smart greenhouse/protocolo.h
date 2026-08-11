#include <iostream>
#include <cstdint> // uint8_t, uint16_t

// Identificador do Protocolo
const uint16_t MAGIC_NUMBER = 0x4553;

// Força alinhamento de 1 byte, 
// Importante especialmente para IdentifyPayload e ClientSetConfigPayload para evitar padding no meio do payload
#pragma pack(push, 1)

struct ProtocolHeader {
    uint16_t magic_number; // Estufa: 0x4553
    uint8_t  msg_type;     // Tipo: 0x01 a 0x07
    uint8_t  device_class; // Classe: 0x00=Gerenciador, 0x01=Sensor, 0x02=Atuador, 0x03=Cliente
    uint16_t device_id;    // Identificador: 0x0000 a 0xFFFF
    uint16_t payload_len;  // Tamanho do payload
};

// Payloads
// 0x02 (IDENTIFY_ACK) e 0x07 (CLIENT_REQ_DATA) não tem payload pois o header é suficiente

// MSG_TYPE = 0x01
struct IdentifyPayload {
    uint8_t device_class;    // 0x01=Sensor, 0x02=Atuador
    uint8_t target_variable; // 0x01=Temp, 0x02=Umid, 0x03=CO2
    uint8_t action_mode;     // 0x01 = Aumenta a grandeza, 0x02 = Diminui a grandeza (Adicão divergente da parte 1 para poder indeficar qual atuador ativar)
};

// MSG_TYPE = 0x03
struct SensorDataPayload {
    float value;    // 4 bytes (IEEE 754)
};

// MSG_TYPE = 0x04
struct ActuatorCmdPayload {
    uint8_t state; // 0x00=Desligar, 0x01=Ligar
};

// MSG_TYPE = 0x05
struct ClientSetConfigPayload {
    uint8_t target_variable;    // 0x01=Temp, 0x02=Umid, 0x03=CO2 
    float min_value;            // 4 bytes (IEEE 754)
    float max_value;            // 4 bytes (IEEE 754)
    float hysteresis;           // A margem de tolerância
};

// MSG_TYPE = 0x07
struct ClientResDataPayload {
    uint16_t sensor_id; // Identificador: 0x0000 a 0xFFFF
    uint16_t status;    // 0x00=OK, 0x01=Offline
    float value;        // 4 bytes (IEEE 754)        
};

// Retorna alinhamento ao default
#pragma pack(pop)