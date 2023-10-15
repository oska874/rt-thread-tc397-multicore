/*
 * Copyright (c) 2006-2018, RT-Thread Development Team
 *rt_interrupt_from_thread_core0
 * SPDX-License-Identifier: GPL-2.0 License
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021/02/01     BalanceTWK   The unify TriCore porting code.
 */

#include <rthw.h>
#include <rtthread.h>
#include <stdint.h>
#include "cpuport.h"
#include "IfxCpu_Trap.h"
#include "UART_Logging.h"
#include "IfxSrc_cfg.h"
#include "rtdef.h"
#include <string.h>

static IfxStm_Timer tricore_timers[6];
static volatile Ifx_STM * const STMs[6] = { &MODULE_STM0, &MODULE_STM1,
		&MODULE_STM2, &MODULE_STM3, &MODULE_STM4, &MODULE_STM5, };
static volatile Ifx_SRC_SRCR * const GPSR[6] = { &SRC_GPSR_GPSR0_SR0,
		&SRC_GPSR_GPSR1_SR0, &SRC_GPSR_GPSR2_SR0, &SRC_GPSR_GPSR3_SR0,
		&SRC_GPSR_GPSR4_SR0, &SRC_GPSR_GPSR5_SR0, };

//static volatile rt_ubase_t rt_interrupt_from_thread_core0 = RT_NULL;
//static volatile rt_ubase_t rt_interrupt_to_thread_core0 = RT_NULL;
//static volatile rt_ubase_t rt_interrupt_from_thread_core1 = RT_NULL;
//static volatile rt_ubase_t rt_interrupt_to_thread_core1 = RT_NULL;

static volatile rt_ubase_t rt_int_to_thread_corex[6] = {RT_NULL,};
static volatile rt_ubase_t rt_int_from_thread_corex[6] = {RT_NULL,};

//static volatile rt_ubase_t rt_thread_switch_interrupt_flag_core0 = RT_NULL;
//static volatile rt_ubase_t rt_thread_switch_interrupt_flag_core1 = RT_NULL;
static volatile rt_ubase_t rtt_switch_int_flag_corex[6] = {RT_NULL,};

typedef struct UpperCtxInfo{
    unsigned long  _PCXI;
    unsigned long  _PSW;
    unsigned long*  _A10;
    unsigned long*  _A11;
    unsigned long  _D8;
    unsigned long  _D9;
    unsigned long  _D10;
    unsigned long  _D11;
    unsigned long*  _A12;
    unsigned long*  _A13;
    unsigned long*  _A14;
    unsigned long*  _A15;
    unsigned long  _D12;
    unsigned long  _D13;
    unsigned long  _D14;
    unsigned long  _D15;
}UpperCtx_T,*UpperCtx_Ptr;

typedef struct LowCtxInfo{
    unsigned long  _PCXI;
    unsigned long*  _PC;
    unsigned long*  _A2;
    unsigned long*  _A3;
    unsigned long  _D0;
    unsigned long  _D1;
    unsigned long  _D2;
    unsigned long  _D3;
    unsigned long*  _A4;
    unsigned long*  _A5;
    unsigned long*  _A6;
    unsigned long*  _A7;
    unsigned long  _D4;
    unsigned long  _D5;
    unsigned long  _D6;
    unsigned long  _D7;
}LowCtx_T,*LowCtx_Ptr;

void rt_hw_systick_init( void )
{
  IfxStm_Timer_Config timer_config;
  uint32_t TRICORE_CPU_ID = IfxCpu_getCoreIndex();
  IfxStm_Timer_initConfig(&timer_config, STMs[TRICORE_CPU_ID]);

  timer_config.base.frequency = RT_TICK_PER_SECOND;
  timer_config.base.isrPriority = 2;
  IfxStm_Timer_init(&tricore_timers[TRICORE_CPU_ID], &timer_config);
  IfxStm_Timer_run(&tricore_timers[TRICORE_CPU_ID]);
}

void rt_hw_usart_init(void)
{
    initUART();
}

void corex_trigger_schedule(uint8_t core_id)
{
    UpperCtx_Ptr ptUpperCtx = NULL;
#ifdef RT_USING_SMP
    rt_base_t level;
    level = rt_hw_local_irq_disable();
#endif
    if(core_id>RT_CPUS_NR)
    {
        rt_kprintf("cts over %d\n",core_id);
    }

    /* 鍒ゆ柇鏄惁闇�瑕佸垏鎹㈠埌 to 绾跨▼ */
    if(rt_int_to_thread_corex[core_id])
    {
        /* To ensure memory coherency, a DSYNC instruction must be executed prior to
           any access to an active CSA memory location. The DSYNC instruction forces
           all internally buffered CSA register state to be written to memory.     */
        __dsync();
        /* 鑾峰彇褰撳墠绾跨▼鐨� CSA 涓婁笅鏂囧湴鍧� */
        ptUpperCtx = (UpperCtx_Ptr)LINKWORD_TO_ADDRESS( __mfcr( CPU_PCXI ) );

        if(rt_int_from_thread_corex[core_id])
        {
            /* 淇濆瓨褰撳墠绾跨▼鐨� CSA 涓婁笅鏂� */
            *( (unsigned long *)rt_int_from_thread_corex[core_id] ) = ptUpperCtx->_PCXI;
        }
        /* 灏唗o绾跨▼鐨� CSA linkword璧嬬粰褰撳墠绾跨▼鐨勪笂灞� 涓婁笅鏂囩殑 LinkWord 锛岀敤浜� TriCore 鑷姩鍒囨崲绾跨▼銆�*/
        ptUpperCtx->_PCXI = *( (unsigned long *)rt_int_to_thread_corex[core_id] );
        __isync();
    }

#ifdef RT_USING_SMP
    rt_hw_local_irq_enable(level);
#endif
}

void corex_interrupt(uint8_t core_id)
{
#ifdef RT_USING_SMP
    struct rt_cpu    *pcpu;
#endif
    if (core_id > RT_CPUS_NR)
    {
        rt_kprintf("ct over %d\n", core_id);
    }
    uint32_t TRICORE_CPU_ID = IfxCpu_getCoreIndex();
    rtt_switch_int_flag_corex[core_id] = 1;
    rt_interrupt_enter();
    IfxStm_Timer_acknowledgeTimerIrq(&tricore_timers[TRICORE_CPU_ID]);
    rt_tick_increase();
    rt_interrupt_leave();
    rtt_switch_int_flag_corex[core_id] = 0;

#ifdef RT_USING_SMP
    pcpu   = rt_cpu_index(core_id);
    /* whether do switch in interrupt */
    if (pcpu->irq_switch_flag)
    {
        rt_scheduler_do_irq_switch();
    }
#endif
}

IFX_INTERRUPT(Core0_INTERRUPT, 0, 2)
{
    corex_interrupt(0);
}

IFX_INTERRUPT(Core1_INTERRUPT, 1, 2)
{
    corex_interrupt(1);
}

IFX_INTERRUPT(Core2_INTERRUPT, 2, 2)
{
    corex_interrupt(2);
}

IFX_INTERRUPT(Core3_INTERRUPT, 3, 2)
{
    corex_interrupt(3);
}

IFX_INTERRUPT(Core4_INTERRUPT, 4, 2)
{
    corex_interrupt(4);
}

IFX_INTERRUPT(Core5_INTERRUPT, 5, 2)
{
    corex_interrupt(5);
}

IFX_INTERRUPT(Core0_YIELD, 0, 1)
{
    rt_interrupt_enter();
    /* clear the SRR bit */
    GPSR[0]->B.CLRR = 1;
    corex_trigger_schedule(0);
    rt_interrupt_leave();
}

IFX_INTERRUPT(Core1_YIELD, 1, 1)
{
    rt_interrupt_enter();
    /* clear the SRR bit */
    GPSR[1]->B.CLRR = 1;
    corex_trigger_schedule(1);
    rt_interrupt_leave();
}

IFX_INTERRUPT(Core2_YIELD, 2, 1)
{
    rt_interrupt_enter();
    /* clear the SRR bit */
    GPSR[2]->B.CLRR = 1;
    corex_trigger_schedule(2);
    rt_interrupt_leave();
}

IFX_INTERRUPT(Core3_YIELD, 3, 1)
{
    rt_interrupt_enter();
    /* clear the SRR bit */
    GPSR[3]->B.CLRR = 1;
    corex_trigger_schedule(3);
    rt_interrupt_leave();
}

IFX_INTERRUPT(Core4_YIELD, 4, 1)
{
    rt_interrupt_enter();
    /* clear the SRR bit */
    GPSR[4]->B.CLRR = 1;

    corex_trigger_schedule(4);
    rt_interrupt_leave();
}

IFX_INTERRUPT(Core5_YIELD, 5, 1)
{
    rt_interrupt_enter();
    /* clear the SRR bit */
    GPSR[5]->B.CLRR = 1;
    corex_trigger_schedule(5);
    rt_interrupt_leave();
}

/**
 * This function will initial board.
 */
#ifdef RT_USING_HEAP
extern unsigned int __HEAP[];
extern unsigned int __HEAP_END[];
#endif
void rt_hw_board_init()
{
    uint32_t TRICORE_CPU_ID = IfxCpu_getCoreIndex();
    IfxStm_setSuspendMode(STMs[TRICORE_CPU_ID], IfxStm_SuspendMode_hard);
#ifdef RT_USING_HEAP
    /* initialize heap */
    rt_system_heap_init(__HEAP, __HEAP_END);
#endif

    /* Set-up the timer interrupt. */
    rt_hw_systick_init();
    /* USART driver initialization is open by default */
#ifdef RT_USING_SERIAL
    rt_hw_usart_init();
#endif

    /* Set the shell console output device */
#ifdef RT_USING_CONSOLE
    // rt_console_set_device(RT_CONSOLE_DEVICE_NAME);
#endif

    /* Board underlying hardware initialization */
#ifdef RT_USING_COMPONENTS_INIT
    rt_components_board_init();
#endif

    IfxSrc_init(GPSR[TRICORE_CPU_ID],
            (IfxSrc_Tos)TRICORE_CPU_ID,
            1);
    IfxSrc_enable(GPSR[TRICORE_CPU_ID]);
}

#ifdef RT_USING_SMP

void rt_hw_context_switch_to(rt_ubase_t to, struct rt_thread *to_thread)
{
    uint32_t core_id = IfxCpu_getCoreIndex();
    if (core_id < RT_CPUS_NR)
    {
//        if(core_id == 2)
//        {
//            rt_kprintf("stw %d\n", core_id);
//        }
        rt_base_t levelCorex = rt_hw_local_irq_disable();
        rt_int_to_thread_corex[core_id] = (*((unsigned long*) to));
        rt_cpu_self()->current_thread = to_thread;
        rt_hw_local_irq_enable(levelCorex);
        switch (core_id)
        {
            case 0 :
                __syscall(0);
                break;
            case 1 :
                __syscall(1);
                break;
            case 2 :
                __syscall(2);
                break;
            case 3 :
                __syscall(3);
                break;
            case 4 :
                __syscall(4);
                break;
            case 5 :
                __syscall(5);
                break;
            default :
                break;
        }
    }
    else{
        rt_kprintf("RHCST %d\n", core_id);
    }
}

void rt_hw_context_switch_interrupt(void *context, rt_ubase_t from, rt_ubase_t to, struct rt_thread *to_thread)
{
    rt_base_t levelCore0,levelCore1;
    uint32_t core_id = IfxCpu_getCoreIndex();

//    if (core_id == 2)
//    {
//        rt_kprintf("swint %d\n", core_id);
//    }
    switch (core_id)
    {
        case 0 :
        case 1 :
        case 2 :
        case 3 :
        case 4 :
        case 5 :
        {
            rt_base_t levelCorex = rt_hw_local_irq_disable();
            rt_int_from_thread_corex[core_id] = (*((unsigned long*) from));
            rt_int_to_thread_corex[core_id] = (*((unsigned long*) to));
            rt_cpu_self()->current_thread = to_thread;
            rt_hw_local_irq_enable(levelCorex);
        }
            break;
        default :
            rt_kprintf("rhcsi %d\n", core_id);
            break;
    }
    /* 鍦ㄤ繚瀛樺畬绾跨▼ CSA LinkWord 鍚�,浼氱粰 SETR 浣嶇疆 1锛岃繖鏍蜂細瑙﹀彂涓�涓紓甯搁櫡闃卞嚱鏁般�傚湪杩欎釜闄烽槺鍑芥暟閲屾墠浼氬幓鐪熸鐨勫垏鎹㈢嚎绋嬨��*/
    GPSR[core_id]->B.SETR = 1;
}

void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to, struct rt_thread *to_thread)
{
//    rt_base_t levelCore0,levelCore1;
    
    uint32_t core_id = IfxCpu_getCoreIndex();

    switch (core_id)
    {
        case 0 :
//            rt_kprintf("zero\n");
            break;
        case 1 :
//            rt_kprintf("one\n");
            break;
        case 2 :
//            rt_kprintf("two\n");
            break;
        case 3 :
//            rt_kprintf("three\n");
            break;
        case 4 :
//            rt_kprintf("four\n");
            break;
        case 5 :
//            rt_kprintf("five\n");
            break;
        default :
            rt_kprintf("schr %d\n", core_id);
            break;
    }
    if (core_id < RT_CPUS_NR)
    {
        rt_base_t levelCorex = rt_hw_local_irq_disable();
        rt_int_from_thread_corex[core_id] = (*((unsigned long*) from));
        rt_int_to_thread_corex[core_id] = (*((unsigned long*) to));
        to = RT_NULL;
        rt_cpu_self()->current_thread = to_thread;
        rt_hw_local_irq_enable(levelCorex);
        /* 鍦ㄤ繚瀛樺畬绾跨▼ CSA LinkWord 鍚庯紝鍒ゆ柇杩欐绾跨▼鍒囨崲鏄惁鏄� systick 瑙﹀彂鐨勭嚎绋嬪垏鎹€��*/
        if (rtt_switch_int_flag_corex[core_id] == 0)
        {
            extern rt_hw_spinlock_t _cpus_lock;
            if (_cpus_lock.slock)
            {
                rt_hw_spin_unlock(&_cpus_lock);
            }
            // rt_interrupt_from_thread_core0 = (*( (unsigned long *)from ));
            /* 濡傛灉涓嶆槸 systick 寮曞彂鐨勭嚎绋嬪垏鎹紝閭ｄ箞灏辫Е鍙戜竴涓嚎绋嬪垏鎹㈢殑寮傚父鍑芥暟锛岃繖涓紓甯稿嚱鏁伴噷浼氱湡姝ｅ仛绾跨▼鍒囨崲鐨勪簨鎯呫��*/
            switch (core_id)
            {
                case 0 :
                    __syscall(0);
                    break;
                case 1 :
                    //                        rt_kprintf("c1s\n");
                    __syscall(1);
                    break;
                case 2 :
//                    rt_kprintf("C2S\n");
                    __syscall(2);
                    break;
                case 3 :
                    __syscall(3);
                    break;
                case 4 :
                    __syscall(4);
                    break;
                case 5 :
                    __syscall(5);
                    break;
                default :
                    break;
            }
        }
    }
}

void rt_hw_secondary_cpu_up(void)
{

}

void time_tick_init(uint8_t core_id)
{
	IfxSrc_Tos cpuid[6] = { IfxSrc_Tos_cpu0, IfxSrc_Tos_cpu1, IfxSrc_Tos_cpu2,
			IfxSrc_Tos_cpu3, IfxSrc_Tos_cpu4, IfxSrc_Tos_cpu5, };
    IfxStm_Timer_Config timer_config;
    IfxStm_Timer_initConfig(&timer_config, STMs[core_id]);

    timer_config.base.trigger.isrProvider = cpuid[core_id];
    timer_config.base.isrProvider = cpuid[core_id];
	timer_config.base.frequency = RT_TICK_PER_SECOND;

	timer_config.base.isrPriority = 2;

	IfxStm_Timer_init(&tricore_timers[core_id], &timer_config);
    IfxStm_Timer_run(&tricore_timers[core_id]);

	IfxSrc_init(GPSR[core_id], cpuid[core_id], 1);

    IfxSrc_enable(GPSR[core_id]);

}

void core1_init(void)
{
    time_tick_init(1);
}

void core2_init(void)
{
    time_tick_init(2);
}

void core3_init(void)
{
    time_tick_init(3);
}

void core4_init(void)
{
    time_tick_init(4);
}

void core5_init(void)
{
    time_tick_init(5);
}

void rt_hw_ipi_send(int ipi_vector, unsigned int cpu_mask)
{
    
}

void rt_hw_spin_lock_init(rt_hw_spinlock_t *lock)
{
    lock->slock = 0;
}

/* 鑾峰彇spinlock锛屽繖绛夊緟鐩村埌鑾峰彇鎴愬姛 */
void rt_hw_spin_lock(rt_hw_spinlock_t *lock)
{
    boolean         retVal = FALSE;
    volatile unsigned int spinLockVal;

    do
    {
        /** \brief This function is a implementation of a binary semaphore
         *  using compare and swap instruction
         * \param address address of resource.
         * \param value This variable is updated with status of address
         * \param condition if the value of address matches with the value 
         * of condition, then swap of value & address occurs.
         *  __cmpswapw((address), ((unsigned long)value), (condition) )
         */
        spinLockVal = 1UL;
        spinLockVal =
        (unsigned int)__cmpAndSwap(((unsigned int *)(&(lock->slock))), spinLockVal, 0);

        /* Check if the SpinLock WAS set before the attempt to acquire spinlock */
        if (spinLockVal == 0)
        {
            retVal = TRUE;
        }
    } while (retVal == FALSE);
}

void rt_hw_spin_unlock(rt_hw_spinlock_t *lock)
{
    boolean         retVal = FALSE;
    volatile unsigned int spinLockVal;

    do
    {
        spinLockVal = 0UL;
        spinLockVal =
            (unsigned int)__cmpAndSwap(((unsigned int *)(&(lock->slock))), spinLockVal, 1);

        /* Check if the SpinLock WAS set before the attempt to acquire spinlock */
        if (spinLockVal == 1)
        {
            retVal = TRUE;
        }
    } while (retVal == FALSE);
}

int rt_hw_cpu_id(void)
{
    return (int)IfxCpu_getCoreIndex();
}

void rt_hw_local_irq_enable(rt_base_t level)
{
    restoreInterrupts((boolean)level);
}

void rt_hw_secondary_cpu_idle_exec(void)
{

}

rt_base_t rt_hw_local_irq_disable()
{
    rt_base_t level;
    level = IfxCpu_disableInterrupts();
    return level;
}
#else

rt_base_t rt_hw_interrupt_disable(void)
{
    rt_base_t level;
    level = IfxCpu_disableInterrupts();
    return level;
}

void rt_hw_interrupt_enable(rt_base_t level)
{
    restoreInterrupts((boolean)level);
}

void rt_hw_context_switch_to(rt_ubase_t to)
{
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    rt_interrupt_to_thread_core0 = (*((unsigned long *)to));
    __syscall( 0 );
    rt_hw_interrupt_enable(level);
   /* Will not get here. */
}

void rt_hw_context_switch_interrupt(rt_ubase_t from, rt_ubase_t to)
{
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    rt_interrupt_from_thread_core0 = (*( (unsigned long *)from ));
    rt_interrupt_to_thread_core0 = (*((unsigned long *)to));
    rt_hw_interrupt_enable(level);
    /* 鍦ㄤ繚瀛樺畬绾跨▼ CSA LinkWord 鍚�,浼氱粰 SETR 浣嶇疆 1锛岃繖鏍蜂細瑙﹀彂涓�涓紓甯搁櫡闃卞嚱鏁般�傚湪杩欎釜闄烽槺鍑芥暟閲屾墠浼氬幓鐪熸鐨勫垏鎹㈢嚎绋嬨��*/
    GPSR[TRICORE_CPU_ID]->B.SETR = 1;
}

void rt_hw_context_switch(rt_ubase_t from, rt_ubase_t to)
{
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    rt_interrupt_from_thread_core0 = (*( (unsigned long *)from ));
    rt_interrupt_to_thread_core0 = (*((unsigned long *)to));
    to = RT_NULL;
    rt_hw_interrupt_enable(level);
    /* 鍦ㄤ繚瀛樺畬绾跨▼ CSA LinkWord 鍚庯紝鍒ゆ柇杩欐绾跨▼鍒囨崲鏄惁鏄� systick 瑙﹀彂鐨勭嚎绋嬪垏鎹€��*/
    if(rt_thread_switch_interrupt_flag_core0 == 0)
    {
         /* 濡傛灉涓嶆槸 systick 寮曞彂鐨勭嚎绋嬪垏鎹紝閭ｄ箞灏辫Е鍙戜竴涓嚎绋嬪垏鎹㈢殑寮傚父鍑芥暟锛岃繖涓紓甯稿嚱鏁伴噷浼氱湡姝ｅ仛绾跨▼鍒囨崲鐨勪簨鎯呫��*/
        __syscall( 0 );
    }
}

#endif

/**
 * This function will initialize thread stack
 *
 * @param tentry the entry of thread
 * @param parameter the parameter of entry
 * @param stack_addr the beginning stack address
 * @param texit the function will be called when thread exit
 *
 * @return stack address
 */
rt_uint8_t *rt_hw_stack_init(void       *tentry,
                             void       *parameter,
                             rt_uint8_t *stack_addr,
                             void       *texit)
{
    rt_base_t level;
    UpperCtx_Ptr upperCtxPtr = NULL;
    LowCtx_Ptr   lowCtxPtr = NULL;
#ifdef RT_USING_SMP
    level = rt_hw_local_irq_disable();
#else
    level = rt_hw_interrupt_disable();
#endif
    {
        /* DSync to ensure that buffering is not a problem. */
        __dsync();

        /* Consume two free CSAs. */
        upperCtxPtr = (UpperCtx_Ptr)LINKWORD_TO_ADDRESS(__mfcr(CPU_FCX));

        if( NULL != upperCtxPtr )
        {
            /* The Lower Links to the Upper. */
            lowCtxPtr = (UpperCtx_Ptr)LINKWORD_TO_ADDRESS( upperCtxPtr->_PCXI );
        }

        /* Check that we have successfully reserved two CSAs. */
        if( ( NULL != lowCtxPtr ) && ( NULL != upperCtxPtr ))
        {
            /* Remove the two consumed CSAs from the free CSA list. */
            __dsync();
            __mtcr( CPU_FCX, lowCtxPtr->_PCXI );
            /* MTCR is an exception and must be followed by an ISYNC instruction. */
            __isync();
        }
        else
        {
            /* Simply trigger a context list depletion trap. */
            __svlcx();
        }
    }
#ifdef RT_USING_SMP
    rt_hw_local_irq_enable(level);
#else
    rt_hw_interrupt_enable(level);
#endif

    memset( upperCtxPtr, 0, TRICORE_NUM_WORDS_IN_CSA * sizeof( unsigned long ) );
    upperCtxPtr->_A11 = ( unsigned long* )texit;
    /* 瀵瑰簲 A10 瀵勫瓨鍣�; 杩欎釜瀵勫瓨鍣ㄧ敤浜庝繚瀛樻爤鎸囬拡 */
    upperCtxPtr->_A10 = ( unsigned long* )stack_addr;
    /* 瀵瑰簲 PSW 瀵勫瓨鍣紱杩欎釜瀵勫瓨鍣ㄧ敤浜庝繚瀛樺綋鍓嶇嚎绋嬬殑鍒濆鐘舵��*/
    upperCtxPtr->_PSW = TRICORE_SYSTEM_PROGRAM_STATUS_WORD;

    /* Clear the lower CSA. */
    memset( lowCtxPtr, 0, TRICORE_NUM_WORDS_IN_CSA * sizeof( unsigned long ) );
    /* 瀵瑰簲 A4 瀵勫瓨鍣�; 鐢ㄤ簬淇濆瓨鍑芥暟鐨勫叆鍙� */
    lowCtxPtr->_A4 = ( unsigned long *) parameter;
    /* 瀵瑰簲 A11 瀵勫瓨鍣�; 鐢ㄤ簬淇濆瓨 PC 鎸囬拡銆�*/
    lowCtxPtr->_PC = ( unsigned long *) tentry;
    /* PCXI pointing to the Upper context. */
    lowCtxPtr->_PCXI = ( TRICORE_INITIAL_PCXI_UPPER_CONTEXT_WORD | ( unsigned long ) ADDRESS_TO_LINKWORD( upperCtxPtr ) );
    /* Save the link to the CSA in the top of stack. */
    *((unsigned long * )stack_addr) = (unsigned long) ADDRESS_TO_LINKWORD( lowCtxPtr );
    /* DSync to ensure that buffering is not a problem. */
    __dsync();

    return stack_addr;
}

void tricore0_trap_yield_for_task( int iTrapIdentification )
{
  switch( iTrapIdentification )
  {
    case 0:
        corex_trigger_schedule(0);
      break;

    default:
      /* Unimplemented trap called. */
      /* TODO */
      break;
  }
}

void tricore1_trap_yield_for_task( int iTrapIdentification )
{
  switch( iTrapIdentification )
  {
    case 1:
        corex_trigger_schedule(1);
      break;

    default:
      /* Unimplemented trap called. */
      /* TODO */
      break;
  }
}

void tricore2_trap_yield_for_task( int iTrapIdentification )
{
  switch( iTrapIdentification )
  {
    case 2:
//        rt_kprintf("trap2\n");
        corex_trigger_schedule(2);
      break;

    default:
      /* Unimplemented trap called. */
      /* TODO */
      break;
  }
}

void tricore3_trap_yield_for_task( int iTrapIdentification )
{
  switch( iTrapIdentification )
  {
    case 3:
        corex_trigger_schedule(3);
      break;

    default:
      /* Unimplemented trap called. */
      /* TODO */
      break;
  }
}

void tricore4_trap_yield_for_task( int iTrapIdentification )
{
  switch( iTrapIdentification )
  {
    case 4:
        corex_trigger_schedule(4);
      break;

    default:
      /* Unimplemented trap called. */
      /* TODO */
      break;
  }
}

void tricore5_trap_yield_for_task( int iTrapIdentification )
{
  switch( iTrapIdentification )
  {
    case 5:
        corex_trigger_schedule(5);
      break;

    default:
      /* Unimplemented trap called. */
      /* TODO */
      break;
  }
}


