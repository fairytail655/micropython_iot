# ESP-IOT-Solution in Micropython

本项目将 ESP-IOT-Solution 的部分组件制作为 Micropython 下的 C 模组, 实现通过 python 调用相关函数功能.

目前实现的组件如下:   

* led / light

## 环境

* Ubuntu 18.04

* ESP-IDF v4.3

* micropython v1.16

* ESP-iot-solution master

## 生成固件

1. 下载源码到 **micropython** 目录下 ***ports/esp32*** 的文件夹中.
	 ```shell
	 cd micropython/ports/esp32
	 git clone https://github.com/fairytail655/micropython_iot/tree/master
	```

2. 在 ***micropython/ports/esp32/main*** 目录下新建文件 `Kconfig.projbuild` , 内容如下:

   ```make
   menu "IoT-Solution Configuration"
   
       config IOT_LIGHT_ENABLE
           default y
           bool "Enable IoT light functionality."
   
       menu "GPIO num"
           visible if IOT_LIGHT_ENABLE
   
           orsource "../micropython_iot/iot_light/Kconfig.in"
       endmenu
   
   endmenu
   ```

3. 在 ***micropython/ports/esp32*** 目录下执行命令 `idf.py menuconfig` 可以对工程进行配置.

4. 进入 ***micropython/ports/esp32*** 目录, 使用 `make` 或 `idf.py` 均可实现固件的编译和烧录.

   * **make**

     * 编译, 需要指定 **USER_C_MODULES** 宏为 `micropython_iot/micropython.cmake` 文件路径(最好采用绝对路径), 如:

         ```shell
     make USER_C_MODULES=/home/user/Desktop/micropython/ports/esp32/micropython_iot/micropython.cmake
         ```

     * 烧录, 执行 `make deploy`.

   * **idf.py**

     * 编译, 需要指定 **USER_C_MODULES** 宏为 `micropython_iot/micropython.cmake` 文件路径(最好采用绝对路径), 如:

         ```shell
     idf.py -DUSER_C_MODULES=/home/user/Desktop/micropython/ports/esp32/micropython_iot/micropython.cmake build
         ```

     * 烧录, 执行 `idf.py flash`.

## 运行脚本

```shell
ampy --port /dev/ttyUSB0 run [file_path]    # file_path 为 micropython_iot/iot_light/example.py 的路径
```

