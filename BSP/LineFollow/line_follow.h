#ifndef __LINE_FOLLOW_H__
#define __LINE_FOLLOW_H__

#include "bsp.h"

#define LF_SENSOR_COUNT  8
#define LF_CENTER_POS    3.5f    /* 理想位置（传感器5和6之间，0-index） */

/* PID 参数默认值 */
#define LF_DEFAULT_KP    120
#define LF_DEFAULT_KI    0
#define LF_DEFAULT_KD    0
#define LF_DEFAULT_SPD   500

void line_follow_init(void);

/*
 * PID 控制周期: 输入8路灰度值 + 陀螺Z轴角速度，输出左右电机速度
 *   gray_values: 8路灰度 ADC 值
 *   gyro_z:      陀螺 Z 轴原始值 (131 LSB/°/s), 无陀螺传 0
 *   left_speed, right_speed: 输出电机速度 (-1000~1000)
 */
void line_follow_update(uint16_t *gray_values,
                         int16_t gyro_z,
                         int16_t *left_speed,
                         int16_t *right_speed);

/* PID 参数实时读写 */
int  line_follow_get_kp(void);
int  line_follow_get_ki(void);
int  line_follow_get_kd(void);
int  line_follow_get_base_speed(void);
void line_follow_set_kp(int val);
void line_follow_set_ki(int val);
void line_follow_set_kd(int val);
void line_follow_set_base_speed(int val);
void line_follow_reset_pid(void);

#endif
