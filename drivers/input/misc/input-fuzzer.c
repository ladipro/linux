#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/random.h>

#include <asm/irq.h>
#include <asm/io.h>

static struct input_dev *input_fuzzer_dev = NULL;
static u8 master_threshold;
static u8 bytes[8192];

static void fill_bits(int *ev_index, int ev_kind, unsigned long *bits, int bit_count)
{
	u8 threshold;
	int i;

	get_random_bytes(bytes, sizeof(bytes));
	if (bytes[0] < master_threshold) {
		printk(KERN_INFO "input-fuzzer: EV %d: not supported\n", ev_kind);
		return;
	}

	input_fuzzer_dev->evbit[BIT_WORD(ev_kind)] |= BIT_MASK(ev_kind);

	if (ev_kind == EV_ABS) {
		input_alloc_absinfo(input_fuzzer_dev);
	}

	threshold = bytes[1];
	for (i = 0; i < bit_count; i++) {
		if (bytes[i + 2] < threshold) {
			bits[BIT_WORD(i)] |= BIT_MASK(i);

			if (ev_kind == EV_ABS) {
				int min = 0, max = 0, fuzz = 0, flat = 0;

				get_random_bytes(&min, 2);
				get_random_bytes(&max, 2);
				get_random_bytes(&fuzz, 1);
				get_random_bytes(&flat, 1);

				if (min > max) {
					int tmp = min;
					min = max;
					max = tmp;
				}

				input_set_abs_params(input_fuzzer_dev, i, min, max, fuzz, flat);
			}
		}
	}

	// computer goes into airplane mode if we claim to support SW_RFKILL_ALL and
	// don't set the switch state to 1
	if (ev_kind == EV_SW) {
		input_fuzzer_dev->sw[BIT_WORD(SW_RFKILL_ALL)] = BIT_MASK(SW_RFKILL_ALL);
	}

	for (i = 0; i < BITS_TO_LONGS(bit_count); i++) {
		printk(KERN_INFO "input-fuzzer: EV %d: bitmap at %d: %08lx\n", ev_kind, i, bits[i]);
	}
}

static int fuzzer_event(struct input_dev *dev,
                        unsigned int type, unsigned int code, int value)
{
	return 0;
}

static int __init input_fuzzer_init(void)
{
	int error;
	int ev_index = 0;

	input_fuzzer_dev = input_allocate_device();
	if (!input_fuzzer_dev) {
		printk(KERN_ERR "input-fuzzer.c: Not enough memory\n");
		error = -ENOMEM;
		goto err_return;
	}

	input_fuzzer_dev->name = "Input fuzzer";
        input_fuzzer_dev->id.bustype = BUS_HOST;

	get_random_bytes(&master_threshold, sizeof(master_threshold));

	fill_bits(&ev_index, EV_KEY, input_fuzzer_dev->keybit, KEY_CNT);
	fill_bits(&ev_index, EV_REL, input_fuzzer_dev->relbit, REL_CNT);
	fill_bits(&ev_index, EV_ABS, input_fuzzer_dev->absbit, ABS_CNT);
	fill_bits(&ev_index, EV_MSC, input_fuzzer_dev->mscbit, MSC_CNT);
	// claiming LED bits beyond LED_SCROLLL causes the kernel to panic (???)
	fill_bits(&ev_index, EV_LED, input_fuzzer_dev->ledbit, /*LED_CNT*/ LED_SCROLLL + 1);
	fill_bits(&ev_index, EV_SND, input_fuzzer_dev->sndbit, SND_CNT);
	fill_bits(&ev_index, EV_FF, input_fuzzer_dev->ffbit, FF_CNT);
	fill_bits(&ev_index, EV_SW, input_fuzzer_dev->swbit, SW_CNT);

	input_fuzzer_dev->event = fuzzer_event;

	error = input_register_device(input_fuzzer_dev);
	if (error) {
		printk(KERN_ERR "input-fuzzer.c: Failed to register device\n");
		goto err_free_dev;
	}

	return 0;

 err_free_dev:
	input_free_device(input_fuzzer_dev);
 err_free_return:
	return error;
}

static void __exit input_fuzzer_exit(void)
{
	if (input_fuzzer_dev)
		input_unregister_device(input_fuzzer_dev);
}

module_init(input_fuzzer_init);
module_exit(input_fuzzer_exit);
