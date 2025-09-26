#include "DS3231.h"

#include "USI_I2C_Master.h"

#define DS3231_ADDRESS          0x68  
#define DS3231_CONTROLREG       0x0E  
#define DS3231_STATUSREG        0x0F  
#define DS3231_TEMPERATUREREG   0x11  //Temperature register high byte (low byte is at 0x12), 10-bit temperature value


/*----- HELPER FUNCTIONS -----*/

uint8_t read_register(uint8_t address, uint8_t reg) 
{
    I2C_init_write(address);
    I2C_write(reg);
    I2C_init_read(address);
    uint8_t data = I2C_read_last();
    I2C_stop();

    return data;
}

bool write_register(uint8_t address, uint8_t reg, uint8_t data) 
{
    I2C_init_write(address);
    I2C_write(reg);
    bool success = I2C_write(data);
    I2C_stop();
    return success;
}

//DS3231 saves data in bcd format. Functions for conversions:
static uint8_t bcd_to_bin (uint8_t val) { return val - 6 * (val >> 4); }
static uint8_t bin_to_bcd (uint8_t val) { return val + 6 * (val / 10); }


//reset oscillator stop flag bit in the DS3231 status register
bool reset_OSF_bit()
{
    uint8_t statusreg = read_register(DS3231_ADDRESS, DS3231_STATUSREG);
    statusreg &= ~(1 << 7); //clear OSF bit
    return write_register(DS3231_ADDRESS, DS3231_STATUSREG, statusreg);
}

/*----------------------------*/


void RTC_init() { /*Nothing to do*/ }

bool RTC_lost_power() 
{
    return (read_register(DS3231_ADDRESS, DS3231_STATUSREG) >> 7);
}

bool RTC_set_time(Time_t time) 
{
    I2C_init_write(DS3231_ADDRESS);
    I2C_write(0); //start at location 0
    I2C_write(0); //seconds
    I2C_write(bin_to_bcd(time.mm));
    I2C_write(bin_to_bcd(time.hh));
    //I2C_write(0); //day sun-sat
    //I2C_write(0); //day 0-31
    //I2C_write(0); //month
    //I2C_write(0); //year
    I2C_stop();

    //time is correct now, clear the oscillator stop flag
    return reset_OSF_bit();
}

Time_t RTC_get_time() 
{
    Time_t time;

    I2C_init_write(DS3231_ADDRESS);
    I2C_write(0);
    I2C_init_read(DS3231_ADDRESS);
    I2C_read();  //seconds
    time.mm = bcd_to_bin(I2C_read()); 
    time.hh = bcd_to_bin(I2C_read_last()); 
    //I2C_read(); // day 0-7
    //I2C_read(); //day 0-31
    //I2C_read(); //month
    //I2C_read_last(); //year
    I2C_stop();
    return time;
}

float RTC_get_temperature() 
{
    //temperature is updated every 64 seconds. 
    //Optionally you can force an update. Not implemented

    uint8_t temp_H, temp_L;
    I2C_init_write(DS3231_ADDRESS);
    I2C_write(DS3231_TEMPERATUREREG);
    I2C_init_read(DS3231_ADDRESS);
    temp_H = I2C_read(); 
    temp_L = I2C_read_last();
    I2C_stop();

    return (float) temp_H + (temp_L >> 6) * 0.25f;
}