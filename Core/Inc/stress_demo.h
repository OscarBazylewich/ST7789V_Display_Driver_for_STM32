/**
 * Stress test for the display module. The stress test shows all the scenes
 * in sequence, again and again. During each scene, the screen shows the
 * frame rate at that time. When you push the blue USER button, the stress
 * test goes to the next scene. The coordinates of the stress test are for a
 * 240x320 panel in portrait.
 */
#ifndef STRESS_DEMO_H
#define STRESS_DEMO_H

#ifdef __cplusplus
extern "C" {
#endif

void StressDemo_Start(void);          /* call it one time, after
                                         ST7789_Init() */
void StressDemo_Update(void);         /* call it in the loop in main() to
                                         draw one frame */
void StressDemo_Tick1ms(void);        /* call it at intervals of 1 ms, from
                                         SysTick_Handler, to read the button */

#ifdef __cplusplus
}
#endif

#endif /* STRESS_DEMO_H */
