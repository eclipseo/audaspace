#pragma once

#include "AUD_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* Creates a new PlaybackManager object.
* \param device The device that will be used to play sounds.
* \return The new PlaybackManager object.
*/
extern AUD_API AUD_PlaybackManager* AUD_PlaybackManager_create(AUD_Device* device);

/**
* Deletes a PlaybackManager object.
* \param manager The PlaybackManager object to be deleted.
*/
extern AUD_API void AUD_PlaybackManager_free(AUD_PlaybackManager* manager);

/**
* Plays a sound through the playback manager, adding it into a category.
* \param manager The PlaybackManager object.
* \param sound The sound to be played.
* \param catKey The key of the category into which the sound will be added. If it doesn't exist a new one will be creatd.
*/
extern AUD_API void AUD_PlaybackManager_play(AUD_PlaybackManager* manager, AUD_Sound* sound, unsigned int catKey);

/**
* Resumes the playback of all the paused sounds assigned to a category of a playback manager.
* \param manager The PlaybackManager object.
* \param catKey The key of the category.
* \return 0 if the category doesn't exist.
*/
extern AUD_API int AUD_PlaybackManager_resume(AUD_PlaybackManager* manager, unsigned int catKey);

/**
* Pauses all the sounds assigned to a category of a playback manager.
* \param manager The PlaybackManager object.
* \param catKey The key of the category.
* \return 0 if the category doesn't exist.
*/
extern AUD_API int AUD_PlaybackManager_pause(AUD_PlaybackManager* manager, unsigned int catKey);

/**
* Retrieves the volume of a category of a playback manager.
* \param manager The PlaybackManager object.
* \param catKey The key of the category.
* \return The volume of the category.
*/
extern AUD_API float AUD_PlaybackManager_getVolume(AUD_PlaybackManager* manager, unsigned int catKey);

/**
* Changes the voulume of a category of a playback manager.
* \param manager The PlaybackManager object.
* \param volume The new volume of the category.
* \param catKey The key of the category.
* \return 0 if the category doesn't exist.
*/
extern AUD_API int AUD_PlaybackManager_setVolume(AUD_PlaybackManager* manager, float volume, unsigned int catKey);

/**
* Stops all the sounds assigned to a category of a playback manager.
* \param manager The PlaybackManager object.
* \param catKey The key of the category.
* \return 0 if the category doesn't exist.
*/
extern AUD_API int AUD_PlaybackManager_stop(AUD_PlaybackManager* manager, unsigned int catKey);

/**
* Cleans all the invalid handles in a playback manager
* \param manager The PlaybackManager object.
*/
extern AUD_API void AUD_PlaybackManager_clean(AUD_PlaybackManager* manager);

#ifdef __cplusplus
}
#endif