// SPDX-License-Identifier: GPL-2.0+
//
// max77693_charger.c - Battery charger driver for the Maxim 77693
//
// Copyright (C) 2014 Samsung Electronics
// Krzysztof Kozlowski <krzk@kernel.org>

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>

#include <linux/mfd/max77705.h>
#include <linux/mfd/max77705_charger.h>
#include <linux/mfd/max77693-common.h>
#include <linux/mfd/max77705-private.h>

#define MAX77705_CHARGER_NAME				"max77705-charger"
#define MAX77705_CHARGER_USB_CDP_NAME				"max77705-charger-usb-cdc"
static const char *max77705_charger_model		= "max77705";
static const char *max77705_charger_manufacturer	= "Maxim Integrated";

static enum power_supply_property max77705_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
//???	POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};

static enum power_supply_property max77705_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};


/* If we have external wireless TRx, enable WCIN interrupt to detect Mis-align only */
static void wpc_detect_work(struct work_struct *work)
{
	struct max77705_charger_data *charger = container_of(work,
								struct max77705_charger_data,
								wpc_work.work);

	max77705_update_reg(charger->i2c,
			    MAX77705_CHG_REG_INT_MASK, 0, MAX77705_WCIN_IM);

	if (is_wireless_type(charger->cable_type)) {
		u8 reg_data, wcin_state, wcin_dtls, wcin_cnt = 0;

		do {
			wcin_cnt++;
			max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK,
					  &reg_data);
			wcin_state =
			    (reg_data & MAX77705_WCIN_OK) >> MAX77705_WCIN_OK_SHIFT;
			max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_00,
					  &reg_data);
			wcin_dtls =
			    (reg_data & MAX77705_WCIN_DTLS) >> MAX77705_WCIN_DTLS_SHIFT;
			if (!wcin_state && !wcin_dtls && wcin_cnt >= 2) {
				union power_supply_propval value;

				pr_info("%s: invalid WCIN, Misalign occurs!\n",
					__func__);
//				value.intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
//				psy_do_property(charger->pdata->wireless_charger_name,
//					set, POWER_SUPPLY_PROP_STATUS, value);
			}
			mdelay(50);
		} while (!wcin_state && !wcin_dtls && wcin_cnt < 2);
	}

	/* Do unmask again. (for frequent wcin irq problem) */
	max77705_update_reg(charger->i2c,
			    MAX77705_CHG_REG_INT_MASK, 0, MAX77705_WCIN_IM);

	wake_unlock(&charger->wpc_wake_lock);
}

static void max77705_aicl_isr_work(struct work_struct *work)
{
	struct max77705_charger_data *charger = container_of(work,
								struct max77705_charger_data,
								aicl_work.work);
	bool aicl_mode = false;
	u8 aicl_state = 0;
	union power_supply_propval value;

	if (!charger->irq_aicl_enabled || charger->cable_type == SEC_BATTERY_CABLE_NONE) {
		pr_info("%s : skip\n", __func__);
		charger->prev_aicl_mode = aicl_mode = false;
		return;
	}

	mutex_lock(&charger->charger_mutex);
	max77705_update_reg(charger->i2c,
			    MAX77705_CHG_REG_INT_MASK,
			    MAX77705_AICL_IM, MAX77705_AICL_IM);
	/* check and unlock */
	check_charger_unlock_state(charger);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &aicl_state);

	if (!(aicl_state & 0x80)) {
		/* AICL mode */
		pr_info("%s : AICL Mode : CHG_INT_OK(0x%02x), prev_aicl(%d)\n",
			__func__, aicl_state, charger->prev_aicl_mode);
		reduce_input_current(charger, REDUCE_CURRENT_STEP);

		if (is_not_wireless_type(charger->cable_type))
			max77705_check_slow_charging(charger, charger->input_current);

		if ((charger->irq_aicl_enabled == 1) &&
			(charger->input_current <= MINIMUM_INPUT_CURRENT) &&
			(charger->slow_charging)) {
			/* Disable AICL IRQ, no more reduce current */
			u8 reg_data;

			charger->irq_aicl_enabled = 0;
			disable_irq_nosync(charger->irq_aicl);
			max77705_read_reg(charger->i2c,
					  MAX77705_CHG_REG_INT_MASK, &reg_data);
			pr_info("%s : disable aicl : 0x%x\n", __func__, reg_data);
			/* notify aicl current, no more aicl check */
			value.intval = max77705_get_input_current(charger);
//			psy_do_property("battery", set,
//					POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value);
		} else {
			aicl_mode = true;
			queue_delayed_work(charger->wqueue, &charger->aicl_work,
				msecs_to_jiffies(AICL_WORK_DELAY));
		}
	} else {
		/* Not in AICL mode */
		pr_info("%s : Not in AICL Mode : CHG_INT_OK(0x%02x), prev_aicl(%d)\n",
			__func__, aicl_state, charger->prev_aicl_mode);
		if (charger->aicl_on && charger->prev_aicl_mode) {
			/* notify aicl current, if aicl is on and aicl state is cleard */
			value.intval = max77705_get_input_current(charger);
//			psy_do_property("battery", set,
//					POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value);
		}
	}

	charger->prev_aicl_mode = aicl_mode;
	max77705_update_reg(charger->i2c,
			    MAX77705_CHG_REG_INT_MASK, 0, MAX77705_AICL_IM);
	mutex_unlock(&charger->charger_mutex);
}

static void max77705_wc_current_work(struct work_struct *work)
{
	struct max77705_charger_data *charger = container_of(work,
								struct max77705_charger_data,
								wc_current_work.work);
	int diff_current = 0;

	if (is_not_wireless_type(charger->cable_type)) {
		charger->wc_pre_current = WC_CURRENT_START;
		max77705_write_reg(charger->i2c,
				   MAX77705_CHG_REG_CNFG_10, 0x10);
		return;
	}

	if (charger->wc_pre_current == charger->wc_current) {
		union power_supply_propval value;

		max77705_set_charge_current(charger, charger->charging_current);
		/* Wcurr-B) Restore Vrect adj room to previous value */
		/*  after finishing wireless input current setting. Refer to Wcurr-A) step */
		msleep(500);
		if (is_nv_wireless_type(charger->cable_type)) {
//			psy_do_property("battery", get,
//					POWER_SUPPLY_PROP_CAPACITY, value);
//			if (value.intval < charger->pdata->wireless_cc_cv)
//				value.intval = WIRELESS_VRECT_ADJ_ROOM_4;	/* WPC 4.5W, Vrect Room 30mV */
//			else
//				value.intval = WIRELESS_VRECT_ADJ_ROOM_5;	/* WPC 4.5W, Vrect Room 80mV */
//		} else if (is_hv_wireless_type(charger->cable_type)) {
//			value.intval = WIRELESS_VRECT_ADJ_ROOM_5;	/* WPC 9W, Vrect Room 80mV */
//		} else
//			value.intval = WIRELESS_VRECT_ADJ_OFF;	/* PMA 4.5W, Vrect Room 0mV */
//		psy_do_property(charger->pdata->wireless_charger_name, set,
//				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
	} else {
		if (charger->wc_pre_current > charger->wc_current) {
			diff_current = charger->wc_pre_current - charger->wc_current;
			if (diff_current < WC_CURRENT_STEP)
				charger->wc_pre_current -= diff_current;
			else
				charger->wc_pre_current -= WC_CURRENT_STEP;
		} else {
			diff_current = charger->wc_current - charger->wc_pre_current;
			if (diff_current < WC_CURRENT_STEP)
				charger->wc_pre_current += diff_current;
			else
				charger->wc_pre_current += WC_CURRENT_STEP;
		}
		max77705_set_input_current(charger, charger->wc_pre_current);
		queue_delayed_work(charger->wqueue, &charger->wc_current_work,
				   msecs_to_jiffies(WC_CURRENT_WORK_STEP));
	}
	pr_info("%s: wc_current(%d), wc_pre_current(%d), diff(%d)\n",
		__func__, charger->wc_current, charger->wc_pre_current, diff_current);
}

static irqreturn_t wpc_charger_irq(int irq, void *data)
{
	struct max77705_charger_data *charger = data;

	pr_info("%s: irq(%d)\n", __func__, irq);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_INT_MASK,
			    MAX77705_WCIN_IM, MAX77705_WCIN_IM);
	queue_delayed_work(charger->wqueue, &charger->wpc_work,
			   msecs_to_jiffies(10000));

	return IRQ_HANDLED;
}

static irqreturn_t max77705_chg_irq_thread(int irq, void *irq_data)
{
	struct max77705_charger_data *charger = irq_data;

	pr_info("%s: Charger interrupt occurred\n", __func__);

	if ((charger->pdata->full_check_type ==
	     SEC_BATTERY_FULLCHARGED_CHGINT) ||
	    (charger->pdata->ovp_uvlo_check_type ==
	     SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

/* register chgin isr after sec_battery_probe */
static void max77705_chgin_init_work(struct work_struct *work)
{
	struct max77705_charger_data *charger = container_of(work,
								struct max77705_charger_data,
								chgin_init_work.work);
	int ret;

	pr_info("%s\n", __func__);
	ret = request_threaded_irq(charger->irq_chgin, NULL,
				   max77705_chgin_irq, 0, "chgin-irq", charger);
	if (ret < 0) {
		pr_err("%s: fail to request chgin IRQ: %d: %d\n",
		       __func__, charger->irq_chgin, ret);
	} else {
		max77705_update_reg(charger->i2c,
				    MAX77705_CHG_REG_INT_MASK, 0,
				    MAX77705_CHGIN_IM);
	}
}

static int max77705_get_charge_current(struct max77705_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02, &reg_data);
	reg_data &= MAX77705_CHG_CC;

	get_current = reg_data <= 0x2 ? 100 : reg_data * 50;

	pr_info("%s: reg:(0x%x), charging_current:(%d)\n",
			__func__, reg_data, get_current);
	return get_current;
}

static int max77705_get_input_current_type(struct max77705_charger_data
					*charger, int cable_type)
{
	u8 reg_data;
	int get_current = 0;

	if (cable_type == SEC_BATTERY_CABLE_WIRELESS) {
		max77705_read_reg(charger->i2c,
				  MAX77705_CHG_REG_CNFG_10, &reg_data);
		/* AND operation for removing the formal 2bit  */
		reg_data &= MAX77705_CHG_WCIN_LIM;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x3F)
			get_current = 1600;
		else
			get_current = (reg_data + 0x01) * 25;
	} else {
		max77705_read_reg(charger->i2c,
				  MAX77705_CHG_REG_CNFG_09, &reg_data);
		/* AND operation for removing the formal 1bit  */
		reg_data &= MAX77705_CHG_CHGIN_LIM;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x7F)
			get_current = 3200;
		else
			get_current = (reg_data + 0x01) * 25;
	}

	pr_info("%s: reg:(0x%x), charging_current:(%d)\n",
			__func__, reg_data, get_current);

	return get_current;
}

static int max77705_get_input_current(struct max77705_charger_data *charger)
{
	if (is_wireless_type(charger->cable_type))
		return max77705_get_input_current_type(charger,
						       SEC_BATTERY_CABLE_WIRELESS);
	else
		return max77705_get_input_current_type(charger,
						       SEC_BATTERY_CABLE_TA);
}

/*move to debugfs*/
static void max77705_test_read(struct max77705_charger_data *charger)
{
	u8 data = 0;
	u32 addr = 0;
	char str[1024] = { 0, };

	for (addr = 0xB1; addr <= 0xC3; addr++) {
		max77705_read_reg(charger->i2c, addr, &data);
		sprintf(str + strlen(str), "[0x%02x]0x%02x, ", addr, data);
	}
	pr_info("max77705 : %s\n", str);
}

static bool max77705_chg_get_wdtmr_status(struct max77705_charger_data *charger)
{
	u8 reg_data;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77705_CHG_DTLS) >> MAX77705_CHG_DTLS_SHIFT);

	if (reg_data == 0x0B) {
		dev_info(charger->dev, "WDT expired 0x%x !!\n", reg_data);
		return true;
	}

	return false;
}

static int max77705_chg_set_wdtmr_en(struct max77705_charger_data *charger, bool enable)
{
	pr_info("%s: WDT en = %d\n", __func__, enable);

	if (enable) {
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
					CHG_CNFG_00_WDTEN_MASK, CHG_CNFG_00_WDTEN_MASK);
	} else {
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
					0, CHG_CNFG_00_WDTEN_MASK);
	}

	return 0;
}

static int max77705_chg_set_wdtmr_kick(struct max77705_charger_data *charger)
{
	pr_info("%s: WDT Kick\n", __func__);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06,
			    (MAX77705_WDTCLR << CHG_CNFG_01_WDTCLR_SHIFT), CHG_CNFG_01_WDTCLR_MASK);

	return 0;
}

static bool max77705_is_constant_current(struct max77705_charger_data *charger)
{
	u8 reg_data;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &reg_data);
	pr_info("%s : charger status (0x%02x)\n", __func__, reg_data);
	reg_data &= 0x0f;

	if (reg_data == 0x01)
		return true;
	return false;
}

static int max77705_get_float_voltage(struct max77705_charger_data *charger)
{
	u8 reg_data = 0;
	int float_voltage;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_04, &reg_data);
	reg_data &= 0x3F;
	float_voltage =
	    reg_data <=
	    0x04 ? reg_data * 50 + 4000 : (reg_data - 4) * 10 + 4200;
	pr_debug("%s: battery cv reg : 0x%x, float voltage val : %d\n",
		__func__, reg_data, float_voltage);

	return float_voltage;
}

static void max77705_set_float_voltage(struct max77705_charger_data *charger,
					int float_voltage)
{
	u8 reg_data = 0;

#if defined(CONFIG_SEC_FACTORY)
	if (is_muic_jig_301k) {
		float_voltage = 4350;
		pr_info("%s: jig 301k float_voltage(%d)\n", __func__, float_voltage);
	}
#endif
	reg_data = float_voltage <= 4000 ? 0x0 :
	    float_voltage >= 4500 ? 0x23 :
	    (float_voltage <= 4200) ? (float_voltage - 4000) / 50 :
	    (((float_voltage - 4200) / 10) + 0x04);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_04,
			    (reg_data << CHG_CNFG_04_CHG_CV_PRM_SHIFT),
			    CHG_CNFG_04_CHG_CV_PRM_MASK);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_04, &reg_data);
	pr_info("%s: battery cv voltage 0x%x\n", __func__, reg_data);
}

static int max77705_get_charging_health(struct max77705_charger_data *charger)
{
	int state = POWER_SUPPLY_HEALTH_GOOD;
	int vbus_state;
	int retry_cnt;
	u8 chg_dtls, reg_data;
	u8 chg_cnfg_00;
	union power_supply_propval value, val_iin, val_vbyp;

	/* watchdog kick */
	max77705_chg_set_wdtmr_kick(charger);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &reg_data);
	reg_data = ((reg_data & MAX77705_BAT_DTLS) >> MAX77705_BAT_DTLS_SHIFT);

	pr_info("%s: reg_data(0x%x)\n", __func__, reg_data);
	switch (reg_data) {
	case 0x00:
		pr_info("%s: No battery and the charger is suspended\n",
			__func__);
		break;
	case 0x01:
		pr_info("%s: battery is okay but its voltage is low(~VPQLB)\n", __func__);
		break;
	case 0x02:
		pr_info("%s: battery dead\n", __func__);
		break;
	case 0x03:
		break;
	case 0x04:
		pr_info("%s: battery is okay but its voltage is low\n", __func__);
		break;
	case 0x05:
		pr_info("%s: battery ovp\n", __func__);
		break;
	default:
		pr_info("%s: battery unknown\n", __func__);
		break;
	}

	if (charger->is_charging) {
		max77705_read_reg(charger->i2c,
			MAX77705_CHG_REG_DETAILS_00, &reg_data);
		pr_info("%s: details00 (0x%x)\n", __func__, reg_data);
	}

	/* VBUS OVP state return battery OVP state */
	vbus_state = max77705_get_vbus_state(charger);
	/* read CHG_DTLS and detecting battery terminal error */
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_DETAILS_01, &chg_dtls);
	chg_dtls = ((chg_dtls & MAX77705_CHG_DTLS) >> MAX77705_CHG_DTLS_SHIFT);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &chg_cnfg_00);

	/* print the log at the abnormal case */
	if ((charger->is_charging == 1)
	    && ((chg_dtls == 0x08) || (chg_dtls == 0x0B))) {
		max77705_test_read(charger);
		max77705_set_charger_state(charger, DISABLE);
		max77705_set_float_voltage(charger, charger->float_voltage);
		max77705_set_charger_state(charger, ENABLE);
	}

	pr_info("%s: vbus_state: 0x%x, chg_dtls: 0x%x\n",
		__func__, vbus_state, chg_dtls);

	/*  OVP is higher priority */
	if (vbus_state == 0x02) {	/*  CHGIN_OVLO */
		pr_info("%s: vbus ovp\n", __func__);
		state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		if (is_wireless_type(charger->cable_type)) {
			retry_cnt = 0;
			do {
				msleep(50);
				vbus_state = max77705_get_vbus_state(charger);
			} while ((retry_cnt++ < 2) && (vbus_state == 0x02));
			if (vbus_state == 0x02) {
				state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
				pr_info("%s: wpc and over-voltage\n", __func__);
			} else
				state = POWER_SUPPLY_HEALTH_GOOD;
		}
	} else if (((vbus_state == 0x0) || (vbus_state == 0x01)) && (chg_dtls & 0x08)
		   && (chg_cnfg_00 & MAX77705_MODE_BUCK)
		   && (chg_cnfg_00 & MAX77705_MODE_CHGR)
		   && is_not_wireless_type(charger->cable_type)) {
		pr_info("%s: vbus is under\n", __func__);
		state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
	}

	return (int)state;
}

static int max77705_get_charge_current(struct max77705_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02, &reg_data);
	reg_data &= MAX77705_CHG_CC;

	get_current = reg_data <= 0x2 ? 100 : reg_data * 50;

	pr_info("%s: reg:(0x%x), charging_current:(%d)\n",
			__func__, reg_data, get_current);
	return get_current;
}

static int max77705_get_input_current_type(struct max77705_charger_data
					*charger, int cable_type)
{
	u8 reg_data;
	int get_current = 0;

	if (cable_type == SEC_BATTERY_CABLE_WIRELESS) {
		max77705_read_reg(charger->i2c,
				  MAX77705_CHG_REG_CNFG_10, &reg_data);
		/* AND operation for removing the formal 2bit  */
		reg_data &= MAX77705_CHG_WCIN_LIM;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x3F)
			get_current = 1600;
		else
			get_current = (reg_data + 0x01) * 25;
	} else {
		max77705_read_reg(charger->i2c,
				  MAX77705_CHG_REG_CNFG_09, &reg_data);
		/* AND operation for removing the formal 1bit  */
		reg_data &= MAX77705_CHG_CHGIN_LIM;

		if (reg_data <= 0x3)
			get_current = 100;
		else if (reg_data >= 0x7F)
			get_current = 3200;
		else
			get_current = (reg_data + 0x01) * 25;
	}

	pr_info("%s: reg:(0x%x), charging_current:(%d)\n",
			__func__, reg_data, get_current);

	return get_current;
}

static int max77705_get_input_current(struct max77705_charger_data *charger)
{
	if (is_wireless_type(charger->cable_type))
		return max77705_get_input_current_type(charger,
						       SEC_BATTERY_CABLE_WIRELESS);
	else
		return max77705_get_input_current_type(charger,
						       SEC_BATTERY_CABLE_TA);
}

static void reduce_input_current(struct max77705_charger_data *charger, int cur)
{
	u8 set_reg = 0, set_mask = 0, set_value = 0;
	unsigned int input_curr_limit_step = 25;
	int input_current = 0;

	if (is_wireless_type(charger->cable_type)) {
		set_reg = MAX77705_CHG_REG_CNFG_10;
		set_mask = MAX77705_CHG_WCIN_LIM;
	} else {
		set_reg = MAX77705_CHG_REG_CNFG_09;
		set_mask = MAX77705_CHG_CHGIN_LIM;
	}

	input_current = max77705_get_input_current(charger);
	if (input_current <= MINIMUM_INPUT_CURRENT)
		return;

	if (input_current - cur < MINIMUM_INPUT_CURRENT)
		input_current = MINIMUM_INPUT_CURRENT;
	else
		input_current -= cur;

	if (is_wireless_type(charger->cable_type))
		input_current = (input_current > 1600) ? 1600 : input_current;
	else
		input_current = (input_current > 3200) ? 3200 : input_current;

	set_value |= (input_current / input_curr_limit_step) - 0x01;
	max77705_update_reg(charger->i2c, set_reg, set_value, set_mask);
	pr_info("%s: reg:(0x%x), val(0x%x), input current(%d)\n",
		__func__, set_reg, set_value, input_current);
	charger->input_current = max77705_get_input_current(charger);
	charger->aicl_on = true;
}

static bool max77705_check_battery(struct max77705_charger_data *charger)
{
	u8 reg_data;
	u8 reg_data2;

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);

	pr_info("%s : CHG_INT_OK(0x%x)\n", __func__, reg_data);

	max77705_read_reg(charger->i2c,
			  MAX77705_CHG_REG_DETAILS_00, &reg_data2);

	pr_info("%s : CHG_DETAILS00(0x%x)\n", __func__, reg_data2);

	if ((reg_data & MAX77705_BATP_OK) || !(reg_data2 & MAX77705_BATP_DTLS))
		return true;
	else
		return false;
}

static void max77705_set_buck(struct max77705_charger_data *charger, int enable)
{
	u8 reg_data;

#if defined(CONFIG_SEC_FACTORY)
	if (is_muic_jig_301k) {
		pr_info("%s: jig 301k\n", __func__);
		enable = ENABLE;
	}
#endif

	if (enable) {
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
				    CHG_CNFG_00_BUCK_MASK, CHG_CNFG_00_BUCK_MASK);
	} else {
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
				    0, CHG_CNFG_00_BUCK_MASK);
	}
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &reg_data);
	pr_info("%s : CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
}

static void max77705_check_cnfg12_reg(struct max77705_charger_data *charger)
{
	static bool is_valid = true;

	if (is_valid) {
		u8 valid_cnfg12, reg_data;

		valid_cnfg12 = is_wireless_type(charger->cable_type) ? MAX77705_CHG_WCINSEL :
			(MAX77705_CHG_WCINSEL | (1 << CHG_CNFG_12_CHGINSEL_SHIFT));
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12, &reg_data);
		pr_info("%s: valid_data = 0x%2x, reg_data = 0x%2x\n",
			__func__, valid_cnfg12, reg_data);
		if (valid_cnfg12 != reg_data) {
			max77705_test_read(charger);
			is_valid = false;
		}
	}
}

static void max77705_change_charge_path(struct max77705_charger_data *charger,
					int enable, int path)
{
	u8 cnfg12;

	if (enable)
		if (is_wireless_type(path))
			cnfg12 = (0 << CHG_CNFG_12_CHGINSEL_SHIFT);
		else
			cnfg12 = (1 << CHG_CNFG_12_CHGINSEL_SHIFT);
	else
		cnfg12 = (0 << CHG_CNFG_12_CHGINSEL_SHIFT);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			    cnfg12, CHG_CNFG_12_CHGINSEL_MASK);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12, &cnfg12);
	pr_info("%s : CHG_CNFG_12(0x%02x)\n", __func__, cnfg12);

	max77705_check_cnfg12_reg(charger);
}

static void max77705_set_input_current(struct max77705_charger_data *charger,
					int input_current)
{
	int curr_step = 25;
	u8 set_reg, set_mask, reg_data = 0;

	mutex_lock(&charger->charger_mutex);

#if defined(CONFIG_SEC_FACTORY)
	if (is_muic_jig_301k) {
		pr_info("%s: jig 301k\n", __func__);
		input_current = 3150;
	}
#endif

	if (is_wireless_type(charger->cable_type)) {
		set_reg = MAX77705_CHG_REG_CNFG_10;
		set_mask = MAX77705_CHG_WCIN_LIM;
	} else {
		set_reg = MAX77705_CHG_REG_CNFG_09;
		set_mask = MAX77705_CHG_CHGIN_LIM;
	}

	if (input_current < 100) {
		reg_data = 0x00;
		max77705_update_reg(charger->i2c, set_reg, reg_data, set_mask);
	} else if (is_wireless_type(charger->cable_type)) {
		input_current = (input_current > 1600) ? 1600 : input_current;
		reg_data = (input_current / curr_step) - 0x01;
		max77705_update_reg(charger->i2c, set_reg, reg_data, set_mask);
	} else {
		input_current = (input_current > 3200) ? 3200 : input_current;
		reg_data = (input_current / curr_step) - 0x01;
		max77705_update_reg(charger->i2c, set_reg, reg_data, set_mask);
	}

	if (!max77705_get_autoibus(charger))
		max77705_set_fw_noautoibus(MAX77705_AUTOIBUS_AT_OFF);

	mutex_unlock(&charger->charger_mutex);
	pr_info("[%s] REG(0x%02x) DATA(0x%02x), CURRENT(%d)\n",
		__func__, set_reg, reg_data, input_current);
}

static void max77705_set_charge_current(struct max77705_charger_data *charger,
					int fast_charging_current)
{
	int curr_step = 50;
	u8 set_mask, reg_data = 0;

	set_mask = MAX77705_CHG_CC;

#if defined(CONFIG_SEC_FACTORY)
	if (is_muic_jig_301k) {
		pr_info("%s: jig 301k\n", __func__);
		fast_charging_current = 100;
	}
#endif

	if (fast_charging_current < 100) {
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02, 0x00, set_mask);
	} else {
		fast_charging_current =
		    (fast_charging_current > 3150) ? 3150 : fast_charging_current;

		reg_data |= (fast_charging_current / curr_step);
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02, reg_data, set_mask);
	}

	pr_info("[%s] REG(0x%02x) DATA(0x%02x), CURRENT(%d)\n",
		__func__, MAX77705_CHG_REG_CNFG_02,
		reg_data, fast_charging_current);
}

static void max77705_set_wireless_input_current(struct max77705_charger_data
						*charger, int input_current)
{
	union power_supply_propval value;

	wake_lock(&charger->wc_current_wake_lock);
	if (is_wireless_type(charger->cable_type)) {
		/* Wcurr-A) In cases of wireless input current change,
		 * configure the Vrect adj room to 270mV for safe wireless charging.
		 */
		wake_lock(&charger->wc_current_wake_lock);
		value.intval = WIRELESS_VRECT_ADJ_ROOM_1;	/* 270mV */
		psy_do_property(charger->pdata->wireless_charger_name, set,
				POWER_SUPPLY_PROP_INPUT_VOLTAGE_REGULATION, value);
		msleep(500);	/* delay 0.5sec */
		charger->wc_pre_current = max77705_get_input_current(charger);
		charger->wc_current = input_current;
		if (charger->wc_current > charger->wc_pre_current) {
			max77705_set_charge_current(charger,
						    charger->charging_current);
		}
	}
	queue_delayed_work(charger->wqueue, &charger->wc_current_work, 0);
}

static void max77705_set_topoff_current(struct max77705_charger_data *charger,
					int termination_current)
{
	int curr_base = 150, curr_step = 50;
	u8 reg_data;

	if (termination_current < curr_base)
		termination_current = curr_base;
	else if (termination_current > 500)
		termination_current = 500;

	reg_data = (termination_current - curr_base) / curr_step;
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_03,
			    reg_data, CHG_CNFG_03_TO_ITH_MASK);

	pr_info("%s: reg_data(0x%02x), topoff(%dmA)\n",
		__func__, reg_data, termination_current);
}

static void max77705_set_charger_state(struct max77705_charger_data *charger, int enable)
{
	u8 cnfg_00, cnfg_12;

#if defined(CONFIG_SEC_FACTORY)
	if (is_muic_jig_301k) {
		pr_info("%s: jig 301k\n", __func__);
		enable = ENABLE;
	}
#endif

	if (enable) {
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
				    CHG_CNFG_00_CHG_MASK, CHG_CNFG_00_CHG_MASK);
	} else {
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
				    0, CHG_CNFG_00_CHG_MASK);
	}
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &cnfg_00);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12, &cnfg_12);
	pr_info("%s : CHG_CNFG_00(0x%02x), CHG_CNFG_12(0x%02x)\n", __func__,
		cnfg_00, cnfg_12);
}

static void max77705_set_skipmode(struct max77705_charger_data *charger, int enable)
{

	if (enable) {
		/* Auto skip mode */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
				(MAX77705_AUTO_SKIP << CHG_CNFG_12_REG_DISKIP_SHIFT),
				CHG_CNFG_12_REG_DISKIP_MASK);
	} else {
		/* Disable skip mode */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
				(MAX77705_DISABLE_SKIP << CHG_CNFG_12_REG_DISKIP_SHIFT),
				CHG_CNFG_12_REG_DISKIP_MASK);
	}
}

static void max77705_set_otg(struct max77705_charger_data *charger, int enable)
{
	union power_supply_propval value;
	u8 reg = 0;
	static u8 chg_int_state;

	pr_info("%s: CHGIN-OTG %s\n", __func__,
		enable > 0 ? "on" : "off");
	if (charger->otg_on == enable || lpcharge)
		return;

	wake_lock(&charger->otg_wake_lock);
	mutex_lock(&charger->charger_mutex);
	/* CHGIN-OTG */
	value.intval = enable;
	charger->otg_on = enable;
	if (enable) {
		psy_do_property("wireless", set,
			POWER_SUPPLY_PROP_USB_TYPE, value);

		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_MASK,
			&chg_int_state);

		/* disable charger interrupt: CHG_I, CHGIN_I */
		/* enable charger interrupt: BYP_I */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_INT_MASK,
			MAX77705_CHG_IM | MAX77705_CHGIN_IM,
			MAX77705_CHG_IM | MAX77705_CHGIN_IM | MAX77705_BYP_IM);

		/* Switching Frequency : 3MHz */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_08,
				(MAX77705_CHG_FSW_3MHz << CHG_CNFG_08_REG_FSW_SHIFT),
				CHG_CNFG_08_REG_FSW_MASK);

		/* Disable skip mode */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
				(MAX77705_DISABLE_SKIP << CHG_CNFG_12_REG_DISKIP_SHIFT),
				CHG_CNFG_12_REG_DISKIP_MASK);

		/* OTG on, boost on */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
				    CHG_CNFG_00_OTG_CTRL, CHG_CNFG_00_OTG_CTRL);
	} else {
		/* OTG off(UNO on), boost off */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			0, CHG_CNFG_00_OTG_CTRL);

		mdelay(50);

		/* enable charger interrupt */
		max77705_write_reg(charger->i2c,
			MAX77705_CHG_REG_INT_MASK, chg_int_state);

		psy_do_property("wireless", set,
			POWER_SUPPLY_PROP_USB_TYPE, value);

		/* Switching Frequency : 1.5MHz */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_08,
				(MAX77705_CHG_FSW_1_5MHz << CHG_CNFG_08_REG_FSW_SHIFT),
				CHG_CNFG_08_REG_FSW_MASK);

		/* Auto skip mode */
		max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
				(MAX77705_AUTO_SKIP << CHG_CNFG_12_REG_DISKIP_SHIFT),
				CHG_CNFG_12_REG_DISKIP_MASK);
	}
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_MASK,
		&chg_int_state);
	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
		&reg);
	mutex_unlock(&charger->charger_mutex);
	pr_info("%s: INT_MASK(0x%x), CHG_CNFG_00(0x%x)\n",
		__func__, chg_int_state, reg);
	power_supply_changed(charger->psy_otg);
}

static void max77705_check_slow_charging(struct max77705_charger_data *charger,
					int input_current)
{
	/* under 400mA considered as slow charging concept for VZW */
	if (input_current <= SLOW_CHARGING_CURRENT_STANDARD &&
	    charger->cable_type != SEC_BATTERY_CABLE_NONE) {
		union power_supply_propval value;

		charger->slow_charging = true;
		pr_info
		    ("%s: slow charging on : input current(%dmA), cable type(%d)\n",
		     __func__, input_current, charger->cable_type);

		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_CHARGE_TYPE, value);
	} else
		charger->slow_charging = false;
}

static void max77705_charger_initialize(struct max77705_charger_data *charger)
{
	u8 reg_data;
	int jig_gpio;

	pr_info("%s\n", __func__);

	/* unmasked: CHGIN_I, WCIN_I, BATP_I, BYP_I */
	/* max77705_write_reg(charger->i2c, max77705_CHG_REG_INT_MASK, 0x9a); */

	/* unlock charger setting protect
	 * slowest LX slope
	 */
	reg_data = (0x03 << 2);
	reg_data |= 0x60;
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_06, reg_data,
			    reg_data);

	/*
	 * fast charge timer disable
	 * restart threshold disable
	 * pre-qual charge disable
	 */
	reg_data = (MAX77705_FCHGTIME_DISABLE << CHG_CNFG_01_FCHGTIME_SHIFT) |
			(MAX77705_CHG_RSTRT_DISABLE << CHG_CNFG_01_CHG_RSTRT_SHIFT) |
			(MAX77705_CHG_PQEN_DISABLE << CHG_CNFG_01_PQEN_SHIFT);
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_01, reg_data,
		(CHG_CNFG_01_FCHGTIME_MASK | CHG_CNFG_01_CHG_RSTRT_MASK | CHG_CNFG_01_PQEN_MASK));

	/*
	 * OTG off(UNO on), boost off
	 */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
			0, CHG_CNFG_00_OTG_CTRL);

	/*
	 * charge current 450mA(default)
	 * otg current limit 900mA
	 */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_02,
			MAX77705_OTG_ILIM_900 << CHG_CNFG_02_OTG_ILIM_SHIFT,
			CHG_CNFG_02_OTG_ILIM_MASK);

	/* BAT to SYS OCP 4.80A */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_05,
			MAX77705_B2SOVRC_4_8A << CHG_CNFG_05_REG_B2SOVRC_SHIFT,
			CHG_CNFG_05_REG_B2SOVRC_MASK);
	/*
	 * top off current 150mA
	 * top off timer 30min
	 */
	reg_data = (MAX77705_TO_ITH_150MA << CHG_CNFG_03_TO_ITH_SHIFT) |
			(MAX77705_TO_TIME_30M << CHG_CNFG_03_TO_TIME_SHIFT) |
			(MAX77705_SYS_TRACK_DISABLE << CHG_CNFG_03_SYS_TRACK_DIS_SHIFT);
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_03, reg_data,
		(CHG_CNFG_03_TO_ITH_MASK | CHG_CNFG_03_TO_TIME_MASK | CHG_CNFG_03_SYS_TRACK_DIS_MASK));

	/*
	 * cv voltage 4.2V or 4.35V
	 * MINVSYS 3.6V(default)
	 */
	max77705_set_float_voltage(charger, charger->pdata->chg_float_voltage);

	/* VCHGIN : REG=4.5V, UVLO=4.7V, WCHGIN : REG=4.5V, UVLO=4.7V */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			    0x00,  CHG_CNFG_12_VCHGIN_REG_MASK | CHG_CNFG_12_WCIN_REG_MASK);

	/* Boost mode possible in FACTORY MODE */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_07,
			    MAX77705_CHG_FMBST, CHG_CNFG_07_REG_FMBST_MASK);

	/* Watchdog Enable */
	max77705_chg_set_wdtmr_en(charger, 1);

	/* Active Discharge Enable */
	max77705_update_reg(charger->pmic_i2c, MAX77705_PMIC_REG_MAINCTRL1,
			    0x01, 0x01);

	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_09,
			    MAX77705_CHG_EN, MAX77705_CHG_EN);

	/* VBYPSET=5.0V */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_11,
			0x00, CHG_CNFG_11_VBYPSET_MASK);

	/* Switching Frequency : 1.5MHz */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_08,
			(MAX77705_CHG_FSW_1_5MHz << CHG_CNFG_08_REG_FSW_SHIFT),
			CHG_CNFG_08_REG_FSW_MASK);

	/* Auto skip mode */
	max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
			(MAX77705_AUTO_SKIP << CHG_CNFG_12_REG_DISKIP_SHIFT),
			CHG_CNFG_12_REG_DISKIP_MASK);

	max77705_test_read(charger);
}

/*######*/

static int max77693_get_charger_state(struct regmap *regmap, int *val)
{
	int ret;
	unsigned int data;

	ret = regmap_read(regmap, MAX77705_CHG_REG_DETAILS_01, &data);
	pr_info("%s : charger status (0x%02x)\n", __func__, data);
	if (ret < 0)
		return ret;

	reg_data &= 0x0f;

	switch (reg_data) {
	case 0x00:
	case 0x01:
	case 0x02:
		*val = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x03:
	case 0x04:
		*val = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x05:
	case 0x06:
	case 0x07:
		*val = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x08:
	case 0xA:
	case 0xB:
		*val = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		*val = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return 0;
}

static int max77705_chg_set_property(struct power_supply *psy,
					enum power_supply_property psp,
					const union power_supply_propval *val)
{
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);
	u8 reg = 0;
	static u8 chg_int_state, chg_reg_cnfg07;
	int buck_state = ENABLE;
	enum power_supply_ext_property ext_psp = psp;
	union power_supply_propval value = {0, };

	/* check unlock status before does set the register */
	max77705_charger_unlock(charger);
	switch (psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
//	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
//		charger->charge_mode = val->intval;
//		switch (charger->charge_mode) {
//		case SEC_BAT_CHG_MODE_BUCK_OFF:
//			buck_state = DISABLE;
//		case SEC_BAT_CHG_MODE_CHARGING_OFF:
//			charger->is_charging = false;
//			break;
//		case SEC_BAT_CHG_MODE_CHARGING:
//			charger->is_charging = true;
//			break;
//		}
//		psy_do_property(charger->pdata->wireless_charger_name,
//					get, POWER_SUPPLY_PROP_ONLINE, value);
//		pr_info("%s: wireless_chg(%d), otg(%d)\n", __func__, value.intval, charger->otg_on);
//		if (value.intval && charger->otg_on) {
//			msleep(200);
//			max77705_set_buck(charger, buck_state);
//			max77705_set_charger_state(charger, charger->is_charging);
//			msleep(5);
//			max77705_set_skipmode(charger, 0);
//		} else {
//		max77705_set_buck(charger, buck_state);
//		max77705_set_charger_state(charger, charger->is_charging);
//		}
//		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		charger->prev_aicl_mode = false;
		charger->aicl_on = false;
		charger->slow_charging = false;
		charger->input_current = max77705_get_input_current(charger);
		max77705_change_charge_path(charger, 1, charger->cable_type);
		if (!max77705_get_autoibus(charger))
			max77705_set_fw_noautoibus(MAX77705_AUTOIBUS_AT_OFF);
		if (charger->cable_type == SEC_BATTERY_CABLE_NONE) {
			charger->wc_pre_current = WC_CURRENT_START;
			wake_unlock(&charger->wpc_wake_lock);
			cancel_delayed_work(&charger->wpc_work);
			max77705_update_reg(charger->i2c,
					    MAX77705_CHG_REG_INT_MASK, 0,
					    MAX77705_WCIN_IM);
			if (charger->enable_sysovlo_irq)
				max77705_set_sysovlo(charger, 1);
			/* Enable AICL IRQ */
			if (charger->irq_aicl_enabled == 0) {
				u8 reg_data;

				charger->irq_aicl_enabled = 1;
				enable_irq(charger->irq_aicl);
				max77705_read_reg(charger->i2c,
						  MAX77705_CHG_REG_INT_MASK, &reg_data);
				pr_info("%s : enable aicl : 0x%x\n", __func__, reg_data);
			}
		} else if (is_hv_wire_type(charger->cable_type) ||
			(charger->cable_type == SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT)) {
			/* Disable AICL IRQ */
			if (charger->irq_aicl_enabled == 1) {
				u8 reg_data;

				charger->irq_aicl_enabled = 0;
				disable_irq_nosync(charger->irq_aicl);
				cancel_delayed_work(&charger->aicl_work);
				wake_unlock(&charger->aicl_wake_lock);
				max77705_read_reg(charger->i2c,
						  MAX77705_CHG_REG_INT_MASK, &reg_data);
				pr_info("%s : disable aicl : 0x%x\n", __func__, reg_data);
				charger->prev_aicl_mode = false;
				charger->aicl_on = false;
				charger->slow_charging = false;
			}
		}
		break;
		/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		{
			int input_current = val->intval;

			if (is_wireless_type(charger->cable_type))
				max77705_set_wireless_input_current(charger, input_current);
			else
				max77705_set_input_current(charger, input_current);

			if (charger->cable_type == SEC_BATTERY_CABLE_NONE)
				max77705_set_wireless_input_current(charger, input_current);
			charger->input_current = input_current;
		}
		break;
		/*  val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->charging_current = val->intval;
		max77705_set_charge_current(charger, val->intval);
		break;
		/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->charging_current = val->intval;
		if (is_not_wireless_type(charger->cable_type)) {
			max77705_set_charge_current(charger,
						    charger->charging_current);
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (charger->otg_on) {
			pr_info("%s: SKIP MODE (%d)\n", __func__, val->intval);
			max77705_set_skipmode(charger, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_FULL:
		max77705_set_topoff_current(charger, val->intval);
		break;
#if defined(CONFIG_AFC_CHARGER_MODE)
	case POWER_SUPPLY_PROP_AFC_CHARGER_MODE:
		muic_hv_charger_init();
		break;
#endif
#if defined(CONFIG_BATTERY_SWELLING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		charger->float_voltage = val->intval;
		pr_info("%s: float voltage(%d)\n", __func__, val->intval);
		max77705_set_float_voltage(charger, val->intval);
		break;
#endif
	case POWER_SUPPLY_PROP_USB_HC:
		break;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		/* if jig attached, change the power source */
		/* from the VBATFG to the internal VSYS */
		max77705_update_reg(charger->i2c,
			MAX77705_CHG_REG_CNFG_07,
			(val->intval << CHG_CNFG_07_REG_FGSRC_SHIFT),
			CHG_CNFG_07_REG_FGSRC_MASK);
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_07,
				  &chg_reg_cnfg07);

		pr_info("%s: POWER_SUPPLY_PROP_ENERGY_NOW: reg(0x%x) val(0x%x)\n",
			__func__, MAX77705_CHG_REG_CNFG_07, chg_reg_cnfg07);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_SHADOW:
		charger->otg_on = false;
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		max77705_set_otg(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL:
		pr_info("%s: WCIN-UNO %s\n", __func__,
			val->intval > 0 ? "on" : "off");
		/* WCIN-UNO */
		if (val->intval) {
			max77705_read_reg(charger->i2c,
					  MAX77705_CHG_REG_INT_MASK,
					  &chg_int_state);
			/* disable charger interrupt: CHG_I, CHGIN_I */
			/* enable charger interrupt: BYP_I */
			max77705_update_reg(charger->i2c,
					    MAX77705_CHG_REG_INT_MASK,
					    MAX77705_CHG_IM | MAX77705_CHGIN_IM,
					    MAX77705_CHG_IM | MAX77705_CHGIN_IM
					    | MAX77705_BYP_IM);

			/* Switching Frequency : 3MHz */
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_08,
					(MAX77705_CHG_FSW_3MHz << CHG_CNFG_08_REG_FSW_SHIFT),
					CHG_CNFG_08_REG_FSW_MASK);

			/* Disable skip mode */
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
					(MAX77705_DISABLE_SKIP << CHG_CNFG_12_REG_DISKIP_SHIFT),
					CHG_CNFG_12_REG_DISKIP_MASK);

			/* UNO on, boost on */
			max77705_update_reg(charger->i2c,
					    MAX77705_CHG_REG_CNFG_00,
					    CHG_CNFG_00_BOOST_MASK,
					    CHG_CNFG_00_UNO_CTRL);
		} else {
			/* boost off */
			max77705_update_reg(charger->i2c,
					    MAX77705_CHG_REG_CNFG_00, 0,
					    CHG_CNFG_00_BOOST_MASK);

			mdelay(50);

			/* enable charger interrupt */
			max77705_write_reg(charger->i2c,
					   MAX77705_CHG_REG_INT_MASK,
					   chg_int_state);

			/* Switching Frequency : 1.5MHz */
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_08,
					(MAX77705_CHG_FSW_1_5MHz << CHG_CNFG_08_REG_FSW_SHIFT),
					CHG_CNFG_08_REG_FSW_MASK);

			/* Auto skip mode */
			max77705_update_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12,
					(MAX77705_AUTO_SKIP << CHG_CNFG_12_REG_DISKIP_SHIFT),
					CHG_CNFG_12_REG_DISKIP_MASK);
		}
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_MASK,
				  &chg_int_state);
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, &reg);
		pr_info("%s: INT_MASK(0x%x), CHG_CNFG_00(0x%x)\n",
			__func__, chg_int_state, reg);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		max77705_enable_aicl_irq(charger);
		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg);
		if (reg & MAX77705_AICL_I)
			queue_delayed_work(charger->wqueue, &charger->aicl_work,
					   msecs_to_jiffies(AICL_WORK_DELAY));
		break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	case POWER_SUPPLY_PROP_POWER_DESIGN:
		max77705_charger_parse_dt(charger);
		break;
#endif
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
/*		case POWER_SUPPLY_EXT_PROP_SURGE:
 *			if (val->intval) {
 *				pr_info
 *				    ("%s : Charger IC reset by surge. charger re-initialize\n",
 *				     __func__);
 *				check_charger_unlock_state(charger);
 *			}
 *			break;
 */
		case POWER_SUPPLY_EXT_PROP_CHGINSEL:
			max77705_change_charge_path(charger,
				val->intval, charger->cable_type);
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77705_chg_get_property(struct power_supply *psy,
					enum power_supply_property psp,
					union power_supply_propval *val)
{
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);
	u8 reg_data;
	enum power_supply_ext_property ext_psp = psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = SEC_BATTERY_CABLE_NONE;
		if (max77705_read_reg(charger->i2c,
				      MAX77705_CHG_REG_INT_OK, &reg_data) == 0) {
			if (reg_data & MAX77705_WCIN_OK) {
				val->intval = SEC_BATTERY_CABLE_WIRELESS;
				charger->wc_w_state = 1;
			} else if (reg_data & MAX77705_CHGIN_OK) {
				val->intval = SEC_BATTERY_CABLE_TA;
			}
		}
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = max77705_check_battery(charger);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77705_get_charger_state(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!charger->is_charging)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else if (charger->slow_charging) {
			val->intval = POWER_SUPPLY_CHARGE_TYPE_SLOW;
			pr_info("%s: slow-charging mode\n", __func__);
		} else
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77705_get_charging_health(charger);
		max77705_check_cnfg12_reg(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->input_current;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (is_wireless_type(val->intval))
			val->intval =
			    max77705_get_input_current_type(charger,
							    SEC_BATTERY_CABLE_WIRELESS);
		else
			val->intval =
			    max77705_get_input_current_type(charger,
							    SEC_BATTERY_CABLE_TA);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = charger->charging_current;
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		val->intval = max77705_get_charge_current(charger);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = max77705_get_float_voltage(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		max77705_read_reg(charger->i2c,
				  MAX77705_CHG_REG_DETAILS_01, &reg_data);
		reg_data &= 0x0F;
		switch (reg_data) {
		case 0x01:
			val->strval = "CC Mode";
			break;
		case 0x02:
			val->strval = "CV Mode";
			break;
		case 0x03:
			val->strval = "EOC";
			break;
		case 0x04:
			val->strval = "DONE";
			break;
		default:
			val->strval = "NONE";
			break;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		mutex_lock(&charger->charger_mutex);
		val->intval = charger->otg_on;
		mutex_unlock(&charger->charger_mutex);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		break;
//	case POWER_SUPPLY_PROP_CHARGE_UNO_CONTROL:
//		max77705_read_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00,
//				  &reg_data);
//		if ((reg_data & CHG_CNFG_00_UNO_CTRL) == CHG_CNFG_00_BOOST_MASK)
//			val->intval = 1;
//		else
//			val->intval = 0;
//		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (max77705_is_constant_current(charger))
			val->intval = 0;
		else
			val->intval = 1;
		break;
//	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
//		switch (ext_psp) {
//		case POWER_SUPPLY_EXT_PROP_WDT_STATUS:
//			if (max77705_chg_get_wdtmr_status(charger)) {
//				dev_info(charger->dev, "charger WDT is expired!!\n");
//				max77705_test_read(charger);
//				if (!is_debug_level_low)
//					panic("charger WDT is expired!!");
//			}
//			break;
//		case POWER_SUPPLY_EXT_PROP_CHIP_ID:
//			if (max77705_read_reg
//			    (charger->i2c, MAX77705_PMIC_REG_PMICREV, &reg_data) == 0) {
//				val->intval = (charger->pmic_ver >= 0x1 && charger->pmic_ver <= 0x03);
//				pr_info("%s : IF PMIC ver.0x%x\n", __func__,
//					charger->pmic_ver);
//			} else {
//				val->intval = 0;
//				pr_info("%s : IF PMIC I2C fail.\n", __func__);
//			}
//			break;
//		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
//			max77705_test_read(charger);
//			max77705_chg_monitor_work(charger);
//			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct power_supply_desc max77705_charger_power_supply_desc = {
	.name = MAX77705_CHARGER_NAME,
	.type       = POWER_SUPPLY_TYPE_BATTERY,
	.properties = max77705_charger_props,
	.num_properties = ARRAY_SIZE(max77705_charger_props),
	.get_property = max77705_chg_get_property,
	.set_property = max77705_chg_set_property,
	.no_thermal = true,
};

static int max77705_otg_get_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     union power_supply_propval *val)
{
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mutex_lock(&charger->charger_mutex);
		val->intval = charger->otg_on;
		mutex_unlock(&charger->charger_mutex);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max77705_otg_set_property(struct power_supply *psy,
				     enum power_supply_property psp,
				     const union power_supply_propval *val)
{
	struct max77705_charger_data *charger = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		max77705_set_otg(charger, val->intval);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct power_supply_desc otg_power_supply_desc = {
	.name = MAX77705_CHARGER_USB_CDP_NAME,
	.type = POWER_SUPPLY_TYPE_USB_CDP,
	.properties = max77705_otg_props,
	.num_properties = ARRAY_SIZE(max77705_otg_props),
	.get_property = max77705_otg_get_property,
	.set_property = max77705_otg_set_property,
};

static int max77705_charger_probe(struct platform_device *pdev)
{
	struct max77705_dev *max77705 = dev_get_drvdata(pdev->dev.parent);
	struct max77705_platform_data *pdata = dev_get_platdata(max77705->dev);
	sec_charger_platform_data_t *charger_data;
	struct max77705_charger_data *charger;
	struct power_supply_config charger_cfg = { };
	int ret = 0;
	u8 reg_data;

	pr_info("%s: max77705 Charger Driver Loading\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger_data = kzalloc(sizeof(sec_charger_platform_data_t), GFP_KERNEL);
	if (!charger_data) {
		ret = -ENOMEM;
		goto err_free;
	}

	mutex_init(&charger->charger_mutex);

	charger->dev = &pdev->dev;
	charger->i2c = max77705->charger;
	charger->pmic_i2c = max77705->i2c;
	charger->pdata = charger_data;
	charger->prev_aicl_mode = false;
	charger->aicl_on = false;
	charger->slow_charging = false;
	charger->is_mdock = false;
	charger->otg_on = false;
	charger->max77705_pdata = pdata;
	charger->wc_pre_current = WC_CURRENT_START;
	charger->cable_type = SEC_BATTERY_CABLE_NONE;

//#if defined(CONFIG_OF)
//	ret = max77705_charger_parse_dt(charger);
//	if (ret < 0)
//		pr_err("%s not found charger dt! ret[%d]\n", __func__, ret);
//#endif
	platform_set_drvdata(pdev, charger);

	max77705_charger_initialize(charger);

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);
	if (reg_data & MAX77705_WCIN_OK)
		charger->cable_type = SEC_BATTERY_CABLE_WIRELESS;
	charger->input_current = max77705_get_input_current(charger);
	charger->charging_current = max77705_get_charge_current(charger);

	if (max77705_read_reg
	    (max77705->i2c, MAX77705_PMIC_REG_PMICREV, &reg_data) < 0) {
		pr_err
		    ("device not found on this channel (this is not an error)\n");
		ret = -ENOMEM;
		goto err_pdata_free;
	} else {
		charger->pmic_ver = (reg_data & 0x7);
		pr_info("%s : device found : ver.0x%x\n", __func__, charger->pmic_ver);
	}

//	(void)debugfs_create_file("max77705-regs",
//				0664, NULL, (void *)charger,
//				  &max77705_debugfs_fops);

	charger->wqueue = create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!charger->wqueue) {
		pr_err("%s: Fail to Create Workqueue\n", __func__);
		goto err_pdata_free;
	}
	INIT_WORK(&charger->chgin_work, max77705_chgin_isr_work);
	INIT_DELAYED_WORK(&charger->aicl_work, max77705_aicl_isr_work);
	INIT_DELAYED_WORK(&charger->chgin_init_work, max77705_chgin_init_work);
	INIT_DELAYED_WORK(&charger->wpc_work, wpc_detect_work);
	INIT_DELAYED_WORK(&charger->wc_current_work, max77705_wc_current_work);
	charger_cfg.drv_data = charger;

	charger->psy_chg =
	    power_supply_register(&pdev->dev,
				  &max77705_charger_power_supply_desc,
				  &charger_cfg);
	if (!charger->psy_chg) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}

	charger->psy_otg =
	    power_supply_register(&pdev->dev, &otg_power_supply_desc,
				  &charger_cfg);
	if (!charger->psy_otg) {
		pr_err("%s: Failed to Register otg_chg\n", __func__);
		goto err_power_supply_register_otg;
	}

//	if (charger->pdata->chg_irq) {
//		INIT_DELAYED_WORK(&charger->isr_work, max77705_chg_isr_work);
//
//		ret = request_threaded_irq(charger->pdata->chg_irq,
//					   NULL, max77705_chg_irq_thread,
//					   charger->pdata->chg_irq_attr,
//					   "charger-irq", charger);
//		if (ret) {
//			pr_err("%s: Failed to Request IRQ\n", __func__);
//			goto err_irq;
//		}
//
//		ret = enable_irq_wake(charger->pdata->chg_irq);
//		if (ret < 0)
//			pr_err("%s: Failed to Enable Wakeup Source(%d)\n",
//			       __func__, ret);
//	}

	charger->wc_w_irq = pdata->irq_base + MAX77705_CHG_IRQ_WCIN_I;
	ret = request_threaded_irq(charger->wc_w_irq,
				   NULL, wpc_charger_irq,
				   IRQF_TRIGGER_FALLING, "wpc-int", charger);
	if (ret) {
		pr_err("%s: Failed to Request IRQ\n", __func__);
	}

	max77705_read_reg(charger->i2c, MAX77705_CHG_REG_INT_OK, &reg_data);
	charger->wc_w_state = (reg_data & MAX77705_WCIN_OK)
	    >> MAX77705_WCIN_OK_SHIFT;

	charger->irq_chgin = pdata->irq_base + MAX77705_CHG_IRQ_CHGIN_I;
	/* enable chgin irq after sec_battery_probe */
	queue_delayed_work(charger->wqueue, &charger->chgin_init_work,
			   msecs_to_jiffies(3000));

	charger->irq_bypass = pdata->irq_base + MAX77705_CHG_IRQ_BYP_I;
	charger->irq_aicl_enabled = -1;
	charger->irq_aicl = pdata->irq_base + MAX77705_CHG_IRQ_AICL_I;

//	ret = max77705_chg_create_attrs(&charger->psy_chg->dev);
//	if (ret) {
//		dev_err(charger->dev,
//			"%s : Failed to create_attrs\n", __func__);
//		goto err_wc_irq;
//	}

	/* watchdog kick */
	max77705_chg_set_wdtmr_kick(charger);

	pr_info("%s: MAX77705 Charger Driver Loaded\n", __func__);

	return 0;

err_irq:
	power_supply_unregister(charger->psy_otg);
err_power_supply_register_otg:
	power_supply_unregister(charger->psy_chg);
err_power_supply_register:
	destroy_workqueue(charger->wqueue);
err_pdata_free:
	kfree(charger_data);
err_free:
	kfree(charger);

	return ret;
}

static int max77705_charger_remove(struct platform_device *pdev)
{
	struct max77705_charger_data *charger = platform_get_drvdata(pdev);

	pr_info("%s: ++\n", __func__);

	destroy_workqueue(charger->wqueue);

	if (charger->i2c) {
		u8 reg_data;

		reg_data = 0x04;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_00, reg_data);
		reg_data = 0x0F;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_09, reg_data);
		reg_data = 0x10;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_10, reg_data);
		reg_data = 0x60;
		max77705_write_reg(charger->i2c, MAX77705_CHG_REG_CNFG_12, reg_data);
	} else {
		pr_err("%s: no max77705 i2c client\n", __func__);
	}

	if (charger->irq_sysovlo)
		free_irq(charger->irq_sysovlo, charger);
	if (charger->wc_w_irq)
		free_irq(charger->wc_w_irq, charger);
	if (charger->psy_chg)
		power_supply_unregister(charger->psy_chg);
	if (charger->psy_otg)
		power_supply_unregister(charger->psy_otg);

	kfree(charger);

	pr_info("%s: --\n", __func__);
	return 0;
}

static const struct platform_device_id max77705_charger_id[] = {
	{ "max77705-charger", 0, },
	{ }
};
MODULE_DEVICE_TABLE(platform, max77705_charger_id);

static struct platform_driver max77705_charger_driver = {
	.driver = {
		   .name = "max77705-charger",
		   },
	.probe = max77705_charger_probe,
	.remove = max77705_charger_remove,
	.id_table	= max77705_charger_id,
};

MODULE_AUTHOR("Dzmitry Sankouski <dsankouski@gmail.com>");
MODULE_DESCRIPTION("Maxim 77705 charger driver");
MODULE_LICENSE("GPL");
