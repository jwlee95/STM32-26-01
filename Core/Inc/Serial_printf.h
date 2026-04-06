/*
 * Serial_printf.h
 *
 *  Created on: Apr 6, 2026
 *      Author: JeonghWhanPro
 */

#ifndef INC_SERIAL_PRINTF_H_
#define INC_SERIAL_PRINTF_H_

#include "main.h"
#include <stdio.h>

extern UART_HandleTypeDef huart2;

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

/**
  * @brief  Retargets the C library printf function to the USART2.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  if (ch == '\n')
    HAL_UART_Transmit(&huart2, (uint8_t*) "\r", 1, 0xFFFF);
  HAL_UART_Transmit(&huart2, (uint8_t*) &ch, 1, 0xFFFF);

  return ch;
}

#endif /* INC_SERIAL_PRINTF_H_ */
