#include "USI_I2C_Master.h"

#include <avr/io.h>
#include <util/delay.h>


#define USI_PORT PORTA
#define USI_DDR DDRA
#define USI_PIN PINA

#define SDA PA6
#define SCL PA4

#define FAST_MODE

/*---------------------*/

inline void SDA_HI() { USI_PORT |= (1 << SDA); }
inline void SCL_HI() { USI_PORT |= (1 << SCL); }
inline void SDA_LO() { USI_PORT &= ~(1 << SDA); } 
inline void SCL_LO() { USI_PORT &= ~(1 << SCL); } 

inline void SDA_IN() { USI_DDR &= ~(1 << SDA); } 
inline void SDA_OUT() { USI_DDR |= (1 << SDA); }

inline void toggle_SCL() { USICR |= (1 << USITC); }
inline void wait_SCL_HI() { while( !(USI_PIN & (1 << SCL)) ); }

#ifdef FAST_MODE
    //Delays for fast mode (100kHz < SCL < 400kHz)
    static inline void delay_T2() { _delay_us(2); } // > 1.3us
    static inline void delay_T4() { _delay_us(1); } // > 0.6us
#else
    //Delays for standard mode (SCL <= 100kHz)
    static inline void delay_T2() { _delay_us(5); } // > 4.7us
    static inline void delay_T4() { _delay_us(4); } // > 4.0us
#endif

inline void clear_USISR_flags() 
{
    //Read timer bits to keep them unchanged
    uint8_t timer_bits = USISR & ((1 << USICNT3) | (1 << USICNT2) | (1 << USICNT1) | (1 << USICNT0));

    //Write 1 to the flags we want cleared, 0 to keep them unchanged
    uint8_t flag_bits = ((1 << USISIF) | (1 << USIOIF) | (1 << USIPF) | (1 << USIDC));

    //Write prepared data to USISR
    USISR = flag_bits | timer_bits; 
}

inline void reset_USIDR() { USIDR = 0xFF; } //Fill with "released level" data

/*---------------------*/


void I2C_init() 
{
    USI_PORT |= (1 << SDA) | (1 << SCL); //Enable pullups on SDA and SCL.
    USI_DDR |= (1 << SDA) | (1 << SCL); //SDA and SCL set as output

    USICR = (1 << USIWM1) | (1 << USICS1) | (1 << USICLK); //Two wire mode, Software stobe as clock

    reset_USIDR(); //Preload data register with "released level" data.

    clear_USISR_flags(); //Clear any flags
    USISR = (0x0 << USICNT0); //Reset clock counter
}

void transfer() 
{
    clear_USISR_flags(); //Clear flags to later detect stop condition

    do {
        delay_T2();
        toggle_SCL(); //Generate positive SCL edge.
        wait_SCL_HI(); //Wait for SCL to go high.
        delay_T4();
        toggle_SCL(); //Generate negative SCL edge.
    } while ( !(USISR & (1 << USIOIF)) ); //Check for transfer complete.

    delay_T2();
}

void transfer_1bit() 
{
    //Set timer to overflow after 2 clock edges (shift 1 bit)
    USISR = (0xE << USICNT0);
    transfer();
}

void transfer_8bit() 
{
    //Set timer to overflow after 16 clock edges (shift 8 bit)
    USISR = (0x0 << USICNT0);
    transfer();
}


bool gen_start_condition() 
{
    //Release SCL to ensure that (repeated) Start can be performed 
    SCL_HI(); 
    wait_SCL_HI(); //Verify that SCL becomes high.
    delay_T2();

    //Generate start condition
    SDA_LO(); 
    delay_T4();
    SCL_LO(); 
    SDA_HI(); 

    if( !(USISR & (1 << USISIF)) ) { 
        return false; //Failed to detect start condition
    }

    return true;
}

bool gen_stop_condition() 
{
    SDA_LO(); 
    SCL_HI();
    wait_SCL_HI(); //Wait if slave is not ready.
    delay_T4();
    SDA_HI();
    delay_T2();
  
    if( !(USISR & (1 << USIPF)) ) { 
        return false; //Failed to detect stop condition
    }

    return true;
}

void send_ACK() 
{
    USIDR = 0x00; //ACK is 0
    transfer_1bit();
    reset_USIDR();
}

void send_NACK() 
{
    USIDR = 0xFF; //NACK is 1
    transfer_1bit();
    reset_USIDR();
}

bool I2C_init_write(uint8_t address) 
{
    gen_start_condition();

    //Write is 7 address bits and a final 0
    return I2C_write((address << 1) | 0); 
}

bool I2C_write(uint8_t data) 
{
    //Write data
    SCL_LO(); //Pull SCL LOW to modify USIDR.
    USIDR = data; //Setup data.
    transfer_8bit(); 
    reset_USIDR();

    //Check ack/nack response from slave
    SDA_IN(); //SDA as input to read
    transfer_1bit(); //Read ACK/NACK
    data = USIDR; //Reuse data variable to immediately reset data register
    reset_USIDR();
    SDA_OUT(); //Restore as output

    if (data & (1 << 0)) { //NACK is a 1 at position 0
        return false; //Recieved NACK
    }

    return true; //ACK is a 0 at position 0, write successful
}

bool I2C_init_read(uint8_t address) 
{
    gen_start_condition();

    //Read is 7 address bits and a final 1
    return I2C_write((address << 1) | 1);
}

uint8_t read() 
{
    SDA_IN(); //SDA input to read
    transfer_8bit();
    uint8_t data = USIDR;
    reset_USIDR();
    SDA_OUT(); //Restore as output

    return data;
}

uint8_t I2C_read() 
{
    uint8_t data = read();
    send_ACK(); //Keep reading
    return data;
}

uint8_t I2C_read_last() 
{
    uint8_t data = read();
    send_NACK(); //Terminate reading
    return data;
}

void I2C_stop() 
{
    gen_stop_condition();
}