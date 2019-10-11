/* Includes ------------------------------------------------------------------*/
#include <stm32f769i_discovery.h>
#include "stm32f7xx_it.h"

#include <bsp_api.h>
#include <stdarg.h>
#include <main.h>
#include <misc_utils.h>
#include <bsp_cmd.h>
#include <lcd_main.h>
#include <audio_main.h>
#include <input_main.h>
#include <debug.h>
#include <dev_io.h>
#include <nvic.h>
#include <mpu.h>
#include <heap.h>
#include <bsp_sys.h>

#ifdef __MICROLIB
#error "I don't want to use microlib"
/*Microlib will reduce code size at the cost of performance,
    also due to HEAP replacement
*/
#endif

#ifdef USE_STM32F769I_DISCO
#include "stm32f769i_discovery_lcd.h"
#else /*USE_STM32F769I_DISCO*/
#error "Not supported"
#endif /*USE_STM32F769I_DISCO*/

#if !defined(MODULE) && defined(BSP_DRIVER)

extern void boot_gui_preinit (void);
extern void VID_PreConfig (void);
extern int mainloop (int argc, const char *argv[]);

int g_dev_debug_level = DBG_ERR;

static void SystemClock_Config(void);

static void sys_exception_handler (uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

static void clock_fault (void)
{
    bug();
}

#if (_USE_LFN == 3)
#error "ff_malloc, ff_free must be redefined to Sys_HeapAlloc"
#endif

#ifdef STM32F7
int const __cache_line_size = 32;
#else
#error "Cache line size unknown"
#endif

void dumpstack (void)
{
}

void fatal_error (char *message, ...)
{
extern void SystemSoftReset (void);

    va_list argptr;

    va_start (argptr, message);
    dvprintf (message, argptr);
    va_end (argptr);

    serial_flush();
    SystemSoftReset();
    for(;;) {}
}

/* Private function prototypes -----------------------------------------------*/

/*TODO : move to gpio.c/gpio.h*/
void hdd_led_on (void)
{
    BSP_LED_On(LED2);
}

void hdd_led_off (void)
{
    BSP_LED_Off(LED2);
}

void serial_led_on (void)
{
    BSP_LED_On(LED1);
}

void serial_led_off (void)
{
    BSP_LED_Off(LED1);
}

static int con_echo (int argc, const char **argv)
{
    int i;
    char buf[128];

    dprintf("@> ");

    for (i = 0; i < argc; i++) {
        snprintf(buf, sizeof(buf), "%s", argv[i]);
        dprintf(" %s", buf);
    }
    dprintf("\n");

    return argc; /*let it be processed by others*/
}

static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct;
    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;
    HAL_StatusTypeDef  ret = HAL_OK;

    /* Enable Power Control clock */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value 
     regarding system frequency refer to product datasheet.  */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 400;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 8;
    RCC_OscInitStruct.PLL.PLLR = 7;

    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if(ret != HAL_OK)
        clock_fault();

    /* Activate the OverDrive to reach the 200 MHz Frequency */
    ret = HAL_PWREx_EnableOverDrive();
    if(ret != HAL_OK)
        clock_fault();

    /* Select PLLSAI output as USB clock source */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_CLK48;
    PeriphClkInitStruct.Clk48ClockSelection = RCC_CLK48SOURCE_PLL;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 192;
    PeriphClkInitStruct.PLLSAI.PLLSAIQ = 4;
    PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV4;
    PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        clock_fault();

    /* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2 clocks dividers */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6);
    if(ret != HAL_OK)
        clock_fault();
}

void CPU_CACHE_Enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

void CPU_CACHE_Disable (void)
{
    SCB_DisableDCache();
    SCB_DisableICache();
    SCB_CleanInvalidateDCache();
    SCB_InvalidateICache();
}

void CPU_CACHE_Reset (void)
{
    CPU_CACHE_Disable();
    CPU_CACHE_Enable();
}

void (*dev_deinit_callback) (void) = NULL;

int dev_hal_init (void)
{
    CPU_CACHE_Enable();
    SystemClock_Config();
    HAL_Init();
    BSP_LED_Init(LED1);
    BSP_LED_Init(LED2);
    serial_init();
    cs_load_code(NULL, NULL, 0);
    return 0;
}

int bsp_drv_init (void)
{
    dev_io_init();

    bsp_stdin_register_if(con_echo);
    cmd_init();

    audio_init();
    input_bsp_init();
    profiler_init();
    vid_init();
    cmd_register_i32(&g_dev_debug_level, "dbglvl");
    cmd_register_i32(&g_serial_rx_eof, "set_rxeof");
    return 0;
}

void dev_deinit (void)
{
    if (dev_deinit_callback) {
        dev_deinit_callback();
        dev_deinit_callback = NULL;
    }

    dprintf("%s() :\n", __func__);
    bsp_stdin_unreg_if(con_echo);

    cmd_deinit();
    dev_io_deinit();
    audio_deinit();
    profiler_deinit();
    input_bsp_deinit();
    vid_deinit();
    heap_dump();
    serial_deinit();
}

int bsp_drv_main (void)
{
    char **argv;
    int argc;

    g_bspapi = bsp_api_attach();
    dev_hal_init();
    mpu_init();
    heap_init();

    bsp_drv_init();
    mpu_init();

    VID_PreConfig();
    mainloop(argc, argv);

    return 0;
}



/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
    sys_exception_handler(0, 0, 0, 0);
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
    sys_exception_handler(0, 0, 0, 0);
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
    sys_exception_handler(0, 0, 0, 0);
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
    sys_exception_handler(0, 0, 0, 0);
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
    sys_exception_handler(0, 0, 0, 0);
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
    sys_exception_handler(0, 0, 0, 0);
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
    sys_exception_handler(0, 0, 0, 0);
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
    sys_exception_handler(0, 0, 0, 0);
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
}

static void sys_exception_handler (uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    bug();
}

#endif /*!defined(MODULE) && defined(BSP_DRIVER)*/

