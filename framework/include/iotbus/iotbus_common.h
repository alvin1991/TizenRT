/****************************************************************************
 *
 * Copyright 2019 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
#ifndef _IOTBUS_COMMON_H__
#define _IOTBUS_COMMON_H__

typedef enum {
	IOTBUS_UART_TX_EMPTY = 0,
	IOTBUS_UART_TX_RDY,
	IOTBUS_UART_RX_AVAIL,
	IOTBUS_UART_RECEIVED,
	IOTBUS_GPIO_FALLING,
	IOTBUS_GPIO_RISING,
	IOTBUS_INTR_MAX,	
} iotbus_int_type_e;

typedef enum {
	IOTBUS_GPIO = 0,
	IOTBUS_PWM,
	IOTBUS_ADC,
	IOTBUS_UART,
	IOTBUS_I2C,
	IOTBUS_SPI,
} iotbus_pin_e;

#endif
