/*
 * PF_Codigo.c
 *
 * 
 * Author : Filipe Assunção - 26696
 */ 


#ifndef F_CPU
#define F_CPU 16000000UL // Velocidade do Clock (16MHz)
#endif


#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>

#include <avr/interrupt.h>


// Definição dos Pinos (Tabela 5)
#define VALVULA PD2
#define BUZZER PD3
#define BOMBA PD4


// Definição de Estados (FSM)
typedef enum {
	ESTADO_TESTE,
	ESTADO_AUTO_REPOUSO,
	ESTADO_AUTO_ENCHIMENTO,
	ESTADO_AUTO_REGA,
	ESTADO_MANUAL
} Estado;


Estado estado_atual = ESTADO_TESTE; // Começa por fazer os testes


// Variáveis Globais (Tempo de Rega e Intervalo de Rega)
volatile uint16_t Tr = 0; // Tempo de rega calculado
volatile uint16_t Ir = 0; // Intervalo de rega calculado


// Implementa o ficheiro em Assembly (CONTROLO_NIVEL)
extern void CONTROLO_NIVEL(void);


// ---------------------------------------------
// Função de configuração de entradas e saídas
void setup_hardware(void) {
	
	// Config. Entradas
	DDRB &= ~((1 << DDB0) | (1 << DDB1) | (1 << DDB2));
	
	// Ativa Pull-Ups internos nas entradas (botões com lógica inversa)
	PORTB |= (1 << PORTB0) | (1 << PORTB1) | (1 << PORTB2);
	
	// Config. Saídas
	DDRD |= (1 << DDD2) | (1 << DDD3) | (1 << DDD4) | (1 << DDD5) | (1 << DDD6) | (1 << DDD7);
	
	// Garante que começa tudo desligado
	PORTD &= ~((1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4));

}
// ---------------------------------------------


// ---------------------------------------------
// UART - Protocolo de Comunicação e Interface Remota

// Inicializar UART
void inicializar_uart(unsigned int baud) { // Recebe a valocidade (Baud Rate)
	
	unsigned int ubrr = 103;
	//UBRR = 16x106/(16x9600) -1 = 106/9600 -1 = 104,16(7) -1 ? 103 (Exemplo PP)

	
	// UBRR0 define a velocidade (ambos os dispositivos tem de ter a mesma velocidade)
	UBRR0H = (unsigned char) (ubrr >> 8); // Parte alta (4 bits)
	UBRR0L = (unsigned char) ubrr; // Parte baixa (8 bits)
	
	// USCR0B: Controlo e Status do Registro B
	// RXEN0 = Receber Dados
	// TXEN0 = Enviar Dados
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	
	// UCSR0C: Controlo e Status do Registo C
	// UART é configurada para 8 bits de dados e 1 bit de paragem
	UCSR0C = (3 << UCSZ00);
}

// Envio de um caracter por UART (só é possível enviar um Byte de cada vez)
void trasmitir_uart(char data) {
	
	// Enquanto o bit UDRE0 for 0, o buffer está cheio e fica à espera
	while (!(UCSR0A & (1 << UDRE0))); // Empty
	
	// Quando estiver livre, vai para UDR0
	UDR0 = data;
}

// Envio de uma frase por UART
void print_uart(char* string) {
	while (*string) { // Vai imprimir as letras (char) até encontar um "\0"
		trasmitir_uart(*string++);
	}
}

// Recebe o que foi digitado pelo utilizador e retorna
char receber_uart(void) {
	while (!(UCSR0A & (1 << RXC0)));
	return UDR0;
}
// ---------------------------------------------


// ---------------------------------------------
// Timers
volatile uint16_t contador_timer0 = 0; // Timer 0
volatile uint16_t duracao_buzzer = 0; // Timer 0
uint16_t tempo_maximo_buzzer = 5; // 5 segundos de aviso sonoro (Timer 0)

volatile uint16_t intervalo_rega = 0; // Timer 1

volatile uint16_t duracao_rega = 0; // Timer 2
volatile uint16_t contador_timer2 = 0; // Timer 2


// Inicialização dos Timers
void inicializar_timers(void) {
	
	// Timer 0
	TCCR0A |= (1 << WGM01); // Modo CTC (Clear Timer on Compare Match)
	OCR0A = 249; // Interrupção a cada 1ms (com prescaler 64 a 16MHz)
	TIMSK0 |= (1 << OCIE0A); // Ativa a interrupção por comparação do Timer 0
	TCCR0B |= (1 << CS01) | (1 << CS00); // Prescaler 64 e inicia o clock
	
	// Timer 1
	TCCR1B |= (1 << WGM12); // Modo CTC
	OCR1A = 15624; // (16MHz / (1024 * 1Hz)) - 1 = 15624
	TIMSK1 |= (1 << OCIE1A); // Ativa interrupção por comparação A
	TCCR1B |= (1 << CS12) | (1 << CS10); // Prescaler 1024 e inicia
	
	// Timer 2
	TCCR2A |= (1 << WGM21); // Modo CTC
	OCR2A = 249; // Interrupção a cada 1ms (com prescaler 64)
	TIMSK2 |= (1 << OCIE2A); // Ativa interrupção
	TCCR2B |= (1 << CS22); // Prescaler 64
}


// Timer 0
ISR(TIMER0_COMPA_vect) {
	// Só conta se o sistema estiver a Encher ou se estiver no Modo Manual
	if (estado_atual == ESTADO_AUTO_ENCHIMENTO || estado_atual == ESTADO_MANUAL) {
		
		// Se o Buzzer ainda estiver ligado, conta o tempo
		if (PORTD & (1 << PORTD3)) {
			
			contador_timer0++;
			
			if (contador_timer0 >= 1000) { // Contou 1000 ms (1s)
				
				contador_timer0 = 0;
				duracao_buzzer++;
				
				if (duracao_buzzer >= tempo_maximo_buzzer) {
					
					PORTD &= ~(1 << PORTD3); // Desliga apenas o Buzzer
					print_uart("Timer 0: Buzzer Desligado.\n");
					
				}
			}
		}
		
		else {
			// Limpa os contadores quando o tanque estiver cheio
			contador_timer0 = 0;
			duracao_buzzer = 0;
		}
	}
}


// Timer 1
ISR(TIMER1_COMPA_vect) {
	if (estado_atual == ESTADO_AUTO_REPOUSO) {
		
		intervalo_rega++;
		
		if (intervalo_rega >= Ir) {
			intervalo_rega = 0;
			estado_atual = ESTADO_AUTO_REGA; // Dispara a rega
			print_uart("Timer 1: A iniciar Rega...\n");
		}
	}
}


// Timer 2
ISR(TIMER2_COMPA_vect) {
	if (estado_atual == ESTADO_AUTO_REGA) {
		
		contador_timer2++;
		
		if (contador_timer2 >= 1000) { // Passa 1 segundo
			contador_timer2 = 0;
			duracao_rega++;
			
			if (duracao_rega >= Tr) {
				duracao_rega = 0;
				
				PORTD &= ~(1 << PORTD4); // Desliga a bomba
				estado_atual = ESTADO_AUTO_REPOUSO; // Passa para repouso
				
				print_uart("Timer 2: Rega concluida.\n");
			}
		}
	}
	else {
		contador_timer2 = 0;
	}
}
// ---------------------------------------------


// ---------------------------------------------
// ADC - Analog to Digital Converter

// Inicializar ADC
void inicializar_adc(void) {
	
	// ADMUX: seleciona a referência de tensão
	// REFS0 e 1: usa os 5V como referência
	ADMUX = (1 << REFS0);
	
	// ADCSRA: Ativa o ADC e define o Prescaler
	// Como o ADC necessita de uma frequência entre 50kHz e 200kHz
	// Ativa os bits ADPS0, ADPS1 e ADPS2
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

// Lê ADC
uint16_t ler_adc(uint8_t canal) {
	
	// Garante que o canal está entre os pinos PC0 a PC5
	if (canal > 5) {
		canal = 5;
	}
	
	// Limpa os bits anteriores do canal no ADMUX e define o novo canal
	ADMUX = (ADMUX & 0xF0) | canal;
	
	// Inicia a conversão (ADC Start Conversion)
	ADCSRA |= (1 << ADSC);
	
	// Espera que a conversão termine (o bit ADSC volta a '0' automaticamente quando acaba)
	while (ADCSRA & (1 << ADSC));
	
	// Retorna o valor convertido
	return ADC;
}
// ---------------------------------------------


// ---------------------------------------------
// Atualizar Tempos de Rega (através dos sensores analógicos) (fórmulas)

void atualizar_tempos_rega(void) {
	
	// Lê a temperatura e converte
	uint16_t leitura_temp = ler_adc(0);
	uint16_t temp_celsius = (leitura_temp * 500) / 1024;
	if (temp_celsius > 45) temp_celsius = 45; // Não pode ser superior a 45º
	
	// Lê a luminosidade
	uint16_t leitura_luz = ler_adc(1);
	if (leitura_luz > 1023) leitura_luz = 1023; // Não pode ser superior a 1023
	
	
	// Calcula os valores reais
	uint32_t tr_local = (uint32_t)leitura_luz + (20 * (uint32_t)temp_celsius);
	uint32_t ir_local = (10 * (1024 - (uint32_t)leitura_luz)) + (200 * (50 - (uint32_t)temp_celsius));
	
	
	// Reduzir os intervalos (para testes)
	uint32_t tr_escala = tr_local / 100;
	uint32_t ir_escala = ir_local / 1200;
	
	// Verificações
	if (tr_escala < 3)  tr_escala = 3;
	if (tr_escala > 10) tr_escala = 10;
	
	if (ir_escala < 4)  ir_escala = 4;
	if (ir_escala > 25) ir_escala = 25;
	
	
	// Guarda os segundos finais controlados nas variáveis globais dos Timers
	Tr = (uint16_t)tr_escala;
	Ir = (uint16_t)ir_escala;
}
// ---------------------------------------------


// ---------------------------------------------
// Função de atualizar a FSM
void atualizar_fsm(void) {
	switch (estado_atual) {
		
		// Estado E0
		case ESTADO_TESTE:
			print_uart("A iniciar teste.\n");
			
			//__asm__ __volatile__ ("break");
		
			// Liga atuadores e Led RGB
			PORTD |= (1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4) |
			(1 << PORTD5) | (1 << PORTD6) | (1 << PORTD7);
		
			_delay_ms(2000); // Delay de 2 segundos
		
			// Desliga tudo novamente
			PORTD &= ~((1 << PORTD2) | (1 << PORTD3) | (1 << PORTD4) |
			(1 << PORTD5) | (1 << PORTD6) | (1 << PORTD7));
		
			print_uart("Teste concluido. A entrar em Repouso.\n");
			estado_atual = ESTADO_AUTO_REPOUSO; // Passa para E1
			
		break;
		
		// Estado E1
		case ESTADO_AUTO_REPOUSO:
		
			// Atualizar Tr e Ir
			atualizar_tempos_rega();
		
			// Liga o RGB Green (está em repouso)
			PORTD |= (1 << PORTD6);
			PORTD &= ~((1 << PORTD5) | (1 << PORTD7));
		
			//__asm__ __volatile__ ("break");
		
			// Chama a rotina em Assembly
			CONTROLO_NIVEL();
		
			// Verifica se o Assembly ativou a válvula (D2)
			if (PIND & (1 << PIND2)) {
				print_uart("Nivel Baixo detetado. A encher.\n");
				estado_atual = ESTADO_AUTO_ENCHIMENTO; // Passa para auto enchimento
			}
		
			// Verifica se o PB2 foi premido para mudar para MANUAL
			if (!(PINB & (1 << PINB2))) {
				_delay_ms(50);
				
				while (!(PINB & (1 << PINB2)));
				
				_delay_ms(50);
				
				//__asm__ __volatile__ ("break");
				
				estado_atual = ESTADO_MANUAL;
				print_uart("Modo: MANUAL LIGADO\n");
			}
		
		break;
		
		// Estado E2
		case ESTADO_AUTO_ENCHIMENTO:
			// Liga o RGB Red (está a encher)
			PORTD |= (1 << PORTD5);
			PORTD &= ~((1 << PORTD6) | (1 << PORTD7));
		
			//__asm__ __volatile__ ("break");
		
			// Chama o Assembly novamente
			CONTROLO_NIVEL();
		
			// Se a válvula desligar, volta para o Repouso (E1)
			if (!(PIND & (1 << PIND2))) {
				print_uart("Tanque Cheio. Parar enchimento.\n");
				estado_atual = ESTADO_AUTO_REPOUSO; // Passa para auto repouso
			}
		
			// Verifica se o PB2 foi premido para mudar para Manual
			if (!(PINB & (1 << PINB2))) {
				
				_delay_ms(50);
				
				while (!(PINB & (1 << PINB2)));
				
				_delay_ms(50);
				
				estado_atual = ESTADO_MANUAL;
				print_uart("Modo: MANUAL LIGADO\n");
			}
		
		break;
		
		// Estado E3
		case ESTADO_AUTO_REGA:
			// Liga o RGB Blue (bomba de circulação ligada)
			PORTD |= (1 << PORTD7);
			PORTD &= ~((1 << PORTD5) | (1 << PORTD6));
			
			// Liga a Bomba de Rega (PD4)
			PORTD |= (1 << PORTD4);

			// Mudar para manual com o botão PB2
			if (!(PINB & (1 << PINB2))) {
				_delay_ms(200);
				PORTD &= ~(1 << PORTD4); // Desliga a bomba
				estado_atual = ESTADO_MANUAL;
				print_uart("Modo: MANUAL (Rega cancelada pelo utilizador)\n");
			}
			
		break;
		
		// Estado E4
		case ESTADO_MANUAL:
			// Garante que o Led RGB fica desligado
			PORTD &= ~((1 << PORTD5) | (1 << PORTD6) | (1 << PORTD7));
		
			// Verifica se existem dados novos na UART
			if (UCSR0A & (1 << RXC0)) {
			
				char cmd = UDR0;
			
				// Encher
				if (cmd == 'E' || cmd == 'e') {
					
					// Reset aos contadores do Timer 0
					contador_timer0 = 0;
					duracao_buzzer = 0;
					
					PORTD |= (1 << PORTD2) | (1 << PORTD3);
					print_uart("Manual: Enchimento ON\n");
				}
				
				// Interromper Enchimento
				else if (cmd == 'F' || cmd == 'f') {
					PORTD &= ~((1 << PORTD2) | (1 << PORTD3));
					print_uart("Manual: Enchimento OFF\n");
				}
				
				// Ativar Irrigação
				else if (cmd == 'R' || cmd == 'r') {
					PORTD |= (1 << PORTD4);
					print_uart("Manual: Bomba de Rega ON\n");
				}
				
				// Interromper Irrigação
				else if (cmd == 'S' || cmd == 's') {
					PORTD &= ~(1 << PORTD4);
					print_uart("Manual: Bomba de Rega OFF\n");
				}
				
				// Leitura de Sensores
				else if (cmd == 'I' || cmd == 'i') {
					
					// Atualiza Tr e Ir com base nos valores reais dos sensores
					atualizar_tempos_rega();
					
					// Lê os valores locais para o print informativo
					uint16_t leitura_temp = ler_adc(0);
					uint16_t temp_celsius = (leitura_temp * 500) / 1024; // Resultado em ºCelsius
					uint16_t leitura_luz = ler_adc(1);
					
					// Recalcular valores corretos (sem estarem reduzidos)
					uint32_t tr_real = (uint32_t)leitura_luz + (20 * temp_celsius);
					uint32_t ir_real = (10 * (1024 - leitura_luz)) + (200 * (50 - temp_celsius));
					
					// Variáveis para evitar corromper a memória do Arduíno
					char str_sensores[60];
					char str_matematica[150];
					
					// Formata os valores do ADC em texto
					sprintf(str_sensores, "\n\n--- Leitura de Sensores ---\nTemp: %u C | Luz: %u\n", temp_celsius, leitura_luz);
					sprintf(str_matematica, "Formula real -> Rega: %lu s | Intervalo: %lu s\nEscala Lab   -> Rega: %u s | Intervalo: %u s\n\n",
					tr_real, ir_real, Tr, Ir);
					
					// Envio para a UART
					print_uart(str_sensores);
					print_uart(str_matematica);
					
				}
			}
		
			// Se carregar em PB2, volta para o Automático
			if (!(PINB & (1 << PINB2))) {
				_delay_ms(200);
				estado_atual = ESTADO_AUTO_REPOUSO;
				print_uart("Modo: AUTOMATICO\n");
			}
		
		break;
	}
}
// ---------------------------------------------


// ---------------------------------------------
int main(void){
	
	setup_hardware();
	
	inicializar_uart(9600);
	inicializar_timers();
	inicializar_adc();
	
	
	// Set Global Interrupt Enable (Ativar Interrupções Globais)
	sei(); // Ativa interrupções globais
	
	while (1) {
		atualizar_fsm();
	}
}
// ---------------------------------------------