/*
Copyright (C) 2019 Jim Jiang <jim@lotlab.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "ssd1306_oled.h"
#include <stdint.h>
#include <string.h>

#include "app_error.h"
#include "nrf_drv_twi.h"

#include "events.h"
#include "keyboard_evt.h"
#include "queue.h"

#define SSD1306_SDA 9
#define SSD1306_SCL 10
#define SSD1306_ADDR 0x3C

const uint8_t ssd1306_init_commands[] = {
    SSD1306_DISPLAYOFF, /* display off */
    SSD1306_SETLOWCOLUMN, /* set lower column address */
    SSD1306_SETHIGHCOLUMN, /* set higher column address */
    SSD1306_SETSTARTLINE, /* set display start line */
    SSD1306_PAGESTARTADDR, /* set page address */
    SSD1306_SETCONTRAST, /* contract control */
    0xff, /* 128 */
    SSD1306_SEGREMAP_INVERSE, /* set segment remap */
    SSD1306_NORMALDISPLAY, /* normal / reverse */
    SSD1306_SETMULTIPLEX, /* multiplex ratio */
    0x1F, /* duty = 1/32 */
    SSD1306_COMSCANDEC, /* Com scan direction */
    SSD1306_SETDISPLAYOFFSET, /* set display offset */
    0x00,
    SSD1306_SETDISPLAYCLOCKDIV, /* set osc division */
    0x80,
    SSD1306_SETPRECHARGE, /* set pre-charge period */
    0x1f,
    SSD1306_SETCOMPINS, /* set COM pins */
    0x00,
    SSD1306_SETVCOMDETECT, /* set vcomh */
    0x40,
    SSD1306_CHARGEPUMP, /* set charge pump enable */
    0x14,
    SSD1306_COLUMNADDR,
    0x00,
    0x7F,
    SSD1306_PAGEADDR,
    0x00,
    0x03,
    SSD1306_DISPLAYON, /* display ON */
    // SSD1306_DISPLAYALLON,
};

static uint8_t ssd1306_display_buffer[128 * 4] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0x7c, 0x3f, 0x1f, 0x0c, 0x04, 0x80, 0xc0, 0xc0,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80, 0xc0, 0xe0, 0xf0, 0xf8,
    0x7c, 0x3e, 0x1f, 0x0f, 0x07, 0x83, 0xc1, 0xe0, 0xf0, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0x0f, 0x07,
    0x8f, 0xff, 0xfe, 0xfc, 0xf8, 0xf0, 0xe0, 0xc0, 0x80, 0x80, 0x00, 0x00, 0x00, 0x02, 0x03, 0xfe,
    0xfe, 0xfe, 0xfe, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0xe0, 0xf0, 0xf0, 0x30,
    0x10, 0xf0, 0xf0, 0xe0, 0xc0, 0x80, 0x00, 0x30, 0xf0, 0xfc, 0xfe, 0xfe, 0x30, 0x30, 0x20, 0x02,
    0x03, 0xfe, 0xfe, 0xfe, 0xfe, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xe0, 0xf0,
    0x10, 0x10, 0xf0, 0xf0, 0xf0, 0xe0, 0x00, 0x00, 0x01, 0x03, 0xff, 0xff, 0xff, 0xff, 0x20, 0x30,
    0x70, 0xf0, 0xf0, 0xe0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f,
    0x7e, 0xfc, 0xf8, 0xf0, 0xe0, 0xc1, 0x83, 0x07, 0x0f, 0x1f, 0x3e, 0x7c, 0xfc, 0xfc, 0xfe, 0xff,
    0x8f, 0x87, 0x03, 0x01, 0x01, 0x01, 0x03, 0x07, 0x03, 0x01, 0x00, 0x00, 0x00, 0x40, 0x40, 0x7f,
    0x7f, 0x7f, 0x7f, 0x60, 0x40, 0x40, 0x40, 0x78, 0x78, 0x00, 0x07, 0x3f, 0x7f, 0x7f, 0x7f, 0xc0,
    0xc0, 0xf0, 0x7f, 0x7f, 0x3f, 0x1f, 0x00, 0x00, 0x3f, 0x7f, 0xff, 0xff, 0xf0, 0x60, 0x60, 0x40,
    0x40, 0x7f, 0x7f, 0x7f, 0x7f, 0x60, 0x40, 0x40, 0x40, 0x78, 0x78, 0x00, 0x10, 0x7c, 0xfc, 0xfe,
    0xe6, 0x42, 0x7f, 0x7f, 0xff, 0xff, 0x60, 0x40, 0x00, 0xc0, 0x7f, 0xff, 0xff, 0x7f, 0x60, 0xe0,
    0xf0, 0xff, 0x7f, 0x7f, 0x1f, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7e, 0xfc, 0xf8, 0x70, 0x20, 0x01, 0x03, 0x07,
    0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// static uint8_t ssd1306_display_buffer[128 * 4];

// #define BLOCKING_MODE
#ifndef BLOCKING_MODE
struct tx_item_flag {
    bool is_pointer : 1;
    bool is_command : 1;
    bool data_send : 1;
};
struct tx_item {
    uint16_t len;
    uint16_t index;
    struct tx_item_flag flag;
    union ssd1306_oled {
        const uint8_t* data_pointer;
        uint8_t data[4];
    } data;
};

QUEUE(struct tx_item, twi_tx_buff, 10);
#endif

static const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(0);

#ifndef BLOCKING_MODE
/**
 * @brief twi 公共发送buff
 * 
 */
static uint8_t twi_common_buff[2] = { 0 };
/**
 * @brief 调用TWI发送函数发送一个项目
 * 
 * @param item 要发送的项目
 */
static void ssd1306_tx_item_send(struct tx_item* item)
{
    twi_common_buff[0] = item->flag.is_command ? 0x00 : 0x40;

    if (item->flag.is_pointer) {
        twi_common_buff[1] = item->data.data_pointer[item->index];
    } else {
        twi_common_buff[1] = item->data.data[item->index];
    }

    uint32_t error = nrf_drv_twi_tx(&m_twi, SSD1306_ADDR, twi_common_buff, 2, false);
    if (error != 0) {
        APP_ERROR_CHECK(error);
    }
    item->index++;

    // 数据长度一致，标志为已发送
    if (item->index >= item->len)
        item->flag.data_send = true;
}
#endif

/**
 * @brief 发送命令或数据
 * 
 * @param is_cmd 此条是命令
 * @param len 长度
 * @param data 数据指针。若发送长度小于等于4，则会尝试复制数据，否则会直接使用指针。在长度大于4时，请保证指针非局部变量。
 */
static void ssd1306_write(bool is_cmd, uint8_t len, const uint8_t* data)
{
#ifndef BLOCKING_MODE
    struct tx_item item;
    item.len = len;
    item.index = 0;

    item.flag.is_command = is_cmd;
    item.flag.data_send = false;

    // 数据较短，尝试直接复制
    if (len <= 4) {
        item.flag.is_pointer = false;
        memcpy(item.data.data, data, len);
    } else {
        item.flag.is_pointer = true;
        item.data.data_pointer = data;
    }

    twi_tx_buff_push(item);

    if (!nrf_drv_twi_is_busy(&m_twi)) {
        // 如果当前等待队列为空，则尝试发送
        ssd1306_tx_item_send(twi_tx_buff_peek());
    }
#else
    uint8_t buff[2] = { is_cmd ? 0x00 : 0x40 };
    for (uint8_t i = 0; i < len; i++) {
        buff[1] = data[i];
        nrf_drv_twi_tx(&m_twi, SSD1306_ADDR, buff, 2, false);
    }
#endif
}

#ifndef BLOCKING_MODE
void ssd1306_handler(nrf_drv_twi_evt_t const* p_event, void* p_context)
{
    switch (p_event->type) {
    case NRF_DRV_TWI_EVT_ADDRESS_NACK:
    case NRF_DRV_TWI_EVT_DATA_NACK:
    case NRF_DRV_TWI_EVT_DONE:
        if (p_event->xfer_desc.type == NRF_DRV_TWI_XFER_TX) {
            // 当前队列不空
            if (!twi_tx_buff_empty()) {
                struct tx_item* item = twi_tx_buff_peek();
                // 如果这个是已经发送过的
                if (item->flag.data_send) {
                    // 就出队列
                    twi_tx_buff_pop();
                    // 然后尝试发送下一个
                    if (!twi_tx_buff_empty()) {
                        ssd1306_tx_item_send(twi_tx_buff_peek());
                    }
                } else {
                    // 不然就尝试发送
                    ssd1306_tx_item_send(item);
                }
            }
        }
        break;
    default:
        break;
    }
}
#endif

/**
 * @brief 初始化OLED屏幕
 * 
 */
static void ssd1306_oled_init()
{
    ssd1306_write(true, sizeof(ssd1306_init_commands), (uint8_t*)ssd1306_init_commands);
}

/**
 * @brief 初始化TWI驱动
 * 
 */
static void ssd1306_twi_init()
{
    const nrf_drv_twi_config_t ssd1306_config = {
        .scl = SSD1306_SCL,
        .sda = SSD1306_SDA,
        .frequency = NRF_DRV_TWI_FREQ_400K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false
    };

#ifndef BLOCKING_MODE
    uint32_t err_code = nrf_drv_twi_init(&m_twi, &ssd1306_config, ssd1306_handler, NULL);
#else
    uint32_t err_code = nrf_drv_twi_init(&m_twi, &ssd1306_config, NULL, NULL);
#endif
    APP_ERROR_CHECK(err_code);

    nrf_drv_twi_enable(&m_twi);
}

/**
 * @brief 显示指定行的Buff
 * 
 * @param row 行号，0-3
 * @param col_start 列开始Index，0-127
 * @param len 显示长度
 */
void ssd1306_show_buff(uint8_t row, uint8_t col_start, uint8_t len)
{
    if (row >= 4)
        return;
    if (col_start + len > 128)
        len = 128 - col_start;

    uint8_t commands[] = { 0xB0 + row, 0x00 + (col_start & 0xF), 0x10 + ((col_start >> 4) & 0xF) };
    ssd1306_write(true, sizeof(commands), commands);
    ssd1306_write(false, len, &ssd1306_display_buffer[row * 128 + col_start]);
}

/**
 * @brief 显示Buff里面的所有内容
 * 
 */
static void ssd1306_show_all()
{
    for (uint8_t i = 0; i < 4; i++) {
        ssd1306_show_buff(i, 0, 128);
    }
}

/**
 * @brief 清空Buff并显示
 * 
 */
void ssd1306_clr()
{
    memset(ssd1306_display_buffer, 0, sizeof(ssd1306_display_buffer));
    ssd1306_show_all();
}

/**
 * @brief 进入睡眠状态
 * 
 */
static void ssd1306_sleep()
{
    uint8_t cmd = SSD1306_DISPLAYOFF;
    ssd1306_write(true, 1, &cmd);
}

/**
 * @brief 退出睡眠状态 
 * 
 */
static void ssd1306_wake()
{
    uint8_t cmd = SSD1306_DISPLAYON;
    ssd1306_write(true, 1, &cmd);
}

static void ssd1306_event_handler(enum user_event event, void* arg)
{
    uint8_t param = (uint32_t)arg;
    switch (event) {
    case USER_EVT_STAGE:
        switch (param) {
        case KBD_STATE_POST_INIT:
            ssd1306_twi_init();
            ssd1306_oled_init();
            break;
        case KBD_STATE_INITED:
            ssd1306_show_all();
            break;
        default:
            break;
        }
        break;
    case USER_EVT_POWERSAVE:
        switch (param) {
        case PWR_SAVE_ENTER:
            ssd1306_sleep();
            break;
        case PWR_SAVE_EXIT:
            ssd1306_wake();
            break;
        default:
            break;
        }
        break;
    case USER_EVT_SLEEP:
        // ssd1306_clr();
        // ssd1306_sleep();
        break;
    default:
        break;
    }
}

EVENT_HANDLER(ssd1306_event_handler);
