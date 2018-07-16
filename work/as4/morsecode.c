#include <linux/module.h>
#include <linux/miscdevice.h>		// for misc-driver calls.
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>

//#error Are we building this?

#define MY_DEVICE_FILE  "morse-code"

/******************************************************
 * Parameter
 ******************************************************/
#define DEFAULT_DOT_TIME 200
static int dottime = DEFAULT_DOT_TIME;

module_param(dottime, int, S_IRUGO);
MODULE_PARM_DESC(dottime, "MS wait for morse code");

/******************************************************
 * LED
 ******************************************************/
#include <linux/leds.h>

DEFINE_LED_TRIGGER(ledtrig_morsecode);

static void my_led_blink(int on, int mult)
{
	if (on) {
		led_trigger_event(ledtrig_morsecode, LED_FULL);
		msleep(dottime*mult);
	} else {
		led_trigger_event(ledtrig_morsecode, LED_OFF);
		msleep(dottime*mult);
	}
}

static void led_register(void)
{
	// Setup the trigger's name:
	led_trigger_register_simple("morse-code", &ledtrig_morsecode);
}

static void led_unregister(void)
{
	// Cleanup
	led_trigger_unregister_simple(ledtrig_morsecode);
}

/**************************************************************
 * FIFO Support
 *************************************************************/
#include <linux/kfifo.h>
#define FIFO_SIZE 512	// Must be a power of 2.
static DECLARE_KFIFO(mc_fifo, char, FIFO_SIZE);

/******************************************************
 * Morse Code & Helper Funcs
 ******************************************************/
static unsigned short morsecode_codes[] = {
		0xB800,	// A 1011 1
		0xEA80,	// B 1110 1010 1
		0xEBA0,	// C 1110 1011 101
		0xEA00,	// D 1110 101
		0x8000,	// E 1
		0xAE80,	// F 1010 1110 1
		0xEE80,	// G 1110 1110 1
		0xAA00,	// H 1010 101
		0xA000,	// I 101
		0xBBB8,	// J 1011 1011 1011 1
		0xEB80,	// K 1110 1011 1
		0xBA80,	// L 1011 1010 1
		0xEE00,	// M 1110 111
		0xE800,	// N 1110 1
		0xEEE0,	// O 1110 1110 111
		0xBBA0,	// P 1011 1011 101
		0xEEB8,	// Q 1110 1110 1011 1
		0xBA00,	// R 1011 101
		0xA800,	// S 1010 1
		0xE000,	// T 111
		0xAE00,	// U 1010 111
		0xAB80,	// V 1010 1011 1
		0xBB80,	// W 1011 1011 1
		0xEAE0,	// X 1110 1010 111
		0xEBB8,	// Y 1110 1011 1011 1
		0xEEA0	// Z 1110 1110 101
};

static unsigned short mcBitmask = 0x8000;

static void processMCforLED(unsigned short code) {
	while (code > 0) {
		if ((code & mcBitmask) > 0) {
			my_led_blink(1, 1);
		} else {
			my_led_blink(0, 1);
		}
		code = code << 1;
	}
	my_led_blink(0, 3);
}

static ssize_t putFIFO(int type, int num) {
	int i;
	for (i = 0; i < num; i++) {
		if (type == 1) {
			if (!kfifo_put(&mc_fifo, '.')) {
				// Fifo full
				return -EFAULT;
			}
		} else if (type == 3) {
			if (!kfifo_put(&mc_fifo, '-')) {
				// Fifo full
				return -EFAULT;
			}
		} else if (type == 0) {
			if (!kfifo_put(&mc_fifo, ' ')) {
				// Fifo full
				return -EFAULT;
			}
		} else if (type == 2) {
			if (!kfifo_put(&mc_fifo, '\n')) {
				// Fifo full
				return -EFAULT;
			}
		}
	}

	return num;
}

static void processMCforFIFO(unsigned short code) {
	int count = 0;
	while (code > 0) {
		if ((code & mcBitmask) > 0) {
			count++;
		} else {
			putFIFO(count, 1);
			count = 0;
		}
		code = code << 1;
	}
	putFIFO(count, 1);
	putFIFO(0, 1);
}

/******************************************************
 * Callbacks
 ******************************************************/
static ssize_t my_read(struct file *file,
		char *buff, size_t count, loff_t *ppos)
{
	// Pull all available data from fifo into user buffer
	int num_bytes_read = 0;
	if (kfifo_to_user(&mc_fifo, buff, count, &num_bytes_read)) {
		return -EFAULT;
	}

	return num_bytes_read;  // # bytes actually read.
}

static ssize_t my_write(struct file *file,
		const char *buff, size_t count, loff_t *ppos)
{
	int buff_idx = 0;

	// Find min character
	for (buff_idx = 0; buff_idx < count; buff_idx++) {
		char ch;
		int ascii;
		// Get the character
		if (copy_from_user(&ch, &buff[buff_idx], sizeof(ch))) {
			return -EFAULT;
		}

		ascii = (int) ch;
		if (ascii == 32) {
			my_led_blink(0, 7);
			putFIFO(0, 2); //not 3 because processMCforFIFO prints one space
		} else if (ascii >= 65 && ascii <= 90) {
			processMCforLED(morsecode_codes[ascii-65]);
			processMCforFIFO(morsecode_codes[ascii-65]);
		} else if (ascii >= 97 && ascii <= 122) {
			processMCforLED(morsecode_codes[ascii-97]);
			processMCforFIFO(morsecode_codes[ascii-97]);
		}
	}
	putFIFO(2, 1);

	// Print out the message
	if (count < 0) {
		printk(KERN_INFO "No characters in buffer to analyze.\n");
	}

	// Return # bytes actually written.
	*ppos += count;
	return count;
}

/******************************************************
 * Misc support
 ******************************************************/
// Callbacks:  (structure defined in /linux/fs.h)
struct file_operations my_fops = {
	.owner    =  THIS_MODULE,
	.read     =  my_read,
	.write    =  my_write,
};

// Character Device info for the Kernel:
static struct miscdevice my_miscdevice = {
		.minor    = MISC_DYNAMIC_MINOR,         // Let the system assign one.
		.name     = MY_DEVICE_FILE,             // /dev/.... file.
		.fops     = &my_fops                    // Callback functions.
};


/******************************************************
 * Driver initialization and exit:
 ******************************************************/
static int __init my_init(void)
{
	int ret;
	printk(KERN_INFO "----> Morse Code Driver Init. /dev/%s\n", MY_DEVICE_FILE);

	// Register as a misc driver:
	ret = misc_register(&my_miscdevice);

	// LED:
	led_register();

	// KFIFO:
	INIT_KFIFO(mc_fifo);

	if(dottime > 2000 || dottime < 0) {
		dottime = 200;
		printk(KERN_WARNING "dottime must be within [0,2000]. Defaulting to 200.\n");
	}

	return ret;
}

static void __exit my_exit(void)
{
	printk(KERN_INFO "<---- Morse Code Driver Exit\n");

	// Unregister misc driver
	misc_deregister(&my_miscdevice);

	// LED:
	led_unregister();
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Jonathan Tung");
MODULE_DESCRIPTION("Morse code driver");
MODULE_LICENSE("GPL");