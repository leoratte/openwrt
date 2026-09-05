// SPDX-License-Identifier: GPL-2.0

/*
 *  AVM_74HC595 - Serial-in/parallel-out 8-bits 74hc595 shift register GPIO driver
 */

#include "linux/bitmap.h"
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/printk.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>

#define AVM_74HC595_NUMBER_GPIOS        8
#define AVM_74HC595_DELAY_US_LOWER_LIMIT        16
#define AVM_74HC595_DELAY_US_UPPER_LIMIT        32

#define is_offset_valid(chip, offset)   ((offset >= 0) && (offset < chip->ngpio))

struct avm_74hc595_chip {
        unsigned long           *buffer;
        struct gpio_chip        gpio_chip;
        struct mutex            lock;
        struct gpio_desc        *pin_shcp;
        struct gpio_desc        *pin_stcp;
        struct gpio_desc        *pin_ds;
};

static struct avm_74hc595_chip *gpio_to_74hc595_chip(struct gpio_chip *gc)
{
        return container_of(gc, struct avm_74hc595_chip, gpio_chip);
}

static void avm_74hc595_reg_delay(void)
{
        usleep_range(AVM_74HC595_DELAY_US_LOWER_LIMIT, AVM_74HC595_DELAY_US_UPPER_LIMIT);
}

static int avm_74hc595_get_value(struct gpio_chip *gc, unsigned int offset)
{
        struct avm_74hc595_chip *chip = gpio_to_74hc595_chip(gc);
        int ret;

        if (!is_offset_valid(gc, offset)) {
                dev_err(gc->parent, "Error: offset %d out of range\n", offset);
                return -EINVAL;
        }

        mutex_lock(&chip->lock);
        ret = test_bit(offset, chip->buffer);
        mutex_unlock(&chip->lock);

        return ret;
}

static int avm_74hc595_set_value(struct gpio_chip *gc,
                unsigned int offset, int val)
{
        struct avm_74hc595_chip *chip = gpio_to_74hc595_chip(gc);
        int i;

        if (!is_offset_valid(gc, offset)) {
                dev_err(gc->parent, "Error: offset %d out of range\n", offset);
                return -EINVAL;
        }

        mutex_lock(&chip->lock);

        if (val)
                set_bit(offset, chip->buffer);
        else
                clear_bit(offset, chip->buffer);

        for (i = chip->gpio_chip.ngpio - 1; i >= 0; i--) {
                gpiod_set_value(chip->pin_shcp, 0);
                avm_74hc595_reg_delay();

                gpiod_set_value(chip->pin_ds, test_bit(i, chip->buffer));
                avm_74hc595_reg_delay();

                gpiod_set_value(chip->pin_shcp, 1);
                avm_74hc595_reg_delay();
        }

        gpiod_set_value(chip->pin_shcp, 0);
        gpiod_set_value(chip->pin_ds, 0);
        avm_74hc595_reg_delay();

        gpiod_set_value(chip->pin_stcp, 1);
        avm_74hc595_reg_delay();
        gpiod_set_value(chip->pin_stcp, 0);

        mutex_unlock(&chip->lock);

	return 0;
}

static int avm_74hc595_direction_output(struct gpio_chip *gc,
                unsigned int offset, int val)
{
	return avm_74hc595_set_value(gc, offset, val);
}

static const struct of_device_id avm_74hc595_avm_dt_ids[] = {
        { .compatible = "avm,74hc595" },
        {},
};
MODULE_DEVICE_TABLE(of, avm_74hc595_avm_dt_ids);

static int avm_74hc595_probe(struct platform_device *pdev)
{
        const struct of_device_id *of_id = of_match_device(avm_74hc595_avm_dt_ids, &pdev->dev);
        struct device_node *np = pdev->dev.of_node;
        struct avm_74hc595_chip *chip;
        int ret = 0;
        u32 num_registers;

        if (!of_id)
                return -ENODEV;

        chip = devm_kmalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
        if (!chip)
                return -ENOMEM;

        chip->pin_shcp = devm_gpiod_get(&pdev->dev, "shcp", GPIOD_ASIS);
        if (IS_ERR(chip->pin_shcp)) {
                dev_err(&pdev->dev, "Error requesting SHCP pin: %ld\n", PTR_ERR(chip->pin_shcp));
                return PTR_ERR(chip->pin_shcp);
        }

        chip->pin_stcp = devm_gpiod_get(&pdev->dev, "stcp", GPIOD_ASIS);
        if (IS_ERR(chip->pin_stcp)) {
                dev_err(&pdev->dev, "Error requesting STCP pin: %ld\n", PTR_ERR(chip->pin_stcp));
                return PTR_ERR(chip->pin_stcp);
        }

        chip->pin_ds = devm_gpiod_get(&pdev->dev, "ds", GPIOD_ASIS);
        if (IS_ERR(chip->pin_ds)) {
                dev_err(&pdev->dev, "Error requesting DS pin: %ld\n", PTR_ERR(chip->pin_ds));
                return PTR_ERR(chip->pin_ds);
        }

        ret = of_property_read_u32(np, "num-registers", &num_registers);
        if (ret) {
                dev_err(&pdev->dev, "Error requesting num-registers property: %d\n", ret);
                return ret;
        }

        mutex_init(&chip->lock);

        chip->gpio_chip = (struct gpio_chip){
                .label = dev_name(&pdev->dev),
                .direction_output = avm_74hc595_direction_output,
                .get = avm_74hc595_get_value,
                .set = avm_74hc595_set_value,
                .base = -1,
                .parent = &pdev->dev,
                .fwnode = of_fwnode_handle(pdev->dev.of_node),
                .ngpio = AVM_74HC595_NUMBER_GPIOS * num_registers,
                .can_sleep = false,
                .owner = THIS_MODULE,
        };

        chip->buffer = bitmap_zalloc(chip->gpio_chip.ngpio, GFP_KERNEL);
        if (!chip->buffer) {
                mutex_destroy(&chip->lock);
                return -ENOMEM;
        }

        platform_set_drvdata(pdev, chip);

        ret = devm_gpiochip_add_data(&pdev->dev, &chip->gpio_chip, NULL);
        if (ret) {
                dev_err(&pdev->dev, "Error registering gpio_chip: %d\n", ret);
                mutex_destroy(&chip->lock);
                bitmap_free(chip->buffer);
                return ret;
        }

        return 0;
}

static void avm_74hc595_remove(struct platform_device *pdev)
{
        struct avm_74hc595_chip *chip = platform_get_drvdata(pdev);

        mutex_destroy(&chip->lock);
        bitmap_free(chip->buffer);
}

static struct platform_driver avm_74hc595_driver = {
        .driver = {
                .name           = "avm_74hc595",
                .of_match_table = avm_74hc595_avm_dt_ids,
        },
        .probe          = avm_74hc595_probe,
        .remove         = avm_74hc595_remove,
};

module_platform_driver(avm_74hc595_driver);

MODULE_AUTHOR("Marc Reinsberg <m.reinsberg@avm.de>");
MODULE_DESCRIPTION("GPIO expander driver for 74HC595 8-bit shift register");
MODULE_LICENSE("GPL");
