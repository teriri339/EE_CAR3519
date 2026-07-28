#include "LineFollow/line_follow.h"
#include "MotorCtrl/motor_ctrl.h"

/*
 * 简单 PID 黑线循迹
 *
 * Background: white gives a high ADC value; black gives a low ADC value.
 *
 * Algorithm:
 *   1. Convert every channel to continuous darkness: frame_max - sample
 *   2. Sensor weights from left to right: -7, -5, -3, -1, +1, +3, +5, +7
 *   3. Error is the darkness-weighted average sensor position
 *   4. Positional PID with an explicit 10 ms sample period
 */

#define INTEGRAL_LIMIT  100     /* 积分限幅 */

/* PID 运行时参数（可通过按键实时调节） */
static int g_kp          = LF_DEFAULT_KP;
static int g_ki          = LF_DEFAULT_KI;
static int g_kd          = LF_DEFAULT_KD;
static int g_base_speed  = LF_DEFAULT_SPD;

static const int8_t sensor_weights[LF_SENSOR_COUNT] = {
    -7, -5, -3, -1, 1, 3, 5, 7
};

static float integral;
static float last_error;
static float tracking_error;
static uint16_t tracking_contrast;
static uint8_t initialized;
static uint8_t has_last_error;

void line_follow_init(void)
{
    integral    = 0.0f;
    last_error  = 0.0f;
    tracking_error = 0.0f;
    tracking_contrast = 0;
    initialized = 1;
    has_last_error = 0;
}

bool line_follow_update(uint16_t *gray_values,
                        int16_t *left_speed,
                        int16_t *right_speed)
{
    uint16_t min_value = 0xFFFFU;
    uint16_t max_value = 0U;
    float weighted_sum = 0.0f;
    uint32_t darkness_sum = 0U;
    int i;

    if (!initialized)
        line_follow_init();

    for (i = 0; i < LF_SENSOR_COUNT; i++)
    {
        if (gray_values[i] < min_value) min_value = gray_values[i];
        if (gray_values[i] > max_value) max_value = gray_values[i];
    }

    tracking_contrast = max_value - min_value;
    if (tracking_contrast < LF_MIN_CONTRAST)
    {
        integral = 0.0f;
        last_error = 0.0f;
        tracking_error = 0.0f;
        has_last_error = 0;
        *left_speed = 0;
        *right_speed = 0;
        return false;
    }

    /* Sensor 0 is far left. Darker channels contribute more strongly. */
    for (i = 0; i < LF_SENSOR_COUNT; i++)
    {
        uint16_t darkness = max_value - gray_values[i];
        weighted_sum += (float)darkness * (float)sensor_weights[i];
        darkness_sum += darkness;
    }

    if (darkness_sum == 0U)
    {
        integral = 0.0f;
        last_error = 0.0f;
        tracking_error = 0.0f;
        has_last_error = 0;
        *left_speed = 0;
        *right_speed = 0;
        return false;
    }

    float error = weighted_sum / (float)darkness_sum;
    tracking_error = error;

    /* 2. Positional PID. */
    integral += error * LF_CONTROL_PERIOD_S;
    if (integral >  INTEGRAL_LIMIT) integral =  INTEGRAL_LIMIT;
    if (integral < -INTEGRAL_LIMIT) integral = -INTEGRAL_LIMIT;

    float derivative = 0.0f;
    if (has_last_error)
    {
        derivative = (error - last_error) / LF_CONTROL_PERIOD_S;
    }
    else
    {
        has_last_error = 1;
    }
    last_error = error;

    float pid_output = g_kp * error + g_ki * integral + g_kd * derivative;

    /* 限幅: pid_output 最大 ±base_speed */
    if (pid_output >  g_base_speed) pid_output =  g_base_speed;
    if (pid_output < -g_base_speed) pid_output = -g_base_speed;

    /* 3. 差速控制
     *    error < 0 (line on left)  -> left faster, right slower -> turn right
     *    error > 0 (line on right) -> left slower, right faster -> turn left
     */
    *left_speed  = (int16_t)(g_base_speed - pid_output);
    *right_speed = (int16_t)(g_base_speed + pid_output);

    /* 限幅到有效范围 -1000~1000 */
    if (*left_speed  >  1000) *left_speed  =  1000;
    if (*left_speed  < -1000) *left_speed  = -1000;
    if (*right_speed >  1000) *right_speed =  1000;
    if (*right_speed < -1000) *right_speed = -1000;

    return true;
}

/* ========== PID 参数实时读写接口 ========== */
int line_follow_get_kp(void)         { return g_kp; }
int line_follow_get_ki(void)         { return g_ki; }
int line_follow_get_kd(void)         { return g_kd; }
int line_follow_get_base_speed(void) { return g_base_speed; }
float line_follow_get_error(void)    { return tracking_error; }
uint16_t line_follow_get_contrast(void) { return tracking_contrast; }

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
