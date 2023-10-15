#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "cpuport.h"
#include "rtthread.h"

extern IfxCpu_syncEvent g_cpuSyncEvent;

static rt_uint32_t core4_CNT = 0x00;
static rt_uint8_t core4_thread_stack[128];
static struct rt_thread core4_thread_1;

static void core4_thread_entry (void *parameter)
{
    while (1)
    {
        rt_kprintf("E%d,%d\n", rt_hw_cpu_id(), core4_CNT);
        core4_CNT++;
        rt_thread_mdelay(1400);
    }
}

void core4_main (void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG4 IS DISABLED HERE!!
     * Enable the watchdog and service it periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());

#ifdef RT_USING_SMP
    core4_init();
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    rt_thread_init(&core4_thread_1, "core4", core4_thread_entry, RT_NULL, &core4_thread_stack[0],
            sizeof(core4_thread_stack), 5, 100);
    rt_thread_control(&core4_thread_1, RT_THREAD_CTRL_BIND_CPU, (void*) 4);
    rt_thread_startup(&core4_thread_1);

    /* start scheduler */
    rt_system_scheduler_start();
#endif
    
    while (1)
    {
    }
}
