#include <avr/io.h>
#include <inttypes.h>

#include "wireprot.h"
#include <util/delay.h>

static data net;

void delayFunc( void ){
    _delay_us( DELAY_TIME );
}

uint8_t send( uint8_t destAddr, uint8_t* msg, uint8_t size, uint8_t blocking ){

    uint8_t packetArray[ 4 + PAYLOAD_SIZE ];
    uint8_t arrayIndex = 0;

    uint8_t packetCount = 0,
            packets = 0,
            tries = 0;
    uint8_t i = 0;

    // Determine how many packets we need to send, one extra packet if we have remainder fields
    packets = size / PAYLOAD_SIZE;
    packets += ( size % PAYLOAD_SIZE ) > 0 ? 1 : 0;

    // For all packets
    for( packetCount = 0; packetCount < packets; packetCount++ ){

        // Construct packet header
        packetArray[ arrayIndex++ ] = destAddr;
        packetArray[ arrayIndex++ ] = net.myAddr;
        packetArray[ arrayIndex++ ] = DATA_TYPE;

        // For all payload bytes
        for( i = packetCount * PAYLOAD_SIZE; 
             i < packetCount * PAYLOAD_SIZE + PAYLOAD_SIZE; 
             i++ ){

                packetArray[ arrayIndex++ ] = msg[ i ];
        }

        // calculate CRC and add to packetArray as the last byte
        packetArray[ arrayIndex++ ] = crc8( packetArray, 3 + PAYLOAD_SIZE );
        
        // Now send the packet
        for( i = 0; i < arrayIndex; i++ ){
            sendByte( packetArray[ i ] );
        }

        // wait in between packets
        delayFunc(); delayFunc(); delayFunc();

        // If we are blocking, resend util we receive an acknowledgement
        while( blocking && 
               recvAck() != packetArray[ arrayIndex - 1 ] ){

            if( tries > net.timeout ){
                // Return how many bytes we managed to get through
                return ( packetCount == 0 ? 0 : packetCount - 1 ) * PAYLOAD_SIZE;
            }

            // wait for a bit
            delayFunc(); delayFunc(); delayFunc();

            // resend last packet until we receive an ack
            for( i = 0; i < arrayIndex; i++ ){
                sendByte( packetArray[ i ] );
            }

            tries++;
        };

        // reset array index
        arrayIndex = 0;
    }
    return size;
}

uint8_t recv( uint8_t* buffer, uint8_t size, uint8_t blocking ){

    // Fill a window twice the packet size
    uint8_t window[ WINDOW_SIZE ];
    uint8_t i = 0;
    uint8_t bufferCount = 0,
            packets = 0,
            packetsCount = 0,
            offset = 0;
    
    // Determine how many packets we are expecting, one extra packet if we have remainder fields
    packets = size / PAYLOAD_SIZE;
    packets += ( size % PAYLOAD_SIZE ) > 0 ? 1 : 0;
    
    while( blocking && bufferCount < size ){

        for( packetsCount = 0; packetsCount < packets; packetsCount++ ){
            // fill the window
            for( i = 0; i < PACKET_SIZE * 2; i++ ){
                window[ i ] = recvByte();
            }

            // reset the offset for this new window
            offset = 0;

            // Walk up the window and run CRC's until one matches
            while( (WINDOW_SIZE - offset) < (PACKET_SIZE - 1) ){

                // crc8 everything up to PACKET_SIZE -1 ( to exclude last byte ( the CRC itself ) )
                if( window[ offset + PACKET_SIZE ] == crc8( window + offset, PACKET_SIZE - 1 ) ){

                    // Found the packet! Is it for us? If not, then try again!
                    if( window[ offset ] != net.myAddr )
                        break;

                    // Check to see if it is an ACK, if so, return CRC
                    if( buffer == NULL && window[ 2 + offset ] == ACK_TYPE ){
                        return window[ 3 + offset + PAYLOAD_SIZE ];
                    }

                    // If we were only expecting ACK, then try again
                    else if( buffer == NULL ){
                        break;
                    }

                    // Found the packet, now fill the buffer with the payload
                    for( i = 0; i < PAYLOAD_SIZE; i++ ){
                        // Skip the header and get the payload
                        buffer[ bufferCount++ ] = window[ 3 + offset + i ];
                    }
                    
                    // if we are not blocking, no need to send ack.
                    if( !blocking ){
                        // send ack
                        sendAck( window[ 1 + offset ], window[ 3 + offset + PAYLOAD_SIZE ] );
                    }

                    break;
                }
                else{
                    offset++;
                }
            }
        }
    }

    // Return how many characters we put into the buffer
    return bufferCount;
}

void sendByte( uint8_t byte ){
    uint8_t bitCount = 0;

    for( bitCount = 0; bitCount < 8; bitCount++ ){
        // Now Send bits, LSB first
        delayFunc();
        TPORT = ( byte & ( 1 << bitCount ) ) > 0 ? (TPORT |= ( 1 << net.port ) ) : (TPORT &= ~( 1 << net.port ) );
        delayFunc();
        TPORT &= ~( 1 << net.port );
    }
}

uint8_t recvByte( void ){
    uint8_t byte = 0, bitCount = 0;

    for( bitCount = 0; bitCount < 8; bitCount++ ){
        // Now recv bits, MSB first down
        delayFunc();
        byte |=  ( ( 7 - bitCount ) << ( TPIN & ( 1 << net.pin ) ) );
    }

    return byte;
}

void sendAck( uint8_t addr, uint8_t CRC ){
    uint8_t i = 0, arrayIndex = 0;
    uint8_t packetArray[ 4 + PAYLOAD_SIZE ];

    // construct packet header
    packetArray[ arrayIndex++ ] = addr;
    packetArray[ arrayIndex++ ] = net.myAddr;
    packetArray[ arrayIndex++ ] = ACK_TYPE;

    for( i = 0; i < PAYLOAD_SIZE; i++ ){
        packetArray[ arrayIndex++ ] = CRC;
    }

    packetArray[ arrayIndex++ ] = crc8( packetArray, 3 + PAYLOAD_SIZE );

    // Now send the packet
    for( i = 0; i < arrayIndex; i++ ){
        sendByte( packetArray[ i ] );
    }
}

uint8_t recvAck( void ){
    return recv( NULL, PAYLOAD_SIZE, 1 );
}


// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
// Adopted from http://github.com/paeaetech/paeae/tree/master/Libraries/ds2482/

// 7 bytes
uint8_t crc8( uint8_t *addr, uint8_t len ){
    uint8_t crc = 0;
    uint8_t i = 0, 
            j = 0, 
            inbyte = 0,
            mix = 0;
        
    for( i = 0; i < len; i++ ){
        inbyte = addr[ i ];

        for ( j=0; j<8; j++ ){
            mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;

            if (mix)
                crc ^= 0x8C;

            inbyte >>= 1;
        }
    }
    return crc;
} 


void setPinWire( uint8_t pin ){ net.pin = pin; }

void setPortWire( uint8_t port ){ net.port = port; }

void setAddr( uint8_t addr ){ net.myAddr = addr; }

void setTimeout( uint8_t timeout ){ net.timeout = timeout; }

//getFailureRate(); // Rate of packets sent are going through on the first try
//getThroughPut(); // bits per second in current conditions

// difference, one is deterministic number of packets sent can be determined, numbered.
// simple - Automatic RQ
// no sliding window for packet ordering and
// 

/*
ACK for every packet.


0x01 - ACK
0x02 - DATA


CRC window
packet 8 bytes

| to   | from | type | byte | byte | byte | byte | CRC  |
| 0xfe | 0xff | 0x02 | 0x03 | 0x00 | 0xac | 0x9b |      |

Window of 16 bytes
 increment buffer++ until CRC calculations matches on the window of buffer[ 0 - 7 ]

ACK with CRC of received packet
| 0xff | 0xfe | 0x01 | 0x00 | 0x00 | 0x00 | 0x00 | CRC |




Overhead of 16 bytes
*/