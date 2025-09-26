#ifndef USI_I2C_Master_H_
#define USI_I2C_Master_H_

#include <stdint.h>
#include <stdbool.h>

void I2C_init();

bool I2C_init_write(uint8_t address);
bool I2C_write(uint8_t data);

bool I2C_init_read(uint8_t address);
uint8_t I2C_read();
uint8_t I2C_read_last();

void I2C_stop();

#endif