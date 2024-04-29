// ===================================================================================
// UART Functions with Receive Buffer and Interrupt for CH551, CH552 and CH554
// ===================================================================================

#pragma once
#include <stdint.h>
#include "ch554.h"

// UART Macros
#define UART_ready()      (UART_readyFlag)    // ready to send data?
#define UART_available()  (UART_readPointer != UART_writePointer) // something in buffer?

// Define initial BAUD rate
#define UART_BAUD         115200
#define UART_BAUD_SET(baud) ((uint8_t)(256 - (((F_CPU / 8 / (baud)) + 1) / 2)))

// Variables
extern __xdata uint8_t  UART_buffer[];
extern volatile uint8_t UART_readPointer;
extern volatile uint8_t UART_writePointer;
extern volatile __bit   UART_readyFlag;

// Setup UART
inline void UART_init(void) {
	U1SM0 = 0;                      // UART1 8 data bits
	U1SMOD = 1;                     // UART1 fast mode
	U1REN = 1;                      // enable RX
	SBAUD1 = UART_BAUD_SET(UART_BAUD);
	U1TI = 0;                       // clean TX interrupt flag
	IE_UART1 = 1;	                  // enable UART1 interrupt
	EA = 1;                         // enable global interrput
}

// Transmit a data byte, return echo
inline void UART_write(uint8_t data) {
  UART_readyFlag = 0;             // clear ready flag
  SBUF1 = data;                   // start transmitting data byte
}

// Receive a data byte
inline uint8_t UART_read(void) {
  uint8_t result = UART_buffer[UART_readPointer++];
  UART_readPointer &= 63;
  return result;
}

// Set BAUD rate
inline void UART_setBAUD(uint32_t baud) {
  SBAUD1 = UART_BAUD_SET(baud);
}

void UART_interrupt(void);
