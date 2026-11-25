#include "platform.h"
#include <drv/pin.h>
#include <pinctrl-mars.h>
#include "cvi_type.h"
#include "iic_recovery.h"
#include <drv/iic.h>

#define GPIO_PIN_MASK(_gpio_num) (1 << _gpio_num)

/*
 * 屏幕的复位与背光由外置 MCU(CIU32F003) 控制，小核通过 IIC0 写其寄存器。
 * 7 位从机地址 0x30，总线写地址字节 0x60。
 * 参考 adoc/panel/扩展控制氛围灯加密操作说明.c 的 IO 扩展寄存器定义。
 */
#define MCU_IIC_IDX             0
#define MCU_IIC_ADDR            0x1c
#define MCU_IIC_TIMEOUT_MS      1000

#define MCU_IO_REG_TP_RST       0x06
#define MCU_IO_REG_LCD_RST      0x07
#define MCU_IO_REG_LCD_BL_PWM   0x08

/* 启动时默认背光亮度，MCU 按 0~100 百分比处理 */
#define PANEL_BACKLIGHT_DEFAULT 80

static csi_iic_t g_mcu_iic;
static int g_mcu_iic_ready;

static void _McuIicPinmux(void)
{
	PINMUX_CONFIG(IIC0_SCL, CV_SCL0__CR_4WTDI);
	PINMUX_CONFIG(IIC0_SDA, CV_SDA0__CR_4WTDO);
}

static int _McuIicInit(void)
{
	if (g_mcu_iic_ready) {
		return 0;
	}

	_McuIicPinmux();

	if (csi_iic_init(&g_mcu_iic, MCU_IIC_IDX) != CSI_OK) {
		printf("mcu iic init failed\r\n");
		return -1;
	}
	if (csi_iic_mode(&g_mcu_iic, IIC_MODE_MASTER) != CSI_OK) {
		printf("mcu iic mode failed\r\n");
		goto fail;
	}
	if (csi_iic_addr_mode(&g_mcu_iic, IIC_ADDRESS_7BIT) != CSI_OK) {
		printf("mcu iic addr mode failed\r\n");
		goto fail;
	}
	if (csi_iic_speed(&g_mcu_iic, IIC_BUS_SPEED_FAST) != CSI_OK) {
		printf("mcu iic speed failed\r\n");
		goto fail;
	}

	g_mcu_iic_ready = 1;
	return 0;

fail:
	csi_iic_uninit(&g_mcu_iic);
	return -1;
}

static int _McuWriteReg(u8 reg, u8 value)
{
	u8 buf[2];
	int32_t ret;

	if (_McuIicInit() != 0) {
		return -1;
	}

	buf[0] = reg;
	buf[1] = value;
	ret = csi_iic_master_send(&g_mcu_iic, MCU_IIC_ADDR, buf, sizeof(buf), MCU_IIC_TIMEOUT_MS);
	if (ret != (int32_t)sizeof(buf)) {
		printf("mcu write reg 0x%02x failed, ret=%d\r\n", reg, (int)ret);
		return -1;
	}

	return 0;
}

/* 数字输出寄存器：0x00 输出低电平，非 0x00 输出高电平 */
static int _McuSetIo(u8 reg, u8 level)
{
	return _McuWriteReg(reg, level ? 1 : 0);
}

void _GPIOSetValue(u8 gpio_grp, u8 gpio_num, u8 level)
{
	csi_error_t ret;
	csi_gpio_t gpio = {0};

	ret = csi_gpio_init(&gpio, gpio_grp);
	if(ret != CSI_OK) {
		printf("csi_gpio_init failed\r\n");
		return;
	}
	// gpio write
	ret = csi_gpio_dir(&gpio , GPIO_PIN_MASK(gpio_num), GPIO_DIRECTION_OUTPUT);

	if(ret != CSI_OK) {
		printf("csi_gpio_dir failed\r\n");
		return;
	}
	csi_gpio_write(&gpio , GPIO_PIN_MASK(gpio_num), level);
	//printf("test pin end and success.\r\n");
}
void PLATFORM_SpkMute(int value)
{
	static int run_once = 0;
	u8 gpio_spken_r_grp = 4;
	u8 gpio_spken_r_num = 2;
#if defined(CONFIG_CHIP_cv1842hp) || defined(CONFIG_CHIP_cv1843hp) || defined(CONFIG_CHIP_cv1841h)
	u8 gpio_spken_l_grp;
	u8 gpio_spken_l_num;
#endif

	if(run_once) {
		return;
	}
	run_once = 1;

#if defined(CONFIG_CHIP_cv1841c) || defined(CONFIG_CHIP_cv1842cp)
	gpio_spken_r_grp = 4;
	gpio_spken_r_num = 2;
#endif

#if defined(CONFIG_CHIP_cv1842hp) || defined(CONFIG_CHIP_cv1843hp) || defined(CONFIG_CHIP_cv1841h)
	gpio_spken_r_grp = 0;
	gpio_spken_r_num = 30;
	gpio_spken_l_grp = 0;
	gpio_spken_l_num = 15;
#endif
//0静音 ，1非静音
    if(value){
        _GPIOSetValue(gpio_spken_r_grp, gpio_spken_r_num, 1);
#if defined(CONFIG_CHIP_cv1842hp) || defined(CONFIG_CHIP_cv1843hp) || defined(CONFIG_CHIP_cv1841h)
        _GPIOSetValue(gpio_spken_l_grp, gpio_spken_l_num, 1);
#endif
    }else{
        _GPIOSetValue(gpio_spken_r_grp, gpio_spken_r_grp, 0);
#if defined(CONFIG_CHIP_cv1842hp) || defined(CONFIG_CHIP_cv1843hp) || defined(CONFIG_CHIP_cv1841h)
        _GPIOSetValue(gpio_spken_l_grp, gpio_spken_l_num, 0);
#endif
    }
}
static void _AudioPinmux(void)
{

#if defined(CONFIG_CHIP_cv1841c) || defined(CONFIG_CHIP_cv1842cp)
    PINMUX_CONFIG(PWR_GPIO2, PWR_GPIO_2);
#endif
#if defined(CONFIG_CHIP_cv1842hp) || defined(CONFIG_CHIP_cv1843hp) || defined(CONFIG_CHIP_cv1841h)
    PINMUX_CONFIG(SPK_EN, XGPIOA_15);
    PINMUX_CONFIG(AUX0, XGPIOA_30);
#endif
	PLATFORM_SpkMute(0);
}
static void _UartPinmux()
{
    // uart2 pinmux
    PINMUX_CONFIG(UART2_TX, UART2_TX);
    PINMUX_CONFIG(UART2_RX, UART2_RX);

    // uart1 pinmux
    // PINMUX_CONFIG(IIC0_SCL, UART1_TX);
    // PINMUX_CONFIG(IIC0_SDA, UART1_RX);

    // uart0 pinmux
    // PINMUX_CONFIG(UART0_TX, UART0_TX);
    // PINMUX_CONFIG(UART0_RX, UART0_RX);
}

static void _SensorPinmux()
{
	// PWR_SEQ2 pinmux unlock
	printf("PWR_SEQ2 pinmux unlock\n");
	mmio_write_32(0x0502707C, 0x11);
	PINMUX_CONFIG(PWR_SEQ2, PWR_GPIO_4); // sensor_RST

	//Sensor Pinmux
#if defined (CONFIG_CHIP_cv1811c) || defined (CONFIG_CHIP_cv1801c) || defined (CONFIG_CHIP_cv1812cp)
	PINMUX_CONFIG(PAD_MIPI_TXP1, IIC2_SCL);
	PINMUX_CONFIG(PAD_MIPI_TXM1, IIC2_SDA);
	PINMUX_CONFIG(PAD_MIPI_TXM0, CAM_MCLK1);
#endif
#if defined (CONFIG_CHIP_cv1811h) || defined (CONFIG_CHIP_cv1812h) || defined (CONFIG_CHIP_cv1811ha)|| defined (CONFIG_CHIP_cv1812ha) || defined (CONFIG_CHIP_cv1813h)
	// PINMUX_CONFIG(IIC2_SCL, IIC2_SCL);
	// PINMUX_CONFIG(IIC2_SDA, IIC2_SDA);

	// PINMUX_CONFIG(IIC3_SCL, IIC3_SCL);
	// PINMUX_CONFIG(IIC3_SDA, IIC3_SDA);
	PINMUX_CONFIG(CAM_MCLK0, CAM_MCLK0);

	// PINMUX_CONFIG(IIC2_SCL, IIC2_SCL);
	// PINMUX_CONFIG(IIC2_SDA, IIC2_SDA);
	PINMUX_CONFIG(CAM_RST0, XGPIOA_2);
	PINMUX_CONFIG(IIC3_SCL, IIC3_SCL);
	PINMUX_CONFIG(IIC3_SDA, IIC3_SDA);
#endif
#if defined (CONFIG_CHIP_cv1801b) || defined (CONFIG_CHIP_cv180zb)
	PINMUX_CONFIG(PAD_MIPIRX0N, IIC1_SCL);
	PINMUX_CONFIG(PAD_MIPIRX1P, IIC1_SDA);

#if 0 //evb mipi switch
	PINMUX_CONFIG(PAD_MIPIRX1N, XGPIOC_8);
#endif

	PINMUX_CONFIG(PAD_MIPIRX0P, CAM_MCLK0);
#endif
}

static void _MipiRxPinmux(void)
{
//mipi rx pinmux
#if 0 //need porting for cv180x
    PINMUX_CONFIG(PAD_MIPIRX4P, XGPIOC_3);
    PINMUX_CONFIG(PAD_MIPIRX4N, XGPIOC_2);
#endif
}

static void _MipiTxPinmux(void)
{
//mipi tx pinmux
#if CONFIG_PANEL_ILI9488
	PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
	PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
	PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
	PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
	PINMUX_CONFIG(IIC0_SCL, XGPIOA_28);
#elif (CONFIG_PANEL_HX8394)
#if CONFIG_BOARD_CV181XC || CONFIG_BOARD_CV184X
	PINMUX_CONFIG(PAD_MIPI_TXM0, XGPIOC_12);
	PINMUX_CONFIG(PAD_MIPI_TXP0, XGPIOC_13);
	PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
	PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
	PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
	PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
	PINMUX_CONFIG(JTAG_CPU_TCK, XGPIOA_18);
	PINMUX_CONFIG(JTAG_CPU_TMS, XGPIOA_19);
	PINMUX_CONFIG(SPK_EN, XGPIOA_15);
#elif defined(__CV181X__)
	PINMUX_CONFIG(PAD_MIPI_TXM0, XGPIOC_12);
	PINMUX_CONFIG(PAD_MIPI_TXP0, XGPIOC_13);
	PINMUX_CONFIG(PAD_MIPI_TXM1, XGPIOC_14);
	PINMUX_CONFIG(PAD_MIPI_TXP1, XGPIOC_15);
	PINMUX_CONFIG(PAD_MIPI_TXM2, XGPIOC_16);
	PINMUX_CONFIG(PAD_MIPI_TXP2, XGPIOC_17);
	PINMUX_CONFIG(PAD_MIPI_TXM3, XGPIOC_20);
	PINMUX_CONFIG(PAD_MIPI_TXP3, XGPIOC_21);
	PINMUX_CONFIG(PAD_MIPI_TXM4, XGPIOC_18);
	PINMUX_CONFIG(PAD_MIPI_TXP4, XGPIOC_19);
#endif
#endif
}

#if (CONFIG_APP_DEBUG_JTAG == 1)
void JTAG_PinmuxIn()
{
    PINMUX_CONFIG(IIC0_SDA, CV_SDA0__CR_4WTDO);
    PINMUX_CONFIG(IIC0_SCL, CV_SCL0__CR_4WTDI);
}
#endif

static void i2c1_pinmux_config_gpio(void)
{
	PINMUX_CONFIG(PAD_ETH_TXP, XGPIOB_25); // IIC1_SCL
	PINMUX_CONFIG(PAD_ETH_TXM, XGPIOB_24); // IIC1_SDA
}

static void i2c1_pinmux_config_iic(void)
{
	PINMUX_CONFIG(PAD_ETH_TXP, IIC1_SCL); // GPIOB[25]
	PINMUX_CONFIG(PAD_ETH_TXM, IIC1_SDA); // GPIOB[24]
}

static void i2c1_recovery_config(void)
{
	PINMUX_CONFIG(PAD_ETH_TXP, IIC1_SCL); // GPIOB[25]
	PINMUX_CONFIG(PAD_ETH_TXM, IIC1_SDA); // GPIOB[24]

	iic_recovery_config_t i2c1_recovery = {
		.enable = 1,
		.scl_gpio_grp = 1,
		.scl_gpio_num = 25,
		.sda_gpio_grp = 1,
		.sda_gpio_num = 24,
		.config_gpio_mode = i2c1_pinmux_config_gpio,
		.config_iic_mode = i2c1_pinmux_config_iic,
	};

	csi_iic_set_recovery_config(1, &i2c1_recovery);
}

void PLATFORM_IoInit(void)
{
	//pinmux 切换接口
	_UartPinmux();
	_MipiRxPinmux();
	_MipiTxPinmux();
	_SensorPinmux();
	_AudioPinmux();
	#if (CONFIG_APP_DEBUG_JTAG == 1)
	JTAG_PinmuxIn();
	#endif
	i2c1_recovery_config();

	PINMUX_CONFIG(CAM_MCLK0, CAM_MCLK0);
	PINMUX_CONFIG(IIC3_SCL, IIC3_SCL);
	PINMUX_CONFIG(IIC3_SDA, IIC3_SDA);

#if 0 //evb mipi switch
	PINMUX_CONFIG(SD1_CMD, IIC3_SCL);
	PINMUX_CONFIG(SD1_CLK, IIC3_SDA);
	PINMUX_CONFIG(ADC1, PWM_3);
#endif
}

void PLATFORM_PowerOff(void)
{
//下电休眠前调用接口
}

void _PanelPinmux(void)
{
#if CONFIG_PANEL_BJX2416F0
	// 复位与背光由 MCU 控制，IIC0 引脚在 _McuIicInit 里配置
#else
	// LCD_RESET SPK-EN
	PINMUX_CONFIG(JTAG_CPU_TCK, PWM_6); // LCD_BL
#endif
}

#ifdef POWER_KEY_CHECK
// 检测是否是看门狗或reboot触发的开机
static bool _IsRebootOrWatchdogWakeup(void)
{
	// bit26: REBOOT flag, bit27: WATCHDOG flag
	return ((mmio_read_32(0x050260f8) >> 26) & 0x3) != 0;
}

// 检测PWR_BUTTON1是否按下
// 返回: true-按键按下, false-按键未按下
static bool _IsPowerButtonPressed(void)
{
	// PWR_GPIO6 INPUT MODE
	mmio_write_32(0x05021004, mmio_read_32(0x05021004) & 0xFFFFFFBF);
	// DETECT PWR_WAKEUP0 LEVEL
	uint32_t key_value = mmio_read_32(0x05021050) & 0x40;
	return key_value != 0;
}

static void PowerKeyCheck(void)
{
	if (!_IsPowerButtonPressed()) {
		// 未按下电源键，走 poweroff 流程
		mmio_write_32(0x050260c0, 0x1); // 使能软件请求下电
		while (mmio_read_32(0x050260c0) != 0x1)
			;
		mmio_write_32(0x05025004, 0xab18); // 解锁对 RTC_CTRL0(0x05025008) 的读写
		while (1) {
			mmio_write_32(0x05025008, 0x10001); // 请求下电
		}
	}
}
#endif

int PLATFORM_PanelInit(void)
{
	// 看门狗或reboot触发的开机，不检测按键，直接继续启动
#ifdef POWER_KEY_CHECK
	if (!_IsRebootOrWatchdogWakeup()) {
		PowerKeyCheck();
	}
#endif
	_PanelPinmux();
#if (!defined(CONFIG_SUPPORT_VO) || (CONFIG_SUPPORT_VO))
#if CONFIG_PANEL_HX8394
	u8 pw_port, pw_pin, bl_port, bl_pin, rst_port, rst_pin;
	pw_port = 4;
	pw_pin = 1;
	bl_port = 4;
	bl_pin = 0;
	rst_port = 4;
	rst_pin = 2;
	_GPIOSetValue(pw_port, pw_pin, 1);
	_GPIOSetValue(bl_port, bl_pin, 1);
	_GPIOSetValue(rst_port, rst_pin, 1);
	udelay(20 * 1000);
	_GPIOSetValue(rst_port, rst_pin, 0);
	udelay(20 * 1000);
	_GPIOSetValue(rst_port, rst_pin, 1);
	udelay(20 * 1000);
#elif CONFIG_PANEL_OTA7290B
	u8 pw_port, pw_pin, bl_port, bl_pin, rst_port, rst_pin;
	pw_port = 0;
	pw_pin = 30;
	bl_port = 4;
	bl_pin = 0;
	rst_port = 0;
	rst_pin = 20;
	_GPIOSetValue(pw_port, pw_pin, 1);
	_GPIOSetValue(bl_port, bl_pin, 1);
	_GPIOSetValue(rst_port, rst_pin, 1);
	udelay(20 * 1000);
	_GPIOSetValue(rst_port, rst_pin, 0);
	udelay(20 * 1000);
	_GPIOSetValue(rst_port, rst_pin, 1);
	udelay(20 * 1000);
#elif CONFIG_PANEL_BJX2416F0
	// LCD_RST(PA0) 由 MCU 的 0x07 寄存器控制
	_McuSetIo(MCU_IO_REG_LCD_RST, 1);
	udelay(20 * 1000);
	_McuSetIo(MCU_IO_REG_LCD_RST, 0);
	udelay(20 * 1000);
	_McuSetIo(MCU_IO_REG_LCD_RST, 1);
	udelay(20 * 1000);
	// 启动时把背光设为默认亮度
	PLATFORM_PanelBacklightCtl(PANEL_BACKLIGHT_DEFAULT);
#elif CONFIG_PANEL_BJX2836F0
	u8 rst_port = 0, rst_pin = 15;
	_GPIOSetValue(rst_port, rst_pin, 1);
	udelay(20 * 1000);
	_GPIOSetValue(rst_port, rst_pin, 0);
	udelay(20 * 1000);
	_GPIOSetValue(rst_port, rst_pin, 1);
	udelay(20 * 1000);
#endif
#endif

    return CVI_SUCCESS;
}

void PLATFORM_PanelBacklightCtl(int level)
{
#if CONFIG_PANEL_BJX2416F0
	// LCD_BL_PWM(PA7)：0~100，超过 100 由 MCU 按 100 处理
	if (level < 0) {
		level = 0;
	} else if (level > 100) {
		level = 100;
	}
	_McuWriteReg(MCU_IO_REG_LCD_BL_PWM, (u8)level);
#else
	(void)level;
#endif
}

int PLATFORM_IrCutCtl(int duty)
{
    return 0;
}
