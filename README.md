# H8.PIPELINE
High-performance, optimized pre-trained template AI application pipelines for systems using Hailo devices. Based on TAPPAS

## INSTALATION
### Requirements
- [HailoRT driver](https://github.com/hailo-ai/hailort-drivers)
- [HailoRT lib](https://github.com/hailo-ai/hailort)
- [TAPPAS](https://github.com/hailo-ai/tappas)
- [OpenCV 4.x.x](https://docs.opencv.org/4.x/d7/d9f/tutorial_linux_install.html)
> Tested on Rockchip RK3588 | Debian 12
```bash
git clone https://github.com/LagushkaLaga/H8.PIPELINE
```


## DRIVER LAUNCH
```bash
sudo bash driver-load.sh
```

## BUILD
```bash
bash build.sh
```

## CONFIGURE RTPS FILE
```bash
nano ./detection/rtps.txt
```
> Formating file be like:
> $name$ $ip$
> (see rtps.txt for more)

## SETUP DIRECTORIES
```bash
bash setup-dir.sh
```

## LAUNCH
```bash
sudo bash launch.sh
```
