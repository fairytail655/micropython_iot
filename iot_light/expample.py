import iot_light
import time

CHANNEL_ID_R = 0
CHANNEL_ID_G = 1
CHANNEL_ID_B = 2
CHANNEL_R_IO = 25
CHANNEL_G_IO = 26
CHANNEL_B_IO = 27

LEDC_TIMER_0 = 0
LEDC_HIGH_SPEED_MODE = 0
LEDC_TIMER_13_BIT = 13
LEDC_CHANNEL_0 = 0
LEDC_CHANNEL_1 = 1
LEDC_CHANNEL_2 = 2

LIGHT_FULL_DUTY = (1 << LEDC_TIMER_13_BIT) - 1
LIGHT_DUTY_FADE_2S = 2

TAG = "light test"


light = iot_light.create(LEDC_TIMER_0, LEDC_HIGH_SPEED_MODE, 1000, 3, LEDC_TIMER_13_BIT)

iot_light.channel_regist(light, CHANNEL_ID_R, CHANNEL_R_IO, LEDC_CHANNEL_0)
iot_light.channel_regist(light, CHANNEL_ID_G, CHANNEL_G_IO, LEDC_CHANNEL_1)
iot_light.channel_regist(light, CHANNEL_ID_B, CHANNEL_B_IO, LEDC_CHANNEL_2)

print("(%s) stage1" % TAG)
iot_light.duty_write(light, CHANNEL_ID_R, LIGHT_FULL_DUTY, LIGHT_DUTY_FADE_2S)
iot_light.breath_write(light, CHANNEL_ID_R, 4000)
iot_light.breath_write(light, CHANNEL_ID_B, 8000)
time.sleep(5)

print("(%s) stage2" % TAG)
iot_light.blink_starte(light, (1<<CHANNEL_ID_R)|(1<<CHANNEL_ID_G)|(1<<CHANNEL_ID_B), 100)
time.sleep(5)
iot_light.blink_stop(light)

print("(%s) stage3" % TAG)
iot_light.duty_write(light, CHANNEL_ID_R, LIGHT_FULL_DUTY, LIGHT_DUTY_FADE_2S)
iot_light.duty_write(light, CHANNEL_ID_G, LIGHT_FULL_DUTY // 8, LIGHT_DUTY_FADE_2S)
iot_light.duty_write(light, CHANNEL_ID_B, LIGHT_FULL_DUTY // 4, LIGHT_DUTY_FADE_2S)
time.sleep(5)

iot_light.delete(light)
