// FIXME's and TODO's
// - Need to put our game code and memory into the state that we're passing around to our wayland callbacks.
//   Then we'll be able to re-render immediately in response to toplevel/surface config events.
//   Though how often will a user e.g. be resizing the window and be like "damn I wish the screen
//   didn't flicker while doing this :("?

#include <xkbcommon/xkbcommon-keysyms.h>
#define _POSIX_C_SOURCE 200112L

#include "platform.h"

// Wayland stuff
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h> // keyboard maps and whatnot
#include <xkbcommon/xkbcommon-keysyms.h>
#include "xdg_shell_client_protocol.h"
#include "xdg_decoration_client_protocol.h" // server-side decoration

#include <time.h>
#include <sys/timerfd.h>
#include <sys/poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <linux/limits.h>
#include <errno.h>

#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef enum PointerEventMask {
	POINTER_EVENT_ENTER         = 1 << 0,
	POINTER_EVENT_LEAVE         = 1 << 1,
	POINTER_EVENT_MOTION        = 1 << 2,
	POINTER_EVENT_BUTTON        = 1 << 3,
	POINTER_EVENT_AXIS          = 1 << 4,
	POINTER_EVENT_AXIS_SOURCE   = 1 << 5,
	POINTER_EVENT_AXIS_STOP     = 1 << 6,
	POINTER_EVENT_AXIS_DISCRETE = 1 << 7,
} PointerEventMask;

typedef struct PointerEvent {
	uint32_t event_mask;
	wl_fixed_t surface_x, surface_y;
	uint32_t button, state;
	uint32_t time;
	uint32_t serial;
	// There are two types of axes: horizontal and vertical.
	struct {
		// Sometimes we'll receive a pointer frame that updates one axis but not the other.
		// If valid then this axis was updated this frame.
		int valid;
		wl_fixed_t value;
		int32_t discrete;
	} axes[2];
	uint32_t axis_source;
} PointerEvent;

// Wayland Client State
typedef struct ClientState {
	/* Globals */
	struct wl_display *wl_display;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct xdg_wm_base *xdg_wm_base;
	struct wl_registry *wl_registry;
	struct wl_seat *wl_seat;
	struct zxdg_decoration_manager_v1 *xdg_decoration_manager;
	/* Objects */
	struct wl_surface *wl_surface;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct wl_keyboard *wl_keyboard;
	struct wl_pointer *wl_pointer;
	struct wl_buffer **wl_buffers; // nbuffers is buffer count
	struct zxdg_toplevel_decoration_v1 *xdg_toplevel_decoration;
	/* State */ struct xkb_state *xkb_state;
	struct xkb_context *xkb_context;
	struct xkb_keymap *xkb_keymap;
	// NOTE(mal): At the moment we just need GameInput in here to be able to grab keyboard events.
	// TODO(mal): Maybe in the future if GameInput has a bunch of stuff, we could just have a
	// separate struct for xkb-keys and translate that into game input keys in a separate step.
	GameInput *game_input;
	char *shm_pool_data;
	int wl_display_fd;
	uint32_t last_render_ms;
	int width;
	int height;
	int bytes_per_pixel;
	int stride;
	unsigned nbuffers;
	unsigned current_buffer_index;
	int shm_pool_size;
	int closed;
	int can_draw;
	struct PointerEvent pointer_event;
} ClientState;

void randname(char *buf) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

int create_shm_file() {
	char name[] = "/wl_shm-XXXXXX";
	randname(name + sizeof(name) - 7); // replace the X's with random stuff
	int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
	if (fd >= 0) {
		// https://pubs.opengroup.org/onlinepubs/9699919799/functions/shm_unlink.html
		// Names of memory objects that were allocated with open() are deleted
		// with unlink() in the usual fashion. Names of memory objects that
		// were allocated with shm_open() are deleted with shm_unlink(). Note
		// that the actual memory object is not destroyed until the last close
		// and unmap on it have occurred if it was already in use.
		shm_unlink(name);
		return fd;
	}
	return -1;
}

int allocate_shm_file(size_t size) {
	int fd = create_shm_file();
	if (fd < 0) {
		return -1;
	}

	// resize the shared memory segment
	int success = ftruncate(fd, size) == 0;
	if (!success) {
		close(fd);
		return -1;
	}

	return fd;
}

const struct wl_buffer_listener wl_buffer_listener;
void create_buffers(ClientState *state) {
	state->shm_pool_size = state->height * state->stride * state->nbuffers;
	int shm_fd = allocate_shm_file(state->shm_pool_size);
	struct wl_shm_pool *pool = wl_shm_create_pool(state->shm, shm_fd, state->shm_pool_size);
	for (int buffer_offset = 0 ; buffer_offset < state->nbuffers; buffer_offset++) {
		state->wl_buffers[buffer_offset] = wl_shm_pool_create_buffer(
			pool, buffer_offset,
			state->width, state->height, state->stride,
			WL_SHM_FORMAT_XRGB8888
		);
		wl_buffer_add_listener(state->wl_buffers[buffer_offset], &wl_buffer_listener, NULL);
	}
	state->shm_pool_data = mmap(NULL, state->shm_pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	// NOTE(mal): The buffer is now independent and references the underlying shared memory of the
	// pool. It is safe to immediately destroy the shared memory pool. From the docs:
	// "The mmapped memory will be released when all buffers that have been created from this pool are gone."
	// Technically it still seems to work if I put the mmap AFTER the pool destruction but whatever.
	wl_shm_pool_destroy(pool);
	// We can close the shm_fd because we've mapped the memory behind it into our process now.
	// Besides, the compositor is probably keeping it open anyway.
	close(shm_fd);
}

//////////////////////////////////////////////////
// WM_BASE_LISTENER
//////////////////////////////////////////////////
void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
	xdg_wm_base_pong(xdg_wm_base, serial);
}

const struct xdg_wm_base_listener xdg_wm_base_listener = {
	.ping = xdg_wm_base_ping,
};

//////////////////////////////////////////////////
// XDG_SURFACE_LISTENER
//////////////////////////////////////////////////
void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
	// TODO(mal): Do we want to handle an initial draw here or something just so we can get a
	// window to appear right away? Maybe set a "needs_initial_draw" var in main() and do something
	// here?
	ClientState *state = data;
	xdg_surface_ack_configure(xdg_surface, serial);
	wl_surface_attach(state->wl_surface, state->wl_buffers[state->current_buffer_index], 0, 0);
	wl_surface_commit(state->wl_surface);
}

const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

//////////////////////////////////////////////////
// WL_BUFFER_LISTENER
//////////////////////////////////////////////////
void wl_buffer_release(void *data, struct wl_buffer *wl_buffer) {
	// Sent by the compositor when it's no longer using this buffer
	wl_buffer_destroy(wl_buffer);
}

const struct wl_buffer_listener wl_buffer_listener = {
	.release = wl_buffer_release
};

//////////////////////////////////////////////////
// WL_CALLBACK_LISTENER
//////////////////////////////////////////////////
const struct wl_callback_listener wl_surface_frame_listener;

void wl_surface_frame_done(void *data, struct wl_callback *cb, uint32_t time) {
	// don't need this callback anymore
	wl_callback_destroy(cb);
	// Request a new callback for the next frame when the compositor is ready for it
	ClientState *state = data;

	// FIXME(mal): This may not actually be the real last render time! This is just the last time
	// the compositor told us it was ready for a frame! While that may be a useful measurement, we
	// may want to keep track of the last time that we actually rendered and SUBMITTED a frame to
	// the compositor, which we do in our main game loop.
	state->last_render_ms = time;
	state->can_draw = 1;
}

const struct wl_callback_listener wl_surface_frame_listener = {
	.done = wl_surface_frame_done
};

//////////////////////////////////////////////////
// WL_POINTER_LISTENER
//////////////////////////////////////////////////
void wl_pointer_enter(
	void *data, struct wl_pointer *wl_pointer, uint32_t serial,
	struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y
)
{
	ClientState *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_ENTER;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.surface_x = surface_x;
	client_state->pointer_event.surface_y = surface_y;
}

void wl_pointer_leave(
	void *data, struct wl_pointer *wl_pointer,
	uint32_t serial, struct wl_surface *surface
)
{
	ClientState *client_state = data;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.event_mask |= POINTER_EVENT_LEAVE;
}

void wl_pointer_motion(
	void *data, struct wl_pointer *wl_pointer, uint32_t time,
	wl_fixed_t surface_x, wl_fixed_t surface_y
)
{
	ClientState *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_MOTION;
	client_state->pointer_event.time = time;
	client_state->pointer_event.surface_x = surface_x;
	client_state->pointer_event.surface_y = surface_y;
}

void wl_pointer_button(
	void *data, struct wl_pointer *wl_pointer, uint32_t serial,
	uint32_t time, uint32_t button, uint32_t state
)
{
	ClientState *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_BUTTON;
	client_state->pointer_event.time = time;
	client_state->pointer_event.serial = serial;
	client_state->pointer_event.button = button;
	client_state->pointer_event.state = state;
}

void wl_pointer_axis(
	void *data, struct wl_pointer *wl_pointer,
	uint32_t time, uint32_t axis, wl_fixed_t value
)
{
	ClientState *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS;
	client_state->pointer_event.time = time;
	client_state->pointer_event.axes[axis].valid = 1;
	client_state->pointer_event.axes[axis].value = value;
}

void wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer, uint32_t axis_source) {
	ClientState *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS_SOURCE;
	client_state->pointer_event.axis_source = axis_source;
}

void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis) {
	ClientState *client_state = data;
	client_state->pointer_event.time = time;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS_SOURCE;
	client_state->pointer_event.axes[axis].valid = 1;
}

void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer, uint32_t axis, int32_t discrete) {
	ClientState *client_state = data;
	client_state->pointer_event.event_mask |= POINTER_EVENT_AXIS_DISCRETE;
	client_state->pointer_event.axes[axis].valid = 1;
	client_state->pointer_event.axes[axis].discrete = discrete;
}

void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
	/*
	ClientState *client_state = data;
	PointerEvent *event = &client_state->pointer_event;

	if (event->event_mask & POINTER_EVENT_ENTER) {
		printf(
			"enter %f, %f ",
			wl_fixed_to_double(event->surface_x),
			wl_fixed_to_double(event->surface_y)
		);
	}

	if (event->event_mask & POINTER_EVENT_LEAVE) {
		printf("leave");
	}

	if (event->event_mask & POINTER_EVENT_MOTION) {
		printf(
			"motion %f, %f ",
			wl_fixed_to_double(event->surface_x),
			wl_fixed_to_double(event->surface_y)
		);
	}

	if (event->event_mask & POINTER_EVENT_BUTTON) {
		char *state = event->state == WL_POINTER_BUTTON_STATE_RELEASED ? "released" : "pressed";
		printf("button %d %s ", event->button, state);
	}

	uint32_t axis_events =
		POINTER_EVENT_AXIS
		| POINTER_EVENT_AXIS_SOURCE
		| POINTER_EVENT_AXIS_STOP
		| POINTER_EVENT_AXIS_DISCRETE;
	char *axis_name[2] = {
		[WL_POINTER_AXIS_VERTICAL_SCROLL]   = "vertical",
		[WL_POINTER_AXIS_HORIZONTAL_SCROLL] = "horizontal",
	};
	char *axis_source[4] = {
		[WL_POINTER_AXIS_SOURCE_WHEEL]      = "wheel",
		[WL_POINTER_AXIS_SOURCE_FINGER]     = "finger",
		[WL_POINTER_AXIS_SOURCE_CONTINUOUS] = "continuous",
		[WL_POINTER_AXIS_SOURCE_WHEEL_TILT] = "wheel tilt",
	};

	if (event->event_mask & axis_events) {
		for (size_t i = 0; i < 2; i++) {
			if (!event->axes[i].valid) {
				continue;
			}

			printf("%s axis ", axis_name[i]);
			if (event->event_mask & POINTER_EVENT_AXIS) {
				printf("value %f ", wl_fixed_to_double(event->axes[i].value));
			}
			if (event->event_mask & POINTER_EVENT_AXIS_DISCRETE) {
				printf("discrete %d ", event->axes[i].discrete);
			}
			if (event->event_mask & POINTER_EVENT_AXIS_SOURCE) {
				printf("via %s ", axis_source[event->axis_source]);
			}
			if (event->event_mask & POINTER_EVENT_AXIS_STOP) {
				printf("(stopped)");
			}
		}
	}

	printf("\n");
	memset(event, 0, sizeof(*event));
	*/
}

const struct wl_pointer_listener wl_pointer_listener = {
	.enter         = wl_pointer_enter,
	.leave         = wl_pointer_leave,
	.motion        = wl_pointer_motion,
	.button        = wl_pointer_button,
	.axis          = wl_pointer_axis,
	.frame         = wl_pointer_frame,
	.axis_source   = wl_pointer_axis_source,
	.axis_stop     = wl_pointer_axis_stop,
	.axis_discrete = wl_pointer_axis_discrete,
};

//////////////////////////////////////////////////
// WL_KEYBOARD_LISTENER
//////////////////////////////////////////////////
// Sent after we obtain our keyboard
void wl_keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int32_t fd, uint32_t size) {
	ClientState *client_state = data;
	// Ensure we got our expected format
	ASSERT(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1);
	// Mapping the memory pointed to by the file descriptor into our addr space
	char *map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	ASSERT(map_shm != MAP_FAILED);

	struct xkb_keymap *xkb_keymap = xkb_keymap_new_from_string(
		client_state->xkb_context, map_shm,
		XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS
	);
	// Now that we have our keymap set up we don't need the shared memory or file descriptor anymore
	munmap(map_shm, size);
	close(fd);

	struct xkb_state *xkb_state = xkb_state_new(xkb_keymap);
	// We unref previous keymaps and states here because it's possible that the compositor might
	// change the keymap at runtime!
	xkb_keymap_unref(client_state->xkb_keymap);
	xkb_state_unref(client_state->xkb_state);
	client_state->xkb_keymap = xkb_keymap;
	client_state->xkb_state = xkb_state;
}

// Sent when the keyboard "enters" our surface e.g. we get keyboard focus
void wl_keyboard_enter(
	void *data,
	struct wl_keyboard *wl_keyboard,
	uint32_t serial,
	struct wl_surface *surface,
	struct wl_array *keys // keys that are pressed at the time of entry
)
{
	/*
	ClientState *client_state = data;
	printf("keyboard enter; keys pressed are \n");
	uint32_t *key;
	wl_array_for_each(key, keys) {
		char buf[128];
		// Remember that the scancode sent from this event is the Linux
		// evdev scancode. To translate to an XKB scancode you have to
		// add 8 to the evdev scancode.
		xkb_keysym_t sym = xkb_state_key_get_one_sym(client_state->xkb_state, *key + 8);
		xkb_state_key_get_utf8(client_state->xkb_state, *key + 8, buf, sizeof(buf));
		printf("utf8: '%s'\n", buf);
	}
	*/
}

GameKey xkb_keysym_to_game_key(xkb_keysym_t keysym) {
	GameKey result = GAME_KEY_UNKNOWN;
	switch (keysym) {
		// ARROWS
		case XKB_KEY_Left : result = GAME_KEY_LEFT; break;
		case XKB_KEY_Up   : result = GAME_KEY_RIGHT; break;
		case XKB_KEY_Right: result = GAME_KEY_RIGHT; break;
		case XKB_KEY_Down : result = GAME_KEY_DOWN; break;
		// NUMERIC
		case XKB_KEY_0: result = GAME_KEY_0; break;
		case XKB_KEY_1: result = GAME_KEY_1; break;
		case XKB_KEY_2: result = GAME_KEY_2; break;
		case XKB_KEY_3: result = GAME_KEY_3; break;
		case XKB_KEY_4: result = GAME_KEY_4; break;
		case XKB_KEY_5: result = GAME_KEY_5; break;
		case XKB_KEY_6: result = GAME_KEY_6; break;
		case XKB_KEY_7: result = GAME_KEY_7; break;
		case XKB_KEY_8: result = GAME_KEY_8; break;
		case XKB_KEY_9: result = GAME_KEY_9; break;

		// ALPHABETIC
		case XKB_KEY_a:
		case XKB_KEY_A: result = GAME_KEY_A; break;
		case XKB_KEY_b:
		case XKB_KEY_B: result = GAME_KEY_B; break;
		case XKB_KEY_c:
		case XKB_KEY_C: result = GAME_KEY_C; break;
		case XKB_KEY_d:
		case XKB_KEY_D: result = GAME_KEY_D; break;
		case XKB_KEY_e:
		case XKB_KEY_E: result = GAME_KEY_E; break;
		case XKB_KEY_f:
		case XKB_KEY_F: result = GAME_KEY_F; break;
		case XKB_KEY_g:
		case XKB_KEY_G: result = GAME_KEY_G; break;
		case XKB_KEY_h:
		case XKB_KEY_H: result = GAME_KEY_H; break;
		case XKB_KEY_i:
		case XKB_KEY_I: result = GAME_KEY_I; break;
		case XKB_KEY_j:
		case XKB_KEY_J: result = GAME_KEY_J; break;
		case XKB_KEY_k:
		case XKB_KEY_K: result = GAME_KEY_K; break;
		case XKB_KEY_l:
		case XKB_KEY_L: result = GAME_KEY_L; break;
		case XKB_KEY_m:
		case XKB_KEY_M: result = GAME_KEY_M; break;
		case XKB_KEY_n:
		case XKB_KEY_N: result = GAME_KEY_N; break;
		case XKB_KEY_o:
		case XKB_KEY_O: result = GAME_KEY_O; break;
		case XKB_KEY_p:
		case XKB_KEY_P: result = GAME_KEY_P; break;
		case XKB_KEY_q:
		case XKB_KEY_Q: result = GAME_KEY_Q; break;
		case XKB_KEY_r:
		case XKB_KEY_R: result = GAME_KEY_R; break;
		case XKB_KEY_s:
		case XKB_KEY_S: result = GAME_KEY_S; break;
		case XKB_KEY_t:
		case XKB_KEY_T: result = GAME_KEY_T; break;
		case XKB_KEY_u:
		case XKB_KEY_U: result = GAME_KEY_U; break;
		case XKB_KEY_v:
		case XKB_KEY_V: result = GAME_KEY_V; break;
		case XKB_KEY_w:
		case XKB_KEY_W: result = GAME_KEY_W; break;
		case XKB_KEY_x:
		case XKB_KEY_X: result = GAME_KEY_X; break;
		case XKB_KEY_y:
		case XKB_KEY_Y: result = GAME_KEY_Y; break;
		case XKB_KEY_z:
		case XKB_KEY_Z: result = GAME_KEY_Z; break;
		// PUNCTUATION
		case XKB_KEY_Tab         : result = GAME_KEY_TAB; break;
		case XKB_KEY_space       : result = GAME_KEY_SPACE; break;
		case XKB_KEY_equal       : result = GAME_KEY_EQUAL; break;
		case XKB_KEY_comma       : result = GAME_KEY_COMMA; break;
		case XKB_KEY_minus       : result = GAME_KEY_MINUS; break;
		case XKB_KEY_period      : result = GAME_KEY_PERIOD; break;
		case XKB_KEY_slash       : result = GAME_KEY_FORWARD_SLASH; break;
		case XKB_KEY_grave       : result = GAME_KEY_GRAVE; break;
		case XKB_KEY_bracketleft : result = GAME_KEY_BRACKET_L; break;
		case XKB_KEY_backslash   : result = GAME_KEY_BACKSLASH; break;
		case XKB_KEY_bracketright: result = GAME_KEY_BRACKET_R; break;
		case XKB_KEY_apostrophe  : result = GAME_KEY_QUOTE; break;
		// FUNCTION
		case XKB_KEY_F1 : result = GAME_KEY_F1; break;
		case XKB_KEY_F2 : result = GAME_KEY_F2; break;
		case XKB_KEY_F3 : result = GAME_KEY_F3; break;
		case XKB_KEY_F4 : result = GAME_KEY_F4; break;
		case XKB_KEY_F5 : result = GAME_KEY_F5; break;
		case XKB_KEY_F6 : result = GAME_KEY_F6; break;
		case XKB_KEY_F7 : result = GAME_KEY_F7; break;
		case XKB_KEY_F8 : result = GAME_KEY_F8; break;
		case XKB_KEY_F9 : result = GAME_KEY_F9; break;
		case XKB_KEY_F10: result = GAME_KEY_F10; break;
		case XKB_KEY_F11: result = GAME_KEY_F11; break;
		case XKB_KEY_F12: result = GAME_KEY_F12; break;
		// MISC
		case XKB_KEY_Escape   : result = GAME_KEY_ESCAPE; break;
		case XKB_KEY_BackSpace: result = GAME_KEY_BACKSPACE; break;
		case XKB_KEY_Delete   : result = GAME_KEY_DELETE; break;
		case XKB_KEY_Caps_Lock: result = GAME_KEY_CAPS; break;
		case XKB_KEY_Shift_L  : result = GAME_KEY_SHIFT_L; break;
		case XKB_KEY_Shift_R  : result = GAME_KEY_SHIFT_R; break;
		case XKB_KEY_Alt_L    : result = GAME_KEY_ALT_L; break;
		case XKB_KEY_Alt_R    : result = GAME_KEY_ALT_R; break;
		case XKB_KEY_Control_L: result = GAME_KEY_CTRL_L; break;
		case XKB_KEY_Control_R: result = GAME_KEY_CTRL_R; break;
		case XKB_KEY_Return   : result = GAME_KEY_ENTER; break;
	}
	return result;
}

void wl_keyboard_key(
	void *data,
	struct wl_keyboard *wl_keyboard,
	uint32_t serial,
	uint32_t time,
	uint32_t key,
	uint32_t state
)
{
	// Note about XKB keycodes: https://gist.github.com/axelkar/dbdbfe7e61baf52c8b0cf1be423a14c1

	ClientState *client_state = data;
	// Remember that the scancode sent from this event is the Linux evdev scancode. To translate to
	// an XKB scancode you have to add 8 to the evdev scancode.
	uint32_t keycode = key + 8;
	// TODO(mal): Maybe add support for position-oriented (e.g. raw scan codes) input instead of layout-oriented (keysyms)
	xkb_keysym_t sym = xkb_state_key_get_one_sym(client_state->xkb_state, keycode);
	GameKey game_key = xkb_keysym_to_game_key(sym);
	if (game_key != GAME_KEY_UNKNOWN) {
		// TODO(mal): Change to b8 or something (bool8_t?) once I add those
		bool is_down = state == WL_KEYBOARD_KEY_STATE_PRESSED || state == WL_KEYBOARD_KEY_STATE_REPEATED;
		client_state->game_input->keys_down[game_key] = is_down;
	}
}

// When we lose keyboard focus
void wl_keyboard_leave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *wl_surface) {
	/*
	printf("keyboard leave\n");
	*/
}

void wl_keyboard_modifiers(
	void *data, struct wl_keyboard *wl_keyboard,
	uint32_t serial, uint32_t mods_depressed,
	uint32_t mods_latched, uint32_t mods_locked,
	uint32_t group
)
{
	ClientState *client_state = data;
	xkb_state_update_mask(client_state->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

void wl_keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
	// TODO, implementation highly application specific
}

const struct wl_keyboard_listener wl_keyboard_listener = {
	.keymap      = wl_keyboard_keymap,
	.enter       = wl_keyboard_enter,
	.leave       = wl_keyboard_leave,
	.key         = wl_keyboard_key,
	.modifiers   = wl_keyboard_modifiers,
	.repeat_info = wl_keyboard_repeat_info,
};

//////////////////////////////////////////////////
// WL_SEAT_LISTENER
//////////////////////////////////////////////////
void wl_seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t capabilities) {
	struct ClientState *state = data;

	int have_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
	// The seat DOES support pointers and we haven't configured one yet
	if (have_pointer && state->wl_pointer == NULL) {
		state->wl_pointer = wl_seat_get_pointer(state->wl_seat);
		wl_pointer_add_listener(state->wl_pointer, &wl_pointer_listener, state);
	}
	// The seat DOESN'T support pointers but we've configured one
	else if (!have_pointer && state->wl_pointer != NULL) {
		wl_pointer_release(state->wl_pointer);
		state->wl_pointer = NULL;
	}

	bool have_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
	if (have_keyboard && state->wl_keyboard == NULL) {
		state->wl_keyboard = wl_seat_get_keyboard(state->wl_seat);
		wl_keyboard_add_listener(state->wl_keyboard, &wl_keyboard_listener, state);
	} else if (!have_keyboard && state->wl_keyboard != NULL) {
		wl_keyboard_release(state->wl_keyboard);
		state->wl_keyboard = NULL;
	}
}

void wl_seat_name(void *data, struct wl_seat *wl_seat, const char *name) {
	printf("seat name: %s\n", name);
}

const struct wl_seat_listener wl_seat_listener = {
	.capabilities = wl_seat_capabilities,
	.name         = wl_seat_name,
};

//////////////////////////////////////////////////
// XDG_TOPLEVEL_LISTENER
//////////////////////////////////////////////////
//  This is where the compositor will be like, "Hey man you should resize to this", along with some
//  other things I'm sure.
void xdg_toplevel_configure(
	void *data, struct xdg_toplevel *xdg_toplevel,
	int32_t width, int32_t height, struct wl_array *states
)
{
	ClientState *state = data;
	if (width == 0 || height == 0) {
		// Compositor is deferring to us
		return;
	}

	// TODO(mal): If we have more than one buffer we need to destroy them all here!
	// Probably in a loop.
	for (int i = 0; i < state->nbuffers; i++) {
		wl_buffer_destroy(state->wl_buffers[i]);
	}
	munmap(state->shm_pool_data, state->shm_pool_size);

	state->width = width;
	state->height = height;
	state->stride = width * state->bytes_per_pixel;
	create_buffers(state);

	GameOffscreenBuffer game_offscreen_buffer;
	game_offscreen_buffer.memory          = state->shm_pool_data + (state->current_buffer_index * state->stride * state->height);
	game_offscreen_buffer.width           = state->width;
	game_offscreen_buffer.height          = state->height;
	game_offscreen_buffer.bytes_per_pixel = state->bytes_per_pixel;

	// FIXME(mal): Send to game to draw here

	wl_surface_attach(state->wl_surface, state->wl_buffers[state->current_buffer_index], 0, 0);
	wl_surface_commit(state->wl_surface);
}

void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
	ClientState *client_state = data;
	client_state->closed = 1;
}

const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_configure,
	.close     = xdg_toplevel_close,
};

//////////////////////////////////////////////////
// WL_REGISTRY_LISTENER
//////////////////////////////////////////////////
void registry_handle_global(
	void *data,
	struct wl_registry *wl_registry,
	uint32_t name, const char *interface, uint32_t version
)
{
	printf(
		"interface: '%s', version: %d, name: %d\n",
		interface, version, name
	);
	
	ClientState *state = data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		// NOTE(mal): version 4 is the latest at the time of writing of the book I'm following
		state->compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		state->xdg_wm_base = wl_registry_bind(wl_registry, name, &xdg_wm_base_interface, 1);
		xdg_wm_base_add_listener(state->xdg_wm_base, &xdg_wm_base_listener, state);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		state->wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, 7);
		wl_seat_add_listener(state->wl_seat, &wl_seat_listener, state);
	} else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0) {
		state->xdg_decoration_manager = wl_registry_bind(wl_registry, name, &zxdg_decoration_manager_v1_interface, 1);
	}
}

void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {}

const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

//////////////////////////////////////////////////
// GAME AND NON-WAYLAND STUFF
//////////////////////////////////////////////////

typedef struct GameCode {
	void *lib_handle;
	__time_t last_modified_time;
	GameInitFunction game_init;
	GameRenderFunction game_render;
	GameUpdateFunction game_update;
} GameCode;

// TODO(mal): error handling
GameCode load_game_code() {
	GameCode game_code = {0};

	game_code.lib_handle = dlopen("./game.so", RTLD_NOW);
	ASSERT_MSG_FMT(game_code.lib_handle, "Failed to open game lib: %s\n", dlerror());

	game_code.game_init   = dlsym(game_code.lib_handle, "game_init");
	game_code.game_update = dlsym(game_code.lib_handle, "game_update");
	game_code.game_render = dlsym(game_code.lib_handle, "game_render");

	ASSERT(game_code.game_init);
	ASSERT(game_code.game_update);
	ASSERT(game_code.game_render);

	// NOTE(mal): Yes, technically if we're entering this function from our main loop we've probably
	// just statted the file. However, this reloading should happen so infrequently and is fast
	// enough as is that it shouldn't matter that we're statting again (and honestly all that info
	// is probably cached anyway by the OS idk I haven't checked).
	struct stat game_so_stat;
	stat("./game.so", &game_so_stat);
	game_code.last_modified_time = game_so_stat.st_mtime;

	return game_code;
}

char *debug_platform_read_entire_file(char *file_path) {
	int fd = open(file_path, O_RDWR);
	ASSERT_MSG_FMT(
		fd != -1,
		"Failed to open file %s for reading: %s\n",
		file_path, strerror(errno)
	);
	struct stat file_stat;
	ASSERT_MSG_FMT(
		fstat(fd, &file_stat) == 0,
		"Failed to stat file %s: %s\n",
		file_path, strerror(errno)
	);
	// NOTE(mal): For now I opted to use memory-mapped file I/O rather than opening and
	// copying the entire file entire memory, which I'm wondering if I should really be
	// doing. MAP_PRIVATE provides copy-on-write behavior for the mapped memory.
	char *file_data = mmap(0, (size_t)file_stat.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	return file_data;
}

void debug_platform_free_entire_file(char *file_data, size_t file_data_len) {
	ASSERT_MSG_FMT(
		munmap(file_data, file_data_len) == 0,
		"Failed to free file %s: %s\n",
		file_data, strerror(errno)
	);
}

int main() {
	char exe_path[PATH_MAX];
	ssize_t exe_path_len = 0;
	ASSERT_MSG(
		(exe_path_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1)) > 0,
		"Failed to get path of executable"
	);
	exe_path[exe_path_len] = '\0';
	size_t exe_dir_path_len = exe_path_len;
	while (exe_path[exe_dir_path_len - 1] != '/') {
		exe_dir_path_len--;
	}
	char exe_dir_path[PATH_MAX];
	exe_dir_path[exe_dir_path_len] = '\0';
	strncpy(exe_dir_path, exe_path, exe_dir_path_len);
	ASSERT(chdir(exe_dir_path) == 0);

	#define NBUFFERS 1 // just single buffering at the moment

	ClientState client_state     = { 0 };
	client_state.xkb_context     = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	client_state.width           = 640;
	client_state.height          = 480;
	client_state.bytes_per_pixel = 4;
	client_state.stride          = client_state.width * client_state.bytes_per_pixel;
	client_state.nbuffers        = NBUFFERS;
	client_state.wl_buffers      = (struct wl_buffer *[NBUFFERS]){0}; // just store it here on the stack for now
	client_state.shm_pool_size   = client_state.height * client_state.stride * client_state.nbuffers;

	client_state.wl_display = wl_display_connect(NULL);
	ASSERT(client_state.wl_display);
	client_state.wl_display_fd = wl_display_get_fd(client_state.wl_display);
	client_state.wl_registry = wl_display_get_registry(client_state.wl_display);
	wl_registry_add_listener(client_state.wl_registry, &registry_listener, &client_state);
	wl_display_roundtrip(client_state.wl_display);

	client_state.wl_surface = wl_compositor_create_surface(client_state.compositor);
	client_state.xdg_surface = xdg_wm_base_get_xdg_surface(client_state.xdg_wm_base, client_state.wl_surface);
	xdg_surface_add_listener(client_state.xdg_surface, &xdg_surface_listener, &client_state);
	client_state.xdg_toplevel = xdg_surface_get_toplevel(client_state.xdg_surface);
	xdg_toplevel_add_listener(client_state.xdg_toplevel, &xdg_toplevel_listener, &client_state);
	xdg_toplevel_set_title(client_state.xdg_toplevel, "Example Client");

	// Set up shared memory buffers and surfaces
	create_buffers(&client_state);

	// Setting up server-side decorations i.e. the compositor will draw borders and stuff for us
	// NOTE(mal): Depending on the desktop environment this may or may not be available!
	// In a real application we should probably have a fallback for drawing our own decorations
	// (borders, interactive buttons for close/maximize/minimize/etc)
	ASSERT(client_state.xdg_decoration_manager);
	client_state.xdg_toplevel_decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(
		client_state.xdg_decoration_manager, client_state.xdg_toplevel
	);
	// TODO(mal): Is there a way to tell the compositor what buttons and stuff we want?
	// TODO(mal): Configure listener for decoration configure event so we can ack it.

	// Let's notify the compositor that our entire surface is opaque so it doesn't have to worry
	// about trying to update parts of windows it's covering.
	struct wl_region *opaque_region = wl_compositor_create_region(client_state.compositor);
	wl_region_add(opaque_region, 0, 0, client_state.width, client_state.height);
	wl_surface_set_opaque_region(client_state.wl_surface, opaque_region);
	wl_region_destroy(opaque_region);

	// Commit all our initial client_state
	wl_surface_commit(client_state.wl_surface);

	// registering a frame callback on the surface
	struct wl_callback *surface_frame_callback  = wl_surface_frame(client_state.wl_surface);
	wl_callback_add_listener(surface_frame_callback, &wl_surface_frame_listener, &client_state);

	#define TARGET_FPS 60
	#define TARGET_FRAMETIME_NS ((1.0 / TARGET_FPS) * 1e9)
	// 0 - blocks, use TFD_NONBLOCK for nonblocking
	// NOTE(mal): In this case it doesn't matter because I'm using `poll` to check
	// when any fd is ready for reading and don't actually read if the fd isn't ready.
	int update_timer_fd = timerfd_create(CLOCK_MONOTONIC, 0); 
	struct timespec update_timer_value    = { .tv_nsec  = TARGET_FRAMETIME_NS };
	struct timespec update_timer_interval = { .tv_nsec  = TARGET_FRAMETIME_NS };
	struct itimerspec update_timer_spec   = { .it_value = update_timer_value, .it_interval = update_timer_interval };
	timerfd_settime(update_timer_fd, 0, &update_timer_spec, NULL);

	GameCode game_code = load_game_code();

	GameMemory game_memory = {0};
	game_memory.debug_platform_read_entire_file = debug_platform_read_entire_file;
	game_memory.debug_platform_free_entire_file = debug_platform_free_entire_file;
	game_memory.storage_size = 4ul * 1024ul * 1024ul;
	game_memory.storage = mmap(NULL, game_memory.storage_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	GameInput game_input = {0};
	// NOTE(mal): Going to send the game input through our wayland listeners to collect keyboard input.
	// TODO(mal): If what I'm trying to do doesn't work, maybe have to instead use two fields for
	// client_state: input_this_frame and input_last_frame. Then we'd be able to properly check if a
	// button was previously down and only update input on key transitions.
	client_state.game_input = &game_input;

	game_code.game_init(&game_memory, client_state.width, client_state.height);

	#define WAYLAND_DISPLAY_POLL 0
	#define UPDATE_TIMER_POLL    1
	struct pollfd pollfds[] = {
		[WAYLAND_DISPLAY_POLL] = { .fd = client_state.wl_display_fd, .events = POLLIN },
		[UPDATE_TIMER_POLL]    = { .fd = update_timer_fd,     .events = POLLIN },
	};
	size_t num_pollfds = sizeof(pollfds) / sizeof(struct pollfd);
	// TODO(mal): Now that we have the ability to easily double buffer, we could draw to our
	// backbuffer whenever convenient while Wayland is still presenting our front buffer.
	while (1) {
		int needs_draw = 0;

		struct stat game_so_stat;
		// TODO(mal): Construct and use abs path! Right now this means our CWD _must_ be in the same
		// dir as both the platform exe and the game shared lib.
		stat("./game.so", &game_so_stat);
		// TODO(mal): Check access with R_OK | X_OK instead of just F_OK?
		// NOTE(mal): Hot-reloading gotcha! https://stackoverflow.com/questions/56334288/how-to-hot-reload-shared-library-on-linux
		if (game_so_stat.st_mtime > game_code.last_modified_time && access("./game.lock", F_OK) != 0) {
			ASSERT(dlclose(game_code.lib_handle) == 0);
			game_code = load_game_code();
		}

		// 0 on success, -1 if queue was not empty
		while (wl_display_prepare_read(client_state.wl_display) != 0) {
			// While queue is not empty, dispatch all pending events
			// Did the frame callback happen?
			wl_display_dispatch_pending(client_state.wl_display);
		}
		wl_display_flush(client_state.wl_display);

		// Beyond this point, we must either call wl_display_cancel_read() or wl_display_read_events()

		// See SDL_waylandopengles.c
		// They define a max interval to wait then begin the above loop
		// of prepping read and dispatching remaining events. After all
		// that, they measure the time again and if the max wait is
		// exceeded they break. Now, that's for swapping buffers in a
		// double-buffered scenario so I don't know if it's relevant to
		// here.

		// Okay now that we've handled all pending events we can sleep
		int num_fds_with_events = poll(pollfds, num_pollfds, -1);
		ASSERT(num_fds_with_events != -1);

		if (pollfds[WAYLAND_DISPLAY_POLL].revents & POLLIN) {
			// Dispatch events to obtain:
			// - input
			// - frame callbacks (will cause a render)
			wl_display_read_events(client_state.wl_display);
		} else {
			wl_display_cancel_read(client_state.wl_display);
		}

		if (pollfds[UPDATE_TIMER_POLL].revents & POLLIN) {
			unsigned long expirations;
			// If we don't read the timer the POLLIN revents bit will never be cleared
			read(pollfds[UPDATE_TIMER_POLL].fd, &expirations, sizeof(expirations));

			game_code.game_update(&game_memory, &game_input);

			needs_draw = 1;
		}

		if (client_state.can_draw && needs_draw) {
			client_state.can_draw = 0;

			GameOffscreenBuffer game_offscreen_buffer;
			game_offscreen_buffer.memory          = client_state.shm_pool_data + (client_state.current_buffer_index * client_state.stride * client_state.height);
			game_offscreen_buffer.width           = client_state.width;
			game_offscreen_buffer.height          = client_state.height;
			game_offscreen_buffer.bytes_per_pixel = client_state.bytes_per_pixel;

			game_code.game_render(&game_memory, &game_offscreen_buffer);

			// Request a new callback
			// IMPORTANT(mal): We have to request a surface frame BEFORE we commit the surface!
			wl_callback_add_listener(wl_surface_frame(client_state.wl_surface), &wl_surface_frame_listener, &client_state);

			// NOTE(mal): We could probably optimize the layout of our ClientState struct so that
			// wl_buffers and current_buffer are right next to each other and therefore hopefully
			// only require one cache line load to do the common "buffers[current_buffer]" indexing.
			wl_surface_attach(client_state.wl_surface, client_state.wl_buffers[client_state.current_buffer_index], 0, 0);
			wl_surface_damage_buffer(client_state.wl_surface, 0, 0, INT32_MAX, INT32_MAX);
			wl_surface_commit(client_state.wl_surface);
		}

		if (client_state.closed) {
			break;
		}
	}

	return 0;
}
