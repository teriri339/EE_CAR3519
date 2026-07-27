#ifndef __MOTOR_CTRL_H__
#define __MOTOR_CTRL_H__
#include "bsp.h"

/* 电机编号 */
typedef enum {
    MOTOR1 = 0,
    MOTOR2 = 1
} MotorId_t;

/* 控制状态 */
typedef enum {
    MOTOR_STOP    = 0,
    MOTOR_FORWARD = 1,
    MOTOR_REVERSE = 2
} MotorState_t;

void motor_ctrl_init(void);
void motor_set_speed(MotorId_t motor, int speed);  /* -1000~1000 */
void motor_set_state(MotorId_t motor, MotorState_t state);
MotorState_t motor_get_state(MotorId_t motor);

#endif
