// ===================================================================================
// USB CDC Functions for CH551, CH552 and CH554
// ===================================================================================

#include "uart.h"
#include "gpio.h"
#include "config.h"
#include "usb_cdc.h"
#include "usb_handler.h"

// ===================================================================================
// Variables and Defines
// ===================================================================================

// Initialize line coding
__xdata CDC_LINE_CODING_TYPE CDC_lineCodingB = {
  .baudrate = 115200,       // baudrate 115200
  .stopbits = 0,            // 1 stopbit
  .parity   = 0,            // no parity
  .databits = 8             // 8 databits
};

// Variables
volatile __xdata uint8_t CDC_controlLineState = 0;  // control line state
volatile __xdata uint8_t CDC_readByteCount = 0;     // number of data bytes in IN buffer
volatile __xdata uint8_t CDC_readPointer   = 0;     // data pointer for fetching
volatile __bit CDC_writeBusyFlag = 0;               // flag of whether upload pointer is busy
__xdata uint8_t CDC_writePointer = 0;               // data pointer for writing

// Macros
#define CDC_DTR_flag      (CDC_controlLineState & 1)
#define CDC_RTS_flag      ((CDC_controlLineState >> 1) & 1)
#define LED_VCP_SET(val)  PIN_write(PIN_LED, !(val))

// CDC class requests
#define SET_LINE_CODING         0x20  // host configures line coding
#define GET_LINE_CODING         0x21  // host reads configured line coding
#define SET_CONTROL_LINE_STATE  0x22  // generates RS-232/V.24 style control signals

// ===================================================================================
// Front End Functions
// ===================================================================================

// Setup USB-CDC
void CDC_init(void) {
//USB_init();
  UEP2_T_LEN  = 0;
  UEP3_T_LEN  = 0;
}

// Check number of bytes in the IN buffer
uint8_t CDC_available(void) {
  return CDC_readByteCount;
}

// Check if OUT buffer is ready to be written
__bit CDC_ready(void) {
  return(!CDC_writeBusyFlag);
}

// Flush the OUT buffer
void CDC_flush(void) {
  if(!CDC_writeBusyFlag && CDC_writePointer > 0) {      // not busy and buffer not empty?
    UEP2_T_LEN = CDC_writePointer;                      // number of bytes in OUT buffer
    UEP2_CTRL  = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_ACK; // respond ACK
    CDC_writeBusyFlag = 1;                              // busy for now
    CDC_writePointer  = 0;                              // reset write pointer
  }
}

// Write single character to OUT buffer
void CDC_write(char c) {
  if(CDC_writeBusyFlag) return;                         // wait for ready to write
  EP2_buffer[64 + CDC_writePointer++] = c;              // write character
  if(CDC_writePointer == EP2_SIZE) CDC_flush();         // flush if buffer full
}

// Read single character from IN buffer
char CDC_read(void) {
  char data;
  while(!CDC_readByteCount);                            // wait for data
  data = EP2_buffer[CDC_readPointer++];                 // get character
  if(--CDC_readByteCount == 0)                          // dec number of bytes in buffer
    UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_R_RES | UEP_R_RES_ACK;// request new data if empty
  return data;
}

// Get DTR flag
__bit CDC_getDTR(void) {
  return CDC_DTR_flag;
}

// Get RTS flag
__bit CDC_getRTS(void) {
  return CDC_RTS_flag;
}

// ===================================================================================
// CDC-Specific USB Handler Functions
// ===================================================================================

// Setup CDC endpoints
void CDC_setup(void) {
  UEP2_DMA    = (uint16_t)EP2_buffer;       // EP2 data transfer address
  UEP3_DMA    = (uint16_t)EP3_buffer;       // EP3 data transfer address
  UEP2_CTRL   = bUEP_AUTO_TOG               // EP2 Auto flip sync flag
              | UEP_T_RES_NAK               // EP2 IN transaction returns NAK
              | UEP_R_RES_ACK;              // EP2 OUT transaction returns ACK
  UEP3_CTRL   = bUEP_AUTO_TOG               // EP3 Auto flip sync flag
              | UEP_T_RES_NAK;              // EP3 IN transaction returns NAK
  UEP2_3_MOD  = bUEP2_RX_EN | bUEP2_TX_EN   // EP2 double buffer (0x0C)
              | bUEP3_TX_EN;                // EP3 TX enable (0x40)
}

// Reset CDC parameters
void CDC_reset(void) {
  UEP2_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK | UEP_R_RES_ACK;
  UEP3_CTRL = bUEP_AUTO_TOG | UEP_T_RES_NAK;
  CDC_readByteCount = 0;                    // reset received bytes counter
  CDC_writeBusyFlag = 0;                    // reset write busy flag
}

// Handle non-standard control requests
uint8_t CDC_control(void) {
  uint8_t i;
  if((USB_setupBuf->bRequestType & USB_REQ_TYP_MASK) == USB_REQ_TYP_CLASS) {
    switch(USB_setupBuf->bRequest) {
      case GET_LINE_CODING:                   // 0x21  currently configured
        for(i=0; i<sizeof(CDC_lineCodingB); i++)
          EP0_buffer[i] = ((uint8_t*)&CDC_lineCodingB)[i]; // transmit line coding to host
        return sizeof(CDC_lineCodingB);
      case SET_CONTROL_LINE_STATE:            // 0x22  generates RS-232/V.24 style control signals
        CDC_controlLineState = EP0_buffer[2]; // read control line state
        // RTS and DTR signals maybe unable to determine the connection status
        LED_VCP_SET(CDC_DTR_flag);            // set LED
        return 0;
      case SET_LINE_CODING:                   // 0x20  Configure
        return 0;            
      default:
        return 0xFF;                          // command not supported
    }
  }
  else return 0xFF;
}

// Endpoint 0 OUT handler
void CDC_EP0_OUT(void) {
  uint8_t i;
  if(SetupReq == SET_LINE_CODING) {                         // set line coding
    if(U_TOG_OK) {
      for(i=0; i<((sizeof(CDC_lineCodingB)<=USB_RX_LEN)?sizeof(CDC_lineCodingB):USB_RX_LEN); i++)
        ((uint8_t*)&CDC_lineCodingB)[i] = EP0_buffer[i];    // receive line coding from host
      UART_setBAUD(CDC_lineCoding->baudrate);               // set UART BAUD rate
      CDC_writeBusyFlag = 0;                                // clear busy flag
      CDC_writePointer  = 0;                                // reset write pointer
      UEP0_T_LEN = 0;
      UEP0_CTRL |= UEP_R_RES_ACK | UEP_T_RES_ACK;           // send 0-length packet
    }
  }
  else {
    UEP0_T_LEN = 0;
    UEP0_CTRL |= UEP_R_RES_ACK | UEP_T_RES_NAK;             // respond Nak
  }
}

// Endpoint 2 IN handler (bulk data transfer to host)
void CDC_EP2_IN(void) {
  UEP2_T_LEN = 0;                                           // no data to send anymore
  UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_T_RES | UEP_T_RES_NAK;  // respond NAK by default
  CDC_writeBusyFlag = 0;                                    // clear busy flag
}

// Endpoint 2 OUT handler (bulk data transfer from host)
void CDC_EP2_OUT(void) {
  if(U_TOG_OK) {                                        // discard unsynchronized packets
    CDC_readByteCount = USB_RX_LEN;                     // set number of received data bytes
    CDC_readPointer = 0;                                // reset read pointer for fetching
    if(CDC_readByteCount) 
      UEP2_CTRL = UEP2_CTRL & ~MASK_UEP_R_RES | UEP_R_RES_NAK; // respond NAK after a packet. Let main code change response after handling.
  }
}

// Endpoint 3 IN handler
void CDC_EP3_IN(void) {
  UEP3_T_LEN = 0;
  UEP3_CTRL = UEP3_CTRL & ~ MASK_UEP_T_RES | UEP_T_RES_NAK; // default NAK
}
