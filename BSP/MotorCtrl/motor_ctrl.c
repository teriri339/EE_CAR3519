#include "MotorCtrl/motor_ctrl.h"

/*
 * DRV8870 IN/IN 模式驱动说明：
 *
 * 接线（按驱动板丝印 - 2026-07-27 修正）:
 *   PWM1(PC2) → M1-IN1    PWM2(PC3) → M1-IN2
 *   PWM3(PC4) → M2-IN1    PWM4(PC5) → M2-IN2
 *
 * DRV8870 控制逻辑:
 *   IN1=1, IN2=0 → 正转 (Forward)
 *   IN1=0, IN2=1 → 反转 (Reverse)
 *   IN1=0, IN2=0 → 停止 (Coast)
 *   IN1=1, IN2=1 → 刹车 (Brake)
 *
 * 正转: IN1=PWM(调速), IN2=0      → 可调速正转
 * 反转: IN1=0, IN2=1(满速,无PWM)  → 全速反转
 * 停止: IN1=0, IN2=0
 *
 * TIMA0 PWM 模式: Down-Counting Edge-Aligned
 *   计数器: LOAD(999) → ... → CC → ... → 1 → 0 → 重载
 *   输出:   CNT==0 时拉 HIGH, CNT==CC 时拉 LOW
 *   占空比 = (period - CC) / period = (1000
 0 - CC) / 1000
 *
 *   CC=0   → 100% duty (全速)     CC=999 → ~0% duty (停止)
 *   CC=500 → 50% duty             CC=1000→ 永不匹配 → 100% (危险!)
 *
 *   前进: CC = period - speed   (speed=500 → CC=500 → 50%)
 *   停止/反转: CC = 999 → IN1≈0 (仅有 1/1000 脉冲, 可忽略)
 */
#define MOTOR_PWM_PERIOD  1000   /* TIMA0 PWM 周期 */

/* M1_IN2 = PC3(PH2), M2_IN2 = PC5(PH1) */
#define M1_IN2(val) ((val) ? DL_GPIO_setPins(MotorCtrl_PORT, MotorCtrl_PH2_PIN) : DL_GPIO_clearPins(MotorCtrl_PORT, MotorCtrl_PH2_PIN))
#define M2_IN2(val) ((val) ? DL_GPIO_setPins(MotorCtrl_PORT, MotorCtrl_PH1_PIN) : DL_GPIO_clearPins(MotorCtrl_PORT, MotorCtrl_PH1_PIN))

/* 电机速度 (-1000~1000) */
static int motor_speed[2] = {0, 0};

void motor_ctrl_init(void)
{
    /* 初始停止: CC=999 → ~0% duty (down-counting) */
    DL_TimerA_setCaptureCompareValue(TimerA0_PWM_INST, MOTOR_PWM_PERIOD - 1, DL_TIMER_CC_0_INDEX);
    DL_TimerA_setCaptureCompareValue(TimerA0_PWM_INST, MOTOR_PWM_PERIOD - 1, DL_TIMER_CC_1_INDEX);
    M1_IN2(0);
    M2_IN2(0);
}

/*
 * 设置指定电机的方向和速度 (-1000~1000)
 *   >0: 正转, IN1=PWM, IN2=0 (可调速)
 *   <0: 反转, IN1=0, IN2=1 (全速, 无PWM调速)
 *   0:  停止
 */
void motor_set_speed(MotorId_t motor, int speed)
{
    if (speed > 1000) speed = 1000;
    if (speed < -1000) speed = -1000;

    motor_speed[motor] = speed;

    if (motor == MOTOR1)
    {
        /* M1: IN1=PC2(PWM/CC0), IN2=PC3(GPIO) */
        if (speed > 0)
        {
            /* 正转: IN1=PWM, IN2=0
             * down-counting: duty = (1000-CC)/1000, 所以 CC = 1000 - speed */
            uint32_t cc = (speed >= MOTOR_PWM_PERIOD) ? 0 : (MOTOR_PWM_PERIOD - speed);
            DL_TimerA_setCaptureCompareValue(TimerA0_PWM_INST, cc, DL_TIMER_CC_0_INDEX);
            M1_IN2(0);
        }
        else if (speed < 0)
        {
            /* 反转: IN1≈0 (CC=999 → ~0% duty), IN2=1 */
            DL_TimerA_setCaptureCompareValue(TimerA0_PWM_INST, MOTOR_PWM_PERIOD - 1, DL_TIMER_CC_0_INDEX);
            M1_IN2(1);
        }
        else
        {
            /* 停止: IN1≈0 (CC=999 → ~0% duty), IN2=0 */
            DL_TimerA_setCaptureCompareValue(TimerA0_PWM_INST, MOTOR_PWM_PERIOD - 1, DL_TIMER_CC_0_INDEX);
            M1_IN2(0);
        }
    }
    else
    {
        /* M2: IN1=PC4(PWM/CC1), IN2=PC5(GPIO) */
        if (speed > 0)
        {
            /* 正转: IN1=PWM, IN2=0 */
            uint32_t cc = (speed >= MOTOR_PWM_PERIOD) ? 0 : (MOTOR_PWM_PERIOD - speed);
            DL_TimerA_setCaptureCompareValue(TimerA0_PWM_INST, cc, DL_TIMER_CC_1_INDEX);
            M2_IN2(0);
        }
        else if (speed < 0)
        {
            /* 反转: IN1≈0 (CC=999 → ~0% duty), IN2=1 */
            DL_TimerA_setCaptureCompareValue(TimerA0_PWM_INST, MOTOR_PWM_PERIOD - 1, DL_TIMER_CC_1_INDEX);
            M2_IN2(1);
        }
        else
        {
            /* 停止: IN1≈0 (CC=999 → ~0% duty), IN2=0 */
            DL_TimerA_setCaptureCompareValue(TimerA0_PWM_INST, MOTOR_PWM_PERIOD - 1, DL_TIMER_CC_1_INDEX);
            M2_IN2(0);
        }
    }
}

/* 按键控制接口 */
void motor_set_state(MotorId_t motor, MotorState_t state)
{
    switch (state)
    {
    case MOTOR_STOP:
        motor_set_speed(motor, 0);
        break;
    case MOTOR_FORWARD:
        motor_set_speed(motor, 800);   /* 80% 占空比正转 */
        break;
    case MOTOR_REVERSE:
        motor_set_speed(motor, -1000); /* 全速反转 (无PWM调速) */
        break;
    }
}

MotorState_t motor_get_state(MotorId_t motor)
{
    if (motor_speed[motor] == 0)
        return MOTOR_STOP;
    else if (motor_speed[motor] > 0)
        return MOTOR_FORWARD;
    else
        return MOTOR_REVERSE;
}
