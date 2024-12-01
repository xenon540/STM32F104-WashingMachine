#ifndef WASHING_PROCEDURE_H
#define WASHING_PROCEDURE_H

#include <stdbool.h>
#include "stm32f1xx_hal.h"
#include "pins_def.h"

void procedure_init( void );
void run_procedure( uint8_t mode[], uint8_t water_level, bool *procedure_run_flag );
void start_procedure(uint8_t mode[]);

void m_fillWater( void );
void m_wash( void );
void m_rinse_and_drain( void );
void m_spin( void );

#endif
