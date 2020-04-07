#include "pmu_api.h"

extern volatile int feature_flag;

void pmu_fw_init_user(void)
{
    feature_flag = 0x8;
}
#if 0
int pmu_fw_process_command(struct pmu_cmd_t *pmu_cmd)
{
    int r = 0;
    
    r = pmu_fw_process_command_test(pmu_cmd);

    return r;
}
#endif

void pmu_fw_process_loop(void)
{
}

/*called in sleep mode*/
#if 0
void pmu_fw_process_sleep_loop(void)
{
}
#endif

void irq_rtc_callback(void)
{
}

void irq_epir_callback(void)
{
}
void irq_ipir_callback(void)
{
}
void pmu_power_mode_sleep_user_pre(void)
{
}
void pmu_power_mode_sleep_user_post(void)
{
}

