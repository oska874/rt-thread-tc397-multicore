#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
//#include "Multicore.h"
#include "cpuport.h"
#include "rtthread.h"
//#include "UART_Logging.h"

extern IfxCpu_syncEvent g_cpuSyncEvent;

static rt_uint32_t core1_CNT = 0x00;
static rt_uint8_t core1_thread_stack[128];
static struct rt_thread core1_thread_1;

static void core1_thread_entry (void *parameter)
{
    while (1)
    {
        rt_kprintf("B%d,%d\n\n", rt_hw_cpu_id(), core1_CNT);
        core1_CNT++;
        rt_thread_mdelay(1100);
    }
}

void core1_main (void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG1 IS DISABLED HERE!!
     * Enable the watchdog and service it periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());

#ifdef RT_USING_SMP
    core1_init();
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    rt_thread_init(&core1_thread_1, "core1", core1_thread_entry, RT_NULL, &core1_thread_stack[0],
            sizeof(core1_thread_stack), 5, 100);
    rt_thread_control(&core1_thread_1, RT_THREAD_CTRL_BIND_CPU, (void*) 1);
    rt_thread_startup(&core1_thread_1);

    /* start scheduler */
    rt_system_scheduler_start();

#endif
    while (1)
    {
        
    }
}
