#ifndef WHITGL_INPUT_H_
#define WHITGL_INPUT_H_

#include <stdbool.h>

#include <whitgl/math.h>

typedef enum
{
	// game
	WHITGL_INPUT_UP = 0,
	WHITGL_INPUT_RIGHT,
	WHITGL_INPUT_DOWN,
	WHITGL_INPUT_LEFT,
	WHITGL_INPUT_Z_UP,
	WHITGL_INPUT_Z_DOWN,
	WHITGL_INPUT_KEYBOARD_UP,
	WHITGL_INPUT_KEYBOARD_RIGHT,
	WHITGL_INPUT_KEYBOARD_DOWN,
	WHITGL_INPUT_KEYBOARD_LEFT,
	WHITGL_INPUT_ESC,
	WHITGL_INPUT_A,
	WHITGL_INPUT_B,
	WHITGL_INPUT_X,
	WHITGL_INPUT_Y,
	WHITGL_INPUT_START,
	WHITGL_INPUT_SELECT,
	WHITGL_INPUT_0,
	WHITGL_INPUT_1,
	WHITGL_INPUT_2,
	WHITGL_INPUT_3,
	WHITGL_INPUT_4,
	WHITGL_INPUT_5,
	WHITGL_INPUT_6,
	WHITGL_INPUT_7,
	WHITGL_INPUT_8,
	WHITGL_INPUT_9,
	WHITGL_INPUT_DEBUG,
	WHITGL_INPUT_RESET,
	WHITGL_INPUT_MOUSE_LEFT,
	WHITGL_INPUT_MOUSE_RIGHT,
	WHITGL_INPUT_MOUSE_MIDDLE,
	WHITGL_INPUT_MOUSE_SCROLL_UP,
	WHITGL_INPUT_MOUSE_SCROLL_DOWN,
	WHITGL_INPUT_ANY,
	WHITGL_INPUT_MAX,
} whitgl_input;

typedef enum
{
	WHITGL_JOYSTICK_NONE,
	WHITGL_JOYSTICK_XBOX,
	WHITGL_JOYSTICK_PLAYSTATION,
} whitgl_input_joystick_style;

void whitgl_input_init();
void whitgl_input_shutdown();
void whitgl_input_update();
bool whitgl_input_held(whitgl_input input);
bool whitgl_input_pressed(whitgl_input input);
whitgl_ivec whitgl_input_mouse_pos(int pixel_size);
whitgl_fvec whitgl_input_joystick();
whitgl_fvec whitgl_input_joystick2();
whitgl_fvec whitgl_input_joystick3();
whitgl_input_joystick_style whitgl_input_get_joystick_style();

#endif // WHITGL_INPUT_H_
