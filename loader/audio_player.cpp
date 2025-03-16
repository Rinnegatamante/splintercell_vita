#include <vitasdk.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define MAX_SOUNDS_NUM (681)
#define MAX_MUSICS_NUM (681)

//#define USE_SDL_MIXER_EXT
//#define USE_SDL_MIXER
#define USE_SOLOUD

#define HAVE_PRELOAD

extern "C" {
void *audio_player_preload(char *path, size_t *sz) {
	FILE *f = fopen(path, "rb");
	fseek(f, 0, SEEK_END);
	*sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	void *buf = malloc(*sz);
	fread(buf, 1, *sz, f);
	fclose(f);
	return buf;
}
};

#if defined(USE_SDL_MIXER_EXT) || defined(USE_SDL_MIXER)
#include <SDL2/SDL.h>
#ifdef USE_SDL_MIXER_EXT
#include <SDL2/SDL_mixer_ext.h>
#else
#include <SDL2/SDL_mixer.h>
#endif

#ifdef USE_SDL_MIXER
typedef struct {
	int handle;
	void *buffer;
	void *source;
	bool valid;
} audio_instance;

audio_instance snd[MAX_SOUNDS_NUM];
audio_instance snd_loop[MAX_MUSICS_NUM];
#endif

extern "C" {

void audio_player_init() {
	SDL_Init(SDL_INIT_AUDIO);
#ifdef USE_SDL_MIXER_EXT
	Mix_Init(MIX_INIT_OGG);
#endif
	Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 1024);
}

float audio_player_get_volume(void *m) {
#ifdef USE_SDL_MIXER
	audio_instance *mus = (audio_instance *)m;
	if (mus == &bgm && mus->valid) {
		return (float)Mix_VolumeMusic(-1) / 128.0f;
	} else if (mus->valid) {
		return (float)Mix_VolumeChunk((Mix_Chunk *)mus->source, -1) / 128.0f;
	}
#else
	return (float)Mix_VolumeMusicStream((Mix_Music *)m, -1) / 128.0f;
#endif
}

void audio_player_set_volume(void *m, float vol) {
#ifdef USE_SDL_MIXER
	audio_instance *mus = (audio_instance *)m;
	if (mus == &bgm && mus->valid) {
		Mix_VolumeMusic((int)(128.0f * vol));
	} else if (mus->valid) {
		Mix_VolumeChunk((Mix_Chunk *)mus->source, (int)(128.0f * vol));
		Mix_Volume(mus->handle, (int)(128.0f * vol));
	}
#else
	Mix_VolumeMusicStream((Mix_Music *)m, (int)(128.0f * vol));
#endif
}

void *audio_player_play(char *path, uint8_t loop, float vol, uint8_t autofree, size_t buf_size, int id) {
#ifdef USE_SDL_MIXER_EXT
#ifndef HAVE_PRELOAD
	Mix_Music *ret = Mix_LoadMUS(path);
#else
	Mix_Music *ret = Mix_LoadMUS_RW(SDL_RWFromConstMem(path, buf_size), 1);
#endif
	if (!ret) {
		sceClibPrintf("Failed to load %s\n", path);
		sceClibPrintf("Mix_Error: %s\n", Mix_GetError());
	}
	Mix_VolumeMusicStream(ret, (int)(128.0f * vol));
	Mix_PlayMusicStream(ret, loop ? -1 : 0);
	if (autofree)
		Mix_SetFreeOnStop(ret, 1);
	return (void *)ret;
#else
	if (loop) {
		//sceClibPrintf("Loading %s in music slot %d\n", path, id);
#ifndef HAVE_PRELOAD
		Mix_Music *ret = Mix_LoadMUS(path);
#else
		Mix_Music *ret = Mix_LoadMUS_RW(SDL_RWFromConstMem(path, buf_size), 1);
#endif
		bgm.valid = true;
		bgm.source = (void *)ret;
		Mix_VolumeMusic((int)(128.0f * vol));
		Mix_PlayMusic(ret, -1);
		return &bgm;
	} else {
		//sceClibPrintf("Loading %s in sound slot %d\n", path, id);
		snd[id].valid = true;
#ifndef HAVE_PRELOAD
		SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0777);
		size_t sz = sceIoLseek32(fd, 0, SCE_SEEK_END);
		snd[id].buffer = malloc(sz);
		sceIoLseek32(fd, 0, SCE_SEEK_SET);
		sceIoRead(fd, snd[id].buffer, sz);
		sceIoClose(fd);
		Mix_Chunk *_snd = Mix_LoadWAV_RW(SDL_RWFromConstMem(snd[id].buffer, sz), 1);
#else
		Mix_Chunk *_snd = Mix_LoadWAV_RW(SDL_RWFromConstMem(path, buf_size), 1);
#endif
		snd[id].source = (void *)_snd;
		Mix_VolumeChunk(_snd, (int)(128.0f * vol));
		snd[id].handle = Mix_PlayChannel(-1, _snd, 0);
		return (void *)&snd[id];
	}
#endif
}

void audio_player_instance(void *m, uint8_t loop, float vol) {
#ifdef USE_SDL_MIXER
	audio_instance *mus = (audio_instance *)m;
	if (loop) {
		Mix_PlayMusic((Mix_Music *)mus->source, -1);
	} else {
		mus->handle = Mix_PlayChannel(-1, (Mix_Chunk *)mus->source, 0);
	}
	if (vol < 2.0f)
		audio_player_set_volume(m, vol);
#else
	Mix_Music *mus = (Mix_Music *)m;
	if (vol < 2.0f)
		Mix_VolumeMusicStream(mus, (int)(128.0f * vol));
	Mix_PlayMusicStream(mus, loop ? -1 : 0);
#endif
}

int audio_player_is_playing(void *m) {
#ifdef USE_SDL_MIXER
	audio_instance *mus = (audio_instance *)m;
	if (mus == &bgm) {
		return Mix_PlayingMusic();
	} else if (mus->valid) {
		return Mix_Playing(mus->handle);
	}
	return 0;
#else
	return Mix_PlayingMusicStream((Mix_Music *)m);
#endif
}

void audio_player_stop(void *m) {
#ifdef USE_SDL_MIXER
	audio_instance *mus = (audio_instance *)m;
	if (mus == &bgm && mus->valid) {
		Mix_HaltMusic();
		Mix_FreeMusic((Mix_Music *)mus->source);
	} else if (mus->valid) {
		Mix_HaltChannel(mus->handle);
		Mix_FreeChunk((Mix_Chunk *)mus->source);
#ifndef HAVE_PRELOAD
		free(mus->buffer);
#endif
	}
	mus->valid = false;
#else
	Mix_Music *mus = (Mix_Music *)m;
	Mix_HaltMusicStream(mus);
	Mix_FreeMusic(mus);
#endif
}

void audio_player_set_pause(void *m, uint8_t val) {
#ifdef USE_SDL_MIXER
	audio_instance *mus = (audio_instance *)m;
	if (mus == &bgm && mus->valid) {
		val ? Mix_PauseMusic() : Mix_ResumeMusic();
	} else if (mus->valid) {
		val ? Mix_Pause(mus->handle) : Mix_Resume(mus->handle);
	}
#else
	val ? Mix_PauseMusicStream((Mix_Music *)m) : Mix_ResumeMusicStream((Mix_Music *)m);		
#endif
}

void audio_player_stop_all_sounds() {
#ifdef USE_SDL_MIXER
	audio_player_stop((void *)&bgm);
	for (int i = 0; i < MAX_SOUNDS_NUM; i++) {
		audio_player_stop((void *)&snd[i]);
	}
#endif
}

void audio_player_set_pause_all_sounds(uint8_t val) {
#ifdef USE_SDL_MIXER
	audio_player_set_pause((void *)&bgm, val);
	for (int i = 0; i < MAX_SOUNDS_NUM; i++) {
		audio_player_set_pause((void *)&snd[i], val);
	}
#else
	val ? Mix_PauseMusicStreamAll() : Mix_ResumeMusicStreamAll();
#endif
}

void audio_player_change_bgm_volume(float vol) {
#ifdef USE_SDL_MIXER
	audio_player_set_volume((void *)&bgm, vol);
#endif
}

void audio_player_change_sfx_volume(float vol) {
#ifdef USE_SDL_MIXER
	for (int i = 0; i < MAX_SOUNDS_NUM; i++) {
		audio_player_set_volume((void *)&snd[i], vol);
	}
#endif
}
};
#endif

#ifdef USE_SOLOUD
#include "soloud.h"
#include "soloud_wavstream.h"

typedef struct {
	int handle;
	SoLoud::WavStream source;
	bool valid;
} audio_instance;

SoLoud::Soloud soloud;
audio_instance snd[MAX_SOUNDS_NUM];
audio_instance snd_loop[MAX_MUSICS_NUM];
audio_instance cycling_snds[MAX_SOUNDS_NUM];
uint32_t cycling_snds_idx[3] = {0, 0, 0};

extern "C" {

void audio_player_init() {
	soloud.init();
}

void audio_player_set_volume(void *m, float vol) {
	audio_instance *mus = (audio_instance *)m;
	mus->source.setVolume(vol);
	if (soloud.isValidVoiceHandle(mus->handle))
		soloud.setVolume(mus->handle, vol);
}

float audio_player_get_volume(void *m) {
	audio_instance *mus = (audio_instance *)m;
	return soloud.isValidVoiceHandle(mus->handle) ? soloud.getVolume(mus->handle) : 1.0f;
}

void *audio_player_play(char *path, uint8_t loop, float vol, uint8_t autofree, size_t buf_size, int id) {
	if (loop) {
		snd_loop[id].valid = true;
#ifndef HAVE_PRELOAD
		//sceClibPrintf("Loading %s in music slot %d\n", path, curr_snd_loop);
		snd_loop[id].source.load(path);
#else
		snd_loop[id].source.loadMem((const unsigned char *)path, buf_size, false, false);
#endif
		snd_loop[id].source.setVolume(vol);
		snd_loop[id].source.setLooping(true);
		snd_loop[id].source.setSingleInstance(true);
		snd_loop[id].handle = soloud.playBackground(snd_loop[id].source);
		return (void *)&snd_loop[id];
	} else {
		if (!autofree) {
			snd[id].valid = true;
#ifndef HAVE_PRELOAD
			//sceClibPrintf("Loading %s in sound slot %d\n", path, curr_snd);
			snd[id].source.load(path);
#else
			snd[id].source.loadMem((const unsigned char *)path, buf_size, false, false);
#endif
			snd[id].source.setVolume(vol);
			snd[id].source.setLooping(false);
			snd[id].handle = soloud.play(snd[id].source);
			return (void *)&snd[id];
		} else {
			uint32_t snd_ix = cycling_snds_idx[id] + id * (MAX_SOUNDS_NUM / 3);
#ifndef HAVE_PRELOAD
			//sceClibPrintf("Loading %s in sound slot %d\n", path, curr_snd);
			cycling_snds[snd_ix].source.load(path);
#else
			cycling_snds[snd_ix].source.loadMem((const unsigned char *)path, buf_size, false, false);
#endif
			cycling_snds[snd_ix].source.setVolume(vol);
			cycling_snds[snd_ix].source.setLooping(false);
			soloud.play(cycling_snds[snd_ix].source);
			cycling_snds_idx[id] = (cycling_snds_idx[id] + 1) % (MAX_SOUNDS_NUM / 3);
			return NULL;			
		}
	}
}

void audio_player_instance(void *m, uint8_t loop, float vol) {
	audio_instance *mus = (audio_instance *)m;
	if (loop) {
		mus->handle = soloud.playBackground(mus->source);
	} else {
		mus->handle = soloud.play(mus->source);
	}
	if (vol < 2.0f)
		audio_player_set_volume(m, vol);
}

int audio_player_is_playing(void *m) {
	audio_instance *mus = (audio_instance *)m;
	return (soloud.isValidVoiceHandle(mus->handle) && !soloud.getPause(mus->handle));
}

void audio_player_stop(void *m) {
	audio_instance *mus = (audio_instance *)m;
	mus->source.stop();
	mus->valid = false;
}

void audio_player_set_pause(void *m, uint8_t val) {
	audio_instance *mus = (audio_instance *)m;
	soloud.setPause(mus->handle, val);
}

void audio_player_stop_all_sounds() {
	soloud.stopAll();
	for (int i = 0; i < MAX_MUSICS_NUM; i++) {
		snd_loop[i].valid = false;
	}
	for (int i = 0; i < MAX_SOUNDS_NUM; i++) {
		snd[i].valid = false;
	}
}

void audio_player_set_pause_all_sounds(uint8_t val) {
	soloud.setPauseAll(val);
}

void audio_player_change_bgm_volume(float vol) {
	for (int i = 0; i < MAX_MUSICS_NUM; i++) {
		if (snd_loop[i].valid) {
			audio_player_set_volume((void *)&snd_loop[i], vol);
		}
	}
}

void audio_player_change_sfx_volume(float vol) {
	for (int i = 0; i < MAX_SOUNDS_NUM; i++) {
		if (snd[i].valid) {
			audio_player_set_volume((void *)&snd[i], vol);
		}
	}
}
};
#endif
