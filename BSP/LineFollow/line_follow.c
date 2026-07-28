#include "LineFollow/line_follow.h"
#include "MotorCtrl/motor_ctrl.h"

/*
 * 黑线循迹 PID 控制
 *
 * 灰度传感器: 白底 → ADC 高 (~3000-4095), 黑线 → ADC 低 (~0-1500)
 *
 * 传感器排列（按 UART 响应顺序 gray[0]..gray[7]）:
 *   实际方向取决于硬件安装，本代码假设 gray[0]=右侧, gray[7]=左侧。
 *   如需反转: 修改 SENSOR_DIR_INVERT 宏即可。
 *
 * 位置计算: 加权平均法
 *   pos = Σ(black_i × i) / Σ(black_i)    [范围 0~7, 中心=3.5]
 *
 * 误差 = 中心 - 位置  (换句话说 Kp 应该为正)
 *   error > 0 → 线偏右 → 右转修正 (左轮加速/右轮减速)
 *   error < 0 → 线偏左 → 左转修正 (右轮加速/左轮减速)
 *
 * 全部使用整数运算（M0+ 无 FPU）。
 */

/* ============ 传感器方向 ============ */
/* 设为 1 反转传感器索引 (gray[0]↔gray[7], gray[1]↔gray[6]..) */
#define SENSOR_DIR_INVERT  0
/* =================================== */

/* ========== 可调参数 ========== */
#define BLACK_TH        1200    /* ADC 低于此值视为黑线 (白~1800-3000, 黑~200-400) */
#define SCALE            10     /* 定点小数缩放因子 */
#define INTEGRAL_LIMIT  (int32_t)(100 * SCALE)
#define GYRO_D_SCALE     64     /* 陀螺 raw → D项 缩放: gyro_z/SCALE(64) ≈ °/s/2 */
/* ============================= */

/* PID 运行时参数 */
static int g_kp          = LF_DEFAULT_KP;
static int g_ki          = LF_DEFAULT_KI;
static int g_kd          = LF_DEFAULT_KD;
static int g_base_speed  = LF_DEFAULT_SPD;

static int32_t integral;
static int16_t last_error;     /* SCALE 倍 */
static uint8_t initialized;

/* 传感器位置权重数组: [0, 10, 20, 30, 40, 50, 60, 70] (SCALE 倍) */
static const int16_t sensor_weight[LF_SENSOR_COUNT] = {
    0, 10, 20, 30, 40, 50, 60, 70
};

void line_follow_init(void)
{
    integral    = 0;
    last_error  = 0;
    initialized = 1;
}

void line_follow_update(uint16_t *gray_values,
                         int16_t gyro_z,
                         int16_t *left_speed,
                         int16_t *right_speed)
{
    int32_t weighted_sum = 0;
    int16_t total_cnt    = 0;
    int i;

    if (!initialized)
        line_follow_init();

    /* 1. 二值化 + 加权位置计算 */
    for (i = 0; i < LF_SENSOR_COUNT; i++)
    {
        uint8_t black = (gray_values[i] < BLACK_TH) ? 1 : 0;
        if (black)
        {
#if SENSOR_DIR_INVERT
            weighted_sum += sensor_weight[LF_SENSOR_COUNT - 1 - i];
#else
            weighted_sum += sensor_weight[i];
#endif
            total_cnt++;
        }
    }

    /* 2. 计算位置 (SCALE 倍, 范围 0 ~ 7*SCALE)  */
    int16_t position;
    if (total_cnt == 0)
    {
        /* 丢线: 保持上次方向，这里暂用直行 (position=center) */
        position = 35;   /* 3.5 * SCALE */
    }
    else if (total_cnt >= LF_SENSOR_COUNT)
    {
        /* 全部传感器都看到黑线 (交叉路口等) → 直行 */
        position = 35;
    }
    else
    {
        position = (int16_t)(weighted_sum / total_cnt);
    }

    /* 3. 计算误差
     *    error = center - position
     *    error > 0: 线偏右 → 需要右转
     *    error < 0: 线偏左 → 需要左转
     */
    int16_t error = 35 - position;    /* 3.5*SCALE - position */

    /* 4. PID 计算 (整数)
     *    D 项: 优先使用陀螺仪角速度 (gyro_z), 无陀螺时(gyro_z==0)用位置微分
     */
    integral += error;
    if (integral >  INTEGRAL_LIMIT) integral =  INTEGRAL_LIMIT;
    if (integral < -INTEGRAL_LIMIT) integral = -INTEGRAL_LIMIT;

    int16_t derivative;
    if (gyro_z != 0)
    {
        /* 陀螺 D: gyro_z / GYRO_D_SCALE  →  与位置微分量级对齐 */
        derivative = gyro_z / GYRO_D_SCALE;
    }
    else
    {
        /* 位置微分 (fallback) */
        derivative = error - last_error;
    }
    last_error = error;

    /* pid_output = (kp*error + ki*integral + kd*derivative) / SCALE */
    int32_t pid = (int32_t)g_kp * error
                + (int32_t)g_ki * integral
                + (int32_t)g_kd * derivative;
    int32_t pid_output = pid / SCALE;

    /* 限幅到 ±base_speed (确保不反转) */
    if (pid_output >  g_base_speed) pid_output =  g_base_speed;
    if (pid_output < -g_base_speed) pid_output = -g_base_speed;

    /* 5. 差速输出
     *    pid_output > 0 (需右转): 左轮加速, 右轮减速
     *    pid_output < 0 (需左转): 左轮减速, 右轮加速
     */
    int32_t l_spd = g_base_speed + pid_output;
    int32_t r_spd = g_base_speed - pid_output;

    /* 限幅 -1000 ~ 1000 */
    if (l_spd >  1000) l_spd =  1000;
    if (l_spd < -1000) l_spd = -1000;
    if (r_spd >  1000) r_spd =  1000;
    if (r_spd < -1000) r_spd = -1000;

    *left_speed  = (int16_t)l_spd;
    *right_speed = (int16_t)r_spd;
}

/* ========== PID 参数接口 ========== */
int  line_follow_get_kp(void)         { return g_kp; }
int  line_follow_get_ki(void)         { return g_ki; }
int  line_follow_get_kd(void)         { return g_kd; }
int  line_follow_get_base_speed(void) { return g_base_speed; }
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
    line_follow_init();
}
