/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
#include <stdio.h>

#include "pins_def.h"
#include "i2c_lcd.h"
#include "eeprom_handler.h"
#include "washing_procedure.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;

/* USER CODE BEGIN PV */
volatile bool onStart = true, backup_run_flag = false, power_loss_flag = false; 
volatile int8_t procedure_run_flag = 0; // flag variable to indicate running washing procedure or not

bool relayOn = 0, motorRun = 0, paused = 0;
volatile bool ZC = 0; // zero crossing detected
volatile uint16_t freq_count = 0; //variable to count pulse from water sensor
volatile uint8_t water_level; //global variable to store water level

uint16_t alpha;						// variable to control motor speed (dimmer)
volatile uint8_t mode_select[3] = {0, 0, 0}; // mang dung luu gia tri che do giat
uint8_t backupData_eeprom[9];		// only use 9 bytes to store recovery data, see power_observer to know role of each byte
const char *mode_names[5] = {"Giat thuong", "Giat ngam", "Giat nhanh", "Giat nhe", "Chi vat"};
volatile bool flag; // flag to keep home screen not refreshing continuously/anti flickering
bool has_backup_data;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */
void updateLCD( void );
void power_observer( void );
void making_beep_sound( int times );
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == watersensor_input_Pin)
	{
		freq_count++;
	}
	if (GPIO_Pin == pulse_input_Pin)
	{
		ZC = true;
		// if (power_loss_flag) {
		// 	power_loss_flag = false;
		// 	HAL_NVIC_SystemReset();
		// }
		// turn on triac and start timer 2
		if (motorRun)
		{
			// __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, alpha); // Change CCR1 value dynamically
			// HAL_TIM_OC_Start_IT(&htim2, TIM_CHANNEL_1);			 // Starts the timer
			HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_SET);
		} else {
			HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_RESET);
		}
	}
	if (GPIO_Pin == button1_Pin)
	{
		HAL_ResumeTick();
		if (has_backup_data)
		{
			/* nếu là vừa mở máy, có dữ liệu backup và nút start được nhấn thì chạy ctrinh backup*/
			backup_run_flag = true; // fix hêrre
			has_backup_data = false;
		}
		else
		{
			// if (procedure_run_flag == 1) {
			// 	flag = true;
			// 	paused = true;
			// 	procedure_run_flag = -4; //tam dung
			// } else if(procedure_run_flag == -4) {
			// 	flag = true;
			// 	paused = false;
			// 	procedure_run_flag = 1; // tiep tuc
			// }
			if ( !paused ) {
				if (!HAL_GPIO_ReadPin(door_sensor_GPIO_Port, door_sensor_Pin))
				{
					if ((mode_select[0] != 0 && mode_select[1] != 0 && mode_select[2] != 0) || (mode_select[0] == 5))
					{
						flag = true;
						procedure_run_flag = 1; // chạy ok
					}
					else
					{
						flag = true;
						procedure_run_flag = -1; //lỗi chưa chọn đủ input
					}
					if (procedure_run_flag == -3) { // tiếp tục chạy sau khi đã đóng cửa
						procedure_run_flag = 1;
					}
				}
				else
				{
					flag = true;
					procedure_run_flag = -2; //lỗi chưa đongs cửa
				}
			}
		}
	}
	if (!procedure_run_flag) {
		if (GPIO_Pin == button2_Pin)
		{
			/*
			mode_select[0] - bien luu che do giat
			== 1 : giat thuong
			== 2 : giat ngam (giat lau lau lau)
			== 3 : giat nhanh (giat nhe)
			== 4 : vat va xa
			== 5 : chi xa
			*/
			mode_select[0]++;
			if (mode_select[0] > 5)
			{
				mode_select[0] = 1;
			}
			has_backup_data = false;
		}
		if (GPIO_Pin == button3_Pin)
		{
			/*
			mode_select[1] - bien luu muc nuoc
			== i : muc nuoc = (i*10)%
			*/
			mode_select[1]++;
			if (mode_select[1] > 10)
			{
				mode_select[1] = 1;
			}
			has_backup_data = false;
		}
		if (GPIO_Pin == button4_Pin)
		{
			/*
			mode_select[2] - bien luu so lan xa
			== i : so lan xa = i (i <= 3)
			*/
			mode_select[2]++;
			if (mode_select[2] > 3)
			{
				mode_select[2] = 1;
			}
			has_backup_data = false;
		}
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM4)
	{
		// Timer 1 count done, means freq_count done in 1s
		// frequency = freq_count
		/*
		 * Water level: 0% ~ f = 11300Hz; 100% ~ f = 9000Hz
		 * Need to make a graph for this, temporarily use Linear
		 */
		int cal_level  = (int)((-0.04563 * freq_count) + 468.5045); // convert into 0-100% ax+b
		if (cal_level < 0) {
			water_level = 0;
		} else {
			water_level = cal_level;
		}
		// char data[16];
		// sprintf(data, "w:%d c:%d%%f:%d", freq_count, water_level , HAL_GetTick());
		// lcd_goto_XY(1, 0);
		// lcd_send_string("test water level");
		// lcd_goto_XY(2, 0);
		// lcd_send_string(data);
		freq_count = 0;
	}
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_ADC1_Init();
	MX_TIM2_Init();
	MX_I2C1_Init();
	MX_I2C2_Init();
	MX_TIM4_Init();
	/* USER CODE BEGIN 2 */
	HAL_Delay(1000); // delay 1000ms to wait for everything
	lcd_init();
	updateLCD();
	onStart = false;
	procedure_init();

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		// updateLCD();
		power_observer();
		if (backup_run_flag) {
			if (HAL_GPIO_ReadPin(door_sensor_GPIO_Port, door_sensor_Pin) && current_step!=1) {
				/*if door is open when in other step than filling water, stop program and display error message*/
				motorRun = false; // tắt động cơ
				flag = true; //display door error message
				procedure_run_flag = -3;
				HAL_SuspendTick();
				HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_RESET);
				HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_RESET);
			} else {
				run_procedure_backup(mode_select, water_level, &backup_run_flag, &motorRun, &alpha);
			}
		} else {
			if (procedure_run_flag == 1 )
			{
				if (HAL_GPIO_ReadPin(door_sensor_GPIO_Port, door_sensor_Pin) && current_step!=1) {
					/*if door is open when in other step than filling water, stop program and display error message*/
					motorRun = false; // tắt động cơ
					flag = true; //display door error message
					procedure_run_flag = -3;
					HAL_SuspendTick();
					HAL_GPIO_WritePin(drain_gate_GPIO_Port, drain_gate_Pin, GPIO_PIN_RESET);
					HAL_GPIO_WritePin(triac_gate_GPIO_Port, triac_gate_Pin, GPIO_PIN_RESET);
				} else {
					run_procedure(mode_select, water_level, &procedure_run_flag, &motorRun, &alpha);
				}
			}
		}
		
		// if (!relayOn) {
		// 	HAL_GPIO_WritePin(buzzer_GPIO_Port, buzzer_Pin, GPIO_PIN_RESET);
		// 	HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, GPIO_PIN_RESET);
		// } else {
		// 	HAL_GPIO_WritePin(buzzer_GPIO_Port, buzzer_Pin, GPIO_PIN_SET);
		// 	HAL_GPIO_WritePin(relay_GPIO_Port, relay_Pin, GPIO_PIN_SET);
		// }
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
	{
		Error_Handler();
	}
	PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
	PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void)
{

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_ChannelConfTypeDef sConfig = {0};

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */

	/** Common config
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.NbrOfConversion = 1;
	if (HAL_ADC_Init(&hadc1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Regular Channel
	 */
	sConfig.Channel = ADC_CHANNEL_2;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void)
{

	/* USER CODE BEGIN I2C1_Init 0 */

	/* USER CODE END I2C1_Init 0 */

	/* USER CODE BEGIN I2C1_Init 1 */

	/* USER CODE END I2C1_Init 1 */
	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN I2C1_Init 2 */

	/* USER CODE END I2C1_Init 2 */
}

/**
 * @brief I2C2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C2_Init(void)
{

	/* USER CODE BEGIN I2C2_Init 0 */

	/* USER CODE END I2C2_Init 0 */

	/* USER CODE BEGIN I2C2_Init 1 */

	/* USER CODE END I2C2_Init 1 */
	hi2c2.Instance = I2C2;
	hi2c2.Init.ClockSpeed = 100000;
	hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c2.Init.OwnAddress1 = 0;
	hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c2.Init.OwnAddress2 = 0;
	hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c2) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN I2C2_Init 2 */

	/* USER CODE END I2C2_Init 2 */
}

/**
 * @brief TIM2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM2_Init(void)
{

	/* USER CODE BEGIN TIM2_Init 0 */

	/* USER CODE END TIM2_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};

	/* USER CODE BEGIN TIM2_Init 1 */

	/* USER CODE END TIM2_Init 1 */
	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 80-1;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 0xFFFF - 1;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIM_OC_Init(&htim2) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_TIMING;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM2_Init 2 */

	/* USER CODE END TIM2_Init 2 */
	HAL_TIM_MspPostInit(&htim2);
}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void)
{

	/* USER CODE BEGIN TIM4_Init 0 */

	/* USER CODE END TIM4_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM4_Init 1 */

	/* USER CODE END TIM4_Init 1 */
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 8000 - 1;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = 1000 - 1;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM4_Init 2 */
	HAL_TIM_Base_Start_IT(&htim4);
	/* USER CODE END TIM4_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1 | GPIO_PIN_12, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5, GPIO_PIN_RESET);

	/*Configure GPIO pin : PC13 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PA1 PA12 */
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PB12 PB13 PB14 PB15 */
	GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : PA15 */
	GPIO_InitStruct.Pin = GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PB3 PB4 PB5 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : PB6 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : PB7 */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* EXTI interrupt init*/
	HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

	HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */
void updateLCD( void )
{
	/*
	Updating LCD if index is different from current index;
	*/
	static uint32_t previous_tick = 0;		 // variable to store previous time
	static int previous_mode[3] = {0, 0, 0}; // make sure initial current == mode_select to display first message
	if (onStart)
	{
		lcd_goto_XY(1, 0);
		lcd_send_string("MyWashingMachine");
		int8_t retval = check_eeprom(backupData_eeprom);
		switch (retval)
		{
		case -1: // eeprom error code
			lcd_goto_XY(2, 0);
			lcd_send_string("Eeprom failed!");
			return;
		case 0: // eeprom ok, no recovery data
			lcd_goto_XY(2, 0);
			lcd_send_string("Eeprom OK!");
			return;
		case 1:
			lcd_goto_XY(1, 0);
			char data[16];
			sprintf(data, "Backup: %d %d %d %d        ", backupData_eeprom[0], backupData_eeprom[1], backupData_eeprom[2], backupData_eeprom[3]);
			lcd_send_string(data);
			// lcd_send_string("Bkup available! ");
			lcd_goto_XY(2, 0);
			lcd_send_string("START to resume ");
			has_backup_data = true;
			return;
		}
	}
	else
	{
		/* nếu chọn chế độ thay đổi thì mới cập nhật LCD */
		for (int i = 0; i < 3; i++)
		{
			if (previous_mode[i] != mode_select[i])
			{
				flag = true;
				previous_tick = HAL_GetTick();
				previous_mode[i] = mode_select[i];
				if (i == 0)
				{
					/*
					 * che do giat changed
					 */
					lcd_goto_XY(1, 0);
					lcd_send_string("CHON MODE GIAT: ");
					lcd_goto_XY(2, 0);
					char data[16];
					sprintf(data, "%s             ", mode_names[previous_mode[0] - 1]);
					lcd_send_string(data);
					return;
				}
				if (i == 1)
				{
					/*
					 * Muc nuoc changed
					 */
					lcd_goto_XY(1, 0);
					lcd_send_string("CHON MUC NUOC:  ");
					char data[16];
					sprintf(data, "%d %%           ", previous_mode[i] * 10);
					lcd_goto_XY(2, 0);
					lcd_send_string(data);
					return;
				}
				if (i == 2)
				{
					/*
					 * Chu ky xa changed
					 */
					lcd_goto_XY(1, 0);
					lcd_send_string("CHON SO LAN XA: ");
					char data[16];
					sprintf(data, "Xa %d lan nuoc   ", previous_mode[i]);
					lcd_goto_XY(2, 0);
					lcd_send_string(data);
					return;
				}
			}
		}
		if (procedure_run_flag == 1 && flag)
		{
			/*hiển thị trực tiếp trong washing_procedure nên khi chạy thì k hiện gì ở đây*/
			return;
		}
		else if (procedure_run_flag == -1 && flag)
		{
			flag = false;
			lcd_goto_XY(1, 0);
			lcd_send_string("Select all input");
			lcd_goto_XY(2, 0);
			lcd_send_string("to run!         ");
			procedure_run_flag = 0; // set procedure_run_flag = 0 to display Home back
			return;
		}
		else if (procedure_run_flag == -2 && flag)
		{
			flag = false;
			lcd_goto_XY(1, 0);
			lcd_send_string("Lid not closed, ");
			lcd_goto_XY(2, 0);
			lcd_send_string("close to run!   ");
			procedure_run_flag = 0; // set procedure_run_flag = 0 to display Home back
			return;
		} else if (procedure_run_flag == -3 && flag)
		{
			flag = false;
			lcd_goto_XY(1, 0);
			lcd_send_string("Lid oppened, cl-");
			lcd_goto_XY(2, 0);
			lcd_send_string("ose to continue!");
			procedure_run_flag = 0; // set procedure_run_flag = 0 to display Home back
			return;
		} else if (procedure_run_flag == -4 && flag)
		{
			flag = false;
			lcd_clear_display();
			lcd_goto_XY(1, 0);
			lcd_send_string("Washing paused! ");
			return;
		} 
		if ((HAL_GetTick() - previous_tick > 2000) && flag)
		{
			// This is HOME SCREEN, get back to Home after 2000ms
			flag = false;
			lcd_clear_display();
			char data[16];
			sprintf(data, " %s", mode_names[previous_mode[0] - 1]);
			lcd_goto_XY(1, 0);
			lcd_send_string(data);
			sprintf(data, "Nuoc: %d%% Xa: %d", previous_mode[1] * 10, previous_mode[2]);
			lcd_goto_XY(2, 0);
			lcd_send_string(data);
		}
	}
}

void power_observer(void)
{
	static uint32_t power_observer_tick;
	static bool data_saved = false;
	if (ZC == true)
	{
		power_observer_tick = HAL_GetTick();
	}

	if (HAL_GetTick() - power_observer_tick > 40)
	{
		// if no pulse in 40ms = 2 cycle means power loss detected
		//  start saving backup data if running (procedure_run_flag == true)
		if (procedure_run_flag && at24_isConnected())
		{
			if (!data_saved) {
				/*
				* Luu trạng thái khi mất điện
				* byte[0]: mode_select[0] - chế độ giặt
				* byte[1]: mode_select[1] - mực nước
				* byte[2]: mode_select[2] - số lần xả yêu cầu
				* byte[3]: current_step - bước giặt hiện tại
				* byte[4]: drained_times - số lần xả đã thực hiện
				* byte[5-8]: elapsed_time_per_mode - 4byte lưu thời gian đang giặt
				*/
				/*Luu cac bien the hien mode giat hien tai*/
				procedure_run_flag = -3; // câu lệnh này để dừng lại SysTick và chu trình giặt
				// at24_write(0, mode_select, 3, 500);
				/*Luu cac bien extern tu washing_procedure*/
				uint8_t data[6] = {mode_select[0], mode_select[1], mode_select[2], current_step, drained_times};
				at24_write(0, data, sizeof(data), 500);
				data_saved = true;
				
			}
		}
		power_loss_flag = true; 
	}
	ZC = false; // set ZC false here, Pulse_EXTI can set it back to true, if not we can know that the power is off
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	   ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
