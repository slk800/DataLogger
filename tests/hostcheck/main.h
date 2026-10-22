/* Host-side compile check stub：镜像 CubeMX 生成的引脚宏 */
#ifndef __MAIN_H
#define __MAIN_H
#include "stm32f1xx_hal.h"

extern GPIO_TypeDef host_GPIOA, host_GPIOB, host_GPIOC;
#define GPIOA (&host_GPIOA)
#define GPIOB (&host_GPIOB)
#define GPIOC (&host_GPIOC)

#define LED_Pin            GPIO_PIN_13
#define LED_GPIO_Port      GPIOC
#define W25Q_CS_Pin        GPIO_PIN_0
#define W25Q_CS_GPIO_Port  GPIOB
#define KEY_UP_Pin         GPIO_PIN_12
#define KEY_UP_GPIO_Port   GPIOB
#define KEY_DOWN_Pin       GPIO_PIN_13
#define KEY_DOWN_GPIO_Port GPIOB
#define KEY_OK_Pin         GPIO_PIN_14
#define KEY_OK_GPIO_Port   GPIOB

void Error_Handler(void);

#endif
