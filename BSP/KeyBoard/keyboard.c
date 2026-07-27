#include "KeyBoard/keyboard.h"

/*
 * 4x4 矩阵键盘，行扫描法
 * 行(H1~H4): PB6, PB7, PB8, PB9  -> GPIO 推挽输出
 * 列(V1~V4): PB20, PB24, PB25, PB27 -> GPIO 上拉输入
 *
 * 按键布局:
 *   [1] [2] [3] [A]
 *   [4] [5] [6] [B]
 *   [7] [8] [9] [C]
 *   [*] [0] [#] [D]
 */
static const char key_map[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

void keyboard_init(void)
{
    /* H1~H4 初始化为高电平 */
    DL_GPIO_setPins(keyboard_PORT,
        keyboard_H1_PIN | keyboard_H2_PIN |
        keyboard_H3_PIN | keyboard_H4_PIN);
}

char get_keyboard_value(void)
{
    uint32_t row_pins[4] = {
        keyboard_H1_PIN, keyboard_H2_PIN,
        keyboard_H3_PIN, keyboard_H4_PIN
    };
    uint32_t col_pins[4] = {
        keyboard_V1_PIN, keyboard_V2_PIN,
        keyboard_V3_PIN, keyboard_V4_PIN
    };
    int key_value = 0;
    int i, j;

    for (i = 0; i < 4; i++)
    {
        /* 将当前行拉低，其他行拉高 */
        DL_GPIO_clearPins(keyboard_PORT, row_pins[i]);
        delay_cycles(5);

        /* 读取所有列 */
        for (j = 0; j < 4; j++)
        {
            if ((DL_GPIO_readPins(keyboard_PORT, col_pins[j]) == 0))
            {
                key_value = i * 4 + j + 1;
            }
        }

        /* 恢复当前行为高电平 */
        DL_GPIO_setPins(keyboard_PORT, row_pins[i]);
    }

    if (key_value == 0)
        return 0;   /* 无按键 */

    return key_map[(key_value - 1) / 4][(key_value - 1) % 4];
}
