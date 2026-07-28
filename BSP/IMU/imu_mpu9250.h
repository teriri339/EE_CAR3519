#ifndef __IMU_MPU9250_H__
#define __IMU_MPU9250_H__

#include "bsp.h"

/* 初始化 MPU9250 (软件 I2C, PA0=SDA, PA1=SCL)
 * 返回 0=成功, -1=总线无设备, -2=WHO_AM_I读失败, -3=PWR_MGMT写失败 */
int imu_init(void);

/* 读取陀螺仪 Z 轴原始值 (带符号 16bit)
 * 量程 ±250dps, 灵敏度 131 LSB/(°/s)
 * 返回 raw 值, 0 表示读取失败 */
int16_t imu_get_gyro_z(void);

/* 诊断: 读取 WHO_AM_I 寄存器, 返回 0x00 表示失败 */
uint8_t imu_get_whoami(void);

/* 诊断: 扫描 I2C 总线, 返回第一个响应的设备地址, 0 表示无设备 */
uint8_t imu_scan_i2c(void);

/* 诊断: 返回当前使用的 MPU9250 I2C 地址 (0 表示未找到) */
uint8_t imu_get_addr(void);

#endif
