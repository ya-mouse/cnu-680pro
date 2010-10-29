extern int platform_timer_setup(void (*timer_int)(int, void *, struct pt_regs *));
extern void platform_timer_eoi(void);
extern void arch_gettod(int *year, int *mon, int *day, int *hour,
			int *min, int *sec);
