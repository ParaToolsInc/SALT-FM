/* The timer calls of config.yaml. They do nothing, so that an output
   also links and runs without TAU. */
static inline void timer_start(const char *name) { (void)name; }
static inline void timer_stop(void) {}
#ifdef __cplusplus
struct timer_scope
{
    timer_scope(const char *name) { (void)name; }
};
#endif
