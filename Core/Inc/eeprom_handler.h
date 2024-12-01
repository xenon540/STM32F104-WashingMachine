#ifndef EEPROM_HANDLER_H
#define EEPROM_HANDLER_H
#include "stm32f1xx_hal.h"
extern I2C_HandleTypeDef hi2c2;
#define		_EEPROM_SIZE_KBIT		256       /* 256K (32,768 x 8) */
#define		_EEPROM_I2C			hi2c2
#define		_EEPROM_ADDRESS			0xA0

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
  * @brief  Checks if memory device is ready for communication.
  * @param  none
  * @retval bool
  */
bool at24_isConnected(void);

/**
  * @brief  Write an amount of data in blocking mode to a specific memory address
  * @param  address Internal memory address
  * @param  data Pointer to data buffer
  * @param  len Amount of data to be sent
  * @param  timeout Timeout duration
  * @retval bool status
  */
bool at24_write(uint16_t address, uint8_t *data, size_t lenInBytes, uint32_t timeout);

/**
  * @brief  Read an amount of data in blocking mode to a specific memory address
  * @param  address Internal memory address
  * @param  data Pointer to data buffer
  * @param  len Amount of data to be sent
  * @param  timeout Timeout duration
  * @retval bool status
  */
bool at24_read(uint16_t address, uint8_t *data, size_t lenInBytes, uint32_t timeout);

/**
  * @brief  Erase memory.
  * @note   This requires time in seconds
  * @param  none
  * @retval bool status
  */
bool at24_eraseChip(void);

int8_t check_eeprom(uint8_t backupData_eeprom[] );

#endif

/* EOF */

