#ifndef WHITGL_TIMER_H_
#define WHITGL_TIMER_H_

#include <whitgl/math.h>

void whitgl_timer_init();
whitgl_float whitgl_timer_tick();
bool whitgl_timer_should_do_frame(whitgl_float fps);
whitgl_int whitgl_timer_fps();
whitgl_float whitgl_timer_frame_completage(whitgl_float fps);
void whitgl_timer_sleep(whitgl_float seconds);

#endif // WHITGL_TIMER_H_
