# PT100：U16 USB Wi-Fi 与 ADB

`WIFI驱动/PT100-M-V1020261026.pdf` 第 7、13、15 页：U7（SGM7227）把同一对 USB 线切给 J1（S=0）或 U16（S=1）。U24 的 PB2 控制 `USB_CHANGE`（寄存器 `0x02`）；PA4 控制 `WIFI_CHIP_EN`（`0x05`），接到 `WIFI_OFF`。按 Q7（P 沟道 MOS）供电方案，低电平给模块供电；若贴 R97 直供，则软件不能断电。

默认从 Linux I²C0 写 MCU 的 **7 位地址 `0x1c`**，与当前 PT100 小核屏幕代码一致。随附的 `屏幕/扩展控制氛围灯加密操作说明.c` 记为 `0x30`。两者不一致，不要同时向两个地址写入；更换 MCU 固件时先确认实际地址。板上已确认是 `0x30` 时，可从串口执行 `PT100_MCU_ADDR=0x30 /etc/pt100-usb-role host` 单独验证，再修改板级脚本默认值。

## 构建

使用 SDK 的 `cv1842cp_pt100_spinand` 与 Sophcam 的 `cv1842cp_pt100_defconfig`。按常规流程编译、烧录即可，不需要 `WIFI驱动/` 目录、下载驱动或单独执行驱动编译命令。升级时同步更新 BOOT、ROOTFS 和 DATA 中的配套 Sophcam；旧应用会把控制器切回 device。

按 BSP README 的常规 SDK 初始化步骤应用 `patches/` 并同步板级目录。`build_all` 先编内核，再由 `build_osdrv` 编译 `osdrv/extdrv/wireless/realtek/rtl8188fu_v515/`（原厂 `v5.15.23-0-g7e9d2525e.20250718`），将本次生成的 `8188fu.ko` 安装到 `/system/ko/3rd/`。仓库不需要预编译模块。

板级 `CONFIG_CP_EXT_WIRELESS=y` 打开外置无线模块编译和安装；内核 `CONFIG_RTL8188FU_OSDRV=m` 选用 U16。旧 USB 选项 `CONFIG_RTL8188FU` 同时用于内核自带驱动，必须关闭以避免同名模块；SDIO `CONFIG_RTL8189FS` 也关闭。其他板型不启用新选项时不会选中新驱动。

## 板上验收（串口）

拔下 USB 数据线并在系统设置打开 Wi-Fi 后执行：

```sh
cat /proc/cviusb/otg_role                    # host
lsusb                                       # 常规 RTL8188FU：0bda:f179
lsmod | grep -E '8188fu|8189fs'              # 只有 8188fu
readlink -f /sys/class/net/wlan0/device     # 应位于 USB 总线上
wpa_cli -i wlan0 scan                       # STA 模式下发起扫描
wpa_cli -i wlan0 scan_results               # 扫描结束后查看结果
```

还需通过应用验证开启热点、手机连接并取得地址，以及关闭热点后 STA 恢复。若没有 USB 枚举，先检查 MCU 地址、U7 的 S 电平和 U16 的 3.3 V，不要先改无线配置。

**USB 优先，在线切换。** 检测到外接 VBUS 时，无论 Wi-Fi 开关如何，均使用外接 USB（ADB）。拔线后只在系统设置的 Wi-Fi 开关打开时启用 U16；开关关闭则两者均不启动。未设置时默认关闭。插线期间也能预设 Wi-Fi，不会抢占 USB。检测依据是 VBUS 电平，因此只供电的充电线也会占用此模式，但不提供 ADB 数据连接。

串口运行 `pt100-mode status` 可查看 VBUS、目标模式和实际模式（`usb` / `wifi` / `off`）。旧 `usb-adb`、`usb-auto` 标记不再生效。
