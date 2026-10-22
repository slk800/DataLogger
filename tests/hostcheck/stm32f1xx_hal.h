/*
 * Host-side compile check stub for STM32F1 HAL.
 *
 * 目的：让 App/BSP(部分)/FS/Log/Config/UI/CLI/Sensor/Utils 可以在 PC 上
 *       做语法与类型检查，不需要 Keil 或 ARM 交叉编译器。
 *
 * 这不是 HAL 仿真，只提供本工程实际用到的类型、宏和函数声明，
 * 且只用于 `tests/run_host_check.sh` 的编译期检查。
 */

#ifndef __STM32F1XX_HAL_H
#define __STM32F1XX_HAL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ---------- 基本类型 ---------- */
typedef enum
{
    HAL_OK = 0x00U,
    HAL_ERROR = 0x01U,
    HAL_BUSY = 0x02U,
    HAL_TIMEOUT = 0x03U
} HAL_StatusTypeDef;

typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

typedef struct { int dummy; } GPIO_TypeDef;

typedef struct {
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
    uint32_t Alternate;
} GPIO_InitTypeDef;

#define GPIO_MODE_INPUT         0x00000000U
#define GPIO_MODE_OUTPUT_PP     0x00000001U
#define GPIO_MODE_OUTPUT_OD     0x00000011U
#define GPIO_MODE_AF_PP         0x00000002U
#define GPIO_MODE_AF_OD         0x00000012U
#define GPIO_MODE_ANALOG        0x00000003U
#define GPIO_MODE_IT_RISING     0x10110000U
#define GPIO_MODE_IT_FALLING    0x10210000U
#define GPIO_MODE_IT_RISING_FALLING 0x10310000U

#define GPIO_NOPULL             0x00000000U
#define GPIO_PULLUP             0x00000001U
#define GPIO_PULLDOWN           0x00000002U

#define GPIO_SPEED_FREQ_LOW     0x00000002U
#define GPIO_SPEED_FREQ_MEDIUM  0x00000001U
#define GPIO_SPEED_FREQ_HIGH    0x00000003U

typedef enum { RESET = 0, SET = !RESET } FlagStatus, ITStatus;

#define __HAL_GPIO_EXTI_GET_IT(pin)        ((uint32_t)0)
#define __HAL_GPIO_EXTI_CLEAR_IT(pin)      do { } while (0)
#define __HAL_PWR_CLEAR_FLAG(flag)         do { } while (0)
#define PWR_FLAG_WU                        (1U << 8)

typedef enum { EXTI1_IRQn = 7, EXTI15_10_IRQn = 40 } IRQn_Type;
void HAL_NVIC_SetPriority(IRQn_Type irq, uint32_t pre, uint32_t sub);
void HAL_NVIC_EnableIRQ(IRQn_Type irq);
void HAL_NVIC_DisableIRQ(IRQn_Type irq);
typedef struct { int dummy; } ADC_HandleTypeDef;
typedef struct { int dummy; } SPI_TypeDef;
typedef struct { SPI_TypeDef *Instance; } SPI_HandleTypeDef;
extern GPIO_TypeDef host_GPIOA, host_GPIOB, host_GPIOC;
#define GPIOA (&host_GPIOA)
#define GPIOB (&host_GPIOB)
#define GPIOC (&host_GPIOC)

extern SPI_TypeDef host_SPI1;
#define SPI1 (&host_SPI1)
typedef struct { int dummy; } I2C_HandleTypeDef;
typedef struct { int dummy; } USART_TypeDef;
typedef struct { USART_TypeDef *Instance; } UART_HandleTypeDef;
extern USART_TypeDef host_USART1;
#define USART1 (&host_USART1)
typedef struct { int dummy; } TIM_HandleTypeDef;

/* ---------- GPIO ---------- */
#define GPIO_PIN_0   ((uint16_t)0x0001)
#define GPIO_PIN_1   ((uint16_t)0x0002)
#define GPIO_PIN_2   ((uint16_t)0x0004)
#define GPIO_PIN_3   ((uint16_t)0x0008)
#define GPIO_PIN_4   ((uint16_t)0x0010)
#define GPIO_PIN_5   ((uint16_t)0x0020)
#define GPIO_PIN_6   ((uint16_t)0x0040)
#define GPIO_PIN_7   ((uint16_t)0x0080)
#define GPIO_PIN_8   ((uint16_t)0x0100)
#define GPIO_PIN_9   ((uint16_t)0x0200)
#define GPIO_PIN_10  ((uint16_t)0x0400)
#define GPIO_PIN_11  ((uint16_t)0x0800)
#define GPIO_PIN_12  ((uint16_t)0x1000)
#define GPIO_PIN_13  ((uint16_t)0x2000)
#define GPIO_PIN_14  ((uint16_t)0x4000)
#define GPIO_PIN_15  ((uint16_t)0x8000)

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
void HAL_GPIO_Init(GPIO_TypeDef *port, void *init);

/* ---------- 时基 ---------- */
void HAL_Init(void);
void HAL_Delay(uint32_t ms);
void HAL_IncTick(void);
uint32_t HAL_GetTick(void);

/* ---------- ADC ---------- */
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *hadc, uint32_t timeout);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc);

/* ---------- SPI ---------- */
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *hspi,
                                          uint8_t *tx, uint8_t *rx,
                                          uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *hspi);
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *hi2c);

/* ---------- I2C ---------- */
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c, uint16_t addr,
                                        uint32_t trials, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *hi2c, uint16_t addr,
                                          uint8_t *data, uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t addr, uint16_t mem_addr,
                                   uint16_t mem_size, uint8_t *data, uint16_t size,
                                   uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t addr, uint16_t mem_addr,
                                    uint16_t mem_size, uint8_t *data, uint16_t size,
                                    uint32_t timeout);
#define I2C_MEMADD_SIZE_8BIT   ((uint16_t)0x0001U)
#define I2C_MEMADD_SIZE_16BIT  ((uint16_t)0x0002U)

/* ---------- UART ---------- */
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data,
                                    uint16_t size, uint32_t timeout);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *data,
                                      uint16_t size);

/* ---------- RCC / PWR / 复位 ---------- */
typedef struct { uint32_t PLLState; uint32_t PLLSource; uint32_t PLLMUL; } RCC_PLLInitTypeDef;

typedef struct {
    uint32_t OscillatorType;
    uint32_t HSEState;
    uint32_t HSEPredivValue;
    uint32_t HSIState;
    uint32_t HSICalibrationValue;
    uint32_t LSEState;
    uint32_t LSIState;
    RCC_PLLInitTypeDef PLL;
} RCC_OscInitTypeDef;

typedef struct {
    uint32_t ClockType;
    uint32_t SYSCLKSource;
    uint32_t AHBCLKDivider;
    uint32_t APB1CLKDivider;
    uint32_t APB2CLKDivider;
} RCC_ClkInitTypeDef;

typedef struct { uint32_t PeriphClockSelection; uint32_t AdcClockSelection; } RCC_PeriphCLKInitTypeDef;

#define RCC_OSCILLATORTYPE_HSE   ((uint32_t)0x00000001)
#define RCC_OSCILLATORTYPE_HSI   ((uint32_t)0x00000002)
#define RCC_OSCILLATORTYPE_LSE   ((uint32_t)0x00000004)
#define RCC_OSCILLATORTYPE_LSI   ((uint32_t)0x00000008)
#define RCC_HSE_ON               ((uint32_t)0x00010000)
#define RCC_HSI_ON               ((uint32_t)0x00000001)
#define RCC_HSE_PREDIV_DIV1      ((uint32_t)0x00000000)
#define RCC_PLL_ON               ((uint32_t)0x00000002)
#define RCC_PLLSOURCE_HSE        ((uint32_t)0x00010000)
#define RCC_PLL_MUL9             ((uint32_t)0x00280000)
#define RCC_CLOCKTYPE_HCLK       ((uint32_t)0x00000001)
#define RCC_CLOCKTYPE_SYSCLK     ((uint32_t)0x00000002)
#define RCC_CLOCKTYPE_PCLK1      ((uint32_t)0x00000004)
#define RCC_CLOCKTYPE_PCLK2      ((uint32_t)0x00000008)
#define RCC_SYSCLKSOURCE_PLLCLK  ((uint32_t)0x00000002)
#define RCC_SYSCLK_DIV1          ((uint32_t)0x00000000)
#define RCC_HCLK_DIV1            ((uint32_t)0x00000000)
#define RCC_HCLK_DIV2            ((uint32_t)0x00000400)
#define FLASH_LATENCY_2          ((uint32_t)0x00000002)

extern uint32_t SystemCoreClock;
void SystemCoreClockUpdate(void);

#define RCC_CSR_PINRSTF   ((uint32_t)0x04000000U)
#define RCC_CSR_PORRSTF   ((uint32_t)0x08000000U)
#define RCC_CSR_SFTRSTF   ((uint32_t)0x10000000U)
#define RCC_CSR_IWDGRSTF  ((uint32_t)0x20000000U)
#define RCC_CSR_WWDGRSTF  ((uint32_t)0x40000000U)
#define RCC_CSR_LPWRRSTF  ((uint32_t)0x80000000U)

HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *cfg);
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *cfg, uint32_t latency);
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *cfg);
void HAL_PWR_EnableBkUpAccess(void);
#define PWR_LOWPOWERREGULATOR_ON   0U
#define PWR_STOPENTRY_WFI          1U
void HAL_PWR_EnterSTOPMode(uint32_t regulator, uint32_t entry);

/* BKP：用普通内存对象模拟，仅用于类型检查 */
typedef struct { volatile uint16_t DR1; volatile uint16_t DR2; volatile uint16_t DR3; } BKP_TypeDef;
typedef struct { volatile uint32_t CSR; } RCC_TypeDef;

extern BKP_TypeDef  host_BKP;
extern RCC_TypeDef  host_RCC;

#define BKP ((BKP_TypeDef *)&host_BKP)
#define RCC ((RCC_TypeDef *)&host_RCC)

#define __HAL_RCC_BKP_CLK_ENABLE()      do { } while (0)
#define __HAL_RCC_GPIOA_CLK_ENABLE()    do { } while (0)
#define __HAL_RCC_GPIOB_CLK_ENABLE()    do { } while (0)
#define __HAL_RCC_GPIOC_CLK_ENABLE()    do { } while (0)
#define __HAL_RCC_PWR_CLK_ENABLE()      do { } while (0)
#define __HAL_RCC_CLEAR_RESET_FLAGS()   do { RCC->CSR = 0U; } while (0)

void NVIC_SystemReset(void);

#endif /* __STM32F1XX_HAL_H */
