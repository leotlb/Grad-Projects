// Codigo elaborado por || Coding done by
// Leonardo Pereira

#define PINO_RX 13		// Não utilizada
#define PINO_TX 13
#define PINO_CLK 12    // Pino dedicado ao Clock Síncrono Emissor-Receptor
#define PINO_RTS 11    // Pino Request to Send (Emissor)
#define PINO_CTS 10    // Pino Clear to Send (Receptor)
#define BAUD_RATE 1
#define HALF_BAUD 1000/(2*BAUD_RATE)	// Não utilizada

#include "Temporizador.h"

// Variáveis modificadas dentro da rotina de interrupção (ISR)
volatile int bitCount = 0;
volatile bool clockState = false;
volatile bool transfer = false;
volatile bool parity;

// Messagem a ser enviada
char charMesssage;

// Usa propriedade do XOR determinar se há uma quantidade ímpar ou par de bit na operação (0 par, 1 ímpar)
bool bitParidade(char dta) {
	
	// Metades sucessivas para comparar paridade de 1s em cada parte
	dta = dta ^ (dta >> 4); 
	dta = dta ^ (dta >> 2); 
	dta = dta ^ (dta >> 1); 
  
  // Dado que o trabalho tem paridade ÍMPAR inverte a regra do XOR (1 par, 0 ímpar)
	if ((dta & 1) == 0) {
		return 1;
	} else {
		return 0;
	}
}

ISR(TIMER1_COMPA_vect){
	if (!transfer) return;

	// Inversão continua a cada interrupção para gerar onda quadrada de clock em PINO_CLK
	clockState = !clockState;
	
	// Estado de ação (subida do clock)
	if (clockState) {
		// Implica na leitura pelo receptor
		digitalWrite(PINO_CLK, HIGH);
		bitCount++;
	}
	// Estado de preparação (descida do clock)
	else {
		// Implica no Receptor ignorando PINO_TX
		digitalWrite(PINO_CLK, LOW);

		// Prepara próxima transmissão
		if (bitCount < 8) {
			bool bitToSend = bitRead(charMesssage, bitCount); 
			digitalWrite(PINO_TX, bitToSend);
		} 
		// Prepara transmissão da paridade
		else if (bitCount == 8) {
			digitalWrite(PINO_TX, parity);
		}
    // Prepara transmissão do Stop Bit (nível HIGH)
		else if (bitCount == 9) {
			digitalWrite(PINO_TX, HIGH);
		}
		// Finaliza transmissão
		else {
			paraTemporizador();
			digitalWrite(PINO_RTS, LOW); // Finaliza o handshake
			transfer = false;
		}
	}
}

void setup(){
	// Protege o setup de uma inicialização falha por culpa de interrupções
	noInterrupts();

	// Estabelece a comunicação Arduino-PC a uma taxa 9600 bits por segundo
	Serial.begin(9600);
  
	// Inicialização dos pinos
	pinMode(PINO_TX, OUTPUT);
	pinMode(PINO_CLK, OUTPUT);
	pinMode(PINO_RTS, OUTPUT);
	pinMode(PINO_CTS, INPUT);

	// Estado neutro no padrão RS-232 é HIGH
	digitalWrite(PINO_TX, HIGH); 
	digitalWrite(PINO_CLK, LOW);
	digitalWrite(PINO_RTS, LOW);
  
	// Configura timer
	// BAUD_RATE * 2 para permitir os dois estado da ISR
	configuraTemporizador(BAUD_RATE * 2);
  
	// Remove proteção
	interrupts();
}


void loop ( ) {
	// Checa se há algo no buffer e se não há algo transmitindo
	if (Serial.available() > 0 && !transfer) {
    Serial.println("Digite o character /n")
		charMesssage = Serial.read();

		// Calculo da paridade feito antes para evitar uma chamada de função dentro da ISR
		parity = bitParidade(charMesssage);

		// Inicializa o handshake
		digitalWrite(PINO_RTS, HIGH);
    
		// Aguarda o handshake do Receptor
		while (digitalRead(PINO_CTS) == LOW) {
		
		}

		// Prepara as variáveis para a interrupção
		bitCount = 0;
		clockState = false;
		transfer = true;

    
		// Envia o Start Bit (sempre LOW)
		digitalWrite(PINO_TX, LOW);
		iniciaTemporizador();

		// Escreve o primeiro bit e inicia o timer que disparara as interrupções para comunicação
		digitalWrite(PINO_TX, bitRead(charMesssage, 7));
		iniciaTemporizador();

		// Laço Busy Waiting mas como em teoria o Arduino não está executando nada além disso não teria problema
		// Em uma implementação mais refinada poderia ser usado um sleep que a interrupção acordaria
		while (transfer) {
			
		}

		// Espera o Receptor sinalizar o final de seu handshake
		while (digitalRead(PINO_CTS) == HIGH) {
			
		}
	}
}
