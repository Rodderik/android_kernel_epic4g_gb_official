#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/clk.h>
#include <linux/time.h>
#include <plat/regs-keypad.h>
#include <asm/io.h>
#include <asm/delay.h>

#if CONFIG_S5PV210_GARNETT_DELTA
// delta 
#define KEYPAD_COLUMNS		2
#define KEYPAD_ROWS		3
#define MAX_KEYPAD_NR		6

#define START	0xE1600000
#define END	0xE1600FFF 
#define SIZE	END-START+1

#define KEYIFCOL_CLEAR          (readl(KEY_BASE+S3C_KEYIFCOL) & ~0xffff)
#define	KEYIFCON_CLEAR		(readl(KEY_BASE+S3C_KEYIFCON) & ~0x1f)
#define KEYIFFC_DIV		(readl(KEY_BASE+S3C_KEYIFFC) | 0x1)

#if defined(CONFIG_CPU_S3C6410)
#define KEYPAD_DELAY            (50)
#elif defined(CONFIG_CPU_S5PC100) || defined(CONFIG_CPU_S5PC110) || defined(CONFIG_CPU_S5PV210)
#define KEYPAD_DELAY            (300)  //600
#endif

static void __iomem *KEY_BASE;
extern void s3c_setup_keypad_cfg_gpio(int rows, int columns);
static ktime_t debounce_time;
struct gpio_input_state *dk;
static u32 keymask[KEYPAD_COLUMNS];
static u32 prevmask[KEYPAD_COLUMNS];
static struct clk *keypad_clock;
static unsigned int keyifcon, keyiffc;

int keypad_keycode[] = {
	50,242,212,242,114,42,
};

#ifdef CONFIG_PM
#include <plat/pm.h>

#define KEYPAD_ROW_GPIOCON     	0xFD500C60 
#define KEYPAD_ROW_GPIOPUD      0xFD500C40 
#define KEYPAD_COL_GPIOCON      0xFD500C68 
#define KEYPAD_COL_GPIOPUD      0xFD500C48 

static struct sleep_save s3c_keypad_save[] = {
        SAVE_ITEM(KEYPAD_ROW_GPIOCON),
        SAVE_ITEM(KEYPAD_COL_GPIOCON),
        SAVE_ITEM(KEYPAD_ROW_GPIOPUD),
        SAVE_ITEM(KEYPAD_COL_GPIOPUD),
};

void keypad_resume()
{
        clk_enable(keypad_clock);

        writel(KEYIFCON_INIT, KEY_BASE+S3C_KEYIFCON);
        writel(keyiffc, KEY_BASE+S3C_KEYIFFC);
        writel(KEYIFCOL_CLEAR, KEY_BASE+S3C_KEYIFCOL);

        s3c_pm_do_restore(s3c_keypad_save, ARRAY_SIZE(s3c_keypad_save));

        enable_irq(IRQ_KEYPAD);
}

void keypad_suspend()
{
        keyifcon = readl(KEY_BASE+S3C_KEYIFCON);
        keyiffc = readl(KEY_BASE+S3C_KEYIFFC);

        s3c_pm_do_save(s3c_keypad_save, ARRAY_SIZE(s3c_keypad_save));

        disable_irq(IRQ_KEYPAD);

        clk_disable(keypad_clock);
}
#endif
#endif


enum {
	DEBOUNCE_UNSTABLE     = BIT(0),	/* Got irq, while debouncing */
	DEBOUNCE_PRESSED      = BIT(1),
	DEBOUNCE_NOTPRESSED   = BIT(2),
	DEBOUNCE_WAIT_IRQ     = BIT(3),	/* Stable irq state */
	DEBOUNCE_POLL         = BIT(4),	/* Stable polling state */

	DEBOUNCE_UNKNOWN =
		DEBOUNCE_PRESSED | DEBOUNCE_NOTPRESSED,
};

struct gpio_key_state {
	struct gpio_input_state *ds;
	uint8_t debounce;
};

struct gpio_input_state {
	struct gpio_event_input_devs *input_devs;
	const struct gpio_event_input_info *info;
	struct hrtimer timer;
	int use_irq;
	int debounce_count;
	spinlock_t irq_lock;
	struct wake_lock wake_lock;
	struct gpio_key_state key_state[0];
};

#ifdef CONFIG_S5PV210_GARNETT_DELTA 
//delta
static int keypad_init(struct gpio_event_input_devs *input_devs)
{
	struct resource *keypad_mem;
	static struct clk *keypad_clock;
		
	debounce_time.tv.nsec = 5 * NSEC_PER_MSEC;
		
	keypad_mem = request_mem_region(START, SIZE, "keypad_max");
	if (keypad_mem == NULL) {
		printk(KERN_ERR "Failed to request_mem_region\n");
		goto err_mem_region;
	}
	
	KEY_BASE = ioremap(START,SIZE);
	if (KEY_BASE==NULL){
		printk(KERN_ERR "Failed to remap register block\n");
		goto err_ioremap;
	}

	keypad_clock = clk_get(&input_devs->dev, "keypad");
	if (IS_ERR(keypad_clock)) {
		printk(KERN_ERR "Failed to get clock\n");
		goto err_clk_get;
	}
	clk_enable(keypad_clock);


	writel(KEYIFCON_INIT, KEY_BASE + S3C_KEYIFCON);
	writel(KEYIFFC_DIV, KEY_BASE + S3C_KEYIFFC);
	/* Set GPIO Port for keypad mode and pull-up disable*/
	s3c_setup_keypad_cfg_gpio(KEYPAD_ROWS, KEYPAD_COLUMNS);
	writel(KEYIFCOL_CLEAR, KEY_BASE+S3C_KEYIFCOL);

return 0;

err_mem_region:
err_clk_get:
	iounmap(KEY_BASE);
err_ioremap:
	release_resource(keypad_mem);
	kfree(keypad_mem);
return -1;
}

static int keypad_scan(void)
{
	u32 col,cval,rval;

	for (col=0; col < KEYPAD_COLUMNS; col++) {
		cval = KEYCOL_DMASK & ~((1 << col) | (1 << col+ 8));
		writel(cval, KEY_BASE+S3C_KEYIFCOL);
		udelay(KEYPAD_DELAY);
		rval = ~(readl(KEY_BASE+S3C_KEYIFROW)) & 0x06 ; 
		keymask[col] = rval;
	}
		
	writel(KEYIFCOL_CLEAR, KEY_BASE+S3C_KEYIFCOL);

return 0;
}

static enum  hrtimer_restart gpio_event_input_timer_func_key(
						struct hrtimer *timer)
{
	u32 press_mask,release_mask,restart_timer = 0;
	int i,col;
	struct gpio_input_state *ds =
			container_of(timer, struct gpio_input_state, timer);
	const struct gpio_event_direct_entry *key_entry = ds->info->keymap;

	while(key_entry->gpio != IRQ_KEYPAD)
		key_entry++;
	
	keypad_scan();

	for(col=0; col < KEYPAD_COLUMNS; col++) {
		press_mask = ((keymask[col] ^ prevmask[col]) & keymask[col]);
		release_mask = ((keymask[col] ^ prevmask[col]) & prevmask[col]);

#ifdef CONFIG_CPU_FREQ
#if USE_PERF_LEVEL_KEYPAD
		if (press_mask || release_mask)
			set_dvfs_target_level(LEV_400MHZ);
#endif
#endif
		i = col * KEYPAD_ROWS;
		while (press_mask) {
			if (press_mask & 1) {
				input_report_key(ds->input_devs->dev[key_entry->dev],keypad_keycode[i],1);
				pr_devel("key Pressed  :" " key %d map %d\n",i,keypad_keycode[i]);
			}
			press_mask >>= 1;
			 i++;
		}
		
		i = col * KEYPAD_ROWS;

		while (release_mask) {
			if (release_mask & 1) {
				input_report_key(ds->input_devs->dev[key_entry->dev],keypad_keycode[i],0);
				pr_devel("key Released :" " key %d  map %d\n",i,keypad_keycode[i]);
			}
			release_mask >>= 1;
			i++;
		}
		prevmask[col] = keymask[col];
		restart_timer |= keymask[col];
	}
	
	if (restart_timer) {
		// restart the timer *** need to add
		writel(KEYIFCON_INIT, KEY_BASE+S3C_KEYIFCON);
	}else{
		writel(KEYIFCON_INIT, KEY_BASE+S3C_KEYIFCON);
		//is_timer_on = FALSE;
	}
	return HRTIMER_NORESTART;
}
#endif

static enum hrtimer_restart gpio_event_input_timer_func(struct hrtimer *timer)
{
	int i;
	int pressed;
	struct gpio_input_state *ds =
		container_of(timer, struct gpio_input_state, timer);
	unsigned gpio_flags = ds->info->flags;
	unsigned npolarity;
	int nkeys = ds->info->keymap_size;
	const struct gpio_event_direct_entry *key_entry;
	struct gpio_key_state *key_state;
	unsigned long irqflags;
	uint8_t debounce;

#if 0
	key_entry = kp->keys_info->keymap;
	key_state = kp->key_state;
	for (i = 0; i < nkeys; i++, key_entry++, key_state++)
		pr_info("gpio_read_detect_status %d %d\n", key_entry->gpio,
			gpio_read_detect_status(key_entry->gpio));
#endif
	key_entry = ds->info->keymap;
	key_state = ds->key_state;
	spin_lock_irqsave(&ds->irq_lock, irqflags);

	for (i = 0; i < nkeys; i++, key_entry++, key_state++) {
#ifdef CONFIG_S5PV210_GARNETT_DELTA
		if(key_entry->gpio == IRQ_KEYPAD)
			continue;
#endif
		debounce = key_state->debounce;
		if (debounce & DEBOUNCE_WAIT_IRQ)
			continue;
		if (key_state->debounce & DEBOUNCE_UNSTABLE) {
			debounce = key_state->debounce = DEBOUNCE_UNKNOWN;
			enable_irq(gpio_to_irq(key_entry->gpio));
			pr_info("gpio_keys_scan_keys: key %x-%x, %d "
				"(%d) continue debounce\n",
				ds->info->type, key_entry->code,
				i, key_entry->gpio);
		}
		npolarity = !(gpio_flags & GPIOEDF_ACTIVE_HIGH);
		pressed = gpio_get_value(key_entry->gpio) ^ npolarity;
		if (debounce & DEBOUNCE_POLL) {
			if (pressed == !(debounce & DEBOUNCE_PRESSED)) {
				ds->debounce_count++;
				key_state->debounce = DEBOUNCE_UNKNOWN;
				if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
					pr_info("gpio_keys_scan_keys: key %x-"
						"%x, %d (%d) start debounce\n",
						ds->info->type, key_entry->code,
						i, key_entry->gpio);
			}
			continue;
		}
		if (pressed && (debounce & DEBOUNCE_NOTPRESSED)) {
			if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_keys_scan_keys: key %x-%x, %d "
					"(%d) debounce pressed 1\n",
					ds->info->type, key_entry->code,
					i, key_entry->gpio);
			key_state->debounce = DEBOUNCE_PRESSED;
			continue;
		}
		if (!pressed && (debounce & DEBOUNCE_PRESSED)) {
			if (gpio_flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_keys_scan_keys: key %x-%x, %d "
					"(%d) debounce pressed 0\n",
					ds->info->type, key_entry->code,
					i, key_entry->gpio);
			key_state->debounce = DEBOUNCE_NOTPRESSED;
			continue;
		}
		/* key is stable */
		ds->debounce_count--;
		if (ds->use_irq)
			key_state->debounce |= DEBOUNCE_WAIT_IRQ;
		else
			key_state->debounce |= DEBOUNCE_POLL;
		if (gpio_flags & GPIOEDF_PRINT_KEYS)
			pr_info("gpio_keys_scan_keys: key %x-%x, %d (%d) "
				"changed to %d\n", ds->info->type,
				key_entry->code, i, key_entry->gpio, pressed);
		input_event(ds->input_devs->dev[key_entry->dev], ds->info->type,
			    key_entry->code, pressed);
	}

#if 0
	key_entry = kp->keys_info->keymap;
	key_state = kp->key_state;
	for (i = 0; i < nkeys; i++, key_entry++, key_state++) {
		pr_info("gpio_read_detect_status %d %d\n", key_entry->gpio,
			gpio_read_detect_status(key_entry->gpio));
	}
#endif

	if (ds->debounce_count)
		hrtimer_start(timer, ds->info->debounce_time, HRTIMER_MODE_REL);
	else if (!ds->use_irq)
		hrtimer_start(timer, ds->info->poll_time, HRTIMER_MODE_REL);
	else
		wake_unlock(&ds->wake_lock);

	spin_unlock_irqrestore(&ds->irq_lock, irqflags);

	return HRTIMER_NORESTART;
}

#ifdef CONFIG_S5PV210_GARNETT_DELTA
//delta
static irqreturn_t gpio_event_input_irq_handler_key(int irq, void *dev_id)
{
	 /* disable keypad interrupt and schedule for keypad timer handler */
        writel(readl(KEY_BASE+S3C_KEYIFCON) & ~(INT_F_EN|INT_R_EN), KEY_BASE+S3C_KEYIFCON);
	hrtimer_start(&dk->timer, debounce_time, HRTIMER_MODE_REL);

	/*Clear the keypad interrupt status*/
        writel(KEYIFSTSCLR_CLEAR, KEY_BASE+S3C_KEYIFSTSCLR);

	return IRQ_HANDLED;
}
#endif

static irqreturn_t gpio_event_input_irq_handler(int irq, void *dev_id)
{
	struct gpio_key_state *ks = dev_id;
	struct gpio_input_state *ds = ks->ds;
	int keymap_index = ks - ds->key_state;
	const struct gpio_event_direct_entry *key_entry;
	unsigned long irqflags;
	int pressed;


	if (!ds->use_irq)
		return IRQ_HANDLED;

	key_entry = &ds->info->keymap[keymap_index];

	if (ds->info->debounce_time.tv64) {
		spin_lock_irqsave(&ds->irq_lock, irqflags);
		if (ks->debounce & DEBOUNCE_WAIT_IRQ) {
			ks->debounce = DEBOUNCE_UNKNOWN;
			if (ds->debounce_count++ == 0) {
				wake_lock(&ds->wake_lock);
				hrtimer_start(
					&ds->timer, ds->info->debounce_time,
					HRTIMER_MODE_REL);
			}
			if (ds->info->flags & GPIOEDF_PRINT_KEY_DEBOUNCE)
				pr_info("gpio_event_input_irq_handler: "
					"key %x-%x, %d (%d) start debounce\n",
					ds->info->type, key_entry->code,
					keymap_index, key_entry->gpio);
		} else {
			disable_irq_nosync(irq);
			ks->debounce = DEBOUNCE_UNSTABLE;
		}
		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
	} else {
		pressed = gpio_get_value(key_entry->gpio) ^
			!(ds->info->flags & GPIOEDF_ACTIVE_HIGH);
		if (ds->info->flags & GPIOEDF_PRINT_KEYS)
			pr_info("gpio_event_input_irq_handler: key %x-%x, %d "
				"(%d) changed to %d\n",
				ds->info->type, key_entry->code, keymap_index,
				key_entry->gpio, pressed);
		input_event(ds->input_devs->dev[key_entry->dev], ds->info->type,
			    key_entry->code, pressed);
	}
	
	return IRQ_HANDLED;
}

static int gpio_event_input_request_irqs(struct gpio_input_state *ds)
{
	int i;
	int err;
	unsigned int irq;
	unsigned long req_flags = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING;

	for (i = 0; i < ds->info->keymap_size; i++) {

#ifdef CONFIG_S5PV210_GARNETT_DELTA
		if (!(ds->info->keymap[i].gpio == IRQ_KEYPAD)){
			err = irq = gpio_to_irq(ds->info->keymap[i].gpio);
			
			if(err < 0)
				goto err_gpio_get_irq_num_failed;
		}else
			irq = IRQ_KEYPAD;			
#else
		err = irq = gpio_to_irq(ds->info->keymap[i].gpio);
		if (err < 0)
			goto err_gpio_get_irq_num_failed;
#endif


#ifdef CONFIG_S5PV210_GARNETT_DELTA
		if(ds->info->keymap[i].gpio == IRQ_KEYPAD)
			err = request_irq(IRQ_KEYPAD, gpio_event_input_irq_handler_key,
					req_flags, "key-matrix", NULL);
		else
			err = request_irq(irq, gpio_event_input_irq_handler,
					req_flags, "gpio_keys", &ds->key_state[i]);
#else
		err = request_irq(irq, gpio_event_input_irq_handler,
				  req_flags, "gpio_keys", &ds->key_state[i]);
#endif

		if (err) {
			pr_err("gpio_event_input_request_irqs: request_irq "
				"failed for input %d, irq %d\n",
				ds->info->keymap[i].gpio, irq);
			goto err_request_irq_failed;
		}
		if (ds->info->info.no_suspend) {
			err = enable_irq_wake(irq);
			if (err) {
				pr_err("gpio_event_input_request_irqs: "
					"enable_irq_wake failed for input %d, "
					"irq %d\n",
					ds->info->keymap[i].gpio, irq);
				goto err_enable_irq_wake_failed;
			}
		}
	}

	return 0;

	for (i = ds->info->keymap_size - 1; i >= 0; i--) {
		irq = gpio_to_irq(ds->info->keymap[i].gpio);
		if (ds->info->info.no_suspend)
			disable_irq_wake(irq);
err_enable_irq_wake_failed:
		free_irq(irq, &ds->key_state[i]);
err_request_irq_failed:
err_gpio_get_irq_num_failed:
		;
	}
	return err;
}

int gpio_event_input_func(struct gpio_event_input_devs *input_devs,
			struct gpio_event_info *info, void **data, int func)
{
	int ret;
	int i;
	unsigned long irqflags;
	struct gpio_event_input_info *di;
	struct gpio_input_state *ds  = *data;
#ifdef CONFIG_S5PV210_GARNETT_DELTA
	dk  = *data;
#endif

	di = container_of(info, struct gpio_event_input_info, info);
	
	if (func == GPIO_EVENT_FUNC_SUSPEND) {
		if (ds->use_irq)
			for (i = 0; i < di->keymap_size; i++){
				disable_irq(gpio_to_irq(di->keymap[i].gpio));
			}
		hrtimer_cancel(&ds->timer);
		return 0;
	}

	if (func == GPIO_EVENT_FUNC_RESUME) {
		spin_lock_irqsave(&ds->irq_lock, irqflags);
		if (ds->use_irq)
			for (i = 0; i < di->keymap_size; i++){
				enable_irq(gpio_to_irq(di->keymap[i].gpio));
			}

		hrtimer_start(&ds->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
		return 0;
	}

	if (func == GPIO_EVENT_FUNC_INIT) {
		if (ktime_to_ns(di->poll_time) <= 0)
			di->poll_time = ktime_set(0, 20 * NSEC_PER_MSEC);

		*data = ds = kzalloc(sizeof(*ds) + sizeof(ds->key_state[0]) *
					di->keymap_size, GFP_KERNEL);
		if (ds == NULL) {
			ret = -ENOMEM;
			pr_err("gpio_event_input_func: "
				"Failed to allocate private data\n");
			goto err_ds_alloc_failed;
		}
		
#ifdef CONFIG_S5PV210_GARNETT_DELTA
		/* we need to release the above heap*/

		dk = kzalloc(sizeof(*ds) + sizeof(ds->key_state[0]) *
					di->keymap_size, GFP_KERNEL);
		if (dk == NULL){
			ret = -ENOMEM;
			 pr_err("gpio_event_input_func: "
				"Failed to allocate private data\n");
			goto err_dk_alloc_failed;
		}
		dk->debounce_count = di->keymap_size;
		dk->input_devs = input_devs;
		dk->info = di;
#endif	
	
		ds->debounce_count = di->keymap_size;
		ds->input_devs = input_devs;
		ds->info = di;
		wake_lock_init(&ds->wake_lock, WAKE_LOCK_SUSPEND, "gpio_input");
		spin_lock_init(&ds->irq_lock);

		for (i = 0; i < di->keymap_size; i++) {
			int dev = di->keymap[i].dev;
			if (dev >= input_devs->count) {
				pr_err("gpio_event_input_func: bad device "
					"index %d >= %d for key code %d\n",
					dev, input_devs->count,
					di->keymap[i].code);
				ret = -EINVAL;
				goto err_bad_keymap;
			}
			input_set_capability(input_devs->dev[dev], di->type,
					     di->keymap[i].code);
#if CONFIG_S5PV210_GARNETT_DELTA
			if(di->keymap[i].code == 212){
				input_set_capability(input_devs->dev[dev], di->type,242);
				input_set_capability(input_devs->dev[dev], di->type,114);
			}
#endif
			ds->key_state[i].ds = ds;
			ds->key_state[i].debounce = DEBOUNCE_UNKNOWN;
		}

		for (i = 0; i < di->keymap_size; i++) {
#ifdef CONFIG_S5PV210_GARNETT_DELTA
			if ( di->keymap[i].gpio == IRQ_KEYPAD)
				continue;
#endif
			ret = gpio_request(di->keymap[i].gpio, "gpio_kp_in");
			if (ret) {
				pr_err("gpio_event_input_func: gpio_request "
					"failed for %d\n", di->keymap[i].gpio);
				goto err_gpio_request_failed;
			}
			ret = gpio_direction_input(di->keymap[i].gpio);
			if (ret) {
				pr_err("gpio_event_input_func: "
					"gpio_direction_input failed for %d\n",
					di->keymap[i].gpio);
				goto err_gpio_configure_failed;
			}
		}

#ifdef CONFIG_S5PV210_GARNETT_DELTA
		/* keypad controller initialisation part*/
		if(keypad_init(input_devs)){
			printk(KERN_INFO"keypad Initialisation failed\n");
			goto err_gpio_configure_failed;			
		}

		hrtimer_init(&dk->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		dk->timer.function = gpio_event_input_timer_func_key;
		hrtimer_start(&dk->timer, ktime_set(0, 0), HRTIMER_MODE_REL);
#endif

		ret = gpio_event_input_request_irqs(ds);

		spin_lock_irqsave(&ds->irq_lock, irqflags);
		ds->use_irq = ret == 0;

		pr_info("GPIO Input Driver: Start gpio inputs for %s%s in %s "
			"mode\n", input_devs->dev[0]->name,
			(input_devs->count > 1) ? "..." : "",
			ret == 0 ? "interrupt" : "polling");

		hrtimer_init(&ds->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ds->timer.function = gpio_event_input_timer_func;
		hrtimer_start(&ds->timer, ktime_set(0, 0), HRTIMER_MODE_REL);

		spin_unlock_irqrestore(&ds->irq_lock, irqflags);
		return 0;
	}

	ret = 0;
	spin_lock_irqsave(&ds->irq_lock, irqflags);
	hrtimer_cancel(&ds->timer);
	if (ds->use_irq) {
		for (i = di->keymap_size - 1; i >= 0; i--) {
			int irq = gpio_to_irq(di->keymap[i].gpio);
			if (ds->info->info.no_suspend)
				disable_irq_wake(irq);
			free_irq(irq, &ds->key_state[i]);
		}
	}
	spin_unlock_irqrestore(&ds->irq_lock, irqflags);

	for (i = di->keymap_size - 1; i >= 0; i--) {
err_gpio_configure_failed:
		gpio_free(di->keymap[i].gpio);
err_gpio_request_failed:
		;
	}
err_bad_keymap:
	wake_lock_destroy(&ds->wake_lock);
	kfree(ds);
#ifdef CONFIG_S5PV210_GARNETT_DELTA 
err_dk_alloc_failed:
	kfree(ds);
#endif
err_ds_alloc_failed:
	return ret;
}
