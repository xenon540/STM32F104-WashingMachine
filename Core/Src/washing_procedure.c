#include "washing_procedure.h"

//static int washing_cycle = 0;
//static uint32_t washing_timer = 0;

uint8_t m_mode_select[3];
uint32_t m_current_water_level;

uint32_t m_wash_interval; //tong thoi gian chay cua buoc giat
uint8_t m_drain_times; //so lan xa
bool wash_done; // bien trang thai buoc giat xong
bool *p_motorRun; //pointer to motorRun
/*Cac bien dung de luu trang thai, dung de luu vao eeprom khi mat dien*/
uint32_t elapsed_time_per_mode; //thoi gian da chay cua tung buoc giat, dung de luu lai khi mat dien
uint8_t drained_times; //so lan da xa
uint8_t current_step; // buoc giat hien tai: 1 - xa nuoc, 2 - giat, 3 - xa, 4 - vat

void (*runningFunc)();

void procedure_init( void ) {
    // run once just to reset the procedure sequence
    runningFunc = &start_procedure;
}
void run_procedure( uint8_t mode[], uint8_t water_level, bool *procedure_run_flag, bool *motorRun, uint16_t *alpha) {
    /*Các biến được truyền vào dạng tham chiếu nhằm thay đổi giá trị trong các hàm này*/
    m_current_water_level = water_level; //gán giá trị
    if (wash_done) {
        /* after washing done, stop procedure*/
        *procedure_run_flag = false;
    }
    if (runningFunc == &start_procedure) {
        wash_done = false;
        p_motorRun = motorRun; //gán địa chỉ, chỉ cần gán 1 lần nên bỏ vô đây
        m_mode_select[0] = mode[0];
        m_mode_select[1] = mode[1];
        m_mode_select[2] = mode[2];
        *alpha = 1000;
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
                m_wash_interval = 15000; //giat nhe, quay cham hon
                *alpha = 750; //giặt nhẹ đặt alpha bé, quay chậm hơn
                break;
            case 5: 
                m_wash_interval = 0; // chi vat, k giat
        }
        m_drain_times = m_mode_select[2];
        drained_times = 0;
    }
    runningFunc();
}

void run_procedure_backup( uint8_t backup_mode_data[], uint8_t water_level, bool *backup_run_flag, bool *motorRun, uint16_t *alpha) {
    /*Các biến được truyền vào dạng tham chiếu nhằm thay đổi giá trị trong các hàm này*/
    if (wash_done) {
        /* after washing done, stop procedure*/
        *backup_run_flag = false;
    }
    m_current_water_level = water_level;
    p_motorRun = motorRun;
    if (runningFunc == &start_procedure) {
        wash_done = false;
        m_mode_select[0] = backup_mode_data[0];
        m_mode_select[1] = backup_mode_data[1];
        m_mode_select[2] = backup_mode_data[2];
        *alpha = 1000;
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
                m_wash_interval = 15000; //giat nhe, quay cham hon
                *alpha = 750; //giặt nhẹ đặt alpha bé, quay chậm hơn
                break;
            case 5: 
                m_wash_interval = 0; // chi vat, k giat
        }
        m_drain_times = m_mode_select[2];
        drained_times = backup_mode_data[4];
        char data[16];
        sprintf(data, "woo:%dc:%d%%tik:%d", drained_times, water_level , HAL_GetTick());
        lcd_goto_XY(1, 0);
        lcd_send_string(data);
        switch (backup_mode_data[3]) {
            case 1: 
                runningFunc = &m_fillWater;
                lcd_goto_XY(2, 0);
                lcd_send_string("test backup mode");
                break;
            case 2:
                runningFunc = &m_wash;
                break;
            case 3: 
                runningFunc = &m_drain;
                break;
            case 4:
                runningFunc = &m_spin;
                break;
            case 5:
                making_3_beep_sound;
        }
    }
    runningFunc();
}

void start_procedure(uint8_t mode[]) {
    runningFunc = &m_fillWater;
}

void m_fillWater( void ) {
    if (m_mode_select[0] == 5) {
        /* Neu chi vat, khong cap nuoc, goi xuong luon ham chi vat */
        runningFunc = &m_spin;
    } else {
        current_step = 1;
        if (    m_current_water_level <= m_mode_select[1]*10 ) { 
            /*
            * Ham chu trinh xa nuoc 
            * Neu nuoc chua du, tiep tuc xa nuoc
            */
        	uint8_t data[16];
            lcd_goto_XY(1, 0);
            lcd_send_string("Filling water...");
            lcd_goto_XY(2, 0);
            lcd_send_string("                ");
            HAL_GPIO_WritePin(water_in_GPIO_Port, water_in_Pin, GPIO_PIN_SET);
        } else {
            HAL_GPIO_WritePin(water_in_GPIO_Port, water_in_Pin, GPIO_PIN_RESET);
            //cap nuoc xong, chuyen sang ham tiep theo
            runningFunc = &m_wash;
        }
    }
    
}
void m_wash( void ) {
    /*
    * Ham chu trinh giat
    */
    static bool firstRun_wash = true, motor_dir;
    static uint32_t time_wash, change_direction_time;
    current_step = 2;
    elapsed_time_per_mode = time_wash + m_wash_interval - HAL_GetTick();
    if ( firstRun_wash ) {
        time_wash = HAL_GetTick(); // thoi gian bat dau giat
        change_direction_time = time_wash; 
        firstRun_wash = false;
    }
    
    char data[16];
    lcd_goto_XY(1, 0);
    lcd_send_string(motor_dir? "Washing -       ":"Washing +       ");
    sprintf(data, "tm:%ds sp:%ds          ", (HAL_GetTick()-time_wash)/1000, m_wash_interval/1000);
    lcd_goto_XY(2, 0);
    lcd_send_string(data);
    if (m_mode_select[0] != 2 ) { // giat ngam
        /*
        * Giat ngam trong 25s:
        * giat 10s, ngam 5s, giat tiep 10s
        */
        if (HAL_GetTick() - time_wash < m_wash_interval) {
            if ((HAL_GetTick() - time_wash < 10000) || (HAL_GetTick() - time_wash > 15000)) {
                *p_motorRun = true;
                HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, motor_dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
                if (HAL_GetTick() - change_direction_time > 3000) {
                    // dao chieu moi 3s
                    motor_dir = !motor_dir;
                    change_direction_time = HAL_GetTick();
                }
            }
            else {
                /* tat dong co, ngam quan ao trong 5s*/
                *p_motorRun = false;
            }
        } else {
            /* Giat xong, tat dong co, chuyen sang vat va xa */
            *p_motorRun = false;
            firstRun_wash = true; //reset bit
            runningFunc = &m_drain; //giat xong, chuyen sang xa
        }
    } else {
        /* Cac mode giat con lai */
        if (HAL_GetTick() - time_wash < m_wash_interval) {
            *p_motorRun = true;
            HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, motor_dir ? GPIO_PIN_SET : GPIO_PIN_RESET);
            if (HAL_GetTick() - change_direction_time > 3000) {
                // dao chieu moi 3s
                motor_dir = !motor_dir;
                change_direction_time = HAL_GetTick();
            }
        } else {
            /* Giat xong, tat dong co, chuyen sang vat va xa */
            *p_motorRun = false;
            firstRun_wash = true; //reset bit
            runningFunc = &m_drain; //giat xong, chuyen sang xa
        }
    }
}

void m_drain( void ) {
    /*
    * Ham chu trinh vat xa
    */
    current_step = 3;
    char data[16];
    lcd_goto_XY(1, 0);
    lcd_send_string("Draining...     ");
    sprintf(data, "cur:%d set:%d  ", drained_times, m_mode_select[2]);
    lcd_goto_XY(2, 0);
    lcd_send_string(data);
    if (m_current_water_level >= 10) {
        HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
        lcd_goto_XY(2, 4);
        lcd_send_string("got here");
        drained_times++;
        if (drained_times != m_mode_select[2]) {
            runningFunc = &m_fillWater;
        } else {
            // xa du so lan roi thi xoay de vat thoi let's go
            drained_times = 0; //reset bien dem
            runningFunc = &m_spin;
        }
        
    }
}

void m_spin( void ) {
    /*
    * Ham chi xa, quay dong co max 10s
    */
    current_step = 4;
    //nao mat dien ma co dien lai thi cho quay lai tu dau cua buoc nay luon cho roi =)))

    static bool firstRun_spin = true;
    static uint32_t time_spin;
    if ( firstRun_spin ) {
        time_spin = HAL_GetTick(); // thoi gian bat dau quay
        firstRun_spin = false;
    }

    char data[16];
    sprintf(data, "tm:%ds sp:%ds", (HAL_GetTick()-time_spin)/1000, 10 );
    lcd_goto_XY(1, 0);
    lcd_send_string("Spinning...     ");
    lcd_goto_XY(2, 0);
    lcd_send_string(data);

    if (HAL_GetTick() - time_spin < 10000) {
        HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_SET);
        *p_motorRun = true;
        HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_RESET);
        *p_motorRun = false;
        HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, GPIO_PIN_RESET);
        firstRun_spin = true; //reset bit
        runningFunc = &making_3_beep_sound; //go to beep
    }
}

void making_3_beep_sound( void ) {
	/* making beep sound, times is number of beep sound*/
    static bool firstRun_beep = true;
    static bool beep = false;
    static uint32_t time_beep;
    static uint8_t beep_count = 0;
    current_step = 5;
    lcd_clear_display();
    lcd_goto_XY(2, 4);
    lcd_send_string("Wash done!");

    if ( firstRun_beep ) {
        time_beep = HAL_GetTick(); // thoi gian bat dau quay
        firstRun_beep = false;
        beep_count = 0;
    }
    if ( beep_count > 3) {
        wash_done = true;
        firstRun_beep = true;
        HAL_GPIO_WritePin(buzzer_GPIO_Port, buzzer_Pin, GPIO_PIN_RESET);
        runningFunc = &start_procedure; //return to start function
        HAL_Delay(100);
    } else {
        HAL_GPIO_WritePin(buzzer_GPIO_Port, buzzer_Pin, beep ? GPIO_PIN_SET : GPIO_PIN_RESET);
        if (HAL_GetTick() - time_beep > 800) {
            beep = !beep;
            beep_count += (beep ? 1 : 0);
            time_beep = HAL_GetTick();
        }
    }
}
