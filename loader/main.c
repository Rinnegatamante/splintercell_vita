/* main.c -- Splinter Cell: Conviction .so loader
 *
 * Copyright (C) 2025 Alessio Tosto
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.	See the LICENSE file for details.
 */

#include <vitasdk.h>
#include <kubridge.h>
#include <vitaGL.h>

#include <pthread.h>

#include <malloc.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <math.h>

#include <sys/time.h>

#include "main.h"
#include "config.h"
#include "dialog.h"
#include "so_util.h"

//#define DEBUG
//#define EXCLUSIVE_VIDEO_PLAYER

game_options opts;

int _newlib_heap_size_user = MEMORY_NEWLIB_MB * 1024 * 1024;

so_module main_mod;

void *__wrap_memcpy(void *dest, const void *src, size_t n) {
	return sceClibMemcpy(dest, src, n);
}

void *__wrap_memmove(void *dest, const void *src, size_t n) {
	return sceClibMemmove(dest, src, n);
}

void *__wrap_memset(void *s, int c, size_t n) {
	return sceClibMemset(s, c, n);
}

int __android_log_write(int prio, const char *tag, const char *text) {
#ifdef DEBUG
	sceClibPrintf("%s: %s\n", tag, text);
#endif
	return 0;
}


int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
#ifdef DEBUG
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);

	sceClibPrintf("[LOG] %s: %s\n", tag, string);
#endif
	return 0;
}

int ret0(void) {
	return 0;
}

void *audio_player_play(char *path, uint8_t loop, float vol, uint8_t autofree, size_t buf_size, int id);
int audio_player_is_playing(void *m);
void audio_player_stop(void *m);
void audio_player_set_pause(void *m, int val);
void audio_player_init();
void audio_player_stop_all_sounds();
void audio_player_set_pause_all_sounds(int val);
void audio_player_instance(void *m, uint8_t loop, float vol);
void audio_player_set_volume(void *m, float vol);
void audio_player_change_bgm_volume(float vol);
void audio_player_change_sfx_volume(float vol);
float audio_player_get_volume(void *m);
void *audio_player_preload(char *path, size_t *sz);

#define AUDIO_SOURCES_NUM (681)
#define AUDIO_THREADS_NUM (3)
float volumes[2];
volatile void *audio_sources[AUDIO_SOURCES_NUM] = {};
volatile uint32_t audio_last_play_tick[AUDIO_SOURCES_NUM];
void *audio_buffers[AUDIO_SOURCES_NUM] = {};
size_t audio_sizes[AUDIO_SOURCES_NUM] = {};
char audio_path[256];

SceUID audio_request_sema[AUDIO_THREADS_NUM];
SceUID audio_free_slot_sema[AUDIO_THREADS_NUM];

typedef struct {
	int id;
	int loop;
} audio_request;
volatile audio_request req[AUDIO_THREADS_NUM] = {};
volatile uint32_t audio_last_global_play_tick = 0;
uint8_t audio_thread_idx = 0;

void *audio_player_thread(void *arg) {
	char path[256];
	int offs = (int)arg;
	for (;;) {
		sceKernelWaitSema(audio_request_sema[offs], 1, NULL);
		audio_request r;
		sceClibMemcpy(&r, &req[offs], sizeof(audio_request));
		if (audio_sources[r.id] == (void *)0xDEADBEEF) {
			audio_sources[r.id] = (void *)0xBEEFDEAD;
			sceKernelSignalSema(audio_free_slot_sema[offs], 1);
			audio_sources[r.id] = audio_player_play(audio_buffers[r.id], r.loop, r.loop ? volumes[1] : volumes[0], 0, audio_sizes[r.id], r.id);
		} else if (!r.loop && audio_sources[r.id] != (void *)0xBEEFDEAD) {
			sceKernelSignalSema(audio_free_slot_sema[offs], 1);
#if 1
			audio_player_play(audio_buffers[r.id], 0, volumes[0], 1, audio_sizes[r.id], offs);
#else
			audio_player_instance(audio_buffers[r.id], 0, volumes[0]);
#endif
		} else {
			sceKernelSignalSema(audio_free_slot_sema[offs], 1);
		}
	}
	pthread_exit(0);
	return NULL;
}

void nativePlaySound(int id, float vol, int loop) {
	//sceClibPrintf("nativePlaySound %d %f %d\n", id, vol, loop);
#if 1
	sceKernelWaitSema(audio_free_slot_sema[audio_thread_idx], 1, NULL);
	req[audio_thread_idx].id = id;
	req[audio_thread_idx].loop = loop;
	sceKernelSignalSema(audio_request_sema[audio_thread_idx], 1);
	audio_thread_idx = (audio_thread_idx + 1) % AUDIO_THREADS_NUM;
#elif 0
	if (audio_sources[id] == (void *)0xDEADBEEF) {
		audio_sources[id] = audio_player_play(audio_buffers[id], loop, loop ? volumes[1] : volumes[0], 0, audio_sizes[id], id);
	} else {
#if 1
		audio_player_play(audio_buffers[id], loop, loop ? volumes[1] : volumes[0], 1, audio_sizes[id], id);
#else
		audio_player_instance(audio_buffers[id], loop, loop ? volumes[1] : volumes[0]);
#endif
	}
#else
	char path[256];
	sprintf(path, "%s/raw_%03d.ogg", audio_path, id);

	if (audio_sources[id] == (void *)0xDEADBEEF) {
		audio_sources[id] = audio_player_play(path, loop, loop ? volumes[1] : volumes[0], 0, 0, id);
	} else {
		audio_player_play(path, loop, loop ? volumes[1] : volumes[0], 1, 0, id);
	}
#endif
}

void nativeStopAllSounds() {
	//sceClibPrintf("nativeStopAllSounds\n");
#if 0
	audio_player_stop_all_sounds();
	for (int i = 0; i < AUDIO_SOURCES_NUM; i++) {
		audio_sources[i] = (void *)0xDEADBEEF;
	}
#else
	for (int i = 0; i < AUDIO_SOURCES_NUM; i++) {
		if (audio_sources[i] != (void *)0xDEADBEEF && audio_sources[i] != (void *)0xBEEFDEAD) {
			audio_player_stop(audio_sources[i]);
			audio_sources[i] = (void *)0xDEADBEEF;
		}
	}
#endif
}

void nativePauseAllSounds() {
	//sceClibPrintf("nativePauseAllSounds\n");
	audio_player_set_pause_all_sounds(1);
}

void nativeResumeAllSounds() {
	//sceClibPrintf("nativeResumeAllSounds\n");
	audio_player_set_pause_all_sounds(0);
}

int nativeIsMediaPlaying(int id) {
	if (audio_sources[id] == (void *)0xDEADBEEF)
		return 0;
	if (audio_sources[id] == (void *)0xBEEFDEAD)
		return 1;
	return audio_player_is_playing(audio_sources[id]);
}

void nativeKillSound(int id) {
	//sceClibPrintf("nativeKill/StopSound %s\n", sounds[id]);
	if (audio_sources[id] != (void *)0xDEADBEEF && audio_sources[id] != (void *)0xBEEFDEAD) {
		audio_player_stop(audio_sources[id]);
		audio_sources[id] = (void *)0xDEADBEEF;
	}
}

void nativePauseSound(int id) {
	//sceClibPrintf("nativePauseSound %s\n", sounds[id]);
	if (audio_sources[id] != (void *)0xDEADBEEF && audio_sources[id] != (void *)0xBEEFDEAD) {
		audio_player_set_pause(audio_sources[id], 1);
	}
}

void nativeResumeSound(int id) {
	//sceClibPrintf("nativeResumeSound %s\n", sounds[id]);
	if (audio_sources[id] != (void *)0xDEADBEEF && audio_sources[id] != (void *)0xBEEFDEAD) {
		audio_player_set_pause(audio_sources[id], 0);
	}
}

void nativeSetSoundVolume(int id, float vol) {
	//sceClibPrintf("nativeSetSoundVolume %s %f\n", sounds[id], vol);
	if (audio_sources[id] != (void *)0xDEADBEEF && audio_sources[id] != (void *)0xBEEFDEAD) {
		audio_player_set_volume(audio_sources[id], vol);
	}
}

float volumes[2];
float nativeGetGroupVolume(int group_id) {
	//sceClibPrintf("nativeGetGroupVolume %d\n", group_id);
	return volumes[group_id - 1];
}

void nativeSetGroupVolume(int group_id, float vol) {
	//sceClibPrintf("nativeSetGroupVolume %d %f\n", group_id, vol);
	if (group_id == 2) {
		audio_player_change_bgm_volume(vol);
	} else {
		audio_player_change_sfx_volume(vol);
	}
	volumes[group_id - 1] = vol;
}

int ret1() {
	return 1;
}

int nativeOpenBrowser(char *url) {
	return 0;
}

int nativeExit() {
	return sceKernelExitProcess(0);
}

int nativeLoadMovie() {
	//sceClibPrintf("nativeLoadMovie\n");
	return 1;
}

int nativeIsVideoCompleted() {
	//sceClibPrintf("nativeIsVideoCompleted\n");
	return 1;
}


void setFarValue(float *this, float val) {
	this[97] = val * opts.far_boost;
}

so_hook camera_hook;
int Camera_OnTouchDragging(void *this, int x, int y, int delta_x, int delta_y) {
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(0, &pad, 1);
	if (pad.rx > 162 || pad.rx < 92 || pad.ry > 162 || pad.ry < 92) {
		return SO_CONTINUE(int, camera_hook, this, 600, 300, (int)(pad.rx - 127) / opts.cam_slowdown, (int)(pad.ry - 127) / opts.cam_slowdown);
	}
	return SO_CONTINUE(int, camera_hook, this, x, y, delta_x, delta_y);
}

void irr_print(void *this, const char *fmt, ...) {
#ifdef DEBUG
	va_list list;
	static char string[0x8000];

	va_start(list, fmt);
	vsprintf(string, fmt, list);
	va_end(list);

	sceClibPrintf("[IRR] %s\n", string);
#endif
}

int nativeGetCurrentTime() {
	return sceKernelGetProcessTimeLow() / 1000;
}

void *audio_preload_thread(void *arg) {
	char path[256];
	int offs = (int)arg;
	for (int i = 0; i < AUDIO_SOURCES_NUM / AUDIO_THREADS_NUM; i++) {
		sprintf(path, "%s/raw_%03d.ogg", audio_path, offs * (AUDIO_SOURCES_NUM / AUDIO_THREADS_NUM) + i);
		//sceClibPrintf("Preload %s\n", path);
		audio_buffers[offs * (AUDIO_SOURCES_NUM / AUDIO_THREADS_NUM) + i] = audio_player_preload(path, &audio_sizes[offs * (AUDIO_SOURCES_NUM / AUDIO_THREADS_NUM) + i]);
	}
	sceClibPrintf("Audio preload thread %d finished\n", offs);
	pthread_exit(0);
	return NULL;
}

void patch_game(void) {
	audio_player_init();
	strcpy(audio_path, "ux0:data/splintercell/gameloft/games/SCHP/data/sound");
	pthread_t t;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 128 * 1024);
	for (int i = 0; i < AUDIO_THREADS_NUM; i++) {
		audio_free_slot_sema[i] = sceKernelCreateSema("audio slots", 0, 1, 1, NULL);
		audio_request_sema[i] = sceKernelCreateSema("audio req", 0, 0, 1, NULL);
		pthread_create(&t, &attr, audio_preload_thread, (void *)i);
		pthread_create(&t, &attr, audio_player_thread, (void *)i);
	}
	for (int i = 0; i < AUDIO_SOURCES_NUM; i++) {
		audio_sources[i] = 0xDEADBEEF;
	}

	hook_addr(so_symbol(&main_mod, "nativeGetCurrentTime"), (uintptr_t)&nativeGetCurrentTime);
	hook_addr(so_symbol(&main_mod, "nativeExit"), (uintptr_t)&nativeExit);
	hook_addr(so_symbol(&main_mod, "nativePlaySoundBig"), (uintptr_t)&nativePlaySound);
	hook_addr(so_symbol(&main_mod, "nativePauseSoundBig"), (uintptr_t)&nativePauseSound);
	hook_addr(so_symbol(&main_mod, "nativeResumeSoundBig"), (uintptr_t)&nativeResumeSound);
	hook_addr(so_symbol(&main_mod, "nativeStopAllMusic"), (uintptr_t)&nativeStopAllSounds);
	hook_addr(so_symbol(&main_mod, "nativeStopAllSounds"), (uintptr_t)&nativeStopAllSounds);
	hook_addr(so_symbol(&main_mod, "nativeIsMediaPlaying"), (uintptr_t)&nativeIsMediaPlaying);
	hook_addr(so_symbol(&main_mod, "nativeStopSoundBig"), (uintptr_t)&nativeKillSound);
	hook_addr(so_symbol(&main_mod, "nativeSetVolumeBig"), (uintptr_t)&nativeSetSoundVolume);
	hook_addr(so_symbol(&main_mod, "nativeResumeAllSoundBig"), (uintptr_t)&nativeResumeAllSounds);
	hook_addr(so_symbol(&main_mod, "nativePauseAllSoundBig"), (uintptr_t)&nativePauseAllSounds);
	hook_addr(so_symbol(&main_mod, "nativeGetVolumeBig"), (uintptr_t)&nativeGetGroupVolume);
	hook_addr(so_symbol(&main_mod, "nativeSetVolumeBig"), (uintptr_t)&nativeSetGroupVolume);
	hook_addr(so_symbol(&main_mod, "nativeLoadMovie"), (uintptr_t)&nativeLoadMovie);
	hook_addr(so_symbol(&main_mod, "nativeIsVideoCompleted"), (uintptr_t)&nativeIsVideoCompleted);
	
	if (opts.hide_elements & JOYSTICK_ELEM) {
		hook_addr(so_symbol(&main_mod, "_ZN14CInputJoystick14RenderJoystickEv"), (uintptr_t)&ret0);
	}
	if (opts.hide_elements & KEYPAD_ELEM) {
		hook_addr(so_symbol(&main_mod, "_ZN6KeyPad4DrawEv"), (uintptr_t)&ret0);
	}
	
	//hook_addr(so_symbol(&main_mod, "debug_out"), (uintptr_t)&sceClibPrintf);
	//hook_addr(so_symbol(&main_mod, "_ZN3irr2os7Printer5printEPKc"), (uintptr_t)&irr_print);
	
	hook_addr(so_symbol(&main_mod, "_ZN3irr5scene16CCameraSceneNode11setFarValueEf"), (uintptr_t)&setFarValue);
	
	camera_hook = hook_addr(so_symbol(&main_mod, "_ZN6Camera15OnTouchDraggingEiiii"), (uintptr_t)&Camera_OnTouchDragging);
}

FILE *fopen_hook(char *fname, char *mode) {
	char new_path[1024];
	if (strncmp(fname, "/sdcard", 7) == 0) {
		snprintf(new_path, sizeof(new_path), "%s%s", DATA_PATH, fname + 7);
		fname = new_path;
	}
	//sceClibPrintf("fopen %s\n", fname);
	return fopen(fname, mode);
}

extern void *__aeabi_atexit;
extern void *__aeabi_d2f;
extern void *__aeabi_d2iz;
extern void *__aeabi_d2uiz;
extern void *__aeabi_dadd;
extern void *__aeabi_dcmpeq;
extern void *__aeabi_dcmpge;
extern void *__aeabi_dcmpgt;
extern void *__aeabi_dcmple;
extern void *__aeabi_dcmplt;
extern void *__aeabi_ddiv;
extern void *__aeabi_dmul;
extern void *__aeabi_dsub;
extern void *__aeabi_f2d;
extern void *__aeabi_f2iz;
extern void *__aeabi_fadd;
extern void *__aeabi_fcmpeq;
extern void *__aeabi_fcmpge;
extern void *__aeabi_fcmpgt;
extern void *__aeabi_fcmple;
extern void *__aeabi_fcmplt;
extern void *__aeabi_fcmpun;
extern void *__aeabi_fdiv;
extern void *__aeabi_fmul;
extern void *__aeabi_fsub;
extern void *__aeabi_i2d;
extern void *__aeabi_i2f;
extern void *__aeabi_idiv;
extern void *__aeabi_idivmod;
extern void *__aeabi_ldivmod;
extern void *__aeabi_lmul;
extern void *__aeabi_ui2d;
extern void *__aeabi_ui2f;
extern void *__aeabi_uidiv;
extern void *__aeabi_uidivmod;
extern void *__cxa_guard_acquire;
extern void *__cxa_guard_release;
extern void *__dso_handle;
extern void *__stack_chk_fail;
extern void *__aeabi_l2d;
extern void *__aeabi_d2lz;
extern void *__aeabi_l2f;
extern void *__aeabi_f2lz;
extern void *_Znaj;
extern void *_ZdaPv;
extern void *_Znwj;
extern void *_ZdlPv;

static int __stack_chk_guard_fake = 0x42424242;

static FILE __sF_fake[0x100][3];

void glEnable_hook(GLenum v) {
	if (v != GL_FOG || opts.have_fog)
		glEnable(v);
}

int pthread_mutex_init_fake(pthread_mutex_t **uid, const pthread_mutexattr_t *mutexattr) {
	pthread_mutex_t *m = calloc(1, sizeof(pthread_mutex_t));
	if (!m)
		return -1;

	const int recursive = (mutexattr && *(const int *)mutexattr == 1);
	*m = recursive ? PTHREAD_RECURSIVE_MUTEX_INITIALIZER
								 : PTHREAD_MUTEX_INITIALIZER;

	int ret = pthread_mutex_init(m, mutexattr);
	if (ret < 0) {
		free(m);
		return -1;
	}

	*uid = m;

	return 0;
}

int pthread_mutex_destroy_fake(pthread_mutex_t **uid) {
	if (uid && *uid && (uintptr_t)*uid > 0x8000) {
		pthread_mutex_destroy(*uid);
		free(*uid);
		*uid = NULL;
	}
	return 0;
}

int pthread_mutex_lock_fake(pthread_mutex_t **uid) {
	int ret = 0;
	if (!*uid) {
		ret = pthread_mutex_init_fake(uid, NULL);
	} else if ((uintptr_t)*uid == 0x4000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	} else if ((uintptr_t)*uid == 0x8000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	if (ret < 0)
		return ret;
	return pthread_mutex_lock(*uid);
}

int pthread_mutex_unlock_fake(pthread_mutex_t **uid) {
	int ret = 0;
	if (!*uid) {
		ret = pthread_mutex_init_fake(uid, NULL);
	} else if ((uintptr_t)*uid == 0x4000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	} else if ((uintptr_t)*uid == 0x8000) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
		ret = pthread_mutex_init_fake(uid, &attr);
		pthread_mutexattr_destroy(&attr);
	}
	if (ret < 0)
		return ret;
	return pthread_mutex_unlock(*uid);
}

int pthread_cond_init_fake(pthread_cond_t **cnd, const int *condattr) {
	pthread_cond_t *c = calloc(1, sizeof(pthread_cond_t));
	if (!c)
		return -1;

	*c = PTHREAD_COND_INITIALIZER;

	int ret = pthread_cond_init(c, NULL);
	if (ret < 0) {
		free(c);
		return -1;
	}

	*cnd = c;

	return 0;
}

int pthread_cond_broadcast_fake(pthread_cond_t **cnd) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	return pthread_cond_broadcast(*cnd);
}

int pthread_cond_signal_fake(pthread_cond_t **cnd) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	return pthread_cond_signal(*cnd);
}

int pthread_cond_destroy_fake(pthread_cond_t **cnd) {
	if (cnd && *cnd) {
		pthread_cond_destroy(*cnd);
		free(*cnd);
		*cnd = NULL;
	}
	return 0;
}

int pthread_cond_wait_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	return pthread_cond_wait(*cnd, *mtx);
}

int pthread_cond_timedwait_fake(pthread_cond_t **cnd, pthread_mutex_t **mtx, const struct timespec *t) {
	if (!*cnd) {
		if (pthread_cond_init_fake(cnd, NULL) < 0)
			return -1;
	}
	return pthread_cond_timedwait(*cnd, *mtx, t);
}

int pthread_create_fake(pthread_t *thread, const void *unused, void *entry, void *arg) {
	pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 512 * 1024);
	return pthread_create(thread, &attr, entry, arg);
}

int pthread_once_fake(volatile int *once_control, void (*init_routine)(void)) {
	if (!once_control || !init_routine)
		return -1;
	if (__sync_lock_test_and_set(once_control, 1) == 0)
		(*init_routine)();
	return 0;
}

GLuint screen_fbo;
void glBindFramebuffer_hook(GLenum target, GLuint framebuffer) {
	if (framebuffer == 0)
		framebuffer = screen_fbo;
	glBindFramebuffer(target, framebuffer);
}

extern void *__cxa_pure_virtual;

extern const char *BIONIC_ctype_;
extern const short *BIONIC_tolower_tab_;
extern const short *BIONIC_toupper_tab_;

static so_default_dynlib default_dynlib[] = {
	{ "_ctype_", (uintptr_t)&BIONIC_ctype_},
	{ "_tolower_tab_", (uintptr_t)&BIONIC_tolower_tab_},
	{ "_toupper_tab_", (uintptr_t)&BIONIC_toupper_tab_},
	{ "_Znaj", (uintptr_t)&_Znaj },
	{ "_ZdaPv", (uintptr_t)&_ZdaPv },
	{ "_Znwj", (uintptr_t)&_Znwj },
	{ "_ZdlPv", (uintptr_t)&_ZdlPv },
	{ "__aeabi_atexit", (uintptr_t)&__aeabi_atexit },
	{ "__aeabi_f2lz", (uintptr_t)&__aeabi_f2lz },
	{ "__aeabi_d2lz", (uintptr_t)&__aeabi_d2lz },
	{ "__aeabi_l2f", (uintptr_t)&__aeabi_l2f },
	{ "__aeabi_l2d", (uintptr_t)&__aeabi_l2d },
	{ "__aeabi_d2f", (uintptr_t)&__aeabi_d2f },
	{ "__aeabi_d2iz", (uintptr_t)&__aeabi_d2iz },
	{ "__aeabi_d2uiz", (uintptr_t)&__aeabi_d2uiz },
	{ "__aeabi_dadd", (uintptr_t)&__aeabi_dadd },
	{ "__aeabi_dcmpeq", (uintptr_t)&__aeabi_dcmpeq },
	{ "__aeabi_dcmpge", (uintptr_t)&__aeabi_dcmpge },
	{ "__aeabi_dcmpgt", (uintptr_t)&__aeabi_dcmpgt },
	{ "__aeabi_dcmple", (uintptr_t)&__aeabi_dcmple },
	{ "__aeabi_dcmplt", (uintptr_t)&__aeabi_dcmplt },
	{ "__aeabi_ddiv", (uintptr_t)&__aeabi_ddiv },
	{ "__aeabi_dmul", (uintptr_t)&__aeabi_dmul },
	{ "__aeabi_dsub", (uintptr_t)&__aeabi_dsub },
	{ "__aeabi_f2d", (uintptr_t)&__aeabi_f2d },
	{ "__aeabi_f2iz", (uintptr_t)&__aeabi_f2iz },
	{ "__aeabi_fadd", (uintptr_t)&__aeabi_fadd },
	{ "__aeabi_fcmpeq", (uintptr_t)&__aeabi_fcmpeq },
	{ "__aeabi_fcmpge", (uintptr_t)&__aeabi_fcmpge },
	{ "__aeabi_fcmpgt", (uintptr_t)&__aeabi_fcmpgt },
	{ "__aeabi_fcmple", (uintptr_t)&__aeabi_fcmple },
	{ "__aeabi_fcmplt", (uintptr_t)&__aeabi_fcmplt },
	{ "__aeabi_fcmpun", (uintptr_t)&__aeabi_fcmpun },
	{ "__aeabi_fdiv", (uintptr_t)&__aeabi_fdiv },
	{ "__aeabi_fmul", (uintptr_t)&__aeabi_fmul },
	{ "__aeabi_fsub", (uintptr_t)&__aeabi_fsub },
	{ "__aeabi_i2d", (uintptr_t)&__aeabi_i2d },
	{ "__aeabi_i2f", (uintptr_t)&__aeabi_i2f },
	{ "__aeabi_idiv", (uintptr_t)&__aeabi_idiv },
	{ "__aeabi_idivmod", (uintptr_t)&__aeabi_idivmod },
	{ "__aeabi_ldivmod", (uintptr_t)&__aeabi_ldivmod },
	{ "__aeabi_lmul", (uintptr_t)&__aeabi_lmul },
	{ "__aeabi_ui2d", (uintptr_t)&__aeabi_ui2d },
	{ "__aeabi_ui2f", (uintptr_t)&__aeabi_ui2f },
	{ "__aeabi_uidiv", (uintptr_t)&__aeabi_uidiv },
	{ "__aeabi_uidivmod", (uintptr_t)&__aeabi_uidivmod },
	{ "__android_log_write", (uintptr_t)&__android_log_write },
	{ "__android_log_print", (uintptr_t)&__android_log_print },
	{ "__cxa_guard_acquire", (uintptr_t)&__cxa_guard_acquire },
	{ "__cxa_guard_release", (uintptr_t)&__cxa_guard_release },
	{ "__cxa_pure_virtual", (uintptr_t)&__cxa_pure_virtual },
	{ "__dso_handle", (uintptr_t)&__dso_handle },
	{ "__sF", (uintptr_t)&__sF_fake },
	{ "__stack_chk_fail", (uintptr_t)&__stack_chk_fail },
	{ "__stack_chk_guard", (uintptr_t)&__stack_chk_guard_fake },
	{ "abort", (uintptr_t)&abort },
	{ "acos", (uintptr_t)&acos },
	{ "acosf", (uintptr_t)&acosf },
	{ "asin", (uintptr_t)&asin },
	{ "asinf", (uintptr_t)&asinf },
	{ "atan", (uintptr_t)&atan },
	{ "atan2", (uintptr_t)&atan2 },
	{ "atan2f", (uintptr_t)&atan2f },
	{ "atanf", (uintptr_t)&atanf },
	{ "atoi", (uintptr_t)&atoi },
	{ "ceilf", (uintptr_t)&ceilf },
	// { "chdir", (uintptr_t)&chdir },
	// { "close", (uintptr_t)&close },
	// { "connect", (uintptr_t)&connect },
	{ "cos", (uintptr_t)&cos },
	{ "cosf", (uintptr_t)&cosf },
	{ "exit", (uintptr_t)&exit },
	{ "expf", (uintptr_t)&expf },
	{ "fclose", (uintptr_t)&fclose },
	{ "fgetc", (uintptr_t)&fgetc },
	{ "floor", (uintptr_t)&floor },
	{ "floorf", (uintptr_t)&floorf },
	{ "fmod", (uintptr_t)&fmod },
	{ "fmodf", (uintptr_t)&fmodf },
	{ "fopen", (uintptr_t)&fopen_hook },
	{ "fprintf", (uintptr_t)&fprintf },
	{ "fread", (uintptr_t)&fread },
	{ "fflush", (uintptr_t)&fflush },
	{ "free", (uintptr_t)&free },
	{ "fseek", (uintptr_t)&fseek },
	{ "ftell", (uintptr_t)&ftell },
	{ "fwrite", (uintptr_t)&fwrite },
	// { "gethostbyname", (uintptr_t)&gethostbyname },
	{ "gettimeofday", (uintptr_t)&gettimeofday },
	{ "glActiveTexture", (uintptr_t)&glActiveTexture },
	{ "glAlphaFunc", (uintptr_t)&glAlphaFunc },
	{ "glBindBuffer", (uintptr_t)&glBindBuffer },
	{ "glBindFramebufferOES", (uintptr_t)&glBindFramebuffer_hook },
	{ "glBindTexture", (uintptr_t)&glBindTexture },
	{ "glBlendFunc", (uintptr_t)&glBlendFunc },
	{ "glBufferData", (uintptr_t)&glBufferData },
	{ "glBufferSubData", (uintptr_t)&glBufferSubData },
	{ "glClear", (uintptr_t)&glClear },
	{ "glClearColor", (uintptr_t)&glClearColor },
	{ "glClearDepthf", (uintptr_t)&glClearDepthf },
	{ "glClientActiveTexture", (uintptr_t)&glClientActiveTexture },
	{ "glClipPlanef", (uintptr_t)&glClipPlanef },
	{ "glColor4f", (uintptr_t)&glColor4f },
	{ "glColor4ub", (uintptr_t)&glColor4ub },
	{ "glColorMask", (uintptr_t)&glColorMask },
	{ "glColorPointer", (uintptr_t)&glColorPointer },
	{ "glCompressedTexImage2D", (uintptr_t)&glCompressedTexImage2D },
	{ "glCopyTexImage2D", (uintptr_t)&glCopyTexImage2D },
	{ "glCullFace", (uintptr_t)&glCullFace },
	{ "glDeleteBuffers", (uintptr_t)&glDeleteBuffers },
	{ "glDeleteTextures", (uintptr_t)&glDeleteTextures },
	{ "glDepthFunc", (uintptr_t)&glDepthFunc },
	{ "glDepthMask", (uintptr_t)&glDepthMask },
	{ "glDepthRangef", (uintptr_t)&glDepthRangef },
	{ "glDisable", (uintptr_t)&glDisable },
	{ "glDisableClientState", (uintptr_t)&glDisableClientState },
	{ "glDrawArrays", (uintptr_t)&glDrawArrays },
	{ "glDrawElements", (uintptr_t)&glDrawElements },
	{ "glEnable", (uintptr_t)&glEnable_hook },
	{ "glEnableClientState", (uintptr_t)&glEnableClientState },
	{ "glFogf", (uintptr_t)&glFogf },
	{ "glFogfv", (uintptr_t)&glFogfv },
	{ "glFrontFace", (uintptr_t)&glFrontFace },
	{ "glFrustumf", (uintptr_t)&glFrustumf },
	{ "glGenBuffers", (uintptr_t)&glGenBuffers },
	{ "glGenTextures", (uintptr_t)&glGenTextures },
	{ "glGetBooleanv", (uintptr_t)&glGetBooleanv },
	{ "glGetError", (uintptr_t)&ret0 },
	{ "glGetFloatv", (uintptr_t)&glGetFloatv },
	{ "glGetIntegerv", (uintptr_t)&glGetIntegerv },
	{ "glGetPointerv", (uintptr_t)&glGetPointerv },
	{ "glGetString", (uintptr_t)&glGetString },
	{ "glGetTexEnviv", (uintptr_t)&glGetTexEnviv },
	{ "glHint", (uintptr_t)&ret0 },
	{ "glIsEnabled", (uintptr_t)&glIsEnabled },
	{ "glLightModelfv", (uintptr_t)&glLightModelfv },
	{ "glLightf", (uintptr_t)&ret0 },
	{ "glLightfv", (uintptr_t)&glLightfv },
	{ "glLineWidth", (uintptr_t)&glLineWidth },
	{ "glLoadIdentity", (uintptr_t)&glLoadIdentity },
	{ "glLoadMatrixf", (uintptr_t)&glLoadMatrixf },
	{ "glMaterialf", (uintptr_t)&ret0 },
	{ "glMaterialfv", (uintptr_t)&glMaterialfv },
	{ "glMatrixMode", (uintptr_t)&glMatrixMode },
	{ "glMultMatrixf", (uintptr_t)&glMultMatrixf },
	{ "glNormal3f", (uintptr_t)&glNormal3f },
	{ "glNormalPointer", (uintptr_t)&glNormalPointer },
	{ "glOrthof", (uintptr_t)&glOrthof },
	{ "glOrthox", (uintptr_t)&glOrthox },
	{ "glPixelStorei", (uintptr_t)&ret0 },
	{ "glPointParameterf", (uintptr_t)&ret0 },
	{ "glPointSize", (uintptr_t)&glPointSize },
	{ "glPolygonOffset", (uintptr_t)&glPolygonOffset },
	{ "glPopMatrix", (uintptr_t)&glPopMatrix },
	{ "glPushMatrix", (uintptr_t)&glPushMatrix },
	{ "glReadPixels", (uintptr_t)&glReadPixels },
	{ "glRotatef", (uintptr_t)&glRotatef },
	{ "glScalef", (uintptr_t)&glScalef },
	{ "glScissor", (uintptr_t)&glScissor },
	{ "glShadeModel", (uintptr_t)&ret0 },
	{ "glStencilFunc", (uintptr_t)&glStencilFunc },
	{ "glStencilMask", (uintptr_t)&glStencilMask },
	{ "glStencilOp", (uintptr_t)&glStencilOp },
	{ "glTexCoordPointer", (uintptr_t)&glTexCoordPointer },
	{ "glTexEnvf", (uintptr_t)&glTexEnvf },
	{ "glTexEnvfv", (uintptr_t)&glTexEnvfv },
	{ "glTexEnvi", (uintptr_t)&glTexEnvi },
	{ "glTexImage2D", (uintptr_t)&glTexImage2D },
	{ "glTexParameterf", (uintptr_t)&glTexParameterf },
	{ "glTexParameteri", (uintptr_t)&glTexParameteri },
	{ "glTexParameterx", (uintptr_t)&glTexParameterx },
	{ "glTexSubImage2D", (uintptr_t)&glTexSubImage2D },
	{ "glTranslatef", (uintptr_t)&glTranslatef },
	{ "glVertexPointer", (uintptr_t)&glVertexPointer },
	{ "glViewport", (uintptr_t)&glViewport },
	{ "logf", (uintptr_t)&logf },
	{ "lrand48", (uintptr_t)&lrand48 },
	{ "malloc", (uintptr_t)&malloc },
	{ "memcmp", (uintptr_t)&memcmp },
	{ "memcpy", (uintptr_t)&memcpy },
	{ "memmove", (uintptr_t)&memmove },
	{ "memset", (uintptr_t)&memset },
	// { "nanosleep", (uintptr_t)&nanosleep },
	{ "pow", (uintptr_t)&pow },
	{ "powf", (uintptr_t)&powf },
	{ "pthread_attr_destroy", (uintptr_t)&ret0 },
	{ "pthread_attr_init", (uintptr_t)&ret0 },
	{ "pthread_attr_setschedparam", (uintptr_t)&ret0 },
	{ "pthread_attr_setdetachstate", (uintptr_t)&ret0 },
	{ "pthread_attr_setstacksize", (uintptr_t)&ret0 },
	{ "pthread_attr_setschedpolicy", (uintptr_t)&ret0 },
	{ "pthread_cond_signal", (uintptr_t)&pthread_cond_signal_fake},
	{ "pthread_cond_broadcast", (uintptr_t)&pthread_cond_broadcast_fake},
	{ "pthread_cond_wait", (uintptr_t)&pthread_cond_wait_fake},
	{ "pthread_create", (uintptr_t)&pthread_create_fake },
	{ "pthread_getschedparam", (uintptr_t)&pthread_getschedparam },
	{ "pthread_getspecific", (uintptr_t)&pthread_getspecific },
	{ "pthread_key_create", (uintptr_t)&pthread_key_create },
	{ "pthread_key_delete", (uintptr_t)&pthread_key_delete },
	{ "pthread_mutex_destroy", (uintptr_t)&pthread_mutex_destroy_fake },
	{ "pthread_mutex_init", (uintptr_t)&pthread_mutex_init_fake },
	{ "pthread_mutex_lock", (uintptr_t)&pthread_mutex_lock_fake },
	{ "pthread_mutex_unlock", (uintptr_t)&pthread_mutex_unlock_fake },
	{ "pthread_once", (uintptr_t)&pthread_once_fake },
	{ "pthread_self", (uintptr_t)&pthread_self },
	{ "pthread_setschedparam", (uintptr_t)&pthread_setschedparam },
	{ "pthread_setspecific", (uintptr_t)&pthread_setspecific },
	{ "printf", (uintptr_t)&printf },
	{ "puts", (uintptr_t)&puts },
	// { "recv", (uintptr_t)&recv },
	{ "remove", (uintptr_t)&remove },
	{ "rename", (uintptr_t)&rename },
	{ "sin", (uintptr_t)&sin },
	{ "sinf", (uintptr_t)&sinf },
	{ "snprintf", (uintptr_t)&snprintf },
	// { "socket", (uintptr_t)&socket },
	{ "sprintf", (uintptr_t)&sprintf },
	{ "sqrt", (uintptr_t)&sqrt },
	{ "sqrtf", (uintptr_t)&sqrtf },
	{ "srand48", (uintptr_t)&srand48 },
	{ "sscanf", (uintptr_t)&sscanf },
	{ "strcasecmp", (uintptr_t)&strcasecmp },
	{ "strcat", (uintptr_t)&strcat },
	{ "strchr", (uintptr_t)&strchr },
	{ "strcmp", (uintptr_t)&strcmp },
	{ "strcpy", (uintptr_t)&strcpy },
	{ "strdup", (uintptr_t)&strdup },
	{ "strlen", (uintptr_t)&strlen },
	{ "strncmp", (uintptr_t)&strncmp },
	{ "strpbrk", (uintptr_t)&strpbrk },
	{ "strstr", (uintptr_t)&strstr },
	{ "strtod", (uintptr_t)&strtod },
	{ "tan", (uintptr_t)&tan },
	{ "tanf", (uintptr_t)&tanf },
	{ "vsprintf", (uintptr_t)&vsprintf },
	{ "wcscmp", (uintptr_t)&wcscmp },
	{ "wcscpy", (uintptr_t)&wcscpy },
	{ "wcslen", (uintptr_t)&wcslen },
	// { "write", (uintptr_t)&write },
};

static char fake_vm[0x1000];
static char fake_env[0x1000];

enum MethodIDs {
	UNKNOWN = 0,
} MethodIDs;

typedef struct {
	char *name;
	enum MethodIDs id;
} NameToMethodID;

static NameToMethodID name_to_method_ids[] = {
};

int GetStaticMethodID(void *env, void *class, const char *name, const char *sig) {
	sceClibPrintf("GetStaticMethodID %s\n", name);
	for (int i = 0; i < sizeof(name_to_method_ids) / sizeof(NameToMethodID); i++) {
		if (strcmp(name, name_to_method_ids[i].name) == 0)
			return name_to_method_ids[i].id;
	}

	return UNKNOWN;
}

int CallStaticIntMethod(void *env, void *obj, int methodID) {
	return 0;
}

float CallStaticFloatMethod(void *env, void *obj, int methodID) {
	return 0.0f;
}

void CallStaticVoidMethod(void *env, void *obj, int methodID) {
}

void *NewGlobalRef(void) {
	return (void *)0x42424242;
}

int check_kubridge(void) {
	int search_unk[2];
	return _vshKernelSearchModuleByName("kubridge", search_unk);
}

int file_exists(const char *path) {
	SceIoStat stat;
	return sceIoGetstat(path, &stat) >= 0;
}

enum {
	KEYCODE_CIRCLE = 4,
	KEYCODE_DPAD_UP = 19,
	KEYCODE_DPAD_DOWN = 20,
	KEYCODE_DPAD_LEFT = 21,
	KEYCODE_DPAD_RIGHT = 22,
	KEYCODE_CROSS = 23,
	KEYCODE_SQUARE = 99,
	KEYCODE_TRIANGLE = 100,
	KEYCODE_BUTTON_L1 = 102,
	KEYCODE_BUTTON_R1 = 103,
	KEYCODE_BUTTON_START = 108,
	KEYCODE_BUTTON_SELECT = 109,
};

enum {
	SCANCODE_UNK = 0,
	SCANCODE_LEFT = 103,
	SCANCODE_DOWN = 105,
	SCANCODE_UP = 106,
	SCANCODE_RIGHT = 108,
	SCANCODE_START = 139,
	SCANCODE_CROSS = 304,
	SCANCODE_CIRCLE = 305,
	SCANCODE_SQUARE = 307,
	SCANCODE_TRIANGLE = 308,
	SCANCODE_L1 = 310,
	SCANCODE_R1 = 311,
	SCANCODE_SELECT = 314
};

enum {
	SOURCE_TOUCHSCREEN = 0x00001002,
	SOURCE_TOUCHPAD = 0x00100008,
};

int GetEnv(void *vm, void **env, int r2) {
	*env = fake_env;
	return 0;
}

int AttachCurrentThread(void *vm, void **p_env, void *thr_args) {
	GetEnv(vm, p_env, 0);
	return 0;
}

void video_open(const char *path);
int video_draw();
void video_stop();
void video_init();
void video_draw_generic(GLuint tex);

void *pthread_main(void *arg);

int main(int argc, char *argv[]) {
	pthread_t t;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024 * 1024);
	pthread_create(&t, &attr, pthread_main, NULL);

	return sceKernelExitDeleteThread(0);
}

void *pthread_main(void *arg) {
	sceSysmoduleLoadModule(SCE_SYSMODULE_RAZOR_CAPTURE);
#ifdef EXCLUSIVE_VIDEO_PLAYER
	sceSysmoduleLoadModule(SCE_SYSMODULE_AVPLAYER);
#endif
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	
	// Check if we want to start the companion app
	sceAppUtilInit(&(SceAppUtilInitParam){}, &(SceAppUtilBootParam){});
	SceAppUtilAppEventParam eventParam;
	sceClibMemset(&eventParam, 0, sizeof(SceAppUtilAppEventParam));
	sceAppUtilReceiveAppEvent(&eventParam);
	if (eventParam.type == 0x05) {
		char buffer[2048];
		sceAppUtilAppEventParseLiveArea(&eventParam, buffer);
		if (strstr(buffer, "config"))
			sceAppMgrLoadExec("app0:/companion.bin", NULL, NULL);
	}
	
	// Load game options
	opts.far_boost = 1.0f;
	opts.have_fog = 0;
	opts.framerate = 30;
	opts.msaa = 2;
	opts.preset = 2;
	opts.cam_slowdown = 8;
	opts.xperia_res = 1;
	opts.hide_elements = JOYSTICK_ELEM;
	opts.autoframerate = 0;

	char buffer[30];
	int value;

	FILE *config = fopen(CONFIG_FILE_PATH, "r");
	if (config) {
		while (EOF != fscanf(config, "%[^ ] %d\n", buffer, &value)) {
			if (strcmp("fps", buffer) == 0) opts.framerate = value;
			else if (strcmp("draw_distance", buffer) == 0) opts.far_boost = (float)value / 100.0f;
			else if (strcmp("fog", buffer) == 0) opts.have_fog = value;
			else if (strcmp("msaa", buffer) == 0) opts.msaa = value;
			else if (strcmp("graphics", buffer) == 0) opts.preset = value;
			else if (strcmp("cam_slowdown", buffer) == 0) opts.cam_slowdown = value;
			else if (strcmp("xperia_res", buffer) == 0) opts.xperia_res = value;
			else if (strcmp("hide_elem", buffer) == 0) opts.hide_elements = value;
		}
		fclose(config);
	}
	
	// IF autoframerate is enabled, force to target 60 FPS
	if (opts.autoframerate)
		opts.framerate = TARGET_FPS;

	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);

	if (check_kubridge() < 0)
		fatal_error("Error kubridge.skprx is not installed.");

	if (!file_exists("ur0:/data/libshacccg.suprx") && !file_exists("ur0:/data/external/libshacccg.suprx"))
		fatal_error("Error libshacccg.suprx is not installed.");

	if (so_file_load(&main_mod, SO_PATH, LOAD_ADDRESS) < 0)
		fatal_error("Error could not load %s.", SO_PATH);

	so_relocate(&main_mod);
	so_resolve(&main_mod, default_dynlib, sizeof(default_dynlib), 0);

#ifdef EXCLUSIVE_VIDEO_PLAYER
	vglInitWithCustomThreshold(0, SCREEN_W, SCREEN_H, 16 * 1024 * 1024, 32 * 1024 * 1024, 0, 26 * 1024 * 1024, SCE_GXM_MULTISAMPLE_NONE);
	
	// Play logo videos
	SceCtrlData pad;
	video_open("ux0:data/splintercell/gameloft/games/SCHP/data/intro/logo.mp4");
	while (!video_draw()) {
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons)
			video_stop();
		vglSwapBuffers(GL_FALSE);
	}
	video_open("ux0:data/splintercell/gameloft/games/SCHP/data/intro/SC5_intro_854.mp4");
	while (!video_draw()) {
		sceCtrlPeekBufferPositive(0, &pad, 1);
		if (pad.buttons)
			video_stop();
		vglSwapBuffers(GL_FALSE);
	}

	sceAppMgrLoadExec("app0:main.bin", NULL, NULL);
#else
	vglUseTripleBuffering(GL_FALSE);
	vglSetVertexPoolSize(70 * 1024 * 1024);
	vglInitWithCustomThreshold(0, SCREEN_W, SCREEN_H, 0, 0, 0, 0, opts.msaa);

	if (opts.xperia_res)
		video_init();
	patch_game();
	so_flush_caches(&main_mod);
	so_initialize(&main_mod);

	memset(fake_vm, 'A', sizeof(fake_vm));
	*(uintptr_t *)(fake_vm + 0x00) = (uintptr_t)fake_vm; // just point to itself...
	*(uintptr_t *)(fake_vm + 0x10) = (uintptr_t)AttachCurrentThread;
	*(uintptr_t *)(fake_vm + 0x18) = (uintptr_t)GetEnv;

	int (* JNI_OnLoad)(void *vm, void *reserved) = (void *)so_symbol(&main_mod, "JNI_OnLoad");
	if (JNI_OnLoad)
		JNI_OnLoad(fake_vm, NULL);
	
	memset(fake_env, 'A', sizeof(fake_env));
	*(uintptr_t *)(fake_env + 0x00) = (uintptr_t)fake_env; // just point to itself...
	*(uintptr_t *)(fake_env + 0x54) = (uintptr_t)NewGlobalRef;
	*(uintptr_t *)(fake_env + 0x1C4) = (uintptr_t)GetStaticMethodID;
	*(uintptr_t *)(fake_env + 0x204) = (uintptr_t)CallStaticIntMethod;
	*(uintptr_t *)(fake_env + 0x21C) = (uintptr_t)CallStaticFloatMethod;
	*(uintptr_t *)(fake_env + 0x234) = (uintptr_t)CallStaticVoidMethod;
	
	int (* Game_nativeInit)(void *env, void *obj, int hide_virtual_left_stick);
	int (* nativeResize)(void *env, void *obj, int width, int height);
	int (* GameRenderer_nativeInit)(void *env, void *obj, int is_moto, int w, int h, int lang);
	int (* nativeRender)();
	int (* GLMediaPlayer_nativeInit)(void *env, void *obj);
	int (* nativeResume)();
	int (* nativeOnTouch)(void *env, void *obj, int port, int action, int x, int y, int id);
	int (* appKeyPressed)(int id, int scancode) = (void *)so_symbol(&main_mod, "appOnKeyDown");
	int (* appKeyReleased)(int id, int scancode) = (void *)so_symbol(&main_mod, "appOnKeyUp");

	Game_nativeInit = (void *)so_symbol(&main_mod, "Java_com_gameloft_android_GAND_GloftSCHP_ML_SCHP_nativeInit");
	nativeResize = (void *)so_symbol(&main_mod, "Java_com_gameloft_android_GAND_GloftSCHP_ML_GameRenderer_nativeResize");
	GameRenderer_nativeInit = (void *)so_symbol(&main_mod, "Java_com_gameloft_android_GAND_GloftSCHP_ML_GameRenderer_nativeInit");
	nativeRender = (void *)so_symbol(&main_mod, "Java_com_gameloft_android_GAND_GloftSCHP_ML_GameRenderer_nativeRender");
	GLMediaPlayer_nativeInit = (void *)so_symbol(&main_mod, "Java_com_gameloft_android_GAND_GloftSCHP_ML_GLMediaPlayer_nativeInit");
	nativeResume = (void *)so_symbol(&main_mod, "Java_com_gameloft_android_GAND_GloftSCHP_ML_GameGLSurfaceView_nativeResume");
	nativeOnTouch = (void *)so_symbol(&main_mod, "Java_com_gameloft_android_GAND_GloftSCHP_ML_GameGLSurfaceView_nativeOnTouch");
	
	// Detect language
	int lang = -1;
	sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &lang);
	switch (lang) {
	case SCE_SYSTEM_PARAM_LANG_ITALIAN:
		lang = 4;
		break;
	case SCE_SYSTEM_PARAM_LANG_SPANISH:
		lang = 3;
		break;
	case SCE_SYSTEM_PARAM_LANG_GERMAN:
		lang = 2;
		break;
	case SCE_SYSTEM_PARAM_LANG_JAPANESE:
		lang = 5;
		break;
	case SCE_SYSTEM_PARAM_LANG_FRENCH:
		lang = 1;
		break;
	default:
		lang = 0;
		break;
	}

	int TARGET_W = SCREEN_W;
	int TARGET_H = SCREEN_H;
	float TOUCHSCREEN_MULT_X = 1.0f;
	float TOUCHSCREEN_MULT_Y = 1.0f;
	GLuint screen_tex;
	if (opts.xperia_res) {
		glGenFramebuffers(1, &screen_fbo);
		glGenTextures(1, &screen_tex);
		glBindTexture(GL_TEXTURE_2D, screen_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, XPERIA_W, XPERIA_H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, screen_tex, 0);
		TARGET_W = XPERIA_W;
		TARGET_H = XPERIA_H;
		TOUCHSCREEN_MULT_X = (float)TARGET_W / (float)SCREEN_W;
		TOUCHSCREEN_MULT_Y = (float)TARGET_H / (float)SCREEN_H;		
	}

	// High presets: 3,4,5,9,11,12
	// Mid: 0
	// Low: 1, 8
	switch (opts.preset) {
	case 0:
		GameRenderer_nativeInit(fake_env, NULL, 8, TARGET_W, TARGET_H, lang);
		break;
	case 1:
		GameRenderer_nativeInit(fake_env, NULL, 0, TARGET_W, TARGET_H, lang);
		break;
	default:
		GameRenderer_nativeInit(fake_env, NULL, 3, TARGET_W, TARGET_H, lang);
		break;
	}
	nativeResize(fake_env, NULL, TARGET_W, TARGET_H);
	Game_nativeInit(fake_env, NULL, 0);
	GLMediaPlayer_nativeInit(fake_env, NULL);
	
	float frametime = 1.0f / (float)opts.framerate;
	uint32_t frame_ms = (uint32_t)(frametime * 1000.0f);
	sceClibPrintf("Booting game with this configuration:\n");
	sceClibPrintf("Graphics quality level: %d\n", opts.preset);
	sceClibPrintf("Framerate target: %d (Frametime: %d ms) - Autoframerate: %s\n", opts.framerate, frame_ms, opts.autoframerate ? "On" : "Off");
	sceClibPrintf("Draw Distance: %d%% %s fog\n", (int)(opts.far_boost * 100.0f), opts.have_fog ? "with" : "without");
	sceClibPrintf("Anti-Aliasing: MSAA %dx\n", opts.msaa * 2);
	sceClibPrintf("Camera Slowdown Factor: %d\n", opts.cam_slowdown);
	sceClibPrintf("Game Resolution: %s\n", opts.xperia_res ? "850x480" : "960x544");
	sceClibPrintf("Hidden Elements Mask: 0x%x\n", opts.hide_elements);

	float *ticksPerFrame = (float *)so_symbol(&main_mod, "_ZN12GApplication18m_FPSTicksPerFrameE");
	int lastX[2] = { -1, -1 };
	int lastY[2] = { -1, -1 };
	
	#define physicalInput(btn, id, code) \
		if ((pad.buttons & btn) == btn) { \
			appKeyPressed(id, code); \
		} else if ((oldpad & btn) == btn) { \
			appKeyReleased(id, code); \
		}		
	
	uint32_t oldpad = 0;
	int left_joy_active = 0, right_joy_active = 0;
	sceClibPrintf("Entering main loop\n");
	nativeResume();
	while (1) {
		SceTouchData touch;
		sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);
		SceCtrlData pad;
		sceCtrlPeekBufferPositive(0, &pad, 1);

		// Touch emulation
		if (pad.lx > 162 || pad.lx < 92 || pad.ly > 162 || pad.ly < 92) {
			if (left_joy_active) {
				nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 2, 124 + ((int)pad.lx - 127) / 2, 356 + ((int)pad.ly - 127) / 2, 0);
			} else {
				nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 1, 124, 356, 0);
				left_joy_active = 1;
			}
		} else if (left_joy_active) {
			left_joy_active = 0;
			nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 0, 124, 356, 0);
		}

		if (pad.rx > 162 || pad.rx < 92 || pad.ry > 162 || pad.ry < 92) {
			if (right_joy_active) {
				nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 2, 520, 300, 1);
			} else {
				nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 1, 520, 300, 1);
				right_joy_active = 1;
			}
		} else if (right_joy_active) {
			right_joy_active = 0;
			nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 0, 520, 300, 1);
		}

		physicalInput(SCE_CTRL_CROSS, KEYCODE_CROSS, SCANCODE_CROSS)
		physicalInput(SCE_CTRL_CIRCLE, KEYCODE_CIRCLE, SCANCODE_CIRCLE)
		physicalInput(SCE_CTRL_SQUARE, KEYCODE_SQUARE, SCANCODE_SQUARE)
		physicalInput(SCE_CTRL_TRIANGLE, KEYCODE_TRIANGLE, SCANCODE_TRIANGLE)
		physicalInput(SCE_CTRL_UP, KEYCODE_DPAD_UP, SCANCODE_UP)
		physicalInput(SCE_CTRL_LEFT, KEYCODE_DPAD_LEFT, SCANCODE_LEFT)
		physicalInput(SCE_CTRL_RIGHT, KEYCODE_DPAD_RIGHT, SCANCODE_RIGHT)
		physicalInput(SCE_CTRL_DOWN, KEYCODE_DPAD_DOWN, SCANCODE_DOWN)
		physicalInput(SCE_CTRL_LTRIGGER, KEYCODE_BUTTON_L1, SCANCODE_L1)
		physicalInput(SCE_CTRL_RTRIGGER, KEYCODE_BUTTON_R1, SCANCODE_R1)
		physicalInput(SCE_CTRL_SELECT, KEYCODE_BUTTON_SELECT, SCANCODE_SELECT)
		physicalInput(SCE_CTRL_START, KEYCODE_BUTTON_START, SCANCODE_START)
		oldpad = pad.buttons;
		
		if (!(left_joy_active || right_joy_active)) {
			for (int i = 0; i < 2; i++) {
				if (i < touch.reportNum) {
					int x = (int)((float)(touch.report[i].x * 0.5f) * TOUCHSCREEN_MULT_X);
					int y = (int)((float)(touch.report[i].y * 0.5f) * TOUCHSCREEN_MULT_Y);

					if (lastX[i] == -1 || lastY[i] == -1) {
						nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 1, x, y, i);
					} else if (lastX[i] != x || lastY[i] != y) {
						nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 2, x, y, i);
					}
					lastX[i] = x;
					lastY[i] = y;
				} else {
					if (lastX[i] != -1 || lastY[i] != -1) {
						nativeOnTouch(fake_env, NULL, SOURCE_TOUCHSCREEN, 0, lastX[i], lastY[i], i);
					}
					lastX[i] = -1;
					lastY[i] = -1;
				}
			}
		}
		
		*ticksPerFrame = frametime;
		uint32_t tick = sceKernelGetProcessTimeLow();
		//GLboolean scissor_enabled;
		if (opts.xperia_res) {
			glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo);
			glViewport(0, 0, TARGET_W, TARGET_H);
		}
		nativeRender();
		if (opts.xperia_res) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, SCREEN_W, SCREEN_H);
			//glGetBooleanv(GL_SCISSOR_TEST, &scissor_enabled);
			glDisable(GL_SCISSOR_TEST);
			glClear(GL_COLOR_BUFFER_BIT);
			video_draw_generic(screen_tex);
			//if (scissor_enabled)
			//	glEnable(GL_SCISSOR_TEST);
		}
		vglSwapBuffers(GL_FALSE);
		uint32_t delta = (sceKernelGetProcessTimeLow() - tick) / 1000;
		if (!opts.autoframerate) {
			if (delta < frame_ms)
				sceKernelDelayThread((frame_ms - delta) * 1000);
		} else {
			frametime = (float)delta / 1000.0f;
		}
	}
#endif
	return NULL;
}
