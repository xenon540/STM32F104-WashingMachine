#include "washing_procedure.h"

//static int washing_cycle = 0;
//static uint32_t washing_timer = 0;

uint8_t m_mode_select[3];
uint8_t m_current_water_level;

uint32_t m_wash_interval; //tong thoi gian chay cua buoc giat
uint8_t m_rinse_and_drain_times; //so lan vat xa
bool wash_done; // bien trang thai buoc giat xong

void (*runningFunc)();

void procedure_init( void ) {
    // run once just to reset the procedure sequence
    runningFunc = &start_procedure;
}
void run_procedure( uint8_t mode[], uint8_t water_level, bool *procedure_run_flag) {
    if (wash_done) {
        /* after washing done, stop procedure*/
        *procedure_run_flag = false;
    }
    m_current_water_level = water_level;
    if (runningFunc == &start_procedure) {
        wash_done = false;
        m_mode_select[0] = mode[0];
        m_mode_select[1] = mode[1];
        m_mode_select[2] = mode[2];
        switch( m_mode_select[0]) {
            case 1: 
                m_wash_interval = 15000; //giat thuong, giat trong 15s
                break;
            case 2:
                m_wash_interval = 25000; //giat ngam, giat trong 25s
                break;
            case 3: 
                m_wash_interval = 10000; //giat nhanh, giat trong 10s
                break;
            case 4:
                m_wash_interval = 0; // vat va xa, k giat
                break;
            case 5: 
                m_wash_interval = 0; // chi vat, k giat
        }
        m_rinse_and_drain_times = m_mode_select[2];
    }
    runningFunc();
}

void start_procedure(uint8_t mode[]) {
    runningFunc = &m_fillWater;
}

void m_fillWater( void ) {
    lcd_goto_XY(2, 0);
    lcd_send_string("filling wota    ");
    if (m_mode_select[0] == 5) {
        /* Neu chi xa, khong cap nuoc, goi xuong luon ham chi xa */
        runningFunc = &m_spin;
    } else {
        if ( m_current_water_level <= m_mode_select[1]*10 ) {
        /*
        * Ham chu trinh xa nuoc 
        * Neu nuoc chua du, tiep tuc xa nuoc
        */
            HAL_GPIO_WritePin(water_in_GPIO_Port, water_in_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(water_in_GPIO_Port, water_in_Pin, GPIO_PIN_RESET);
            //cap nuoc xong, chuyen sang ham tiep theo
            if (!wash_done) {
                runningFunc = &m_wash;
            } else {
                runningFunc = &m_rinse_and_drain;
            }
        }
    }
    
}
void m_wash( void ) {
    /*
    * Ham chu trinh giat
    */
    static bool firstRun_wash = true, motor_dir;
    static uint32_t time_wash, change_direction_time;
    if ( firstRun_wash ) {
        time_wash = HAL_GetTick(); // thoi gian bat dau giat
        change_direction_time = time_wash; 
        firstRun_wash = false;
    }
    
    char data[16];
    // sprintf(data, "Wa:%d %d", m_current_water_level, m_mode_select[1]*10 );
    // lcd_goto_XY(1, 0);
    // lcd_send_string(data);
    // lcd_goto_XY(2, 0);
    // lcd_send_string(motor_dir? "washin -":"washin +");

    if (m_mode_select[0] != 2 ) { // giat ngam
        /*
        * Giat ngam trong 25s:
        * giat 10s, ngam 5s, giat tiep 10s
        */
        if (HAL_GetTick() - time_wash < m_wash_interval) {
            if ((HAL_GetTick() - time_wash < 10000) || (HAL_GetTick() - time_wash > 15000)) {
                HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_SET);
                if (HAL_GetTick() - change_direction_time > 3000) {
                    // dao chieu moi 3s
                    motor_dir = !motor_dir;
                    change_direction_time = HAL_GetTick();
                }
                HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, motor_dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
            }
            else {
                /* tat dong co, ngam quan ao trong 5s*/
                HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_RESET);
            }
        } else {
            /* Giat xong, tat dong co, chuyen sang vat va xa */
            HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_RESET);
            firstRun_wash = true; //reset bit
            runningFunc = &m_rinse_and_drain; //giat xong, chuyen sang xa
        }
    } else {
        /* Cac mode giat con lai */
        if (HAL_GetTick() - time_wash < m_wash_interval) {
            HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_SET);
            if (HAL_GetTick() - change_direction_time > 3000) {
                // dao chieu moi 3s
                motor_dir = !motor_dir;
                change_direction_time = HAL_GetTick();
            }
            HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, motor_dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
        } else {
            /* Giat xong, tat dong co, chuyen sang vat va xa */
            HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_RESET);
            firstRun_wash = true; //reset bit
            runningFunc = &m_rinse_and_drain; //giat xong, chuyen sang xa
        }
    }
}

void m_rinse_and_drain( void ) {
    /*
    * Ham chu trinh vat xa
    */
    static uint8_t rinse_times = 0; // so lan vat xa da thuc hien
    char data[16];
    // sprintf(data, "rnd:%d-  %d         ", m_current_water_level, rinse_times);
    // lcd_goto_XY(1, 0);
    // lcd_send_string(data);
    // lcd_goto_XY(2, 0);
    // lcd_send_string("rinse nad drain ");
    if (m_current_water_level >= 10) {
        HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        rinse_times++;
        if (rinse_times != m_mode_select[2]) {
            runningFunc = &m_fillWater;
        } else {
            // xa du so lan roi thi xoay de vat thoi let's go
            rinse_times = 0;
            runningFunc = &m_spin;
        }
        
    }
}

void m_spin( void ) {
    /*
    * Ham chi xa, quay dong co max 10s
    */
    static bool firstRun_spin = true;
    static uint32_t time_spin;
    if ( firstRun_spin ) {
        time_spin = HAL_GetTick(); // thoi gian bat dau quay
        firstRun_spin = false;
    }

    // char data[16];
    // sprintf(data, "sp:%d %d", time_spin, HAL_GetTick() );
    // lcd_goto_XY(1, 0);
    // lcd_send_string(data);
    // lcd_goto_XY(2, 0);
    // lcd_send_string(wash_done? "spuin         ":"spinning        ");

    if (HAL_GetTick() - time_spin < 10000) {
        HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, GPIO_PIN_SET);
    } else {
        wash_done = true;
        firstRun_spin = true; //reset bit
        runningFunc = &start_procedure; //return to start function
    }
}
