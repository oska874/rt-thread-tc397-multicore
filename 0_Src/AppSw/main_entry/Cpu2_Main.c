#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "cpuport.h"
#include "rtthread.h"
#include "rthw.h"

extern IfxCpu_syncEvent g_cpuSyncEvent;

static rt_uint32_t core2_CNT = 0x00;
static rt_uint8_t core2_thread_stack[128];
static struct rt_thread core2_thread_1;

static void core2_thread_entry (void *parameter)
{

    while (1)
    {
        rt_kprintf("C%d,%d,%d\n", rt_hw_cpu_id(), rt_tick_get(), core2_CNT);

        core2_CNT++;
        rt_thread_mdelay(1200);
    }
}

void core2_main (void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG2 IS DISABLED HERE!!
     * Enable the watchdog and service it periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());

#ifdef RT_USING_SMP

    core2_init();
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    rt_thread_init(&core2_thread_1, "core2", core2_thread_entry, RT_NULL, &core2_thread_stack[0],
            sizeof(core2_thread_stack), 5, 100);
    rt_thread_control(&core2_thread_1, RT_THREAD_CTRL_BIND_CPU, (void*) 2);
    rt_thread_startup(&core2_thread_1);

    /* start scheduler */
    rt_system_scheduler_start();

#endif
    
    while (1)
    {
//        controlLEDflag(); /* Toggle the global variable g_turnLEDon approximately every 1 second */
    }
}
