/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_uart.h"
#include "bsp_flash.h"
#include "utils.h"
#include "fs_core.h"
#include "fs_file.h"
#include <stdio.h>
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
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId defaultTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* GetTimerTaskMemory prototype (linked to static allocation support) */
#if (configUSE_TIMERS == 1)
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );
#endif

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE BEGIN 4 */
__weak void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/* USER CODE BEGIN 5 */
__weak void vApplicationMallocFailedHook(void)
{
   /* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h. It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created. It is also called by various parts of the
   demo application. If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
}
/* USER CODE END 5 */

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/* USER CODE BEGIN GET_TIMER_TASK_MEMORY */
#if (configUSE_TIMERS == 1)
static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
  /* place for user code */
}
#endif
/* USER CODE END GET_TIMER_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
    uint8_t write_buf[16] = "Hello Flash!";
    uint8_t read_buf[16] = {0};
    uint32_t test_addr = 0x000000;    /* 测试地址 */
    char msg_buf[64];

    /* 等待串口输出完成 */
    osDelay(100);

    BSP_UART_SendString("\r\n[Task] Flash Read/Write Test...\r\n");

    /* 擦除扇区 */
    BSP_UART_SendString("[Task] Erasing sector 0...\r\n");
    BSP_Flash_SectorErase(test_addr);
    osDelay(100);

    /* 写入数据 */
    BSP_UART_SendString("[Task] Writing data...\r\n");
    BSP_Flash_PageWrite(test_addr, write_buf, sizeof(write_buf));
    osDelay(10);

    /* 读取数据 */
    BSP_UART_SendString("[Task] Reading data...\r\n");
    BSP_Flash_ReadData(test_addr, read_buf, sizeof(read_buf));

    /* 打印读取结果 */
    snprintf(msg_buf, sizeof(msg_buf), "[Task] Read: %s\r\n", read_buf);
    BSP_UART_SendString(msg_buf);

    /* 验证数据 */
    if (memcmp(write_buf, read_buf, sizeof(write_buf)) == 0)
    {
        BSP_UART_SendString("[Task] Flash Test: PASSED!\r\n");
    }
    else
    {
        BSP_UART_SendString("[Task] Flash Test: FAILED!\r\n");
    }

    /* ========== Utils 模块测试 ========== */
    BSP_UART_SendString("\r\n[Task] Utils Module Test...\r\n");

    /* 测试 CRC32 */
    uint32_t crc = Utils_CRC32_Calc((uint8_t *)"123456", 6);
    snprintf(msg_buf, sizeof(msg_buf), "[Task] CRC32(\"123456\") = 0x%08lX\r\n", crc);
    BSP_UART_SendString(msg_buf);
    snprintf(msg_buf, sizeof(msg_buf), "[Task] Expected: 0x0972D361\r\n");
    BSP_UART_SendString(msg_buf);

    /* 测试 CRC32_Verify */
    if (Utils_CRC32_Verify((uint8_t *)"123456", 6, 0x0972D361))
    {
        BSP_UART_SendString("[Task] CRC32_Verify: PASSED!\r\n");
    }
    else
    {
        BSP_UART_SendString("[Task] CRC32_Verify: FAILED!\r\n");
    }

    /* 测试 StrToInt */
    int32_t num = Utils_StrToInt("-123");
    snprintf(msg_buf, sizeof(msg_buf), "[Task] StrToInt(\"-123\") = %ld\r\n", num);
    BSP_UART_SendString(msg_buf);

    /* 测试 IntToStr */
    char int_buf[16];
    Utils_IntToStr(-456, int_buf, sizeof(int_buf));
    snprintf(msg_buf, sizeof(msg_buf), "[Task] IntToStr(-456) = %s\r\n", int_buf);
    BSP_UART_SendString(msg_buf);

    /* 测试 StrSplit */
    char split_buf[] = "hello world foo";
    char *tokens[8];
    uint8_t token_count = Utils_StrSplit(split_buf, ' ', tokens, 8);
    snprintf(msg_buf, sizeof(msg_buf), "[Task] StrSplit: %d tokens\r\n", token_count);
    BSP_UART_SendString(msg_buf);
    snprintf(msg_buf, sizeof(msg_buf), "[Task] tokens[0] = \"%s\"\r\n", tokens[0]);
    BSP_UART_SendString(msg_buf);

    /* 测试 StrTrim */
    char trim_buf[] = "  hello  ";
    Utils_StrTrim(trim_buf);
    snprintf(msg_buf, sizeof(msg_buf), "[Task] StrTrim: \"%s\"\r\n", trim_buf);
    BSP_UART_SendString(msg_buf);

    BSP_UART_SendString("[Task] Utils Test Complete!\r\n");

    /* ========== 文件系统测试 ========== */
    BSP_UART_SendString("\r\n[Task] File System Test...\r\n");

    FSResult_e fs_result;
    uint8_t test_data[64];
    uint8_t read_data[64];
    uint32_t read_len;
    uint8_t file_count;
    FileEntry_t file_list[8];
    char fs_buf[64];

    /* 准备测试数据 */
    for (uint8_t i = 0; i < sizeof(test_data); i++)
    {
        test_data[i] = i;
    }

    /* 测试1: 首次初始化（未格式化时应返回错误） */
    fs_result = FS_Init();
    if (fs_result == FS_ERR_NOT_INIT || fs_result == FS_ERR_CRC)
    {
        BSP_UART_SendString("[Task] FS_Init: Not formatted (expected)\r\n");

        /* 格式化文件系统 */
        BSP_UART_SendString("[Task] FS_Formatting...\r\n");
        fs_result = FS_Format();
        if (fs_result == FS_OK)
        {
            BSP_UART_SendString("[Task] FS_Format: OK\r\n");
        }
        else
        {
            snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_Format: FAILED (%d)\r\n", fs_result);
            BSP_UART_SendString(fs_buf);
        }
    }
    else if (fs_result == FS_OK)
    {
        BSP_UART_SendString("[Task] FS_Init: Already formatted\r\n");
    }
    else
    {
        snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_Init: Error %d\r\n", fs_result);
        BSP_UART_SendString(fs_buf);
    }

    /* 测试2: 格式化后初始化 */
    fs_result = FS_Init();
    if (fs_result == FS_OK)
    {
        BSP_UART_SendString("[Task] FS_Init after format: OK\r\n");
    }
    else
    {
        snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_Init: FAILED (%d)\r\n", fs_result);
        BSP_UART_SendString(fs_buf);
    }

    /* 测试3: 获取文件系统状态 */
    uint32_t used_bytes, total_bytes;
    FS_GetStatus(&used_bytes, &total_bytes);
    snprintf(fs_buf, sizeof(fs_buf), "[Task] FS Status: %lu/%lu bytes\r\n", used_bytes, total_bytes);
    BSP_UART_SendString(fs_buf);

    /* 测试4: 创建文件 */
    fs_result = FS_File_Create("log", FILE_TYPE_LOG);
    if (fs_result == FS_OK)
    {
        BSP_UART_SendString("[Task] FS_File_Create(\"log\"): OK\r\n");
    }
    else if (fs_result == FS_ERR_EXIST)
    {
        BSP_UART_SendString("[Task] FS_File_Create(\"log\"): Already exists\r\n");
    }
    else
    {
        snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_File_Create: FAILED (%d)\r\n", fs_result);
        BSP_UART_SendString(fs_buf);
    }

    /* 测试5: 写入文件 */
    fs_result = FS_File_Write("log", test_data, sizeof(test_data));
    if (fs_result == FS_OK)
    {
        snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_File_Write: %d bytes OK\r\n", sizeof(test_data));
        BSP_UART_SendString(fs_buf);
    }
    else
    {
        snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_File_Write: FAILED (%d)\r\n", fs_result);
        BSP_UART_SendString(fs_buf);
    }

    /* 测试6: 读取文件 */
    memset(read_data, 0, sizeof(read_data));
    fs_result = FS_File_Read("log", read_data, sizeof(read_data), &read_len);
    if (fs_result == FS_OK)
    {
        snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_File_Read: %lu bytes OK\r\n", read_len);
        BSP_UART_SendString(fs_buf);

        /* 验证数据 */
        if (memcmp(test_data, read_data, sizeof(test_data)) == 0)
        {
            BSP_UART_SendString("[Task] Data verification: PASSED!\r\n");
        }
        else
        {
            BSP_UART_SendString("[Task] Data verification: FAILED!\r\n");
        }
    }
    else
    {
        snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_File_Read: FAILED (%d)\r\n", fs_result);
        BSP_UART_SendString(fs_buf);
    }

    /* 测试7: 列出文件 */
    fs_result = FS_File_List(file_list, 8, &file_count);
    if (fs_result == FS_OK)
    {
        snprintf(fs_buf, sizeof(fs_buf), "[Task] FS_File_List: %d files\r\n", file_count);
        BSP_UART_SendString(fs_buf);
        for (uint8_t i = 0; i < file_count; i++)
        {
            snprintf(fs_buf, sizeof(fs_buf), "  - %s: %lu bytes\r\n",
                     file_list[i].name, file_list[i].size);
            BSP_UART_SendString(fs_buf);
        }
    }

    /* 测试8: 创建第二个文件 */
    fs_result = FS_File_Create("config", FILE_TYPE_CONFIG);
    if (fs_result == FS_OK || fs_result == FS_ERR_EXIST)
    {
        BSP_UART_SendString("[Task] FS_File_Create(\"config\"): OK\r\n");
    }

    /* 再次列出 */
    fs_result = FS_File_List(file_list, 8, &file_count);
    snprintf(fs_buf, sizeof(fs_buf), "[Task] File count: %d\r\n", file_count);
    BSP_UART_SendString(fs_buf);

    /* 测试9: 删除文件 */
    fs_result = FS_File_Delete("config");
    if (fs_result == FS_OK)
    {
        BSP_UART_SendString("[Task] FS_File_Delete(\"config\"): OK\r\n");
    }

    /* 验证删除后读取 */
    fs_result = FS_File_Read("config", read_data, sizeof(read_data), &read_len);
    if (fs_result == FS_ERR_NOT_FOUND)
    {
        BSP_UART_SendString("[Task] Deleted file read: NOT_FOUND (expected)\r\n");
    }

    BSP_UART_SendString("[Task] File System Test Complete!\r\n");

  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

