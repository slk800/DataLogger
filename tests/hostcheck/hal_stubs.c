/* Host-side compile check stubs implementation */
#include "stm32f1xx_hal.h"
#include "main.h"

BKP_TypeDef host_BKP;
SPI_TypeDef host_SPI1;
USART_TypeDef host_USART1;
RCC_TypeDef host_RCC;
GPIO_TypeDef host_GPIOA, host_GPIOB, host_GPIOC;
ADC_HandleTypeDef  hadc1;
SPI_HandleTypeDef  hspi1;
I2C_HandleTypeDef  hi2c1;
UART_HandleTypeDef huart1;

static uint32_t s_tick;

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin)
{ (void)port; (void)pin; return GPIO_PIN_SET; }

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{ (void)port; (void)pin; (void)state; }

void HAL_GPIO_Init(GPIO_TypeDef *port, void *init) { (void)port; (void)init; }
void HAL_Init(void) { }
void HAL_Delay(uint32_t ms) { (void)ms; }
void HAL_IncTick(void) { s_tick++; }
uint32_t HAL_GetTick(void) { return s_tick; }

HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h, uint32_t t)
{ (void)h; (void)t; return HAL_OK; }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h) { (void)h; return 2048U; }

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *h, uint8_t *tx,
                                          uint8_t *rx, uint16_t n, uint32_t t)
{ (void)h; (void)t; for (uint16_t i = 0; i < n; i++) rx[i] = 0xFFU; (void)tx; return HAL_OK; }
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef *h) { (void)h; return HAL_OK; }

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t a, uint32_t n, uint32_t t)
{ (void)h; (void)a; (void)n; (void)t; return HAL_OK; }
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t a,
                                          uint8_t *d, uint16_t n, uint32_t t)
{ (void)h; (void)a; (void)d; (void)n; (void)t; return HAL_OK; }

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *h, uint16_t a, uint16_t m,
                                   uint16_t ms, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h;(void)a;(void)m;(void)ms;(void)t; for(uint16_t i=0;i<n;i++) d[i]=0; return HAL_OK; }

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *h, uint16_t a, uint16_t m,
                                    uint16_t ms, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h;(void)a;(void)m;(void)ms;(void)d;(void)n;(void)t; return HAL_OK; }

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t)
{ (void)h; (void)t; for (uint16_t i = 0; i < n; i++) putchar(d[i]); return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n)
{ (void)h; (void)d; (void)n; return HAL_OK; }

HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *c) { (void)c; return HAL_OK; }
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *c, uint32_t l) { (void)c; (void)l; return HAL_OK; }
HAL_StatusTypeDef HAL_RCCEx_PeriphCLKConfig(RCC_PeriphCLKInitTypeDef *c) { (void)c; return HAL_OK; }
void HAL_PWR_EnableBkUpAccess(void) { }
void NVIC_SystemReset(void) { }

void MX_ADC1_Init(void) { }
void MX_SPI1_Init(void) { }
void MX_I2C1_Init(void) { }
void MX_USART1_UART_Init(void) { }
void Error_Handler(void) { }

void HAL_NVIC_SetPriority(IRQn_Type irq, uint32_t pre, uint32_t sub)
{ (void)irq; (void)pre; (void)sub; }
void HAL_NVIC_EnableIRQ(IRQn_Type irq) { (void)irq; }
void HAL_NVIC_DisableIRQ(IRQn_Type irq) { (void)irq; }
