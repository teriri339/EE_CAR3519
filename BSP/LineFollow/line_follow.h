#ifndef __LINE_FOLLOW_H__
#define __LINE_FOLLOW_H__

#include "bsp.h"

#define LF_SENSOR_COUNT  8
#define LF_CONTROL_PERIOD_S  0.010f  /* Nominal PID update period: 10 ms */
#define LF_MIN_CONTRAST  120U    /* Minimum max-min grayscale difference */

/* PID 参数默认值 */
#define LF_DEFAULT_KP    60
#define LF_DEFAULT_KI    0
#define LF_DEFAULT_KD    0
#define LF_DEFAULT_SPD   500

void line_follow_init(void);

/*
 * Positional PID: output = Kp*error + Ki*integral(error*dt)
 *                          + Kd*d(error)/dt
 * Input: 8-channel grayscale values; output: motor speeds (-1000~1000).
 */
bool line_follow_update(uint16_t *gray_values,
                        int16_t *left_speed,
                        int16_t *right_speed);

/* PID 参数实时读写 */
int  line_follow_get_kp(void);
int  line_follow_get_ki(void);
int  line_follow_get_kd(void);
int  line_follow_get_base_speed(void);
float line_follow_get_error(void);
uint16_t line_follow_get_contrast(void);
void line_follow_set_kp(int val);
void line_follow_set_ki(int val);
void line_follow_set_kd(int val);
void line_follow_set_base_speed(int val);
void line_follow_reset_pid(void);

#endif
