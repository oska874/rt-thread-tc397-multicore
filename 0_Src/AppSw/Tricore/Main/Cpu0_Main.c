#include "Ifx_Types.h"
#include "IfxCpu.h"
#include "IfxScuWdt.h"
//#include "Multicore.h"
#include "rtthread.h"
#include "cpuport.h"
//#include "UART_Logging.h"

int rtthread_startup (void);

IFX_ALIGN(4)IfxCpu_syncEvent g_cpuSyncEvent = 0;

void get_core_id(void)
{
    rt_kprintf("idle %d\n", rt_hw_cpu_id());
    rt_thread_mdelay(1000);

}
void core0_main (void)
{
    IfxCpu_enableInterrupts();
    
    /* !!WATCHDOG0 AND SAFETY WATCHDOG ARE DISABLED HERE!!
     * Enable the watchdogs and service them periodically if it is required
     */
    IfxScuWdt_disableCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    IfxScuWdt_disableSafetyWatchdog(IfxScuWdt_getSafetyWatchdogPassword());
    /* Wait for CPU sync event: wait until all CPUs are in CpuX_Main to avoid variables' initialization problems */

    rtthread_startup();
    while (1)
    {

    }
}

static rt_uint32_t Led_CNT = 0x00;
static rt_uint8_t led_thread_stack[128];
static struct rt_thread led_thread_thread;
static void led_thread_entry (void *parameter)
{
    while (1)
    {
        rt_kprintf("LED %d, %d\n", rt_hw_cpu_id(), Led_CNT);
        Led_CNT++;
        rt_thread_mdelay(1000);
    }
}

uint32 Main_CNT = 0x00;

int main (void)
{
//    rt_thread_init(&led_thread_thread, "led", led_thread_entry, RT_NULL,\
//    &led_thread_stack[0], sizeof(led_thread_stack), RT_MAIN_THREAD_PRIORITY+1, 20);
//#ifdef RT_USING_SMP
//    rt_thread_control(&led_thread_thread, RT_THREAD_CTRL_BIND_CPU, (void*)1);
//#endif
//    rt_thread_startup(&led_thread_thread);

    IfxCpu_emitEvent(&g_cpuSyncEvent);
    IfxCpu_waitEvent(&g_cpuSyncEvent, 1);
    for (;;)
    {
        rt_kprintf("A%d,%d,%d\n\n", rt_hw_cpu_id(), rt_tick_get(),Main_CNT);
        Main_CNT++;
        rt_thread_mdelay(1000);
    }
}

