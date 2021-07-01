#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "soc/ledc_reg.h"
#include "driver/ledc.h"
#include "iot_light.h"

#include "sdkconfig.h"
#include "py/runtime.h"

STATIC const char* TAG = "light";

#define IOT_CHECK(tag, a, ret)  if(!(a)) {       \
        return mp_obj_new_int(ret);                            \
        }
#define ERR_ASSERT(tag, param, ret)  IOT_CHECK(tag, (param) == ESP_OK, ret)
#define POINT_ASSERT(tag, param)	IOT_CHECK(tag, (param) != NULL, ESP_FAIL)
#define LIGHT_NUM_MAX   4

STATIC light_t* g_light_group[LIGHT_NUM_MAX];
STATIC bool g_fade_installed = false;

STATIC void breath_timer_callback(TimerHandle_t xTimer)
{
    for (int i = 0; i < LIGHT_NUM_MAX; i++) {
        if (g_light_group[i] != NULL) {
            light_t* light = g_light_group[i];
            for (int j = 0; j < light->channel_num; j++) {
                if (light->channel_group[j] != NULL && light->channel_group[j]->timer == xTimer) {
                    light_channel_t* l_chn = light->channel_group[j];
                    ledc_set_fade_with_time(l_chn->mode, l_chn->channel, l_chn->next_duty, l_chn->breath_period / 2);
                    l_chn->next_duty = light->full_duty - l_chn->next_duty;
                    ledc_fade_start(l_chn->mode, l_chn->channel, LEDC_FADE_NO_WAIT);
                }
            }
        }
    }
}

STATIC light_channel_t* light_channel_create(gpio_num_t io_num, ledc_channel_t channel, ledc_mode_t mode, ledc_timer_t timer)
{
    ledc_channel_config_t ledc_channel = {
        .channel = channel,
        .duty = 0,
        .gpio_num = io_num,
        .intr_type = LEDC_INTR_FADE_END,
        .speed_mode = mode,
        .timer_sel = timer
    };
    ERR_ASSERT(TAG, ledc_channel_config(&ledc_channel), NULL);
    light_channel_t* pwm = (light_channel_t*)calloc(1, sizeof(light_channel_t));

    pwm->io_num = io_num;
    pwm->channel = channel;
    pwm->mode = mode;
    pwm->breath_period = 0;
    pwm->timer = NULL;
    pwm->next_duty = 0;
    return pwm;
}

STATIC mp_obj_t light_channel_delete(light_channel_t* light_channel)
{
    POINT_ASSERT(TAG, light_channel);
    if (light_channel->timer != NULL) {
        xTimerDelete(light_channel->timer, portMAX_DELAY);
        light_channel->timer = NULL;
    }
    free(light_channel);
    return mp_obj_new_int(ESP_OK);
}

STATIC void iot_light_control(mp_obj_t gpio_num_obj, mp_obj_t level_obj) {
	int gpio_num = mp_obj_get_int(gpio_num_obj);
	int level = mp_obj_get_int(level_obj);

    ESP_LOGE(TAG, "%d", CONFIG_LIGHT_GPIO_NUM);
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
	gpio_set_level(gpio_num, level);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(control_obj, iot_light_control);

STATIC mp_obj_t iot_light_create(mp_uint_t n_args, const mp_obj_t *arg_in) 
{
	ledc_timer_t timer = (ledc_timer_t)mp_obj_get_int(arg_in[0]);
	ledc_mode_t speed_mode = (ledc_mode_t)mp_obj_get_int(arg_in[1]);
	uint32_t freq_hz = mp_obj_get_int(arg_in[2]);
	uint8_t channel_num = mp_obj_get_int(arg_in[3]);
	ledc_timer_bit_t timer_bit = mp_obj_get_int(arg_in[4]);
    IOT_CHECK(TAG, channel_num != 0, NULL);
    ledc_timer_config_t timer_conf = {
        .timer_num = timer,
        .speed_mode = speed_mode,
        .freq_hz = freq_hz,
        .duty_resolution = timer_bit
    };
    ERR_ASSERT(TAG, ledc_timer_config( &timer_conf), NULL);
    light_handle_t light_ptr = (light_t*)calloc(1, sizeof(light_t) + sizeof(light_channel_t*) * channel_num);

    light_ptr->channel_num = channel_num;
    light_ptr->ledc_timer = timer;
    light_ptr->full_duty = (1 << timer_bit) - 1;
    light_ptr->freq_hz = freq_hz;
    light_ptr->mode = speed_mode;
    light_ptr->timer_bit = timer_bit;
    for (int i = 0; i < channel_num; i++) {
        light_ptr->channel_group[i] = NULL;
    }
    for (int i = 0; i < LIGHT_NUM_MAX; i++) {
        if (g_light_group[i] == NULL) {
            g_light_group[i] = light_ptr;
            break;
        }
    }
    return (mp_obj_t)light_ptr;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(create_obj, 5, iot_light_create);

STATIC mp_obj_t iot_light_delete(mp_obj_t light_handle)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);

    for (int i = 0; i < light->channel_num; i++) {
        if (light->channel_group[i] != NULL) {
            light_channel_delete(light->channel_group[i]);
        }
    }
    for (int i = 0; i < LIGHT_NUM_MAX; i++) {
        if (g_light_group[i] == light) {
            g_light_group[i] = NULL;
            break;
        }
    }
    for (int i = 0; i < LIGHT_NUM_MAX; i++) {
        if (g_light_group[i] != NULL) {
            goto FREE_MEM;
        }
    }
    ledc_fade_func_uninstall();
    g_fade_installed = false;
FREE_MEM:
    free(light_handle);
    return mp_obj_new_int(ESP_OK);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(delete_obj, iot_light_delete);

STATIC mp_obj_t iot_light_channel_regist(mp_uint_t n_args, const mp_obj_t *arg_in)
{
    light_t* light = (light_t*)arg_in[0];
	uint8_t channel_idx = mp_obj_get_int(arg_in[1]);
	gpio_num_t io_num = (gpio_num_t)mp_obj_get_int(arg_in[2]);
	ledc_channel_t channel = (ledc_channel_t)mp_obj_get_int(arg_in[3]);
    POINT_ASSERT(TAG, light);
    IOT_CHECK(TAG, channel_idx < light->channel_num, FAIL);

    if (light->channel_group[channel_idx] != NULL) {
        ESP_LOGE(TAG, "this channel index has been registered");
        return ESP_FAIL;
    }
    light->channel_group[channel_idx] = light_channel_create(io_num, channel, light->mode, light->ledc_timer);
    if (g_fade_installed == false) {
        ledc_fade_func_install(0);
        g_fade_installed = true;
    }
    return mp_obj_new_int(ESP_OK);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(channel_regist_obj, 4, iot_light_channel_regist);

STATIC mp_obj_t iot_light_duty_write(mp_uint_t n_args, const mp_obj_t *arg_in)
{
    light_t* light = (light_t*)arg_in[0];
	uint8_t channel_id = mp_obj_get_int(arg_in[1]);
	uint32_t duty = mp_obj_get_int(arg_in[2]);
	light_duty_mode_t duty_mode = (light_duty_mode_t)mp_obj_get_int(arg_in[3]);
    POINT_ASSERT(TAG, light);
    IOT_CHECK(TAG, channel_id < light->channel_num, ESP_FAIL);
    POINT_ASSERT(TAG, light->channel_group[channel_id]);
    light_channel_t* l_chn = light->channel_group[channel_id];

    if(l_chn->timer != NULL) {
        xTimerStop(l_chn->timer, portMAX_DELAY);
    }
    IOT_CHECK(TAG, duty_mode < LIGHT_DUTY_FADE_MAX, ESP_FAIL);
    switch (duty_mode) {
        case LIGHT_SET_DUTY_DIRECTLY:
            ledc_set_duty(l_chn->mode, l_chn->channel, duty);
            ledc_update_duty(l_chn->mode, l_chn->channel);
            break;
        default:
            ledc_set_fade_with_time(l_chn->mode, l_chn->channel, duty, duty_mode * 1000);
            ledc_fade_start(l_chn->mode, l_chn->channel, LEDC_FADE_NO_WAIT);
            break;
    }
    return mp_obj_new_int(ESP_OK);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR(duty_write_obj, 4, iot_light_duty_write);

STATIC mp_obj_t iot_light_breath_write(mp_obj_t light_handle, mp_obj_t channel_id_obj, mp_obj_t breath_period_ms_obj)
{
    light_t* light = (light_t*)light_handle;
	uint8_t channel_id = mp_obj_get_int(channel_id_obj);
	int breath_period_ms = mp_obj_get_int(breath_period_ms_obj);
    POINT_ASSERT(TAG, light_handle);
    IOT_CHECK(TAG, channel_id < light->channel_num, FAIL);
    POINT_ASSERT(TAG, light->channel_group[channel_id]);
    light_channel_t* l_chn = light->channel_group[channel_id];

    if (l_chn->breath_period != breath_period_ms) {
        if(l_chn->timer != NULL) {
            xTimerDelete(l_chn->timer, portMAX_DELAY);
        }
        l_chn->timer = xTimerCreate("light_breath", (breath_period_ms / 2) / portTICK_PERIOD_MS, pdTRUE, (void*) 0, breath_timer_callback);
    }
    l_chn->breath_period = breath_period_ms;
    ledc_set_duty(l_chn->mode, l_chn->channel, 0);
    ledc_update_duty(l_chn->mode, l_chn->channel);
    POINT_ASSERT(TAG, light->channel_group[channel_id]->timer);
    xTimerStart(l_chn->timer, portMAX_DELAY);
    ledc_set_fade_with_time(l_chn->mode, l_chn->channel, light->full_duty, breath_period_ms / 2);
    l_chn->next_duty = 0;
    ledc_fade_start(l_chn->mode, l_chn->channel, LEDC_FADE_NO_WAIT);
    return mp_obj_new_int(ESP_OK);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(breath_write_obj, iot_light_breath_write);

STATIC mp_obj_t iot_light_blink_starte(mp_obj_t light_handle, mp_obj_t channel_mask_obj, mp_obj_t period_ms_obj)
{
    light_t* light = (light_t*)light_handle;
	uint32_t channel_mask = mp_obj_get_int(channel_mask_obj);
	uint32_t period_ms = mp_obj_get_int(period_ms_obj);
    POINT_ASSERT(TAG, light_handle);
    IOT_CHECK(TAG, period_ms > 0 && period_ms <= 1000, ESP_FAIL);
    ledc_timer_config_t timer_conf = {
        .timer_num = light->ledc_timer,
        .speed_mode = light->mode,
        .freq_hz = 1000 / period_ms,
        .duty_resolution = LEDC_TIMER_10_BIT,
    };
    ERR_ASSERT(TAG, ledc_timer_config( &timer_conf), ESP_FAIL);
	mp_obj_t params[4];
	params[0] = light_handle;
	params[3] = mp_obj_new_int(LIGHT_SET_DUTY_DIRECTLY);

    for (int i = 0; i < light->channel_num; i++) {
        if (light->channel_group[i] != NULL) {
			params[1] = mp_obj_new_int(i);
            if (light->channel_group[i]->timer != NULL) {
                xTimerStop(light->channel_group[i]->timer, portMAX_DELAY);
            }
            if (channel_mask & 1<<i) {
				params[2] = mp_obj_new_int((1 << LEDC_TIMER_10_BIT) / 2);
                iot_light_duty_write(sizeof(params), params);
            } else {
				params[2] = mp_obj_new_int(0);
                iot_light_duty_write(sizeof(params), params);
            }
        }
    }
    return mp_obj_new_int(ESP_OK);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(blink_starte_obj, iot_light_blink_starte);

STATIC mp_obj_t iot_light_blink_stop(mp_obj_t light_handle)
{
    light_t* light = (light_t*)light_handle;
    POINT_ASSERT(TAG, light_handle);
    ledc_timer_config_t timer_conf = {
        .timer_num = light->ledc_timer,
        .speed_mode = light->mode,
        .freq_hz = light->freq_hz,
        .duty_resolution = light->timer_bit,
    };
    ERR_ASSERT(TAG, ledc_timer_config( &timer_conf), ESP_FAIL);
	mp_obj_t params[4];
	params[0] = light_handle;
	params[2] = mp_obj_new_int(0);
	params[3] = mp_obj_new_int(LIGHT_SET_DUTY_DIRECTLY);

    for (int i = 0; i < light->channel_num; i++) {
		params[1] = mp_obj_new_int(i);
        iot_light_duty_write(sizeof(params), params);
    }
    return mp_obj_new_int(ESP_OK);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(blink_stop_obj, iot_light_blink_stop);

STATIC const mp_rom_map_elem_t module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_iot_light) },
    { MP_ROM_QSTR(MP_QSTR_control), MP_ROM_PTR(&control_obj) },
    { MP_ROM_QSTR(MP_QSTR_create), MP_ROM_PTR(&create_obj) },
    { MP_ROM_QSTR(MP_QSTR_delete), MP_ROM_PTR(&delete_obj) },
    { MP_ROM_QSTR(MP_QSTR_channel_regist), MP_ROM_PTR(&channel_regist_obj) },
    { MP_ROM_QSTR(MP_QSTR_duty_write), MP_ROM_PTR(&duty_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_breath_write), MP_ROM_PTR(&breath_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_blink_starte), MP_ROM_PTR(&blink_starte_obj) },
    { MP_ROM_QSTR(MP_QSTR_blink_stop), MP_ROM_PTR(&blink_stop_obj) },
};
STATIC MP_DEFINE_CONST_DICT(module_globals, module_globals_table);

// Define module object.
const mp_obj_module_t iot_light_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&module_globals,
};

// Register the module to make it available in Python.
// Note: the "1" in the third argument means this module is always enabled.
// This "1" can be optionally replaced with a macro like MODULE_CEXAMPLE_ENABLED
// which can then be used to conditionally enable this module.
MP_REGISTER_MODULE(MP_QSTR_iot_light, iot_light_module, CONFIG_IOT_LIGHT_ENABLE);
