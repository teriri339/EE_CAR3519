#ifndef __COURSE_H__
#define __COURSE_H__

#include <stdbool.h>
#include <stdint.h>

void course_signal_init(void);
void course_show_menu(bool imu_ok);
bool course_run(uint8_t task, bool imu_ok);

#endif
