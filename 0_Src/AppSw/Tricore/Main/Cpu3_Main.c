#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
#include "cpuport.h"
#include "rtthread.h"

extern IfxCpu_syncEvent g_cpuSyncEvent;

static rt_uint32_t core3_CNT = 0x00;
static rt_uint8_t core3_thread_stack[128];
static struct rt_thread core3_thread_1;

static void core3_thread_entry (void *parameter)
{
    while (1)
    {
        rt_kprintf("D%d,%d\n", rt_hw_cpu_id(), core3_CNT);
        core3_CNT++;
        rt_thread_mdelay(1300);
    }
}

void core3_main (void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG3 IS DISABLED HERE!!
     * Enable the watchdog and service it periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());

#ifdef RT_USING_SMP
    core3_init();
    /* Wait for CPU sync event */
    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);

    rt_thread_init(&core3_thread_1, "core3", core3_thread_entry, RT_NULL, &core3_thread_stack[0],
            sizeof(core3_thread_stack), 5, 100);
    rt_thread_control(&core3_thread_1, RT_THREAD_CTRL_BIND_CPU, (void*) 3);
    rt_thread_startup(&core3_thread_1);

    /* start scheduler */
    rt_system_scheduler_start();
#endif
    
    while (1)
    {
        if (core3_CNT % 1000 == 0)
        {
//            rt_kprintf("c3ore %d, %d\n", rt_hw_cpu_id(), core3_CNT);
            core3_CNT++;
        }

    }
}
