#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "cpuport.h"
#include "rtthread.h"
#include "rthw.h"

extern IfxCpu_syncEvent g_cpuSyncEvent;

static rt_uint32_t core5_CNT = 0x00;
static rt_uint8_t core5_thread_stack[128];
static struct rt_thread core5_thread_1;

static void core5_thread_entry (void *parameter)
{
    while (1)
    {
        rt_kprintf("F%d,%d\n", rt_hw_cpu_id(), core5_CNT);
        core5_CNT++;
        rt_thread_mdelay(1500);
    }
}

void core5_main (void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG5 IS DISABLED HERE!!
     * Enable the watchdog and service it periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());

#ifdef RT_USING_SMP
    core5_init();
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    rt_thread_init(&core5_thread_1, "core5", core5_thread_entry, RT_NULL, &core5_thread_stack[0],
            sizeof(core5_thread_stack), 5, 100);
    rt_thread_control(&core5_thread_1, RT_THREAD_CTRL_BIND_CPU, (void*) 5);
    rt_thread_startup(&core5_thread_1);

    /* start scheduler */
    rt_system_scheduler_start();
#endif
    
    while (1)
    {
        if (core5_CNT % 1000)
        {
//            rt_kprintf("c5ore %d, %d\n", IfxCpu_getCoreId(), core5_CNT);
            core5_CNT++;
        }
    }
}
