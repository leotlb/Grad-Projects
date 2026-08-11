// para compilar: make
// para executar: make run

/*  Integrantes
    Lucas Sales Duarte  11734490
    Daniel Filho        13677114
    Daniel Umeda        13676541
    Manoel Thomaz       13676392
	Leonardo Pereira 	9039361
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>    //  Para o uso da função sleep()
#include <pthread.h>   //  Para uso das threads
#include <semaphore.h> //  Para uso dos semaforos

// Macros pré definidas pelo usuário:
int MAX_MATERIA_PRIMA, ENVIO_MATERIA_INTERACAO, TEMPO_ENVIO_DEPFAB, TEMPO_PRODUCAO_CANETA, MAX_DEPOSITO_CANETA, COMPRA_POR_INTERACAO, DELAY_COMPRA_CONSUMIDOR;

pthread_t thread_list_ID[6];

int slots_disponiveis;
int materia_prima_disponivel;
int quant_interation;
int canetasCompradas = 0;

pthread_mutex_t canetas_do_comprador = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t quant_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t acessar_deposito_caneta = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t acessar_deposito_materiaPrima = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t sinal_materia_prima = PTHREAD_COND_INITIALIZER;
pthread_cond_t sinal_deposito_caneta = PTHREAD_COND_INITIALIZER;
pthread_cond_t sinal_ao_criador = PTHREAD_COND_INITIALIZER;

sem_t decrementa_materiaPrima;
sem_t materia_deslocada;
sem_t produza;
sem_t producao_concluida;
sem_t liberar_deposito_caneta;

/*
Suposições lógicas para descrição do projeto
    ->  Produtor tem um mini armazém de matéria prima para fabricar uma caneta por vez,
        assim como também as envia uma por vez (não tem armazém de produto pronto na fábrica,
        apenas de matéria-prima), pois é o que faz mais sentido (exemplo da fábrica volkswagem em aula).

    ->  Se há n canetas disponíveis no depósito e o comprador pedir m canetas, tal que m > n, o depósito envia apenas n canetas
        Pois não há como ser diferente.

    ->  Presume-se que o envio do deposito para a fabrica demore = "TEMPO_ENVIO_DEPFAB"
        independentemente de "ENVIO_MATERIA_INTERACAO". Ou seja, o caminhão que transporte
        demore o mesmo tempo, seja com 2 ou 20 unidades de matéria prima
            *Obs: A mesma lógica se aplica ao tempo de compra pelo comprador será DELAY_COMPRA_CONSUMIDOR
            independentemente de quantas irá comprar

*/

/*  RANKS:

        rank 0: função            = Criador
        rank 1: thread_list_ID[1] = Depósito de Matéria-Prima
        rank 2: thread_list_ID[2] = Célula de Fabricação de Canetas
        rank 3: thread_list_ID[3] = Controle
        rank 4: thread_list_ID[4] = Depósito de Canetas
        rank 5: thread_list_ID[5] = Comprador
*/
// -------------------------- Funções acessoras --------------------------

// -------------------------- Threads --------------------------

void controle(void)
{
    while (1) //   Variável controlada pelo criador para terminar esta thread
    {

        if (materia_prima_disponivel > 0 && slots_disponiveis > 0)
        {
            pthread_mutex_lock(&acessar_deposito_materiaPrima);
            pthread_mutex_lock(&acessar_deposito_caneta);
            if (slots_disponiveis > ENVIO_MATERIA_INTERACAO)	// Enforça parâmetro pré-definido caso haja espaço excedente
                quant_interation = ENVIO_MATERIA_INTERACAO;
            else												// Limita quantidade de acordo com o espaço disponível
                quant_interation = slots_disponiveis;

            if (materia_prima_disponivel < quant_interation)	// Limita quantidade de acordo com a matéria-prima disponível
                quant_interation = materia_prima_disponivel;

            if (quant_interation > 0 && materia_prima_disponivel > 0) // Para produzir uma quantidade n de canetas não nula há de haver espaço e matéria-prima
            {
                sem_post(&decrementa_materiaPrima);                                      // Libera o acesso ao estoque de matéria-prima para o depósito de matéria-prima
                pthread_cond_wait(&sinal_materia_prima, &acessar_deposito_materiaPrima); // Espera o sinal para retomar o controle do estoque de matéria-prima
                sem_wait(&materia_deslocada);                                            // Espera até a matéria-prima ser deslocada entre o depósito e a fábrica
                sem_post(&produza);                                                      // Assim que a matéria-prima chegar, libera a produção de n canetas
                sem_wait(&producao_concluida);                                           // Controle espera a produção acabar
                sem_post(&liberar_deposito_caneta);                                      // Ao acabar a produção, libera o envio de uma caneta ao depósito de canetas
                pthread_cond_wait(&sinal_deposito_caneta, &acessar_deposito_caneta);     // Espera o sinal do depósito de canetas para voltar a ter acesso ao estoque de canetas
                
                // pthread_mutex_lock(&canetas_do_comprador);
                // printf("\nControle:  | Canetas compradas: %d | Slots: %d | Matéria Prima: %d |\n", canetasCompradas, slots_disponiveis, materia_prima_disponivel);
                // pthread_mutex_unlock(&canetas_do_comprador);
            }
            pthread_mutex_unlock(&acessar_deposito_caneta);								 // Revoga o acesso ao estoque de canetas
            pthread_mutex_unlock(&acessar_deposito_materiaPrima);						 //	Revoga o acesso ao estoque de matéria-prima
        }

        else if (slots_disponiveis < MAX_DEPOSITO_CANETA)			// Ainda há slots, mas a matéria-prima acabou
        {
            /*  Anteriormente usado para um controle ao vivo das variaveis. Habilitar caso deseje ver

            pthread_mutex_lock(&acessar_deposito_materiaPrima);
            pthread_mutex_lock(&acessar_deposito_caneta);

            // quant_interation = COMPRA_POR_INTERACAO;
            pthread_mutex_lock(&canetas_do_comprador);
            // printf("\nControle:  | Canetas compradas: %d | Slots: %d | Matéria Prima: %d |\n", canetasCompradas, slots_disponiveis, materia_prima_disponivel);
            pthread_mutex_unlock(&canetas_do_comprador);

            pthread_mutex_unlock(&acessar_deposito_caneta);
            pthread_mutex_unlock(&acessar_deposito_materiaPrima);
			
            */
            pthread_cond_signal(&sinal_deposito_caneta); // Libera o comprador o acesso do deposito para que compre as canetas finais do depósito
        }
        else if (slots_disponiveis == MAX_DEPOSITO_CANETA && materia_prima_disponivel == 0) // Caso acabou os slots e a matéria-prima, deve acabar
        {
            /*  Anteriormente usado para ver a ultima interação da thread controle. Habilitar caso o desejo de ver

            //  Finalizar programa
            pthread_mutex_lock(&acessar_deposito_materiaPrima);
            pthread_mutex_lock(&acessar_deposito_caneta);
            pthread_mutex_lock(&canetas_do_comprador);
            printf("\nControle:  | Canetas compradas: %d | Slots: %d | Matéria Prima: %d |\n", canetasCompradas, slots_disponiveis, materia_prima_disponivel);
            pthread_mutex_unlock(&canetas_do_comprador);
            pthread_mutex_unlock(&acessar_deposito_caneta);
            pthread_mutex_unlock(&acessar_deposito_materiaPrima);
            printf("\n\tCódigo finalizado. Estoque de matéria-prima acabou e o depósito de canetas está vazioz\n");

            */
        }
    }
}

void depos_materia(void)
{

    while (1) //   Variável controlada pelo criador para terminar esta thread
    {
        sem_wait(&decrementa_materiaPrima);			// Aguarda acesso a quantidade de matéria-prima

        // pthread_mutex_lock(&acessar_deposito_materiaPrima);

        pthread_mutex_lock(&acessar_deposito_materiaPrima); 	// Obtém acesso exclusivo ao estoque
        pthread_mutex_lock(&quant_mutex);                   	// Obtém acesso a quantidade de decremento do depósito de matéria-prima
        materia_prima_disponivel -= quant_interation;       	// Decrementa o estoque de matéria-prima
        pthread_mutex_unlock(&quant_mutex);						// Revoga acesso a quantidade de decremento do depósito de matéria-prima
        pthread_mutex_unlock(&acessar_deposito_materiaPrima);	// Revoga acesso exclusivo ao estoque de matéria-prima

        pthread_cond_signal(&sinal_materia_prima);				// Envia sinal ao controle que realizou envio de matéria-prima
        sleep(TEMPO_ENVIO_DEPFAB);								// Dorme pelo tempo de envio
        sem_post(&materia_deslocada);							// Libera controle para prosseguir com sinais para a produção
		
        // printf("\nDEPMAT: matéria-prima disponivel: %d", materia_prima_disponivel);
    }
}

void celula_fabrica(void)
{

    while (1) //   Variável controlada pelo criador para terminar esta thread
    {

        sem_wait(&produza); //  Ao aguardo do controle para produzir

        pthread_mutex_lock(&quant_mutex); 	// Ganha acesso à quantidade n de canetas a serem fabricadas

        // printf("\nFAB:  Fabricando %d unidade", quant_interation);
        for (int i = 0; i < quant_interation; i++) // Fabrica n canetas garantindo a quantidade certa que saiu do depósito
            sleep(TEMPO_PRODUCAO_CANETA);          // Tempo em que está fabricando um caneta

        pthread_mutex_unlock(&quant_mutex);			// Revoga acesso à quantidade n de canetas a serem fabricadas
        sem_post(&producao_concluida);				// Termina o processo liberando a continuidade para o controle
    }
}

void depos_caneta(void)
{
    while (1) //   Variável controlada pelo criador para terminar esta thread
    {
        sem_wait(&liberar_deposito_caneta);           		//  Aguarda o controle avisar que chegou n canetas
        pthread_mutex_lock(&quant_mutex);             		//  Obtém a quantidade que será produzida
        pthread_mutex_lock(&acessar_deposito_caneta); 		//  Garante exclusão mútua no acesso a quantidade de slots no depósito de canetas

        slots_disponiveis -= quant_interation;				//  Decrementa a quantidade de slots, agora ocupadados por n canetas

        pthread_mutex_unlock(&quant_mutex);             	//  Revoga acesso a quantidade de canetas produzidas
        pthread_mutex_unlock(&acessar_deposito_caneta); 	//  Revoga o acesso a quantidade de slots disponíveis no depósito de canetas
        pthread_cond_signal(&sinal_deposito_caneta); 		//  Manda sinal para o controle prosseguir com execução
        // printf("\nDEPCAN:   quantidade de canetas no depósito são %d e slots %d", MAX_DEPOSITO_CANETA - slots_disponiveis, slots_disponiveis);
    }
}

void comprador(void)
{
    while (1) //   Variável controlada pelo criador para terminar esta thread
    {
        pthread_mutex_lock(&acessar_deposito_caneta);				// Ganha acesso a quantidade de canetas no depósito
        // pthread_cond_wait(&sinal_deposito_caneta, &acessar_deposito_caneta);

        pthread_mutex_lock(&canetas_do_comprador);					// Ganha acesso a quantidade de canetas a serem compradas
        int local_max = MAX_DEPOSITO_CANETA - slots_disponiveis;	// Guarda a quantidade de canetas
        if (local_max > 0)
        {
            int interacao_compras;

            if (local_max < COMPRA_POR_INTERACAO) 					// Se há menos canetas disponíveis do que o pedido, restringe a quantidade compradas
                interacao_compras = local_max;
            else
                interacao_compras = COMPRA_POR_INTERACAO;

            for (int i = 0; i < interacao_compras; i++)				// Retira do estoque e compra uma caneta por vez
            {
                slots_disponiveis++;
                canetasCompradas++;
            }
        }
        pthread_mutex_unlock(&canetas_do_comprador);
        pthread_mutex_unlock(&acessar_deposito_caneta);
        pthread_cond_signal(&sinal_ao_criador);

        sleep(DELAY_COMPRA_CONSUMIDOR); // Delay por compra, independente da quantidade
    }
}

int criador(void)
{
    //  Inicializar os semáforos
    sem_init(&decrementa_materiaPrima, 0, 0); //  Inicia o semáforo para controle de estoque de matéria-prima
    sem_init(&produza, 0, 0);                 //  Inicia o semáforo para não produzindo
    sem_init(&liberar_deposito_caneta, 0, 0); //  Inicia o semáforo para o envio das canetas

    //  Criação das threads com atributos default e sem passagem de argumentos para sua função inicializadora

    if (pthread_create(&thread_list_ID[1], 0, (void *)depos_materia, 0) != 0)
    {
        fprintf(stderr, "Erro ao criar a Célula de Depósito de Matéria-Prima\n");
    }
    printf("\nDepósito de Matéria-Prima iniciado!");

    if (pthread_create(&thread_list_ID[2], 0, (void *)celula_fabrica, 0) != 0)
    {
        fprintf(stderr, "Erro ao criar a Célula de Fabricação de Canetas\n");
    }
    printf("\nCélula de Fabricação iniciada!");
    if (pthread_create(&thread_list_ID[3], 0, (void *)controle, 0) != 0)
    {
        fprintf(stderr, "Erro ao criar o Controle\n");
    }
    printf("\nControle iniciado!");
    if (pthread_create(&thread_list_ID[4], 0, (void *)depos_caneta, 0) != 0)
    {
        fprintf(stderr, "Erro ao criar o Depósito de Canetas\n");
    }
    printf("\nDepósito de Canetas iniciado!");
    if (pthread_create(&thread_list_ID[5], 0, (void *)comprador, 0) != 0)
    {
        fprintf(stderr, "Erro ao criar o Comprador\n");
    }
    printf("\nComprador iniciado!");
    int contador_canetas = 0;			// Canetas compradas totais
	
    while (contador_canetas < MAX_MATERIA_PRIMA)						// Enquanto a matéria-prima for suficiente para fabricar as canetas necessárias
    {
        pthread_mutex_lock(&canetas_do_comprador);						// Ganha acesso a quantidade de canetas compradas nesta iteração
        pthread_cond_wait(&sinal_ao_criador, &canetas_do_comprador);	// Espera o sinal que o comprador tentou/concluiu uma compra
        if (canetasCompradas > contador_canetas)						
        {
			// Atualiza na tela as compras feitas nesta iteração
            printf("\nComprador comprou %2.d canetas nesta interação e o total já comprado é: %2.d", canetasCompradas - contador_canetas, canetasCompradas);
            fflush(stdout);
            contador_canetas = canetasCompradas;						// Atualiza na memória a quantidade de compras feitas até aqui
        }
        else if (canetasCompradas == contador_canetas)
        {
            printf("\nNão houve compra nessa interação");
            fflush(stdout);
        }
        pthread_mutex_unlock(&canetas_do_comprador);					// Revoga acesso a quantidade de canetas nesta iteração
    }
    printf("\n\n-- -- -- -- Compra finalizada com sucesso!!-- -- -- -- \n");

    //  Finaliza os semáforos
    sem_destroy(&decrementa_materiaPrima);
    sem_destroy(&produza);
    sem_destroy(&liberar_deposito_caneta);

    //  Finaliza os mutexes
    pthread_mutex_destroy(&canetas_do_comprador);
    pthread_mutex_destroy(&quant_mutex);
    pthread_mutex_destroy(&acessar_deposito_caneta);
    pthread_mutex_destroy(&acessar_deposito_materiaPrima);

    //  Finaliza as variáveis de condição
    pthread_cond_destroy(&sinal_materia_prima);
    pthread_cond_destroy(&sinal_deposito_caneta);
    pthread_cond_destroy(&sinal_ao_criador);

    //  Não foi dado join em nenhuma thread, pois basta o comprador ter comprado tudo que o programa pode ser finalizado com sucesso.
    return 0;
}

int main()
{

    printf("\nDigite os 7 parâmetros na ordem:\n\t MAX_MATERIA_PRIMA, ENVIO_MATERIA_INTERACAO, TEMPO_ENVIO_DEPFAB, TEMPO_PRODUCAO_CANETA, MAX_DEPOSITO_CANETA, COMPRA_POR_INTERACAO, DELAY_COMPRA_CONSUMIDOR\n");
    scanf("%d %d %d %d %d %d %d", &MAX_MATERIA_PRIMA, &ENVIO_MATERIA_INTERACAO, &TEMPO_ENVIO_DEPFAB, &TEMPO_PRODUCAO_CANETA, &MAX_DEPOSITO_CANETA, &COMPRA_POR_INTERACAO, &DELAY_COMPRA_CONSUMIDOR);
    //  Util para mudança manual nos parâmetros
    // MAX_MATERIA_PRIMA = 25;
    // ENVIO_MATERIA_INTERACAO = 1;
    // TEMPO_ENVIO_DEPFAB = 3;
    // TEMPO_PRODUCAO_CANETA = 3;
    // MAX_DEPOSITO_CANETA = 4;
    // COMPRA_POR_INTERACAO = 4;
    // DELAY_COMPRA_CONSUMIDOR = 1;
// 25 1 3 3 5 4 1
    slots_disponiveis = MAX_DEPOSITO_CANETA;
    materia_prima_disponivel = MAX_MATERIA_PRIMA;
    return criador();
}
