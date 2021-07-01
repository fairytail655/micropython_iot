#ifndef _IOT_LIGHT_H_
#define _IOT_LIGHT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/ledc.h"
#include "freertos/timers.h"

typedef struct {
    gpio_num_t io_num;
    ledc_mode_t mode;
    ledc_channel_t channel; 
    TimerHandle_t timer;
    int breath_period;
    uint32_t next_duty;
} light_channel_t;

typedef struct {
    uint8_t channel_num;
    ledc_mode_t mode;
    ledc_timer_t ledc_timer;
    uint32_t full_duty;
    uint32_t freq_hz;
    ledc_timer_bit_t timer_bit;
    light_channel_t* channel_group[0];
} light_t;
typedef light_t* light_handle_t;

typedef enum {
    LIGHT_SET_DUTY_DIRECTLY = 0,    /*!< set duty directly */
    LIGHT_DUTY_FADE_1S,             /*!< set duty with fade in 1 second */
    LIGHT_DUTY_FADE_2S,             /*!< set duty with fade in 2 second */
    LIGHT_DUTY_FADE_3S,             /*!< set duty with fade in 3 second */
    LIGHT_DUTY_FADE_MAX,            /*!< user shouldn't use this */
} light_duty_mode_t;

#ifdef __cplusplus
}
#endif

#endif
