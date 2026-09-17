#if defined(RW_GL3) && defined(LIBRW_SDL2)

#include "glfw_sdl2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "Pad.h"

#ifdef RGDS_PLUS
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#endif

static int gShouldClose;
static int gMouseButtons;
static int gKeyState[GLFW_KEY_LAST + 1];
static double gCursorX, gCursorY;
static GLFWvidmode gVidMode;
static unsigned char gJoyButtons[32];
static float gJoyAxes[8];
static SDL_GameController *gPads[GLFW_JOYSTICK_LAST + 1];
static SDL_Joystick *gJoys[GLFW_JOYSTICK_LAST + 1];

static GLFWkeyfun gKeyCB;
static GLFWframebuffersizefun gFbCB;
static GLFWscrollfun gScrollCB;
static GLFWcursorposfun gCursorCB;
static GLFWcursorenterfun gEnterCB;
static GLFWjoystickfun gJoyCB;

static const int kScanToGlfw[] = {
	/* 0-3 */ 0, GLFW_KEY_ESCAPE, GLFW_KEY_1, GLFW_KEY_2,
	/* 4-7 */ GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6,
	/* 8-11 */ GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9, GLFW_KEY_0,
	/* 12-15 */ GLFW_KEY_MINUS, GLFW_KEY_EQUAL, GLFW_KEY_BACKSPACE, GLFW_KEY_TAB,
	/* 16-19 */ GLFW_KEY_Q, GLFW_KEY_W, GLFW_KEY_E, GLFW_KEY_R,
	/* 20-23 */ GLFW_KEY_T, GLFW_KEY_Y, GLFW_KEY_U, GLFW_KEY_I,
	/* 24-27 */ GLFW_KEY_O, GLFW_KEY_P, GLFW_KEY_LEFT_BRACKET, GLFW_KEY_RIGHT_BRACKET,
	/* 28-31 */ GLFW_KEY_ENTER, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_A, GLFW_KEY_S,
	/* 32-35 */ GLFW_KEY_D, GLFW_KEY_F, GLFW_KEY_G, GLFW_KEY_H,
	/* 36-39 */ GLFW_KEY_J, GLFW_KEY_K, GLFW_KEY_L, GLFW_KEY_SEMICOLON,
	/* 40-43 */ GLFW_KEY_APOSTROPHE, GLFW_KEY_GRAVE_ACCENT, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_BACKSLASH,
	/* 44-47 */ GLFW_KEY_Z, GLFW_KEY_X, GLFW_KEY_C, GLFW_KEY_V,
	/* 48-51 */ GLFW_KEY_B, GLFW_KEY_N, GLFW_KEY_M, GLFW_KEY_COMMA,
	/* 52-55 */ GLFW_KEY_PERIOD, GLFW_KEY_SLASH, GLFW_KEY_RIGHT_SHIFT, GLFW_KEY_KP_MULTIPLY,
	/* 56-59 */ GLFW_KEY_LEFT_ALT, GLFW_KEY_SPACE, GLFW_KEY_CAPS_LOCK, GLFW_KEY_F1,
	/* 60-63 */ GLFW_KEY_F2, GLFW_KEY_F3, GLFW_KEY_F4, GLFW_KEY_F5,
	/* 64-67 */ GLFW_KEY_F6, GLFW_KEY_F7, GLFW_KEY_F8, GLFW_KEY_F9,
	/* 68-71 */ GLFW_KEY_F10, GLFW_KEY_NUM_LOCK, GLFW_KEY_SCROLL_LOCK, GLFW_KEY_KP_7,
	/* 72-75 */ GLFW_KEY_KP_8, GLFW_KEY_KP_9, GLFW_KEY_KP_SUBTRACT, GLFW_KEY_KP_4,
	/* 76-79 */ GLFW_KEY_KP_5, GLFW_KEY_KP_6, GLFW_KEY_KP_ADD, GLFW_KEY_KP_1,
	/* 80-83 */ GLFW_KEY_KP_2, GLFW_KEY_KP_3, GLFW_KEY_KP_0, GLFW_KEY_KP_DECIMAL,
};

static int
scancodeToGlfw(SDL_Scancode sc)
{
	switch (sc) {
	case SDL_SCANCODE_ESCAPE: return GLFW_KEY_ESCAPE;
	case SDL_SCANCODE_RETURN: return GLFW_KEY_ENTER;
	case SDL_SCANCODE_TAB: return GLFW_KEY_TAB;
	case SDL_SCANCODE_BACKSPACE: return GLFW_KEY_BACKSPACE;
	case SDL_SCANCODE_INSERT: return GLFW_KEY_INSERT;
	case SDL_SCANCODE_DELETE: return GLFW_KEY_DELETE;
	case SDL_SCANCODE_RIGHT: return GLFW_KEY_RIGHT;
	case SDL_SCANCODE_LEFT: return GLFW_KEY_LEFT;
	case SDL_SCANCODE_DOWN: return GLFW_KEY_DOWN;
	case SDL_SCANCODE_UP: return GLFW_KEY_UP;
	case SDL_SCANCODE_PAGEUP: return GLFW_KEY_PAGE_UP;
	case SDL_SCANCODE_PAGEDOWN: return GLFW_KEY_PAGE_DOWN;
	case SDL_SCANCODE_HOME: return GLFW_KEY_HOME;
	case SDL_SCANCODE_END: return GLFW_KEY_END;
	case SDL_SCANCODE_CAPSLOCK: return GLFW_KEY_CAPS_LOCK;
	case SDL_SCANCODE_SCROLLLOCK: return GLFW_KEY_SCROLL_LOCK;
	case SDL_SCANCODE_NUMLOCKCLEAR: return GLFW_KEY_NUM_LOCK;
	case SDL_SCANCODE_PRINTSCREEN: return GLFW_KEY_PRINT_SCREEN;
	case SDL_SCANCODE_PAUSE: return GLFW_KEY_PAUSE;
	case SDL_SCANCODE_F1: return GLFW_KEY_F1;
	case SDL_SCANCODE_F2: return GLFW_KEY_F2;
	case SDL_SCANCODE_F3: return GLFW_KEY_F3;
	case SDL_SCANCODE_F4: return GLFW_KEY_F4;
	case SDL_SCANCODE_F5: return GLFW_KEY_F5;
	case SDL_SCANCODE_F6: return GLFW_KEY_F6;
	case SDL_SCANCODE_F7: return GLFW_KEY_F7;
	case SDL_SCANCODE_F8: return GLFW_KEY_F8;
	case SDL_SCANCODE_F9: return GLFW_KEY_F9;
	case SDL_SCANCODE_F10: return GLFW_KEY_F10;
	case SDL_SCANCODE_F11: return GLFW_KEY_F11;
	case SDL_SCANCODE_F12: return GLFW_KEY_F12;
	case SDL_SCANCODE_KP_0: return GLFW_KEY_KP_0;
	case SDL_SCANCODE_KP_1: return GLFW_KEY_KP_1;
	case SDL_SCANCODE_KP_2: return GLFW_KEY_KP_2;
	case SDL_SCANCODE_KP_3: return GLFW_KEY_KP_3;
	case SDL_SCANCODE_KP_4: return GLFW_KEY_KP_4;
	case SDL_SCANCODE_KP_5: return GLFW_KEY_KP_5;
	case SDL_SCANCODE_KP_6: return GLFW_KEY_KP_6;
	case SDL_SCANCODE_KP_7: return GLFW_KEY_KP_7;
	case SDL_SCANCODE_KP_8: return GLFW_KEY_KP_8;
	case SDL_SCANCODE_KP_9: return GLFW_KEY_KP_9;
	case SDL_SCANCODE_KP_DECIMAL: return GLFW_KEY_KP_DECIMAL;
	case SDL_SCANCODE_KP_DIVIDE: return GLFW_KEY_KP_DIVIDE;
	case SDL_SCANCODE_KP_MULTIPLY: return GLFW_KEY_KP_MULTIPLY;
	case SDL_SCANCODE_KP_MINUS: return GLFW_KEY_KP_SUBTRACT;
	case SDL_SCANCODE_KP_PLUS: return GLFW_KEY_KP_ADD;
	case SDL_SCANCODE_KP_ENTER: return GLFW_KEY_KP_ENTER;
	case SDL_SCANCODE_KP_EQUALS: return GLFW_KEY_KP_EQUAL;
	case SDL_SCANCODE_LSHIFT: return GLFW_KEY_LEFT_SHIFT;
	case SDL_SCANCODE_LCTRL: return GLFW_KEY_LEFT_CONTROL;
	case SDL_SCANCODE_LALT: return GLFW_KEY_LEFT_ALT;
	case SDL_SCANCODE_LGUI: return GLFW_KEY_LEFT_SUPER;
	case SDL_SCANCODE_RSHIFT: return GLFW_KEY_RIGHT_SHIFT;
	case SDL_SCANCODE_RCTRL: return GLFW_KEY_RIGHT_CONTROL;
	case SDL_SCANCODE_RALT: return GLFW_KEY_RIGHT_ALT;
	case SDL_SCANCODE_RGUI: return GLFW_KEY_RIGHT_SUPER;
	case SDL_SCANCODE_MENU: return GLFW_KEY_MENU;
	case SDL_SCANCODE_SPACE: return GLFW_KEY_SPACE;
	default:
		if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
			return GLFW_KEY_A + (sc - SDL_SCANCODE_A);
		if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_0)
			return GLFW_KEY_1 + ((sc == SDL_SCANCODE_0) ? 9 : (sc - SDL_SCANCODE_1));
		if ((int)sc < (int)(sizeof(kScanToGlfw) / sizeof(kScanToGlfw[0])))
			return kScanToGlfw[sc];
		return -1;
	}
}

static void
ensureAnbernicMapping(int index)
{
	const char *name = SDL_JoystickNameForIndex(index);
	if (name == nil || strstr(name, "ANBERNIC") == nil)
		return;

	char guid[64];
	SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof(guid));
	char mapping[512];
	/* Official ANBERNIC-rk3568-keys layout. There is no right analog. */
	snprintf(mapping, sizeof(mapping),
		"%s,ANBERNIC-rk3568-keys,platform:Linux,a:b1,b:b0,x:b2,y:b3,"
		"back:b6,guide:b13,start:b7,leftstick:b9,rightstick:b12,"
		"leftshoulder:b4,rightshoulder:b5,lefttrigger:b10,righttrigger:b11,"
		"leftx:a0,lefty:a1,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,",
		guid);
	if (SDL_GameControllerAddMapping(mapping) >= 0) {
		static int logged;
		if (!logged) {
			printf("RGDS: mapped %s guid %s\n", name, guid);
			logged = 1;
		}
	}
}

static void
openPad(int index)
{
	if (index < 0 || index > GLFW_JOYSTICK_LAST)
		return;
	ensureAnbernicMapping(index);
	if (gPads[index] || gJoys[index])
		return;
	if (SDL_IsGameController(index))
		gPads[index] = SDL_GameControllerOpen(index);
	else
		gJoys[index] = SDL_JoystickOpen(index);
}

static void
closePad(int index)
{
	if (index < 0 || index > GLFW_JOYSTICK_LAST)
		return;
	if (gPads[index]) {
		SDL_GameControllerClose(gPads[index]);
		gPads[index] = nil;
	}
	if (gJoys[index]) {
		SDL_JoystickClose(gJoys[index]);
		gJoys[index] = nil;
	}
}

#ifdef RGDS_PLUS
static volatile int gRgdsThrX;
static volatile int gRgdsThrY;
static volatile int gRgdsThrDown;
static int gRgdsTouchX, gRgdsTouchY;
static int gRgdsSpanMouseDown;
static int gRgdsLookThisFrame;
static int gRgdsThrWasDown = -1;
static FILE *gRgdsTouchLog;

static void
touchDbg(const char *fmt, ...)
{
	va_list ap;
	if (gRgdsTouchLog == nil)
		return;
	va_start(ap, fmt);
	vfprintf(gRgdsTouchLog, fmt, ap);
	va_end(ap);
	fflush(gRgdsTouchLog);
}

static int
openGt9xxFd(void)
{
	char path[64];
	int fd = open("/dev/input/event1", O_RDONLY);
	if (fd >= 0) {
		char name[256];
		memset(name, 0, sizeof(name));
		if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 &&
		    (strstr(name, "gt9xx") || strstr(name, "Goodix"))) {
			touchDbg("open /dev/input/event1 (%s)\n", name);
			return fd;
		}
		close(fd);
	}

	DIR *dir = opendir("/dev/input");
	if (dir == nil)
		return -1;
	struct dirent *ent;
	while ((ent = readdir(dir)) != nil) {
		if (strncmp(ent->d_name, "event", 5) != 0)
			continue;
		snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
		fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;
		char name[256];
		memset(name, 0, sizeof(name));
		if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 &&
		    (strstr(name, "gt9xx") || strstr(name, "Goodix"))) {
			touchDbg("open %s (%s)\n", path, name);
			closedir(dir);
			return fd;
		}
		close(fd);
	}
	closedir(dir);
	return -1;
}

struct RgdsInputEvent {
	uint64_t sec;
	uint64_t usec;
	uint16_t type;
	uint16_t code;
	int32_t value;
} __attribute__((packed));

static void *
rgdsTouchThread(void *)
{
	/* tpctrl=1 gates the Goodix IRQ off. Drastic sets 0 so event1 emits. */
	{
		FILE *tp = fopen("/sys/class/anbernic_misc/tpctrl", "w");
		if (tp) {
			fputs("0\n", tp);
			fclose(tp);
		}
	}

	int fd = openGt9xxFd();
	if (fd < 0) {
		touchDbg("open failed errno=%d\n", errno);
		printf("RGDS: gt9xx open failed errno=%d\n", errno);
		fflush(stdout);
		return nil;
	}

	int grabbed = (ioctl(fd, EVIOCGRAB, 1) == 0);
	char tpstate = '?';
	{
		FILE *tp = fopen("/sys/class/anbernic_misc/tpctrl", "r");
		if (tp) {
			tpstate = (char)fgetc(tp);
			fclose(tp);
		}
	}
	printf("RGDS: gt9xx thread fd=%d grab=%d evsz=%d tpctrl=%c\n",
		fd, grabbed, (int)sizeof(struct RgdsInputEvent), tpstate);
	fflush(stdout);
	touchDbg("fd=%d grab=%d evsz=%d tpctrl=%c\n",
		fd, grabbed, (int)sizeof(struct RgdsInputEvent), tpstate);

	int slot = 0;
	int id[16];
	int mx[16], my[16];
	int absx = 0, absy = 0, btn = 0;
	int nlog = 0;
	for (int i = 0; i < 16; i++)
		id[i] = -1;

	struct RgdsInputEvent ev;
	while (read(fd, &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
		if (nlog < 80) {
			touchDbg("ev t=%u c=%u v=%d\n", ev.type, ev.code, ev.value);
			nlog++;
		}
		if (ev.type == EV_ABS) {
			if (ev.code == ABS_MT_SLOT) {
				slot = ev.value;
				if (slot < 0) slot = 0;
				if (slot > 15) slot = 15;
			} else if (ev.code == ABS_MT_TRACKING_ID) {
				id[slot] = ev.value;
			} else if (ev.code == ABS_MT_POSITION_X) {
				mx[slot] = ev.value;
				absx = ev.value;
			} else if (ev.code == ABS_MT_POSITION_Y) {
				my[slot] = ev.value;
				absy = ev.value;
			} else if (ev.code == ABS_X) {
				absx = ev.value;
			} else if (ev.code == ABS_Y) {
				absy = ev.value;
			}
		} else if (ev.type == EV_KEY &&
			   (ev.code == BTN_TOUCH || ev.code == BTN_TOOL_FINGER)) {
			btn = ev.value != 0;
		} else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
			int down = btn;
			int x = absx, y = absy;
			for (int i = 0; i < 16; i++) {
				if (id[i] >= 0) {
					down = 1;
					x = mx[i];
					y = my[i];
					break;
				}
			}
			gRgdsThrX = x;
			gRgdsThrY = y;
			gRgdsThrDown = down;
			gRgdsTouchX = x;
			gRgdsTouchY = y;
		}
	}
	touchDbg("thread read ended errno=%d\n", errno);
	ioctl(fd, EVIOCGRAB, 0);
	close(fd);
	return nil;
}

static void
startRgdsTouchThread(void)
{
	static int started;
	static pthread_t th;
	if (started)
		return;
	started = 1;
	gRgdsTouchLog = fopen("/mnt/sdcard/Ports/gta3/touch.log", "w");
	if (gRgdsTouchLog == nil)
		gRgdsTouchLog = fopen("touch.log", "w");
	pthread_create(&th, nil, rgdsTouchThread, nil);
	pthread_detach(th);
}

static void
emitLookTouch(int px, int py, int pressed, int released, int down, const char *src)
{
	if (px < 0) px = 0;
	if (py < 0) py = 0;
	if (px > 1023) px = 1023;
	if (py > 767) py = 767;
	static int logged;
	if (logged < 12 && (down || pressed || released)) {
		printf("RGDS: touch look %s %d,%d down=%d press=%d release=%d\n",
			src, px, py, down, pressed, released);
		fflush(stdout);
		logged++;
	}
	CPad::AffectFromLinuxTouch(px, py, down != 0, pressed != 0, released != 0);
}

static void
pollGt9xxTouch(void)
{
	startRgdsTouchThread();
	int down = gRgdsThrDown;
	if (!down && gRgdsThrWasDown != 1) {
		gRgdsThrWasDown = 0;
		return;
	}
	int pressed = down && gRgdsThrWasDown != 1;
	int released = !down && gRgdsThrWasDown == 1;
	emitLookTouch(gRgdsThrX, gRgdsThrY, pressed, released, down, "thread");
	if (down)
		gRgdsLookThisFrame = 1;
	gRgdsThrWasDown = down;
}

static void
pollSdlFingersAndMouse(void)
{
	if (gRgdsLookThisFrame)
		return;

	static int sdlWasDown;
	int ndev = SDL_GetNumTouchDevices();
	for (int d = 0; d < ndev; d++) {
		SDL_TouchID tid = SDL_GetTouchDevice(d);
		int nf = SDL_GetNumTouchFingers(tid);
		if (nf <= 0)
			continue;
		SDL_Finger *finger = SDL_GetTouchFinger(tid, 0);
		if (finger == nil)
			continue;
		int px = (int)(finger->x * 1024.0f);
		int py = (int)(finger->y * 768.0f);
		if (finger->x >= 0.5f)
			px = (int)((finger->x - 0.5f) * 2.0f * 1024.0f);
		int pressed = !sdlWasDown;
		emitLookTouch(px, py, pressed, 0, 1, "sdl-finger");
		sdlWasDown = 1;
		gRgdsLookThisFrame = 1;
		return;
	}

	if (sdlWasDown) {
		emitLookTouch(gRgdsTouchX, gRgdsTouchY, 0, 1, 0, "sdl-finger");
		sdlWasDown = 0;
	}

	int mx = 0, my = 0;
	Uint32 buttons = SDL_GetMouseState(&mx, &my);
	int lmb = (buttons & SDL_BUTTON_LMASK) != 0;
	int down = 0;
	int px = mx;
	int py = my;
	if (lmb && mx >= 1024) {
		down = 1;
		px = mx - 1024;
	} else if (lmb && ndev > 0 && mx < 1024) {
		/* gt9xx sometimes arrives as a 1024-wide mouse, not the 2048 window. */
		down = 1;
	}
	if (down || gRgdsSpanMouseDown) {
		int pressed = down && !gRgdsSpanMouseDown;
		int released = !down && gRgdsSpanMouseDown;
		emitLookTouch(px, py, pressed, released, down, "mouse");
		gRgdsSpanMouseDown = down;
		if (down)
			gRgdsLookThisFrame = 1;
	}
}
#endif

static void
handleTouch(const SDL_TouchFingerEvent &finger, int pressed, int released, int down)
{
#ifdef RGDS_PLUS
	if (gRgdsLookThisFrame)
		return;
	int px = (int)(finger.x * 1024.0f);
	int py = (int)(finger.y * 768.0f);
	if (finger.x >= 0.5f)
		px = (int)((finger.x - 0.5f) * 2.0f * 1024.0f);
	emitLookTouch(px, py, pressed, released, down, "sdl-event");
	if (down)
		gRgdsLookThisFrame = 1;
	return;
#else
	int px = (int)(finger.x * 320.0f);
	int py = (int)(finger.y * 240.0f);
#endif
	if (px < 0) px = 0;
	if (py < 0) py = 0;
	if (px > 319) px = 319;
	if (py > 239) py = 239;
	CPad::AffectFromLinuxTouch(px, py, down != 0, pressed != 0, released != 0);
}

void
glfwSetCursorPos(GLFWwindow *window, double xpos, double ypos)
{
	if (window)
		SDL_WarpMouseInWindow(window, (int)xpos, (int)ypos);
	gCursorX = xpos;
	gCursorY = ypos;
}

void
glfwGetCursorPos(GLFWwindow *window, double *xpos, double *ypos)
{
	int x = 0, y = 0;
	if (window)
		SDL_GetMouseState(&x, &y);
	if (xpos) *xpos = x;
	if (ypos) *ypos = y;
	gCursorX = x;
	gCursorY = y;
}

void
glfwGetWindowSize(GLFWwindow *window, int *width, int *height)
{
	if (window)
		SDL_GetWindowSize(window, width, height);
	else {
		if (width) *width = 0;
		if (height) *height = 0;
	}
}

void
glfwSetWindowSize(GLFWwindow *window, int width, int height)
{
#ifdef LINUX_DUAL_SCREEN_SEPARATE_WINDOWS
	(void)window; (void)width; (void)height;
#else
	if (window)
		SDL_SetWindowSize(window, width, height);
#endif
}

void
glfwSetWindowAttrib(GLFWwindow *window, int attrib, int value)
{
	if (window && attrib == GLFW_RESIZABLE)
		SDL_SetWindowResizable(window, value ? SDL_TRUE : SDL_FALSE);
}

void
glfwSetWindowSizeLimits(GLFWwindow *window, int minw, int minh, int maxw, int maxh)
{
	if (window)
		SDL_SetWindowMinimumSize(window, minw, minh);
	if (window)
		SDL_SetWindowMaximumSize(window, maxw, maxh);
}

int
glfwWindowShouldClose(GLFWwindow *window)
{
	(void)window;
	return gShouldClose;
}

int
glfwGetWindowAttrib(GLFWwindow *window, int attrib)
{
	if (window == nil)
		return 0;
	Uint32 flags = SDL_GetWindowFlags(window);
	if (attrib == GLFW_ICONIFIED)
		return (flags & SDL_WINDOW_MINIMIZED) != 0;
	if (attrib == GLFW_RESIZABLE)
		return (flags & SDL_WINDOW_RESIZABLE) != 0;
	return 0;
}

void
glfwSetInputMode(GLFWwindow *window, int mode, int value)
{
	(void)window;
	if (mode == GLFW_CURSOR) {
		if (value == GLFW_CURSOR_HIDDEN || value == GLFW_CURSOR_DISABLED)
			SDL_ShowCursor(SDL_DISABLE);
		else
			SDL_ShowCursor(SDL_ENABLE);
		if (value == GLFW_CURSOR_DISABLED)
			SDL_SetRelativeMouseMode(SDL_TRUE);
		else
			SDL_SetRelativeMouseMode(SDL_FALSE);
	}
}

int
glfwGetKey(GLFWwindow *window, int key)
{
	(void)window;
	if (key < 0 || key > GLFW_KEY_LAST)
		return GLFW_RELEASE;
	return gKeyState[key];
}

int
glfwGetMouseButton(GLFWwindow *window, int button)
{
	(void)window;
	int sdl = 0;
	if (button == GLFW_MOUSE_BUTTON_LEFT) sdl = SDL_BUTTON_LMASK;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT) sdl = SDL_BUTTON_RMASK;
	else if (button == GLFW_MOUSE_BUTTON_MIDDLE) sdl = SDL_BUTTON_MMASK;
	else if (button == GLFW_MOUSE_BUTTON_4) sdl = SDL_BUTTON_X1MASK;
	else if (button == GLFW_MOUSE_BUTTON_5) sdl = SDL_BUTTON_X2MASK;
	return (SDL_GetMouseState(nil, nil) & sdl) ? GLFW_PRESS : GLFW_RELEASE;
}

GLFWwindow *
glfwGetPrimaryMonitor(void)
{
	return nil;
}

const GLFWvidmode *
glfwGetVideoMode(GLFWwindow *monitor)
{
	(void)monitor;
	SDL_DisplayMode mode;
	if (SDL_GetDesktopDisplayMode(0, &mode) != 0) {
		gVidMode.width = 1024;
		gVidMode.height = 768;
		gVidMode.redBits = gVidMode.greenBits = gVidMode.blueBits = 8;
		gVidMode.refreshRate = 60;
		return &gVidMode;
	}
	gVidMode.width = mode.w;
	gVidMode.height = mode.h;
	gVidMode.redBits = gVidMode.greenBits = gVidMode.blueBits = 8;
	gVidMode.refreshRate = mode.refresh_rate;
	return &gVidMode;
}

int
glfwJoystickPresent(int jid)
{
	return jid >= 0 && jid < SDL_NumJoysticks();
}

int
glfwJoystickIsGamepad(int jid)
{
	ensureAnbernicMapping(jid);
	return SDL_IsGameController(jid);
}

const char *
glfwGetJoystickName(int jid)
{
	const char *name = SDL_JoystickNameForIndex(jid);
	return name ? name : "";
}

const unsigned char *
glfwGetJoystickButtons(int jid, int *count)
{
	openPad(jid);
	SDL_Joystick *js = gJoys[jid] ? gJoys[jid] :
		(gPads[jid] ? SDL_GameControllerGetJoystick(gPads[jid]) : nil);
	if (js == nil) {
		if (count) *count = 0;
		return gJoyButtons;
	}
	int n = SDL_JoystickNumButtons(js);
	if (n > (int)sizeof(gJoyButtons))
		n = (int)sizeof(gJoyButtons);
	for (int i = 0; i < n; i++)
		gJoyButtons[i] = SDL_JoystickGetButton(js, i);
	if (count) *count = n;
	return gJoyButtons;
}

const float *
glfwGetJoystickAxes(int jid, int *count)
{
	openPad(jid);
	SDL_Joystick *js = gJoys[jid] ? gJoys[jid] :
		(gPads[jid] ? SDL_GameControllerGetJoystick(gPads[jid]) : nil);
	if (js == nil) {
		if (count) *count = 0;
		return gJoyAxes;
	}
	int n = SDL_JoystickNumAxes(js);
	if (n > (int)(sizeof(gJoyAxes) / sizeof(gJoyAxes[0])))
		n = (int)(sizeof(gJoyAxes) / sizeof(gJoyAxes[0]));
	for (int i = 0; i < n; i++)
		gJoyAxes[i] = SDL_JoystickGetAxis(js, i) / 32767.0f;
	if (count) *count = n;
	return gJoyAxes;
}

int
glfwGetGamepadState(int jid, GLFWgamepadstate *state)
{
	if (state == nil)
		return GLFW_FALSE;
	memset(state, 0, sizeof(*state));
	ensureAnbernicMapping(jid);
	openPad(jid);
	if (gPads[jid] == nil)
		return GLFW_FALSE;

	SDL_GameController *pad = gPads[jid];
	SDL_GameControllerUpdate();
	state->buttons[GLFW_GAMEPAD_BUTTON_A] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
	state->buttons[GLFW_GAMEPAD_BUTTON_B] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B);
	state->buttons[GLFW_GAMEPAD_BUTTON_X] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X);
	state->buttons[GLFW_GAMEPAD_BUTTON_Y] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y);
	state->buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
	state->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
	state->buttons[GLFW_GAMEPAD_BUTTON_BACK] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK);
	state->buttons[GLFW_GAMEPAD_BUTTON_START] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
	state->buttons[GLFW_GAMEPAD_BUTTON_GUIDE] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_GUIDE);
	state->buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSTICK);
	state->buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
	state->buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
	state->buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
	state->buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	state->buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);

	state->axes[GLFW_GAMEPAD_AXIS_LEFT_X] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
	state->axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
#ifdef RGDS_PLUS
	state->axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = 0.0f;
	state->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = 0.0f;
#else
	state->axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) / 32767.0f;
	state->axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) / 32767.0f;
#endif
	state->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 32767.0f * 2.0f - 1.0f;
	state->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 32767.0f * 2.0f - 1.0f;
#ifdef RGDS_PLUS
	{
		SDL_Joystick *js = SDL_GameControllerGetJoystick(pad);
		if (js) {
			if (SDL_JoystickGetButton(js, 10))
				state->axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = 1.0f;
			if (SDL_JoystickGetButton(js, 11))
				state->axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = 1.0f;
		}
	}
#endif

	/* RG DS PLUS D-pad is a separate keyboard (dierct-keys-polled), not a full -1..1 hat. */
	{
		const Uint8 *ks = SDL_GetKeyboardState(nil);
		if (ks) {
			if (ks[SDL_SCANCODE_UP]) state->buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] = 1;
			if (ks[SDL_SCANCODE_DOWN]) state->buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = 1;
			if (ks[SDL_SCANCODE_LEFT]) state->buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = 1;
			if (ks[SDL_SCANCODE_RIGHT]) state->buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = 1;
		}
	}
	return GLFW_TRUE;
}

int
glfwUpdateGamepadMappings(const char *string)
{
	if (string == nil)
		return GLFW_FALSE;
	return SDL_GameControllerAddMapping(string) >= 0 ? GLFW_TRUE : GLFW_FALSE;
}

void
glfwSetKeyCallback(GLFWwindow *, GLFWkeyfun cb) { gKeyCB = cb; }
void
glfwSetFramebufferSizeCallback(GLFWwindow *, GLFWframebuffersizefun cb) { gFbCB = cb; }
void
glfwSetScrollCallback(GLFWwindow *, GLFWscrollfun cb) { gScrollCB = cb; }
void
glfwSetCursorPosCallback(GLFWwindow *, GLFWcursorposfun cb) { gCursorCB = cb; }
void
glfwSetCursorEnterCallback(GLFWwindow *, GLFWcursorenterfun cb) { gEnterCB = cb; }
void
glfwSetJoystickCallback(GLFWjoystickfun cb) { gJoyCB = cb; }

static Uint32 gBackHeldSince;

static int
isLinuxBackHeld(void)
{
	const Uint8 *ks = SDL_GetKeyboardState(nil);
	if (ks && ks[SDL_SCANCODE_AC_BACK])
		return 1;

	SDL_GameControllerUpdate();
	for (int i = 0; i <= GLFW_JOYSTICK_LAST; i++) {
		if (gPads[i] && SDL_GameControllerGetButton(gPads[i], SDL_CONTROLLER_BUTTON_BACK))
			return 1;
		if (gJoys[i] && (SDL_JoystickGetButton(gJoys[i], 6) || SDL_JoystickGetButton(gJoys[i], 10)))
			return 1;
	}
	return 0;
}

static void
pollBackHoldToQuit(void)
{
	if (!isLinuxBackHeld()) {
		gBackHeldSince = 0;
		return;
	}
	Uint32 now = SDL_GetTicks();
	if (gBackHeldSince == 0)
		gBackHeldSince = now;
	else if (now - gBackHeldSince >= 2000)
		gShouldClose = 1;
}

void
glfwPollEvents(void)
{
	SDL_Event e;
	SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK | SDL_INIT_EVENTS);
#ifdef RGDS_PLUS
	gRgdsLookThisFrame = 0;
	pollGt9xxTouch();
#endif
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_QUIT:
			gShouldClose = 1;
			break;
		case SDL_WINDOWEVENT:
			if (e.window.event == SDL_WINDOWEVENT_CLOSE)
				gShouldClose = 1;
			else if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && gFbCB) {
				SDL_Window *win = SDL_GetWindowFromID(e.window.windowID);
				gFbCB(win, e.window.data1, e.window.data2);
			} else if (e.window.event == SDL_WINDOWEVENT_ENTER && gEnterCB) {
				gEnterCB(SDL_GetWindowFromID(e.window.windowID), GLFW_TRUE);
			} else if (e.window.event == SDL_WINDOWEVENT_LEAVE && gEnterCB) {
				gEnterCB(SDL_GetWindowFromID(e.window.windowID), GLFW_FALSE);
			}
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			{
				int key = scancodeToGlfw(e.key.keysym.scancode);
				int action = (e.type == SDL_KEYDOWN) ?
					(e.key.repeat ? GLFW_REPEAT : GLFW_PRESS) : GLFW_RELEASE;
				if (key >= 0 && key <= GLFW_KEY_LAST)
					gKeyState[key] = (action == GLFW_RELEASE) ? GLFW_RELEASE : GLFW_PRESS;
				if (gKeyCB)
					gKeyCB(SDL_GetWindowFromID(e.key.windowID), key, e.key.keysym.scancode, action, 0);
			}
			break;
		case SDL_MOUSEMOTION:
			gCursorX = e.motion.x;
			gCursorY = e.motion.y;
			if (gCursorCB)
				gCursorCB(SDL_GetWindowFromID(e.motion.windowID), e.motion.x, e.motion.y);
			break;
		case SDL_MOUSEWHEEL:
			if (gScrollCB)
				gScrollCB(SDL_GetWindowFromID(e.wheel.windowID), e.wheel.x, e.wheel.y);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			gMouseButtons = SDL_GetMouseState(nil, nil);
			break;
		case SDL_FINGERDOWN:
			handleTouch(e.tfinger, 1, 0, 1);
			break;
		case SDL_FINGERUP:
			handleTouch(e.tfinger, 0, 1, 0);
			break;
		case SDL_FINGERMOTION:
			handleTouch(e.tfinger, 0, 0, 1);
			break;
		case SDL_CONTROLLERDEVICEADDED:
			openPad(e.cdevice.which);
			if (gJoyCB)
				gJoyCB(e.cdevice.which, GLFW_CONNECTED);
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			closePad(e.cdevice.which);
			if (gJoyCB)
				gJoyCB(e.cdevice.which, GLFW_DISCONNECTED);
			break;
		case SDL_JOYDEVICEADDED:
			ensureAnbernicMapping(e.jdevice.which);
			if (!SDL_IsGameController(e.jdevice.which)) {
				openPad(e.jdevice.which);
				if (gJoyCB)
					gJoyCB(e.jdevice.which, GLFW_CONNECTED);
			}
			break;
		case SDL_JOYDEVICEREMOVED:
			closePad(e.jdevice.which);
			if (gJoyCB)
				gJoyCB(e.jdevice.which, GLFW_DISCONNECTED);
			break;
		default:
			break;
		}
	}
	pollBackHoldToQuit();
#ifdef RGDS_PLUS
	pollSdlFingersAndMouse();
#endif
	CPad::UpdateLinuxTouchIdle();
}

#endif
