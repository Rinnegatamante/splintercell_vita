#ifndef __CONFIG_H__
#define __CONFIG_H__

#define LOAD_ADDRESS 0x98000000

#define MEMORY_NEWLIB_MB 256

#define DATA_PATH "ux0:data/splintercell"
#define SO_PATH DATA_PATH "/" "libschp.so"
#define CONFIG_FILE_PATH DATA_PATH "/" "settings.cfg"

#define SCREEN_W 960
#define SCREEN_H 544
#define XPERIA_W 850
#define XPERIA_H 480
#define TARGET_FPS 60

enum {
	JOYSTICK_ELEM = 0x01,
	KEYPAD_ELEM = 0x02,
};

typedef struct {
	float far_boost; // Boost factor for draw distance
	int have_fog; // Render fog
	int framerate; // Framerate to target
	int msaa; // Antialiasing
	int preset; // Graphical preset
	int cam_slowdown; // Camera slowdown
	int xperia_res; // Game resolution
	int hide_elements; // UI elements to hide
	int autoframerate; // Autoframerate
	int audio_instance_delay; // Same SFX delay in ms
	int audio_global_delay; // Global SFX delay in ms
} game_options;

#endif
