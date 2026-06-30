
/*
 * controlo_nivel.s
 *
 * 
 *  Author: Filipe Assunção - 26696
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>

.section .text
.global CONTROLO_NIVEL

CONTROLO_NIVEL:
    SBIC _SFR_IO_ADDR(PINB), 0 // Se PB0 (Bóia Inferior) for pressionado salta para o Testa_superior
    RJMP Testa_Superior

Sistema_ON:
    // Só liga o Buzzer se a Válvula (PD2) ainda estiver Desligada
    SBIC _SFR_IO_ADDR(PIND), 2 // Se PD2 for '0' (Válvula desligada), salta a próxima instrução
    RJMP Ativa_Valvula_Apenas // Se já estava a encher, não volta a ligar o Buzzer

    SBI _SFR_IO_ADDR(PORTD), 3 // Liga Buzzer (PD3)

Ativa_Valvula_Apenas:
    SBI _SFR_IO_ADDR(PORTD), 2 // Mantém Válvula Ligada (PD2)
    RET

Testa_Superior:
    SBIC _SFR_IO_ADDR(PINB), 1 // Se estiver pressionado (Clear) salta
    RET 

Sistema_OFF:
    // Se o tanque encheu, desliga imediatamente os atuadores
    CBI _SFR_IO_ADDR(PORTD), 2 // Desliga Válvula (PD2)
    CBI _SFR_IO_ADDR(PORTD), 3 // Desliga Buzzer (PD3)
    RET // Retorna ao C com os pinos desligados