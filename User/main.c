#include "ti_msp_dl_config.h"
#include "bsp.h"

/* 前向声明 */
static void show_main_menu(int speed);

/* 电机运行方向 */
typedef enum {
    DIR_STOP = 0,
    DIR_FORWARD,
    DIR_REVERSE,
    DIR_LEFT,
    DIR_RIGHT
} Direction_t;

/* 按方向和速度驱动电机 */
static void apply_direction(Direction_t dir, int speed)
{
    switch (dir)
    {
    case DIR_FORWARD:
        motor_set_speed(MOTOR1, speed);
        motor_set_speed(MOTOR2, speed);
        break;
    case DIR_REVERSE:
        motor_set_speed(MOTOR1, -speed);
        motor_set_speed(MOTOR2, -speed);
        break;
    case DIR_LEFT:
        motor_set_speed(MOTOR1, -speed);
        motor_set_speed(MOTOR2, speed);
        break;
    case DIR_RIGHT:
        motor_set_speed(MOTOR1, speed);
        motor_set_speed(MOTOR2, -speed);
        break;
    case DIR_STOP:
    default:
        motor_set_speed(MOTOR1, 0);
        motor_set_speed(MOTOR2, 0);
        break;
    }
}

int main(void)
{
    SYSCFG_DL_init();

    char key;
    char last_key = 0;
    static int current_speed = 500;
    static Direction_t current_dir = DIR_STOP;
    static bool line_follow_active = false;
    static bool show_pid_params = false;
    static bool show_gyro_debug = false;
    static int imu_status = -1;
    static uint16_t follow_cnt = 0;

    OLED_Init();
    OLED_Clear();

    keyboard_init();
    motor_ctrl_init();
    gray_sensor_init();
    imu_status = imu_init();   /* MPU9250 陀螺仪 */

    /* 显示主菜单 */
    show_main_menu(current_speed);

    while (1)
    {
        key = get_keyboard_value();

        if (key != 0 && key != last_key)
        {
            last_key = key;

            /* 循迹模式下仅 B、*、0 退出，数字键/A 为 PID 调参 */
            if (line_follow_active && key != 'B' && key != '*')
            {
                bool is_pid_key = (key >= '1' && key <= '9') || key == 'A';
                if (!is_pid_key)
                {
                    line_follow_active = false;
                    show_pid_params = false;
                    current_dir = DIR_STOP;
                    apply_direction(DIR_STOP, 0);
                }
            }

            switch (key)
            {
            case '1':   /* 减速 / 循迹 KI-- */
                if (line_follow_active)
                {
                    int v = line_follow_get_ki();
                    line_follow_set_ki(v - 1);
                    show_pid_params = true;
                }
                else
                {
                    current_speed -= 100;
                    if (current_speed < 0) current_speed = 0;
                    apply_direction(current_dir, current_speed);
                    {
                        char buf[17];
                        snprintf(buf, sizeof(buf), "Speed: %-4d     ", current_speed);
                        OLED_ShowString(0, 0, (u8*)buf);
                    }
                    OLED_ShowString(0, 6, (u8*)"Speed-100       ");
                }
                break;

            case '3':   /* 加速 / 循迹 KI++ */
                if (line_follow_active)
                {
                    int v = line_follow_get_ki();
                    line_follow_set_ki(v + 1);
                    show_pid_params = true;
                }
                else
                {
                    current_speed += 100;
                    if (current_speed > 1000) current_speed = 1000;
                    apply_direction(current_dir, current_speed);
                    {
                        char buf[17];
                        snprintf(buf, sizeof(buf), "Speed: %-4d     ", current_speed);
                        OLED_ShowString(0, 0, (u8*)buf);
                    }
                    OLED_ShowString(0, 6, (u8*)"Speed+100       ");
                }
                break;

            case '#':   /* 速度重置 500 */
                current_speed = 500;
                apply_direction(current_dir, current_speed);
                {
                    char buf[17];
                    snprintf(buf, sizeof(buf), "Speed: %-4d     ", current_speed);
                    OLED_ShowString(0, 0, (u8*)buf);
                }
                OLED_ShowString(0, 6, (u8*)"Speed Reset 500 ");
                break;

            case '2':   /* 前进 / 循迹 KD-- */
                if (line_follow_active)
                {
                    int v = line_follow_get_kd();
                    line_follow_set_kd(v - 1);
                    show_pid_params = true;
                }
                else
                {
                    current_dir = DIR_FORWARD;
                    apply_direction(DIR_FORWARD, current_speed);
                    OLED_ShowString(0, 6, (u8*)"Forward         ");
                }
                break;

            case '8':   /* 后退 / 循迹 KD++ */
                if (line_follow_active)
                {
                    int v = line_follow_get_kd();
                    line_follow_set_kd(v + 1);
                    show_pid_params = true;
                }
                else
                {
                    current_dir = DIR_REVERSE;
                    apply_direction(DIR_REVERSE, current_speed);
                    OLED_ShowString(0, 6, (u8*)"Reverse         ");
                }
                break;

            case '4':   /* 左旋回 / 循迹 KP-- */
                if (line_follow_active)
                {
                    int v = line_follow_get_kp();
                    line_follow_set_kp(v - 10);
                    show_pid_params = true;
                }
                else
                {
                    current_dir = DIR_LEFT;
                    apply_direction(DIR_LEFT, current_speed);
                    OLED_ShowString(0, 6, (u8*)"Spin Left       ");
                }
                break;

            case '6':   /* 右旋回 / 循迹 KP++ */
                if (line_follow_active)
                {
                    int v = line_follow_get_kp();
                    line_follow_set_kp(v + 10);
                    show_pid_params = true;
                }
                else
                {
                    current_dir = DIR_RIGHT;
                    apply_direction(DIR_RIGHT, current_speed);
                    OLED_ShowString(0, 6, (u8*)"Spin Right      ");
                }
                break;

            case '5':   /* 停止 / 循迹停止 */
                if (line_follow_active)
                {
                    current_dir = DIR_STOP;
                    apply_direction(DIR_STOP, 0);
                    show_pid_params = true;
                }
                else
                {
                    current_dir = DIR_STOP;
                    apply_direction(DIR_STOP, 0);
                    OLED_ShowString(0, 6, (u8*)"Stop            ");
                }
                break;

            case '0':   /* 紧急停止 */
                current_dir = DIR_STOP;
                apply_direction(DIR_STOP, 0);
                OLED_ShowString(0, 6, (u8*)"Emergency Stop! ");
                break;

            case '7':   /* 循迹 SPD-- */
            case '9':   /* 循迹 SPD++ */
                if (line_follow_active)
                {
                    int v = line_follow_get_base_speed();
                    if (key == '7') v -= 50; else v += 50;
                    if (v < 0) v = 0;
                    if (v > 1000) v = 1000;
                    line_follow_set_base_speed(v);
                    show_pid_params = true;
                }
                /* 非循迹模式无操作 */
                break;

            case '*':   /* 返回主菜单 */
                line_follow_active = false;
                show_pid_params = false;
                show_gyro_debug = false;
                current_dir = DIR_STOP;
                apply_direction(DIR_STOP, 0);
                show_main_menu(current_speed);
                break;

            case 'A':   /* 灰度 → 陀螺 → 循环 / 循迹切换 PID 显示 */
                if (line_follow_active)
                {
                    show_pid_params = !show_pid_params;
                }
                else
                {
                    show_gyro_debug = !show_gyro_debug;
                    if (show_gyro_debug)
                    {
                        /* 陀螺仪诊断 */
                        OLED_ShowString(0, 0, (u8*)"-- Gyro Diag ---");
                        OLED_ShowString(0, 7, (u8*)"A:Gray  *:Back  ");
                    }
                    else
                    {
                        /* 灰度传感器 */
                        uint16_t gray_val[8];
                        char buf[17];
                        int ret = gray_sensor_read_all(gray_val);

                        if (ret >= 0)
                        {
                            snprintf(buf, sizeof(buf), "1:%4d 2:%4d   ",
                                     gray_val[0], gray_val[1]);
                            OLED_ShowString(0, 0, (u8*)buf);
                            snprintf(buf, sizeof(buf), "3:%4d 4:%4d   ",
                                     gray_val[2], gray_val[3]);
                            OLED_ShowString(0, 1, (u8*)buf);
                            snprintf(buf, sizeof(buf), "5:%4d 6:%4d   ",
                                     gray_val[4], gray_val[5]);
                            OLED_ShowString(0, 2, (u8*)buf);
                            snprintf(buf, sizeof(buf), "7:%4d 8:%4d   ",
                                     gray_val[6], gray_val[7]);
                            OLED_ShowString(0, 3, (u8*)buf);
                            OLED_ShowString(0, 7, (u8*)"A:Gyro  *:Back  ");
                        }
                        else
                        {
                            OLED_ShowString(0, 0, (u8*)"Gray Sensor Err ");
                            OLED_ShowString(0, 1, (u8*)(ret == -1 ? "Timeout!        " : "Frame Err!      "));
                            OLED_ShowString(0, 3, (u8*)"A:Retry  *:Back ");
                        }
                    }
                }
                break;

            case 'B':   /* 切换黑线循迹模式 */
            {
                line_follow_active = !line_follow_active;
                if (line_follow_active)
                {
                    show_pid_params = false;
                    line_follow_init();
                    OLED_ShowString(0, 0, (u8*)"-- LineFollow --");
                    OLED_ShowString(0, 1, (u8*)"Pos:  0.0      ");
                    OLED_ShowString(0, 2, (u8*)"L:+500 R:+500  ");
                    OLED_ShowString(0, 3, (u8*)"B:Exit *:Back  ");
                    OLED_ShowString(0, 4, (u8*)"A:PID Mode     ");
                    follow_cnt = 0;
                }
                else
                {
                    show_pid_params = false;
                    current_dir = DIR_STOP;
                    apply_direction(DIR_STOP, 0);
                    show_main_menu(current_speed);
                }
                break;
            }

            default:
                break;
            }
        }
        else if (key == 0)
        {
            last_key = 0;
        }

        /* 黑线循迹控制周期 */
        if (line_follow_active)
        {
            uint16_t gray_val[8];
            int16_t left_spd, right_spd;

            if (gray_sensor_read_all(gray_val) >= 0)
            {
                int16_t gyro_z = imu_get_gyro_z();
                line_follow_update(gray_val, gyro_z, &left_spd, &right_spd);
                motor_set_speed(MOTOR1, left_spd);
                motor_set_speed(MOTOR2, right_spd);

                if (++follow_cnt >= 5)
                {
                    follow_cnt = 0;
                    char buf[24];

                    if (show_pid_params)
                    {
                        /* PID 参数显示模式 */
                        OLED_ShowString(0, 0, (u8*)"-- PID Tune ----");
                        snprintf(buf, sizeof(buf), "KP: %-4d        ",
                                 line_follow_get_kp());
                        OLED_ShowString(0, 1, (u8*)buf);
                        snprintf(buf, sizeof(buf), "KI: %-4d        ",
                                 line_follow_get_ki());
                        OLED_ShowString(0, 2, (u8*)buf);
                        snprintf(buf, sizeof(buf), "KD: %-4d        ",
                                 line_follow_get_kd());
                        OLED_ShowString(0, 3, (u8*)buf);
                        snprintf(buf, sizeof(buf), "SPD: %-4d       ",
                                 line_follow_get_base_speed());
                        OLED_ShowString(0, 4, (u8*)buf);
                        OLED_ShowString(0, 5, (u8*)"B:Exit *:Back  ");
                        OLED_ShowString(0, 6, (u8*)"A:Normal Mode  ");
                    }
                    else
                    {
                        /* 正常循迹显示 */
                        float pos = 0;
                        float ws = 0, tw = 0;
                        for (int i = 0; i < 8; i++)
                        {
                            int b = (gray_val[i] < 2000) ? 1 : 0;
                            ws += b * i;  tw += b;
                        }
                        pos = (tw > 0.1f) ? (ws / tw) : 3.5f;

                        OLED_ShowString(0, 0, (u8*)"-- LineFollow --");
                        snprintf(buf, sizeof(buf), "Pos: %5.1f      ", (double)pos);
                        OLED_ShowString(0, 1, (u8*)buf);
                        snprintf(buf, sizeof(buf), "L:%+4d R:%+4d  ",
                                 left_spd, right_spd);
                        OLED_ShowString(0, 2, (u8*)buf);
                        OLED_ShowString(0, 3, (u8*)"B:Exit *:Back  ");
                        OLED_ShowString(0, 4, (u8*)"A:PID Mode     ");
                    }
                }
            }
            delay_ms(10);
        }
        else if (show_gyro_debug)
        {
            /* 陀螺仪诊断实时刷新 */
            char buf[17];

            if (imu_status != 0)
            {
                OLED_ShowString(0, 1, (u8*)"IMU NOT FOUND!  ");
                if (imu_status == -1)
                {
                    /* 实时扫描 I2C 总线 */
                    uint8_t dev = imu_scan_i2c();
                    if (dev != 0)
                        snprintf(buf, sizeof(buf), "Dev@0x%02X (not68)", dev);
                    else
                        snprintf(buf, sizeof(buf), "No I2C device!  ");
                    OLED_ShowString(0, 2, (u8*)buf);
                }
                else if (imu_status == -2)
                    OLED_ShowString(0, 2, (u8*)"WHO_AM_I fail    ");
                else if (imu_status == -3)
                    OLED_ShowString(0, 2, (u8*)"PWR_MGMT fail    ");
                else
                    OLED_ShowString(0, 2, (u8*)"GYRO_CONFIG fail ");
                OLED_ShowString(0, 3, (u8*)"Chk wiring + pwr ");
                OLED_ShowString(0, 7, (u8*)"A:Gray  *:Back  ");
                delay_ms(200);
                continue;
            }

            int16_t gyro_z = imu_get_gyro_z();
            uint8_t whoami = imu_get_whoami();
            int16_t dps_approx = gyro_z * 10 / 131;  /* °/s ×10 */

            /* WHO_AM_I (hex) + Z raw */
            snprintf(buf, sizeof(buf), "ID:%02X Z:%6d", whoami, gyro_z);
            OLED_ShowString(0, 1, (u8*)buf);

            snprintf(buf, sizeof(buf), "dps : %-4d.%1d    ",
                     dps_approx / 10, (dps_approx < 0 ? -dps_approx : dps_approx) % 10);
            OLED_ShowString(0, 2, (u8*)buf);

            if (gyro_z > 1000)
                OLED_ShowString(0, 4, (u8*)"<<< Turn LEFT   ");
            else if (gyro_z < -1000)
                OLED_ShowString(0, 4, (u8*)">>> Turn RIGHT  ");
            else
                OLED_ShowString(0, 4, (u8*)"---- Straight ---");

            delay_ms(50);
        }
        else
        {
            delay_ms(10);
        }
    }
}

/* 显示主菜单 */
static void show_main_menu(int speed)
{
    char buf[17];
    snprintf(buf, sizeof(buf), "Speed: %-4d     ", speed);
    OLED_ShowString(0, 0, (u8*)buf);
    OLED_ShowString(0, 1, (u8*)"2:Fwd   8:Rev   ");
    OLED_ShowString(0, 2, (u8*)"4:Left  6:Right ");
    OLED_ShowString(0, 3, (u8*)"1:-Spd  3:+Spd  ");
    OLED_ShowString(0, 4, (u8*)"#:Rst   5:Stop  ");
    OLED_ShowString(0, 5, (u8*)"0:Estp  A:Gray  ");
    OLED_ShowString(0, 6, (u8*)"B:LF   *:Menu   ");
    OLED_ShowString(0, 7, (u8*)"                ");
}
