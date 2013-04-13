#ifndef __WIRE_PROT__
#define __WIRE_PROT__

#define F_CPU 1000000UL // Define software reference clock for delay duration

#define TPORT PORTB
#define TPIN  PINB

#define PAYLOAD_SIZE 4
#define DELAY_TIME   100

#define DATA_TYPE 0x01
#define ACK_TYPE  0x02

#define WINDOW_SIZE ( 4 + PAYLOAD_SIZE ) * 2
#define PACKET_SIZE ( 4 + PAYLOAD_SIZE )

#define NULL 0

typedef struct{
    uint8_t pin:3;
    uint8_t port:3;
    uint8_t myAddr;
    uint8_t timeout;
} data;

/* API */
uint8_t send( uint8_t destAddr, uint8_t* msg, uint8_t size, uint8_t blocking );
uint8_t recv( uint8_t* buffer, uint8_t size, uint8_t blocking );

void setPinWire( uint8_t pin );
void setPortWire( uint8_t port );
void setAddr( uint8_t addr );

void setTimeout( uint8_t timeout );

/* Internal Functions */
uint8_t crc8( uint8_t *addr, uint8_t len );

void sendAck( uint8_t addr, uint8_t CRC );
uint8_t recvAck( void );

void sendByte( uint8_t byte );
uint8_t recvByte( void );

#endif /* __WIRE_PROT__ */