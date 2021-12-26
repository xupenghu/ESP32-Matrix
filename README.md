# ESP32-Matrix
ESP32 | Matrix | FFT | Music

## 简介

## 下载ESP-ADF

本项目使用esp-adf v2.3 release版本,下载esp-adf源码是为了更好的分析代码。

```shell
git clone https://github.com/espressif/esp-adf.git esp-adf-v2.3
cd esp-adf-v2.3/
git checkout v2.3
git submodule update --init --recursive
```

## 编译

使用docker编译，请先安装docker环境到你的电脑，具体的安装教程，请点击[这里](https://blog.csdn.net/u014421520/article/details/120172516)。

```shell
docker pull hubertxxu/esp32_adf_build:0.1
```

## 工程简介

| 工程名                                         | 作用                           |
| ---------------------------------------------- | ------------------------------ |
| [part0_hardware](./part0_hardware)             | 硬件设计（原理图和PCB文件）    |
| [part1_hello_world](./part1_hello_world)       | 开发、编译、烧录环境确认       |
| [part2_matrix_display](./part2_matrix_display) | 点阵屏驱动测试、点阵屏坏点检测 |
| [part3_fft](./part3_fft)                       | FFT 测试                       |





























