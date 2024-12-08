#ifndef WASHING_PROCEDURE_H
#define WASHING_PROCEDURE_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f1xx_hal.h"
#include "pins_def.h"

/*extern may cai bien nay de dung trong main*/
extern uint32_t elapsed_time_per_mode; 
extern uint8_t drained_times; 
extern uint8_t current_step;

void procedure_init( void );
void run_procedure( uint8_t mode[], uint8_t water_level, bool *procedure_run_flag, bool *motorRun, uint16_t *alpha );
void run_procedure_backup( uint8_t backup_mode_data[], uint8_t water_level, bool *backup_run_flag, bool *motorRun, uint16_t *alpha);
void start_procedure(uint8_t mode[]);

void m_fillWater( void );
void m_wash( void );
void m_drain( void );
void m_spin( void );

void making_3_beep_sound( void );

#endif
