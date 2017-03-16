
#include <irrKlang.h>
#include <stdbool.h>
#include <stddef.h>

extern "C"
{
#include <whitgl/logging.h>
}
#include <whitgl/sound.h>


// FMOD_SYSTEM *fmodSystem;
// typedef struct
// {
// 	int id;
// 	FMOD_SOUND* sound;
// 	FMOD_CHANNEL* channel;
// } whitgl_sound;
// #define WHITGL_SOUND_MAX (64)
// whitgl_sound sounds[WHITGL_SOUND_MAX];
// int num_sounds;

// void _whitgl_sound_errcheck(const char* location, FMOD_RESULT result)
// {
//     if (result != FMOD_OK)
//     	WHITGL_LOG("FMOD error in %s! (%d) %s\n", location, result, FMOD_ErrorString(result));
// }

irrklang::ISoundEngine* irrklang_engine = NULL;
typedef struct
{
	int id;
	irrklang::ISoundSource* sound;
} whitgl_sound;
#define WHITGL_SOUND_MAX (64)
whitgl_sound sounds[WHITGL_SOUND_MAX];
int num_sounds;

void whitgl_sound_init()
{
	irrklang_engine = irrklang::createIrrKlangDevice();
	if(!irrklang_engine)
		WHITGL_PANIC("Could not startup irrklang engine\n");
	num_sounds = 0;
}
void whitgl_sound_shutdown()
{
	if(!irrklang_engine)
		WHITGL_PANIC("whitgl_sound_shutdown without whitgl_sound_init?");
	irrklang_engine->drop();
}
void whitgl_sound_update()
{
}
void whitgl_sound_volume(float volume)
{
	irrklang_engine->setSoundVolume(volume);
}
void whitgl_sound_add(int id, const char* filename)
{
	if(num_sounds >= WHITGL_SOUND_MAX)
		WHITGL_PANIC("ran out of sounds");
	sounds[num_sounds].id = id;

	sounds[num_sounds].sound = irrklang_engine->addSoundSourceFromFile(filename, irrklang::ESM_NO_STREAMING, true);
	num_sounds++;
}
int _whitgl_get_index(int id)
{
	int index = -1;
	int i;
	for(i=0; i<num_sounds; i++)
	{
		if(sounds[i].id == id)
		{
			index = i;
			continue;
		}
	}
	if(index == -1)
		WHITGL_PANIC("ERR Cannot find sound %d", id);
	return index;
}
void whitgl_sound_play(int id, float volume, float pitch)
{
	whitgl_int index = _whitgl_get_index(id);
	irrklang::ISound* sound = irrklang_engine->play2D(sounds[index].sound, false, true);
	sound->setVolume(volume);
	sound->setPlaybackSpeed(pitch);
	sound->setIsPaused(false);
	sound->drop();
}

void whitgl_loop_add(int id, const char* filename)
{
	(void)id;
	(void)filename;
}
void whitgl_loop_add_positional(int id, const char* filename)
{
	(void)id;
	(void)filename;
}
void whitgl_loop_volume(int id, float volume)
{
	(void)id;
	(void)volume;
}
void whitgl_loop_set_paused(int id, bool paused)
{
	(void)id;
	(void)paused;
}
void whitgl_loop_seek(int id, float time)
{
	(void)id;
	(void)time;
}
void whitgl_loop_frequency(int id, float adjust)
{
	(void)id;
	(void)adjust;
}
void whitgl_loop_set_listener(whitgl_fvec p, whitgl_fvec v, whitgl_float angle)
{
	(void)p;
	(void)v;
	(void)angle;
}
void whitgl_loop_set_position(int id, whitgl_fvec p, whitgl_fvec v)
{
	(void)id;
	(void)p;
	(void)v;
}
