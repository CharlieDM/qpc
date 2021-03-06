/*****************************************************************************
* Product: DPP example, STM32746G-Discovery board, preemptive QK kernel
* Last Updated for Version: 5.9.0
* Date of the Last Update:  2017-04-13
*
*                    Q u a n t u m     L e a P s
*                    ---------------------------
*                    innovating embedded systems
*
* Copyright (C) Quantum Leaps, LLC. All rights reserved.
*
* This program is open source software: you can redistribute it and/or
* modify it under the terms of the GNU General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* Alternatively, this program may be distributed and modified under the
* terms of Quantum Leaps commercial licenses, which expressly supersede
* the GNU General Public License and are specifically designed for
* licensees interested in retaining the proprietary status of their code.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
* Contact information:
* https://state-machine.com
* mailto:info@state-machine.com
*****************************************************************************/
#include "qpc.h"
#include "dpp.h"
#include "bsp.h"

/* STM32Cube include files */
#include "stm32f7xx_hal.h"
#include "stm32746g_discovery.h"
/* add other drivers if necessary... */

Q_DEFINE_THIS_FILE

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! CAUTION !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
* Assign a priority to EVERY ISR explicitly by calling NVIC_SetPriority().
* DO NOT LEAVE THE ISR PRIORITIES AT THE DEFAULT VALUE!
*/
enum KernelUnawareISRs { /* see NOTE00 */
    USART1_PRIO,
    /* ... */
    MAX_KERNEL_UNAWARE_CMSIS_PRI  /* keep always last */
};
/* "kernel-unaware" interrupts can't overlap "kernel-aware" interrupts */
Q_ASSERT_COMPILE(MAX_KERNEL_UNAWARE_CMSIS_PRI <= QF_AWARE_ISR_CMSIS_PRI);

enum KernelAwareISRs {
    GPIO_EVEN_PRIO = QF_AWARE_ISR_CMSIS_PRI, /* see NOTE00 */
    SYSTICK_PRIO,
    /* ... */
    MAX_KERNEL_AWARE_CMSIS_PRI /* keep always last */
};
/* "kernel-aware" interrupts should not overlap the PendSV priority */
Q_ASSERT_COMPILE(MAX_KERNEL_AWARE_CMSIS_PRI <= (0xFF >>(8-__NVIC_PRIO_BITS)));

/* ISRs defined in this BSP ------------------------------------------------*/
void SysTick_Handler(void);
//void GPIO_EVEN_IRQHandler(void);
void USART1_IRQHandler(void);

/* Local-scope objects -----------------------------------------------------*/
static uint32_t l_rnd;      /* random seed */
static QMutex   l_rndMutex; /* mutex to protect the random seed */

#ifdef Q_SPY

    QSTimeCtr QS_tickTime_;
    QSTimeCtr QS_tickPeriod_;

    /* QS source IDs */
    static uint8_t const l_SysTick_Handler = (uint8_t)0;
    static uint8_t const l_GPIO_EVEN_IRQHandler = (uint8_t)0;
    static UART_HandleTypeDef l_uartHandle;

    enum AppRecords { /* application-specific trace records */
        PHILO_STAT = QS_USER,
        COMMAND_STAT
    };

#endif

/*..........................................................................*/
void SysTick_Handler(void) {
    /* state of the button debouncing, see below */
    static struct ButtonsDebouncing {
        uint32_t depressed;
        uint32_t previous;
    } buttons = { ~0U, ~0U };
    uint32_t current;
    uint32_t tmp;

    QK_ISR_ENTRY();   /* inform QK about entering an ISR */

#ifdef Q_SPY
    {
        tmp = SysTick->CTRL; /* clear SysTick_CTRL_COUNTFLAG */
        QS_tickTime_ += QS_tickPeriod_; /* account for the clock rollover */
    }
#endif

    QF_TICK_X(0U, &l_SysTick_Handler); /* process time events for rate 0 */

    /* Perform the debouncing of buttons. The algorithm for debouncing
    * adapted from the book "Embedded Systems Dictionary" by Jack Ganssle
    * and Michael Barr, page 71.
    */
    current = BSP_PB_GetState(BUTTON_KEY); /* read the Key button */
    tmp = buttons.depressed; /* save the debounced depressed buttons */
    buttons.depressed |= (buttons.previous & current); /* set depressed */
    buttons.depressed &= (buttons.previous | current); /* clear released */
    buttons.previous   = current; /* update the history */
    tmp ^= buttons.depressed;     /* changed debounced depressed */
    if (tmp != 0U) {  /* debounced Key button state changed? */
        if (buttons.depressed != 0U) { /* PB0 depressed?*/
            static QEvt const pauseEvt = { PAUSE_SIG, 0U, 0U};
            QF_PUBLISH(&pauseEvt, &l_SysTick_Handler);
        }
        else { /* the button is released */
            static QEvt const serveEvt = { SERVE_SIG, 0U, 0U};
            QF_PUBLISH(&serveEvt, &l_SysTick_Handler);
        }
    }

    QK_ISR_EXIT();  /* inform QK about exiting an ISR */
}
/*..........................................................................*/
#ifdef Q_SPY
/*
* ISR for receiving bytes from the QSPY Back-End
* NOTE: This ISR is "QF-unaware" meaning that it does not interact with
* the QF/QK and is not disabled. Such ISRs don't need to call QK_ISR_ENTRY/
* QK_ISR_EXIT and they cannot post or publish events.
*/
void USART1_IRQHandler(void) {
    /* is RX register NOT empty? */
    if ((l_uartHandle.Instance->ISR & USART_ISR_RXNE) != 0) {
        uint32_t b = l_uartHandle.Instance->RDR;
        QS_RX_PUT(b);
        l_uartHandle.Instance->ISR &= ~USART_ISR_RXNE; /* clear interrupt */
    }
}
/*..........................................................................*/
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *UartHandle) {
    (void)UartHandle;
    /* dummy implementation needed for STM32Cube */
}
#endif

/*..........................................................................*/
void BSP_init(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct;
    RCC_ClkInitTypeDef RCC_ClkInitStruct;

    SCB_EnableICache(); /* Enable I-Cache */
    SCB_EnableDCache(); /* Enable D-Cache */

    /* Configure Flash prefetch and Instr. cache through ART accelerator */
#if (ART_ACCLERATOR_ENABLE != 0)
    __HAL_FLASH_ART_ENABLE();
#endif /* ART_ACCLERATOR_ENABLE */

    /* Configure the system clock to 216 MHz... */
    /* Enable HSE Oscillator and activate PLL with HSE as source */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 432;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 9;
    Q_ALLEGE(HAL_RCC_OscConfig(&RCC_OscInitStruct) == HAL_OK);

    /* Activate the OverDrive to reach the 216 MHz Frequency */
    Q_ALLEGE(HAL_PWREx_EnableOverDrive() == HAL_OK);

    /* Set PLL as system clock source
    * and configure the HCLK, PCLK1 and PCLK2 clocks dividers
    */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
                                 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    Q_ALLEGE(HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7)
             == HAL_OK);

    /* configure the FPU usage by choosing one of the options... */
#if 1
    /* OPTION 1:
    * Use the automatic FPU state preservation and the FPU lazy stacking.
    *
    * NOTE:
    * Use the following setting when FPU is used in more than one task or
    * in any ISRs. This setting is the safest and recommended, but requires
    * extra stack space and CPU cycles.
    */
    FPU->FPCCR |= (1U << FPU_FPCCR_ASPEN_Pos) | (1U << FPU_FPCCR_LSPEN_Pos);
#else
    /* OPTION 2:
    * Do NOT to use the automatic FPU state preservation and
    * do NOT to use the FPU lazy stacking.
    *
    * NOTE:
    * Use the following setting when FPU is used in ONE task only and not
    * in any ISR. This setting is very efficient, but if more than one task
    * (or ISR) start using the FPU, this can lead to corruption of the
    * FPU registers. This option should be used with CAUTION.
    */
    FPU->FPCCR &= ~((1U << FPU_FPCCR_ASPEN_Pos) | (1U << FPU_FPCCR_LSPEN_Pos));
#endif

    /* Configure LED1 */
    BSP_LED_Init(LED1);

    /* Configure the User Button in GPIO Mode */
    BSP_PB_Init(BUTTON_KEY, BUTTON_MODE_GPIO);

    //...
    BSP_randomSeed(1234U);

    if (QS_INIT((void *)0) == 0) { /* initialize the QS software tracing */
        Q_ERROR();
    }
    QS_OBJ_DICTIONARY(&l_SysTick_Handler);
    QS_OBJ_DICTIONARY(&l_GPIO_EVEN_IRQHandler);
    QS_USR_DICTIONARY(PHILO_STAT);
    QS_USR_DICTIONARY(COMMAND_STAT);
}
/*..........................................................................*/
void BSP_displayPhilStat(uint8_t n, char const *stat) {
    if (stat[0] == 'e') {
        BSP_LED_On(LED1);
    }
    else {
        BSP_LED_Off(LED1);
    }

    QS_BEGIN(PHILO_STAT, AO_Philo[n]) /* application-specific record begin */
        QS_U8(1, n);  /* Philosopher number */
        QS_STR(stat); /* Philosopher status */
    QS_END()
}
/*..........................................................................*/
void BSP_displayPaused(uint8_t paused) {
    if (paused != 0U) {
        //BSP_LED_On(LED2); not enough LEDs
    }
    else {
        //BSP_LED_Off(LED2); not enough LEDs
    }
}
/*..........................................................................*/
uint32_t BSP_random(void) { /* a very cheap pseudo-random-number generator */
    uint32_t rnd;

    /* Some flating point code is to exercise the VFP... */
    float volatile x = 3.1415926F;
    x = x + 2.7182818F;

    QMutex_lock(&l_rndMutex); /* lock the random-seed mutex */
    /* "Super-Duper" Linear Congruential Generator (LCG)
    * LCG(2^32, 3*7*11*13*23, 0, seed)
    */
    rnd = l_rnd * (3U*7U*11U*13U*23U);
    l_rnd = rnd; /* set for the next time */
    QMutex_unlock(&l_rndMutex); /* unlock the random-seed mutex */

    return (rnd >> 8);
}
/*..........................................................................*/
void BSP_randomSeed(uint32_t seed) {
    l_rnd = seed;
    QMutex_init(&l_rndMutex, N_PHILO); /* ceiling == max Philo priority */
}
/*..........................................................................*/
void BSP_terminate(int16_t result) {
    (void)result;
}

/*..........................................................................*/
void QF_onStartup(void) {
    /* assing all priority bits for preemption-prio. and none to sub-prio. */
    NVIC_SetPriorityGrouping(0U);

    /* set up the SysTick timer to fire at BSP_TICKS_PER_SEC rate */
    SysTick_Config(SystemCoreClock / BSP_TICKS_PER_SEC);

    /* set priorities of ALL ISRs used in the system, see NOTE00
    *
    * !!!!!!!!!!!!!!!!!!!!!!!!!!!! CAUTION !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    * Assign a priority to EVERY ISR explicitly by calling NVIC_SetPriority().
    * DO NOT LEAVE THE ISR PRIORITIES AT THE DEFAULT VALUE!
    */
    NVIC_SetPriority(USART1_IRQn,  USART1_PRIO);
    NVIC_SetPriority(SysTick_IRQn, SYSTICK_PRIO);
    //NVIC_SetPriority(GPIO_EVEN_IRQn, GPIO_EVEN_PRIO);
    /* ... */

    /* enable IRQs... */
    //NVIC_EnableIRQ(GPIO_EVEN_IRQn);
#ifdef Q_SPY
    NVIC_EnableIRQ(USART1_IRQn); /* UART1 interrupt used for QS-RX */
#endif
}
/*..........................................................................*/
void QF_onCleanup(void) {
}
/*..........................................................................*/
void QK_onIdle(void) {
    /* toggle the User LED on and then off, see NOTE01 */
    QF_INT_DISABLE();
    //BSP_LED_On(LED3); not enough LEDs
    //BSP_LED_On(LED3); not enough LEDs
    QF_INT_ENABLE();

#ifdef Q_SPY
    QS_rxParse();  /* parse all the received bytes */

    if ((l_uartHandle.Instance->ISR & UART_FLAG_TXE) != 0U) { /* TXE empty? */
        uint16_t b;

        QF_INT_DISABLE();
        b = QS_getByte();
        QF_INT_ENABLE();

        if (b != QS_EOD) {  /* not End-Of-Data? */
            l_uartHandle.Instance->TDR = (b & 0xFFU);  /* put into TDR */
        }
    }
#elif defined NDEBUG
    /* Put the CPU and peripherals to the low-power mode.
    * you might need to customize the clock management for your application,
    * see the datasheet for your particular Cortex-M MCU.
    */
    /* !!!CAUTION!!!
    * The WFI instruction stops the CPU clock, which unfortunately disables
    * the JTAG port, so the ST-Link debugger can no longer connect to the
    * board. For that reason, the call to __WFI() has to be used with CAUTION.
    *
    * NOTE: If you find your board "frozen" like this, strap BOOT0 to VDD and
    * reset the board, then connect with ST-Link Utilities and erase the part.
    * The trick with BOOT(0) is it gets the part to run the System Loader
    * instead of your broken code. When done disconnect BOOT0, and start over.
    */
    //__WFI(); /* Wait-For-Interrupt */
#endif
}

/*..........................................................................*/
void Q_onAssert(char const *module, int loc) {
    /*
    * NOTE: add here your application-specific error handling
    */
    (void)module;
    (void)loc;
    QS_ASSERTION(module, loc, (uint32_t)10000U); /* report assertion to QS */

#ifndef NDEBUG
    /* light up both LEDs */
    BSP_LED_On(LED1);
    /* for debugging, hang on in an endless loop... */
    for (;;) {
    }
#endif

    NVIC_SystemReset();
}

/* QS callbacks ============================================================*/
#ifdef Q_SPY
/*..........................................................................*/
uint8_t QS_onStartup(void const *arg) {
    static uint8_t qsTxBuf[2*1024]; /* buffer for QS transmit channel */
    static uint8_t qsRxBuf[100];    /* buffer for QS receive channel */

    QS_initBuf  (qsTxBuf, sizeof(qsTxBuf));
    QS_rxInitBuf(qsRxBuf, sizeof(qsRxBuf));

    l_uartHandle.Instance        = USART1;
    l_uartHandle.Init.BaudRate   = 115200;
    l_uartHandle.Init.WordLength = UART_WORDLENGTH_8B;
    l_uartHandle.Init.StopBits   = UART_STOPBITS_1;
    l_uartHandle.Init.Parity     = UART_PARITY_NONE;
    l_uartHandle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    l_uartHandle.Init.Mode       = UART_MODE_TX_RX;
    l_uartHandle.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&l_uartHandle) != HAL_OK) {
        return (uint8_t)0; /* return failure */
    }

    /* Set UART to receive 1 byte at a time via interrupt */
    HAL_UART_Receive_IT(&l_uartHandle, (uint8_t *)qsRxBuf, 1);

    QS_tickPeriod_ = SystemCoreClock / BSP_TICKS_PER_SEC;
    QS_tickTime_ = QS_tickPeriod_; /* to start the timestamp at zero */

    /* setup the QS filters... */
    QS_FILTER_ON(QS_SM_RECORDS);
    QS_FILTER_ON(QS_UA_RECORDS);

    return (uint8_t)1; /* return success */
}
/*..........................................................................*/
void QS_onCleanup(void) {
}
/*..........................................................................*/
QSTimeCtr QS_onGetTime(void) {  /* NOTE: invoked with interrupts DISABLED */
    if ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) { /* not set? */
        return QS_tickTime_ - (QSTimeCtr)SysTick->VAL;
    }
    else { /* the rollover occured, but the SysTick_ISR did not run yet */
        return QS_tickTime_ + QS_tickPeriod_ - (QSTimeCtr)SysTick->VAL;
    }
}
/*..........................................................................*/
void QS_onFlush(void) {
    uint16_t b;

    QF_INT_DISABLE();
    while ((b = QS_getByte()) != QS_EOD) { /* while not End-Of-Data... */
        QF_INT_ENABLE();
        /* while TXE not empty */
        while ((l_uartHandle.Instance->ISR & UART_FLAG_TXE) == 0U) {
        }
        l_uartHandle.Instance->TDR = (b & 0xFFU);  /* put into TDR */
        QF_INT_DISABLE();
    }
    QF_INT_ENABLE();
}
/*..........................................................................*/
/*! callback function to reset the target (to be implemented in the BSP) */
void QS_onReset(void) {
    NVIC_SystemReset();
}
/*..........................................................................*/
/*! callback function to execute a user command (to be implemented in BSP) */
void QS_onCommand(uint8_t cmdId,
                  uint32_t param1, uint32_t param2, uint32_t param3)
{
    void assert_failed(char const *module, int loc);
    (void)cmdId;
    (void)param1;
    (void)param2;
    (void)param3;
    QS_BEGIN(COMMAND_STAT, (void *)1) /* application-specific record begin */
        QS_U8(2, cmdId);
        QS_U32(8, param1);
    QS_END()

    if (cmdId == 10U) {
        Q_ERROR();
    }
    else if (cmdId == 11U) {
        assert_failed("QS_onCommand", 123);
    }
}

#endif /* Q_SPY */
/*--------------------------------------------------------------------------*/

/*****************************************************************************
* NOTE00:
* The QF_AWARE_ISR_CMSIS_PRI constant from the QF port specifies the highest
* ISR priority that is disabled by the QF framework. The value is suitable
* for the NVIC_SetPriority() CMSIS function.
*
* Only ISRs prioritized at or below the QF_AWARE_ISR_CMSIS_PRI level (i.e.,
* with the numerical values of priorities equal or higher than
* QF_AWARE_ISR_CMSIS_PRI) are allowed to call the QK_ISR_ENTRY/QK_ISR_ENTRY
* macros or any other QF services. These ISRs are "QF-aware".
*
* Conversely, any ISRs prioritized above the QF_AWARE_ISR_CMSIS_PRI priority
* level (i.e., with the numerical values of priorities less than
* QF_AWARE_ISR_CMSIS_PRI) are never disabled and are not aware of the kernel.
* Such "QF-unaware" ISRs cannot call any QF services. In particular they
* can NOT call the macros QK_ISR_ENTRY/QK_ISR_ENTRY. The only mechanism
* by which a "QF-unaware" ISR can communicate with the QF framework is by
* triggering a "QF-aware" ISR, which can post/publish events.
*
* NOTE01:
* The User LED is used to visualize the idle loop activity. The brightness
* of the LED is proportional to the frequency of invcations of the idle loop.
* Please note that the LED is toggled with interrupts locked, so no interrupt
* execution time contributes to the brightness of the User LED.
*/
