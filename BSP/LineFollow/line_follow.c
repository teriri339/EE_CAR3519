#include "LineFollow/line_follow.h"
#include "MotorCtrl/motor_ctrl.h"

/*
 * 简单 PID 黑线循迹
 *
 * 背景: 白色 → ADC 值大 (~3000~4095)，占空比高
 * 黑线: 黑色 → ADC 值小 (~0~1500)，占空比低
 *
 * 算法:
 *   1. 二值化: ADC < BLACK_TH → 黑线(1); 否则 → 白色(0)
 *   2. 加权平均求黑线位置: pos = Σ(bit_i × i) / Σ(bit_i)
 *   3. 误差: error = pos - 3.5 (中心)
 *   4. PID 输出修正电机差速
 */

/* ========== 可调参数 ========== */
#define BLACK_TH        2000    /* ADC 低于此值视为黑线 */
#define INTEGRAL_LIMIT  100     /* 积分限幅 */
/* ============================= */

/* PID 运行时参数（可通过按键实时调节） */
static int g_kp          = LF_DEFAULT_KP;
static int g_ki          = LF_DEFAULT_KI;
static int g_kd          = LF_DEFAULT_KD;
static int g_base_speed  = LF_DEFAULT_SPD;

static float integral;
static float last_error;
static uint8_t initialized;

void line_follow_init(void)
{
    integral    = 0.0f;
    last_error  = 0.0f;
    initialized = 1;
}

void line_follow_update(uint16_t *gray_values,
                         int16_t *left_speed,
                         int16_t *right_speed)
{
    float weighted_sum = 0.0f;
    float total_weight = 0.0f;
    int i;

    if (!initialized)
        line_follow_init();

    /* 1. 二值化 + 加权位置计算 (sensor 1=index0=最左) */
    for (i = 0; i < LF_SENSOR_COUNT; i++)
    {
        uint8_t black = (gray_values[i] < BLACK_TH) ? 1 : 0;
        weighted_sum += (float)(black * i);
        total_weight += (float)black;
    }

    float position;
    if (total_weight < 0.1f)
    {
        /* 全白（没压到黑线）→ 默认直行 */
        position = LF_CENTER_POS;
    }
    else if (total_weight > 7.9f)
    {
        /* 全黑（所有传感器都在黑线上）→ 直行 */
        position = LF_CENTER_POS;
    }
    else
    {
        position = weighted_sum / total_weight;
    }

    /* 2. PID 计算 */
    float error = position - LF_CENTER_POS;     /* 左负右正 */

    integral += error;
    if (integral >  INTEGRAL_LIMIT) integral =  INTEGRAL_LIMIT;
    if (integral < -INTEGRAL_LIMIT) integral = -INTEGRAL_LIMIT;

    float derivative = error - last_error;
    last_error = error;

    float pid_output = g_kp * error + g_ki * integral + g_kd * derivative;

    /* 限幅: pid_output 最大 ±base_speed */
    if (pid_output >  g_base_speed) pid_output =  g_base_speed;
    if (pid_output < -g_base_speed) pid_output = -g_base_speed;

    /* 3. 差速控制
     *    error > 0 (偏右) → pid_output > 0 → 左轮快、右轮慢 → 左转校正
     *    error < 0 (偏左) → pid_output < 0 → 左轮慢、右轮快 → 右转校正
     */
    *left_speed  = (int16_t)(g_base_speed + pid_output);
    *right_speed = (int16_t)(g_base_speed - pid_output);

    /* 限幅到有效范围 -1000~1000 */
    if (*left_speed  >  1000) *left_speed  =  1000;
    if (*left_speed  < -1000) *left_speed  = -1000;
    if (*right_speed >  1000) *right_speed =  1000;
    if (*right_speed < -1000) *right_speed = -1000;
}

/* ========== PID 参数实时读写接口 ========== */
int line_follow_get_kp(void)         { return g_kp; }
int line_follow_get_ki(void)         { return g_ki; }
int line_follow_get_kd(void)         { return g_kd; }
int line_follow_get_base_speed(void) { return g_base_speed; }

void line_follow_set_kp(int val)         { g_kp = val; }
void line_follow_set_ki(int val)         { g_ki = val; }
void line_follow_set_kd(int val)         { g_kd = val; }
void line_follow_set_base_speed(int val) { g_base_speed = val; }

void line_follow_reset_pid(void)
{
    g_kp = LF_DEFAULT_KP;
    g_ki = LF_DEFAULT_KI;
    g_kd = LF_DEFAULT_KD;
    g_base_speed = LF_DEFAULT_SPD;
    line_follow_init();   /* 积分器归零 */
}
