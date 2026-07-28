#include "bsp.h"
#include "Course/course.h"
#include <stdlib.h>

/* Change to -1 if a physical left turn produces a negative gyro Z value. */
#define COURSE_GYRO_Z_SIGN          1

#define COURSE_CONTROL_MS           10U
#define COURSE_TURN_CONTROL_MS       5U
#define COURSE_STRAIGHT_SPEED      550
#define COURSE_HEADING_DIVISOR     100L
#define COURSE_HEADING_CORR_MAX    220
#define COURSE_DIAGONAL_ANGLE_MDEG 38660L
#define COURSE_TURN_TOL_MDEG       1500L
#define COURSE_STRAIGHT_TIMEOUT    800U
#define COURSE_ARC_TIMEOUT         900U
#define COURSE_LINE_IGNORE_TICKS    40U
#define COURSE_LINE_DROP           220U
#define COURSE_LINE_CONFIRM          3U
#define COURSE_LINE_LOST_CONFIRM      5U

#define COURSE_LED_PORT             GPIOB
#define COURSE_LED_PIN              DL_GPIO_PIN_22
#define COURSE_LED_IOMUX            IOMUX_PINCM50
#define COURSE_BUZZER_PORT          GPIOB
#define COURSE_BUZZER_PIN           DL_GPIO_PIN_27
#define COURSE_BUZZER_IOMUX         IOMUX_PINCM58

typedef enum {
    COURSE_OK = 0,
    COURSE_ABORTED,
    COURSE_IMU_ERROR,
    COURSE_GRAY_ERROR,
    COURSE_TIMEOUT
} CourseResult;

static int32_t yaw_mdeg;
static uint16_t white_level[GRAY_CHANNELS];

static void stop_motors(void)
{
    motor_set_speed(MOTOR1, 0);
    motor_set_speed(MOTOR2, 0);
}

static void show_status(const char *stage, int32_t angle_mdeg)
{
    char buffer[22];

    OLED_ShowString(0, 0, (u8*)"-- Course Run -- ");
    OLED_ShowString(0, 1, (u8*)stage);
    snprintf(buffer, sizeof(buffer), "Yaw:%+5ld.%1ld  ",
             (long)(angle_mdeg / 1000L),
             (long)(labs(angle_mdeg) % 1000L) / 100L);
    OLED_ShowString(0, 2, (u8*)buffer);
    OLED_ShowString(0, 7, (u8*)"5:STOP          ");
}

static bool abort_requested(void)
{
    char key = get_keyboard_value();
    return key == '5' || key == '*';
}

static void restore_buzzer_pin_to_keyboard(void)
{
    DL_GPIO_disableOutput(COURSE_BUZZER_PORT, COURSE_BUZZER_PIN);
    DL_GPIO_initDigitalInputFeatures(COURSE_BUZZER_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_PULL_UP,
        DL_GPIO_HYSTERESIS_DISABLE, DL_GPIO_WAKEUP_DISABLE);
}

static void signal_point(void)
{
    uint16_t i;

    DL_GPIO_clearPins(COURSE_LED_PORT, COURSE_LED_PIN);
    DL_GPIO_initDigitalOutputFeatures(COURSE_BUZZER_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);
    DL_GPIO_enableOutput(COURSE_BUZZER_PORT, COURSE_BUZZER_PIN);

    for (i = 0; i < 100U; i++)
    {
        DL_GPIO_setPins(COURSE_BUZZER_PORT, COURSE_BUZZER_PIN);
        delay_us(250);
        DL_GPIO_clearPins(COURSE_BUZZER_PORT, COURSE_BUZZER_PIN);
        delay_us(250);
    }

    restore_buzzer_pin_to_keyboard();
    DL_GPIO_setPins(COURSE_LED_PORT, COURSE_LED_PIN);
}

void course_signal_init(void)
{
    DL_GPIO_initDigitalOutputFeatures(COURSE_LED_IOMUX,
        DL_GPIO_INVERSION_DISABLE, DL_GPIO_RESISTOR_NONE,
        DL_GPIO_DRIVE_STRENGTH_LOW, DL_GPIO_HIZ_DISABLE);
    DL_GPIO_setPins(COURSE_LED_PORT, COURSE_LED_PIN);
    DL_GPIO_enableOutput(COURSE_LED_PORT, COURSE_LED_PIN);
}

void course_show_menu(bool imu_ok)
{
    OLED_Clear();
    OLED_ShowString(0, 0, (u8*)"-- Course Mode --");
    OLED_ShowString(0, 1, (u8*)"1:A-B           ");
    OLED_ShowString(0, 2, (u8*)"2:Outer Lap     ");
    OLED_ShowString(0, 3, (u8*)"3:Cross Lap     ");
    OLED_ShowString(0, 4, (u8*)"4:Cross x4      ");
    OLED_ShowString(0, 6, (u8*)(imu_ok ? "IMU:OK          " : "IMU:NOT FOUND   "));
    OLED_ShowString(0, 7, (u8*)"*:Back          ");
}

static CourseResult update_yaw(uint16_t period_ms)
{
    int16_t raw_z;
    int32_t corrected;

    if (imu_read_gyro_z(&raw_z) != 0)
        return COURSE_IMU_ERROR;
    corrected = (int32_t)raw_z - imu_get_gyro_z_bias();
    corrected *= COURSE_GYRO_Z_SIGN;
    yaw_mdeg += corrected * (int32_t)period_ms / 131L;
    return COURSE_OK;
}

static CourseResult turn_relative(int32_t delta_mdeg)
{
    int32_t start_yaw = yaw_mdeg;
    int32_t target = yaw_mdeg + delta_mdeg;
    uint16_t ticks = 0;

    show_status(delta_mdeg > 0 ? "Turning LEFT    " : "Turning RIGHT   ", yaw_mdeg);
    while (labs(target - yaw_mdeg) > COURSE_TURN_TOL_MDEG)
    {
        int32_t error = target - yaw_mdeg;

        if (abort_requested())
        {
            stop_motors();
            return COURSE_ABORTED;
        }
        if (error > 0)
        {
            motor_set_speed(MOTOR1, -1000);
            motor_set_speed(MOTOR2, 1000);
        }
        else
        {
            motor_set_speed(MOTOR1, 1000);
            motor_set_speed(MOTOR2, -1000);
        }

        delay_ms(COURSE_TURN_CONTROL_MS);
        if (update_yaw(COURSE_TURN_CONTROL_MS) != COURSE_OK)
        {
            stop_motors();
            return COURSE_IMU_ERROR;
        }
        if (++ticks > 500U)
        {
            stop_motors();
            return COURSE_TIMEOUT;
        }
        if (ticks == 10U &&
            (yaw_mdeg - start_yaw) * delta_mdeg < 0L)
        {
            /* The configured gyro sign is opposite to the real installation. */
            stop_motors();
            return COURSE_IMU_ERROR;
        }
    }
    stop_motors();
    delay_ms(60);
    return COURSE_OK;
}

static void reset_line_detector(void)
{
    uint8_t i;
    for (i = 0; i < GRAY_CHANNELS; i++)
        white_level[i] = 0U;
}

static bool target_line_present(const uint16_t *gray)
{
    uint16_t min_value = 0xFFFFU;
    uint16_t max_value = 0U;
    uint16_t max_drop = 0U;
    uint8_t i;

    for (i = 0; i < GRAY_CHANNELS; i++)
    {
        uint16_t drop;

        if (gray[i] > white_level[i])
            white_level[i] = gray[i];
        drop = white_level[i] > gray[i] ? white_level[i] - gray[i] : 0U;
        if (drop > max_drop) max_drop = drop;
        if (gray[i] < min_value) min_value = gray[i];
        if (gray[i] > max_value) max_value = gray[i];
    }
    return max_drop >= COURSE_LINE_DROP &&
           (uint16_t)(max_value - min_value) >= LF_MIN_CONTRAST;
}

static CourseResult drive_straight_to_line(const char *stage)
{
    int32_t target_yaw = yaw_mdeg;
    uint16_t gray[GRAY_CHANNELS];
    uint16_t ticks;
    uint8_t detected_count = 0;
    uint8_t uart_errors = 0;

    reset_line_detector();
    show_status(stage, yaw_mdeg);
    for (ticks = 0; ticks < COURSE_STRAIGHT_TIMEOUT; ticks++)
    {
        int32_t heading_error;
        int32_t correction;
        int left_speed;
        int right_speed;

        if (abort_requested())
        {
            stop_motors();
            return COURSE_ABORTED;
        }
        if (update_yaw(COURSE_CONTROL_MS) != COURSE_OK)
        {
            stop_motors();
            return COURSE_IMU_ERROR;
        }

        if (gray_sensor_read_all(gray) < 0)
        {
            if (++uart_errors >= 5U)
            {
                stop_motors();
                return COURSE_GRAY_ERROR;
            }
        }
        else
        {
            uart_errors = 0;
            if (ticks >= COURSE_LINE_IGNORE_TICKS && target_line_present(gray))
                detected_count++;
            else
                detected_count = 0;

            if (detected_count >= COURSE_LINE_CONFIRM)
            {
                stop_motors();
                return COURSE_OK;
            }
        }

        heading_error = yaw_mdeg - target_yaw;
        correction = heading_error / COURSE_HEADING_DIVISOR;
        if (correction > COURSE_HEADING_CORR_MAX) correction = COURSE_HEADING_CORR_MAX;
        if (correction < -COURSE_HEADING_CORR_MAX) correction = -COURSE_HEADING_CORR_MAX;
        left_speed = COURSE_STRAIGHT_SPEED + (int)correction;
        right_speed = COURSE_STRAIGHT_SPEED - (int)correction;
        motor_set_speed(MOTOR1, left_speed);
        motor_set_speed(MOTOR2, right_speed);
        delay_ms(COURSE_CONTROL_MS);
    }
    stop_motors();
    return COURSE_TIMEOUT;
}

static CourseResult follow_arc(const char *stage)
{
    uint16_t gray[GRAY_CHANNELS];
    uint16_t ticks;
    int16_t left_speed = COURSE_STRAIGHT_SPEED;
    int16_t right_speed = COURSE_STRAIGHT_SPEED;
    uint8_t good_count = 0;
    uint8_t lost_count = 0;
    uint8_t uart_errors = 0;

    line_follow_init();
    show_status(stage, yaw_mdeg);
    for (ticks = 0; ticks < COURSE_ARC_TIMEOUT; ticks++)
    {
        bool tracking;

        if (abort_requested())
        {
            stop_motors();
            return COURSE_ABORTED;
        }
        if (update_yaw(COURSE_CONTROL_MS) != COURSE_OK)
        {
            stop_motors();
            return COURSE_IMU_ERROR;
        }
        if (gray_sensor_read_all(gray) < 0)
        {
            if (++uart_errors >= 5U)
            {
                stop_motors();
                return COURSE_GRAY_ERROR;
            }
            tracking = false;
        }
        else
        {
            uart_errors = 0;
            tracking = line_follow_update(gray, &left_speed, &right_speed);
        }

        if (tracking)
        {
            if (good_count < 255U) good_count++;
            lost_count = 0;
            motor_set_speed(MOTOR1, left_speed);
            motor_set_speed(MOTOR2, right_speed);
        }
        else if (good_count >= COURSE_LINE_CONFIRM)
        {
            if (++lost_count >= COURSE_LINE_LOST_CONFIRM)
            {
                stop_motors();
                return COURSE_OK;
            }
            motor_set_speed(MOTOR1, left_speed);
            motor_set_speed(MOTOR2, right_speed);
        }
        else
        {
            motor_set_speed(MOTOR1, COURSE_STRAIGHT_SPEED);
            motor_set_speed(MOTOR2, COURSE_STRAIGHT_SPEED);
        }
        delay_ms(COURSE_CONTROL_MS);
    }
    stop_motors();
    return COURSE_TIMEOUT;
}

static CourseResult run_outer_lap(void)
{
    CourseResult result;

    result = drive_straight_to_line("A -> B          ");
    if (result != COURSE_OK) return result;
    signal_point();
    result = follow_arc("B arc C        ");
    if (result != COURSE_OK) return result;
    signal_point();
    result = drive_straight_to_line("C -> D          ");
    if (result != COURSE_OK) return result;
    signal_point();
    result = follow_arc("D arc A        ");
    if (result != COURSE_OK) return result;
    signal_point();
    return COURSE_OK;
}

static CourseResult run_cross_lap(void)
{
    CourseResult result;

    result = turn_relative(-COURSE_DIAGONAL_ANGLE_MDEG);
    if (result != COURSE_OK) return result;
    result = drive_straight_to_line("A -> C          ");
    if (result != COURSE_OK) return result;
    signal_point();
    result = turn_relative(COURSE_DIAGONAL_ANGLE_MDEG);
    if (result != COURSE_OK) return result;
    result = follow_arc("C arc B        ");
    if (result != COURSE_OK) return result;
    signal_point();
    result = turn_relative(COURSE_DIAGONAL_ANGLE_MDEG);
    if (result != COURSE_OK) return result;
    result = drive_straight_to_line("B -> D          ");
    if (result != COURSE_OK) return result;
    signal_point();
    result = turn_relative(-COURSE_DIAGONAL_ANGLE_MDEG);
    if (result != COURSE_OK) return result;
    result = follow_arc("D arc A        ");
    if (result != COURSE_OK) return result;
    signal_point();
    return COURSE_OK;
}

static void show_result(CourseResult result)
{
    OLED_Clear();
    OLED_ShowString(0, 0, (u8*)"-- Course End -- ");
    switch (result)
    {
    case COURSE_OK:         OLED_ShowString(0, 2, (u8*)"COMPLETED       "); break;
    case COURSE_ABORTED:    OLED_ShowString(0, 2, (u8*)"STOPPED         "); break;
    case COURSE_IMU_ERROR:  OLED_ShowString(0, 2, (u8*)"IMU ERROR       "); break;
    case COURSE_GRAY_ERROR: OLED_ShowString(0, 2, (u8*)"GRAY UART ERROR "); break;
    default:                OLED_ShowString(0, 2, (u8*)"STAGE TIMEOUT   "); break;
    }
    OLED_ShowString(0, 7, (u8*)"Press any key   ");
}

static void wait_result_ack(void)
{
    delay_ms(300);
    while (get_keyboard_value() != 0) delay_ms(10);
    while (get_keyboard_value() == 0) delay_ms(10);
    while (get_keyboard_value() != 0) delay_ms(10);
}

bool course_run(uint8_t task, bool imu_ok)
{
    CourseResult result = COURSE_OK;
    uint8_t lap;

    stop_motors();
    if (!imu_ok)
    {
        show_result(COURSE_IMU_ERROR);
        wait_result_ack();
        return false;
    }

    OLED_Clear();
    OLED_ShowString(0, 0, (u8*)"Keep car STILL  ");
    OLED_ShowString(0, 1, (u8*)"Calibrating IMU ");
    if (imu_calibrate_gyro_z(200U, 5U) != 0)
    {
        show_result(COURSE_IMU_ERROR);
        wait_result_ack();
        return false;
    }
    yaw_mdeg = 0;

    if (task == 1U)
    {
        result = drive_straight_to_line("A -> B          ");
        if (result == COURSE_OK) signal_point();
    }
    else if (task == 2U)
    {
        result = run_outer_lap();
    }
    else if (task == 3U || task == 4U)
    {
        uint8_t lap_count = task == 4U ? 4U : 1U;
        for (lap = 0; lap < lap_count && result == COURSE_OK; lap++)
            result = run_cross_lap();
    }
    else
    {
        result = COURSE_ABORTED;
    }

    stop_motors();
    show_result(result);
    wait_result_ack();
    return result == COURSE_OK;
}
