/*------------------------------------------------------------------------------------------------*/
/*                                                                                                */
/*           Copyright (C) 2018 NeuronBasic Co., Ltd. All rights reserved.                        */
/*                                                                                                */
/*------------------------------------------------------------------------------------------------*/
#include "nbsdk.h"
#include "bsp.h"
#include "detect_win_basic.h"

#define BASIC_FACE_MODE
#define DETECT_BASIC_FACE_SCORE	   150000

//#define USE_HAND 1
//#define USE_HEAD 1
#define USE_MOTION 1

#define LIGHT_BOOT_STATUS 0
#define LIGHT_IDLE_STATUS 1
#define LIGHT_ON_STATUS   2
#define LIGHT_OFF_STATUS  3

static int light_status = LIGHT_BOOT_STATUS;

struct detect_basic_face_plan_t basic_face_plan_para = {0};

static struct detect_basic_face_plan_t *basic_get_plan_para( void )
{
    return &basic_face_plan_para;
}

void internal_sensor_output_enable(int no_msb);
void board_init_user( void )
{
    system_config_share(0);

    sys_set_padshare(IOA2, PAD_FUNC3, PAD_PULL_NO_PULL, PAD_STRENGTH_DIS); //LED3

    // internal_sensor_output_enable(0);
}

/*implement custom detect action*/
void DetectUserAction(struct fe_output_frame_t *frame,int frame_state)
{
    if (!frame_state)
    {
        //printf("O");
    }
    else
    {
        printf("X");
        //scaler_dump_wins(&frame->wins);
    }
}

#ifdef NOT_USE_AUTO_EG
// 自动调整亮度，每一帧的帧结束中断时调用。
// 如果需要自己修改 AEC/AGC 算法，请把下列函数增加到 main.c 或 test.c 中
int FrameAdjustAecAgc_internal( void )
{
    if (!sensor_conf->auto_exposure)
    {
        return 0;
    }

    /************************************
    *   你可以在这里实现你的AEC / AGC 调整算法。
    ************************************/

    return 0;
}
#endif

static void play_audio(int aid)
{
    audio_play_fid_pending(aid);
}

#define PWM_SYSTEM_CLOCK (system_clock())

static int pwm_map[] = {
//  0, 1, 2,  3,  4,  5,  6,  7,  8,  9,  10
    0, 5, 12, 20, 30, 41, 53, 65, 80, 90, 100
};

static int pwm_id;
static int pwm_clk;
static int prescaler_clk;
static int pwm_freq; 
static int pwm_period;
static int pwm_duty_ration;
static int pwm_duty_index;

static void pwm_start(void)
{
    pwm_id = IOA2; // PORTA2 PWM5
    
    /*system input clock: equal core clock*/
    pwm_clk = PWM_SYSTEM_CLOCK;
    
    prescaler_clk = pwm_clk;
    
    /*pwm output freq*/
    pwm_freq        = 2000; /*2K HZ*/
    pwm_period      = prescaler_clk/pwm_freq;
    pwm_duty_ration = 100;
    pwm_duty_index  = 10;

    //config and enable pwm
    pwm_init(pwm_id, 0);
    pwm_enable(pwm_id, 1);
    pwm_set_peroid(pwm_id, pwm_period);
    pwm_set_sample(pwm_id, pwm_period);
}

static void end_pwm(void)
{
    pwm_enable(pwm_id, 0);
}

static void set_pwm(int per)
{
    /*pwm out duty cycle setting*/

    pwm_duty_ration = per;
    int sample = pwm_period*pwm_duty_ration/100;

    pwm_set_sample(pwm_id, sample);
}

static void down_pwm(void)
{
    if (pwm_duty_ration >= 100) {
        pwm_duty_ration = 100;
    } else {
        pwm_duty_ration += 7;
    }
    if (pwm_duty_ration >= 100) {
        pwm_duty_ration = 100;
    }
    set_pwm(pwm_duty_ration);
}

static void up_pwm(void)
{
    if (pwm_duty_ration <= 0) {
        pwm_duty_ration = 0;
    } else {
        pwm_duty_ration -= 7;
    }
    if (pwm_duty_ration <= 0) {
        pwm_duty_ration = 0;
    }
    set_pwm(pwm_duty_ration);
}

static void off_pwm(void)
{
    pwm_duty_ration = 100;
    pwm_duty_index  = 10;
    set_pwm(pwm_duty_ration);
}

static void on_pwm(void)
{
    pwm_duty_ration = 0;
    pwm_duty_index  = 0;
    set_pwm(pwm_duty_ration);
}

static int low_power_time_flag = 0;
static unsigned long stime = 0, etime = 0, ctime = 0;

void* plan_para_init( void )
{
    struct detect_basic_face_plan_t *plan_para = basic_get_plan_para();

    printf("plan_para_init .\n");
    memset( plan_para, 0, sizeof(struct detect_basic_face_plan_t));

    plan_para->displayEn = 1;
    plan_para->detectWin = cv_detectWin_para;
    plan_para->detectWinNum = sizeof(cv_detectWin_para) / sizeof(struct detect_wins_t);
    plan_para->dynamic_load = 0;
    plan_para->displayEn = 1;
    plan_para->threshold = DETECT_BASIC_FACE_SCORE;
    plan_para->timeout = TIMEOUT;

    printf("basic_face_plan_para.detectWinNum = %d\n", plan_para->detectWinNum);
    printf("basic_face_plan_para.displayEn = %d\n", plan_para->displayEn);

    // 第一次需要申请一块buffer
    if( plan_para->buf == NULL )
    {
        plan_para->buf = (char * )malloc( 1152 );
    }

    return (void *)&basic_face_plan_para;
}

// low power 唤醒配置
static int pm_wakeup_setup( int wakeup_flag )
{
    int r;
    int r_his = 0;

    pmu_remove_all_wakeup_source();

    printf("wakeup flag:%x\n",wakeup_flag);
    if (wakeup_flag & 0x2)
    {
	r = pmu_add_wakeup_source(WAKEUP_SRC_EPIR,0,PM_STATE_NORMAL,0);
	if (r<0)
	{
	    debug("pmu communicate fail 2:%d\n",r);
	    if (!r_his)
		r_his = r;
	}
    }
    debug("pmu_set_all_wakeup\n");
    r = pmu_set_all_wakeup();
    if (r)
    {
        debug("enable wakeup src fail\n");
        if (!r_his)
	    r_his = r;
    }
    debug("pm_check_wakeup_source\n");

    return r_his;
}

static int basic_low_power_mode( int mode )
{
    int ret = 0;
    int wakeup_flag = 0;
    int slave_addr = 0x18;
    int g_sensor_mode = 2;

    // 这里使用默认的 g_sensor_mode，使用g-sensor 中断唤醒。
    // 如果需要修改使用不同的唤醒方式，需要增改下面的代码。
    if( mode == g_sensor_mode )
    {
		debug("enable epir wakeup\n");
		wakeup_flag |= 0x2;

        //config gensor
        i2c_write_reg8(slave_addr,0xf,0x5);
        i2c_write_reg8(slave_addr,0x10,0x8);
        i2c_write_reg8(slave_addr,0x17,0x4);
        i2c_write_reg8(slave_addr,0x19,0x2);
        i2c_write_reg8(slave_addr,0x21,0x8e);
        i2c_write_reg8(slave_addr,0x26,0x40);

        printf("set wakeup source to pmu core\n");
        ret = pm_wakeup_setup( wakeup_flag );
        if (ret)
        {
            debug("set wakeup source fail\n");
        }
        printf("switch to PIR\n");
        ret = pm_switch_power_mode(PM_STATE_PIR);
        if ( ret < 0 )
        {
            debug("pm_switch_power_mode_to_pir:%d\n",ret);
        }
    }

    return ret;
}

static int basic_face_detect_check(int *model, char *feature, int size, int threshold)
{
    int score = 0;

    score = cv_detect_score(model, feature, size);

    if (score >= threshold)
    {
        return score;
    }
    return 0;
}

static int basic_face_detect( int id, short *nbnnBuf, int size )
{
    struct detect_basic_face_plan_t *plan_para = basic_get_plan_para();

    // 5组scaler, 在第一组时初始化
    if( id == 0 )
    {
	plan_para->basic_face_flag = 0;
    }

    // Short type data normalized to char type
    // Here is the function after the fixed point
    memset(plan_para->buf, 0, size * sizeof(char));
    cv_Normalize1D_fixed( plan_para->buf, nbnnBuf, size, 256 );

    int facdScore = basic_face_detect_check( VectorFaceTest, plan_para->buf, size, plan_para->threshold );
    if (facdScore > 0)
    {
        led_set_state(LED1, 1 ); // open LED
        led_set_state(LED2, 1 ); // open LED

#ifdef USE_HEAD

        unsigned long diff = 0;

        if (light_status == LIGHT_BOOT_STATUS) {
            ctime = perf_get_ticks()&0xFFFFFFFF;
            diff = ctime - stime;
            if ((diff/PERF_TIMER_MS_CNT) > 5*1000) {
                stime = perf_get_ticks()&0xFFFFFFFF;
                light_status = LIGHT_OFF_STATUS;
            }
        } else if (light_status == LIGHT_OFF_STATUS) {
            on_pwm();
            stime = perf_get_ticks()&0xFFFFFFFF;
            light_status = LIGHT_ON_STATUS;
        }
#endif /* USE_HEAD */

        plan_para->basic_face_flag++;

        printf("%d ", plan_para->basic_face_flag);
    }

    return 0;
}

static int basic_face_check_result( void )
{
    struct detect_basic_face_plan_t *plan_para = basic_get_plan_para();
    // 如果检测到人脸，或者第一次，重新计时。超时提示使用
    if( low_power_time_flag )
    {
        stime = perf_get_ticks()&0xFFFFFFFF;
        low_power_time_flag = 0;
    }

    if(plan_para->basic_face_flag != 0)
    {
        play_audio(PCM_FIND_YOU);
        low_power_time_flag = 1;
    }

    led_set_state(LED1, 0 ); // close LED
    led_set_state(LED2, 0 ); // close LED

#ifdef USE_HEAD

    unsigned long diff = 0;

    if (light_status == LIGHT_ON_STATUS) {
        //off_pwm();
        ctime = perf_get_ticks()&0xFFFFFFFF;
        diff  = ctime - stime;
        if ((diff/PERF_TIMER_MS_CNT) > 20*1000) {
            off_pwm();
            stime = perf_get_ticks()&0xFFFFFFFF;
            light_status = LIGHT_OFF_STATUS;
        }
    }
#endif /* USE_HEAD */


#if 0
    // 超过 20 s 没有检测到人脸，进入低功耗模式，使用g-sensor 唤醒
    if( low_power_time_flag == 0 )
    {
        etime = perf_get_ticks()&0xFFFFFFFF;
        ctime = etime - stime;
        if( (ctime/PERF_TIMER_MS_CNT) >= (unsigned long)(plan_para->timeout ) )
        {
            etime = 0;
            stime = 0;
            low_power_time_flag = 1;
            printf("basic_low_power_mode(EPIR_MODE) : \n");
            int g_sensor_mode = 2;
            basic_low_power_mode( g_sensor_mode );
        }
    }
#endif

    return 0;
}

/*implement custom detect win function*/
int detect_win(void)
{
    int ch = 0;
    short *nbnnBuf = NULL;
    int cSize = 64;
    int detect_width = 32;
    int detect_height = 32;
    int size = 0;
    int n_bins = 9;
    size = (detect_width * detect_height) * n_bins / cSize;
    int ret = 0;

    for (ch = 0; ch < 5; ch++)
    {
        ret = cv_detect_check_buf_ready( ch );
        if( !ret )
        {
            continue;
        }
        nbnnBuf = cv_detect_get_buf( ch, 0 );
        if( nbnnBuf )
        {
            basic_face_detect(ch, nbnnBuf, size);
        }
    }
    basic_face_check_result();

    return 0;
}


static void save_plan_wins(void)
{
    struct detect_basic_face_plan_t *plan_para = basic_get_plan_para();
    struct scaler_wins_t *plan_win = cv_detect_get_plan_win();

    // 获取这一帧的 scaler 坐标
    for (int id = 0; id < 5; id++)
    {
        plan_para->lastWin[id].sx = plan_win->win[id].sx;
        plan_para->lastWin[id].sy = plan_win->win[id].sy;
        plan_para->lastWin[id].w = plan_win->win[id].ex - plan_win->win[id].sx + 1;
        plan_para->lastWin[id].h = plan_win->win[id].ey - plan_win->win[id].sy + 1;
    }
}

static int old_cx = -1, old_cy = -1;
static int old_w  = 0,  old_h  = 0;
static int stop_count  = 0;
static int setup_light = 0;
static int light_count = 0;

/*implement custom plan win function*/
int plan_win( void )
{
    struct detect_basic_face_plan_t *plan_para = basic_get_plan_para();
    struct md_result_win_t *motion_win = cv_detect_get_md_win(); // 获取motion窗口信息
    uint16_t width;
    uint16_t height;
    int new_cx, new_cy;
    unsigned long diff = 0;

    width  = motion_win->width;
    height = motion_win->height;

    new_cx = motion_win->sx + width/2;
    new_cy = motion_win->sy + height/2;

#ifdef USE_MOTION
    if (light_status == LIGHT_BOOT_STATUS) {
        ctime = perf_get_ticks()&0xFFFFFFFF;
        diff = ctime - stime;
        if ((diff/PERF_TIMER_MS_CNT) > 3*1000) {
            stime = perf_get_ticks()&0xFFFFFFFF;
            light_status = LIGHT_IDLE_STATUS;
        }
    } else if (light_status == LIGHT_IDLE_STATUS) {
        if (width > 10 && height > 10) {
            if (new_cx > (old_cx + 1) || new_cx < (old_cx - 1)) {
                on_pwm();
                stime = perf_get_ticks()&0xFFFFFFFF;
                light_status = LIGHT_ON_STATUS;
            } else if (new_cy > (old_cy + 1) || new_cy < (old_cy - 1)) {
                on_pwm();
                stime = perf_get_ticks()&0xFFFFFFFF;
                light_status = LIGHT_ON_STATUS;
            }
        }
    } else if (light_status == LIGHT_ON_STATUS) {
        ctime = perf_get_ticks()&0xFFFFFFFF;
        diff = ctime - stime;
        if ((diff/PERF_TIMER_MS_CNT) > 20*1000) {
            set_pwm(20);
            stime = perf_get_ticks()&0xFFFFFFFF;
            light_status = LIGHT_OFF_STATUS;
        }
    } else if (light_status == LIGHT_OFF_STATUS) {
        etime = perf_get_ticks()&0xFFFFFFFF;
        diff = etime - stime;
        if ((diff/PERF_TIMER_MS_CNT) > 3*1000) {
            off_pwm();
            light_status = LIGHT_BOOT_STATUS;
        }
    }
#endif /* USE_MOTION */

#ifdef USE_HAND

    printf("Motion Window X %d, Y %d, W %d. H %d V %d C %d\n", new_cx, new_cy, width, height, pwm_duty_ration, stop_count);
    if (width > 10 && height > 10) {
        if (setup_light == 0) {
            if (new_cx > old_cx) {
                up_pwm();
                printf("UPUP..........%d\n", pwm_duty_ration);
            } else if (new_cx < old_cx) {
                down_pwm();
                printf("DOWN..........%d\n", pwm_duty_ration);
            }
            light_count = 0;

        } else {

            ctime = perf_get_ticks()&0xFFFFFFFF;
            diff = ctime - etime;

            light_count++;

            if ((diff/PERF_TIMER_MS_CNT) > 20*1000) {
                setup_light = 0;
                printf("Light is RELEASED!!! %d", ctime);
            }
        }

        if (new_cx == old_cx && new_cy == old_cy && setup_light == 0) {

            etime = perf_get_ticks()&0xFFFFFFFF;
            diff  = etime - stime;

            stop_count++;

            if ((diff/PERF_TIMER_MS_CNT) > 3*1000) {
                setup_light = 1;
                printf("Light is FIXED!!! %d", etime);
            }

        } else {

            stime = perf_get_ticks()&0xFFFFFFFF;

            stop_count  = 0;
        }
    }

#endif /* USE_HAND */

    old_cx = new_cx;
    old_cy = new_cy;
    old_w  = width;
    old_h  = height;

    int winNum = 0;

    winNum = plan_para->detectWinNum;
    //printf("detectWinNum = %d\n", plan_para->detectWinNum);

    plan_para->lastWinTableNum = plan_para->winShift - 5;
    if (plan_para->lastWinTableNum < 0)
    {
        plan_para->lastWinTableNum = 0;
    }

    plan_para->winShift += 5;
    if (plan_para->winShift >= winNum)
    {
        plan_para->winShift = 0;
    }

    plan_scaler_split_policy(0, plan_para->detectWin[plan_para->winShift + 0].sx, plan_para->detectWin[plan_para->winShift + 0].sy, plan_para->detectWin[plan_para->winShift + 0].w, plan_para->detectWin[plan_para->winShift + 0].h, 32, 32, 0, 0);
    plan_scaler_split_policy(1, plan_para->detectWin[plan_para->winShift + 1].sx, plan_para->detectWin[plan_para->winShift + 1].sy, plan_para->detectWin[plan_para->winShift + 1].w, plan_para->detectWin[plan_para->winShift + 1].h, 32, 32, 0, 0);
    plan_scaler_split_policy(2, plan_para->detectWin[plan_para->winShift + 2].sx, plan_para->detectWin[plan_para->winShift + 2].sy, plan_para->detectWin[plan_para->winShift + 2].w, plan_para->detectWin[plan_para->winShift + 2].h, 32, 32, 0, 0);
    plan_scaler_split_policy(3, plan_para->detectWin[plan_para->winShift + 3].sx, plan_para->detectWin[plan_para->winShift + 3].sy, plan_para->detectWin[plan_para->winShift + 3].w, plan_para->detectWin[plan_para->winShift + 3].h, 32, 32, 0, 0);
    plan_scaler_split_policy(4, plan_para->detectWin[plan_para->winShift + 4].sx, plan_para->detectWin[plan_para->winShift + 4].sy, plan_para->detectWin[plan_para->winShift + 4].w, plan_para->detectWin[plan_para->winShift + 4].h, 32, 32, 0, 0);

    // 在非准确定位模式下获取这一帧的 scaler 坐标
    // 获取这一帧的 scaler 坐标
    save_plan_wins();

    return 0;
}

int keypad_handler(int key_code)
{
    printf("key code:%d\n",key_code);
    if (key_code == GPIO_LONG_PRESS_KEY_CODE_PLUS+ GPIO_KEY0)
    {
        printf("switch to next firmware\n");
        switch_next_firmware();
    }
    return 0;
}

int main( void )
{
    int ret;

    light_status = LIGHT_BOOT_STATUS;
    stime = perf_get_ticks()&0xFFFFFFFF;

    printf("detect basic mode demo\n");

    /*board bsp init: pad,sensor,led*/
    ret = bsp_init();
    if (ret)
        printf("bsp init fail\n");

    /*init detect framework: basic mode*/
    ret = basic_mode_init(6);
    if (ret)
        printf("detect framework init fail\n");

    for( int i = 0; i < 5; i++ )
    {
        led_set_state(LED1, 1 );
        led_set_state(LED2, 1 );
        mdelay(50);
        led_set_state(LED1, 0 );
        led_set_state(LED2, 0 );
        mdelay(50);
    }

    /*init audio playback*/
    ret = audio_play_init(DEV_PWM,IOC6);
    if (ret)
        printf("audio playback init fail\n");

    pwm_start();

#ifdef USE_HEAD
    set_pwm(30);
#endif /* USE_HEAD */

    /* optional: add cv_detect_set_feature() to change features*/
    /*eg: change CV_DETECT_FEATURE_PLAN_PER_FRAME so do plan every frame*/
    //cv_detect_set_feature(CV_DETECT_FEATURE_PLAN_PER_FRAME,1);

    /*start framework*/
    ret = cv_detect_start();
    if (ret)
        printf("detect framework start fail\n");

    // val = 1, check each frame
    cv_detect_set_feature( CV_DETECT_FEATURE_PLAN_PER_FRAME, 1 );

    play_audio(PCM_BOOT_SOUND);

    printf("main end.\n");

    return 0;
}

