/*******************************************************************************
 * Copyright 2009-2013 Jörg Müller
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#pragma once

#include "AUD_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Loads a sound file from a memory buffer.
 * \param buffer The buffer which contains the sound file.
 * \param size The size of the buffer.
 * \return A handle of the sound file.
 */
extern AUD_Sound* AUD_Sound_bufferFile(unsigned char* buffer, int size);

/**
 * Caches a sound into a memory buffer.
 * \param sound The sound to cache.
 * \return A handle of the cached sound.
 */
extern AUD_Sound* AUD_Sound_cache(AUD_Sound* sound);

/**
 * Loads a sound file.
 * \param filename The filename of the sound file.
 * \return A handle of the sound file.
 */
extern AUD_Sound* AUD_Sound_file(const char* filename);

/**
 * Creates a sawtooth sound.
 * \param frequency The frequency of the generated sawtooth sound.
 * \param rate The sample rate of the sawtooth sound.
 * \return A handle of the sound.
 */
extern AUD_Sound* AUD_Sound_sawtooth(float frequency, aud::SampleRate rate);

/**
 * Creates a quiet sound.
 * \return A handle of the sound.
 */
extern AUD_Sound* AUD_Sound_silence();

/**
 * Creates a sine sound.
 * \param frequency The frequency of the generated sine sound.
 * \param rate The sample rate of the sine sound.
 * \return A handle of the sound.
 */
extern AUD_Sound* AUD_Sound_sine(float frequency, aud::SampleRate rate);

/**
 * Creates a square sound.
 * \param frequency The frequency of the generated square sound.
 * \param rate The sample rate of the square sound.
 * \return A handle of the sound.
 */
extern AUD_Sound* AUD_Sound_square(float frequency, aud::SampleRate rate);

/**
 * Creates a triangle sound.
 * \param frequency The frequency of the generated triangle sound.
 * \param rate The sample rate of the triangle sound.
 * \return A handle of the sound.
 */
extern AUD_Sound* AUD_Sound_triangle(float frequency, aud::SampleRate rate);

/**
 * Accumulates a sound by summing over positive input differences thus generating a monotonic sigal.
 * If additivity is set to true negative input differences get added too, but positive ones with a factor of two.
 * Note that with additivity the signal is not monotonic anymore.
 * \param sound The sound to accumulate.
 * \param additive Whether the accumulation should be additive or not.
 * \return A handle of the accumulated sound.
 */
extern AUD_Sound* AUD_Sound_accumulate(AUD_Sound* sound, int additive);

/**
 * Attack-Decay-Sustain-Release envelopes the volume of a sound.
 * Note: there is currently no way to trigger the release with this API.
 * \param sound The sound to filter.
 * \param attack The attack time in seconds.
 * \param decay The decay time in seconds.
 * \param sustain The sustain level.
 * \param release The release time in seconds.
 * \return A handle of the filtered sound.
 */
extern AUD_Sound* AUD_Sound_ADSR(AUD_Sound* sound, float attack, float decay, float sustain, float release);

/**
 * Delays a sound.
 * \param sound The sound to dealy.
 * \param delay The delay in seconds.
 * \return A handle of the delayed sound.
 */
extern AUD_Sound* AUD_Sound_delay(AUD_Sound* sound, float delay);

/**
 * Envelopes a sound.
 * \param sound The sound to envelope.
 * \param attack The attack time in seconds.
 * \param release The release time in seconds.
 * \param threshold The general threshold value.
 * \param arthreshold The attack/release threshold value.
 * \return A handle of the enveloped sound.
 */
extern AUD_Sound* AUD_Sound_envelope(AUD_Sound* sound, float attack, float release, float threshold, float arthreshold);

/**
 * Fade in a sound.
 * \param sound The sound to be fade in.
 * \param start The time when the fading should start in seconds.
 * \param length The duration of the fade in seconds.
 * \return A handle of the faded sound.
 */
extern AUD_Sound* AUD_Sound_fadein(AUD_Sound* sound, float start, float length);

/**
 * Fade out a sound.
 * \param sound The sound to be fade out.
 * \param start The time when the fading should start in seconds.
 * \param length The duration of the fade in seconds.
 * \return A handle of the faded sound.
 */
extern AUD_Sound* AUD_Sound_fadeout(AUD_Sound* sound, float start, float length);

/**
 * Filter a sound.
 * \param sound The sound to be filtered.
 * \param b The nominator filter coefficients, may be NULL.
 * \param b_length The length of the b array.
 * \param a The denominator filter coefficients, may be NULL.
 * \param a_length The length of the a array.
 * \return A handle of the filtered sound.
 */
extern AUD_Sound* AUD_Sound_filter(AUD_Sound* sound, float* b, int b_length, float* a, int a_length);

/**
 * Highpass filters a sound.
 * \param sound The sound to filter.
 * \param frequency The filter cut-off frequency.
 * \param Q The filter quality. If usunsure which value to use, pass 1.0f.
 * \return A handle of the filtered sound.
 */
extern AUD_Sound* AUD_Sound_highpass(AUD_Sound* sound, float frequency, float Q);

/**
 * Limits a sound.
 * \param sound The sound to limit.
 * \param start The start time in seconds.
 * \param end The stop time in seconds.
 * \return A handle of the limited sound.
 */
extern AUD_Sound* AUD_Sound_limit(AUD_Sound* sound, float start, float end);

/**
 * Loops a sound.
 * \param sound The sound to loop.
 * \return A handle of the looped sound.
 */
extern AUD_Sound* AUD_Sound_loop(AUD_Sound* sound);

/**
 * Lowpass filters a sound.
 * \param sound The sound to filter.
 * \param frequency The filter cut-off frequency.
 * \param Q The filter quality. If usunsure which value to use, pass 1.0f.
 * \return A handle of the filtered sound.
 */
extern AUD_Sound* AUD_Sound_lowpass(AUD_Sound* sound, float frequency, float Q);

/**
 * Changes the pitch of a sound.
 * \param sound The sound to change.
 * \param factor The factor to change the pitch with.
 * \return A handle of the pitched sound.
 */
extern AUD_Sound* AUD_Sound_pitch(AUD_Sound* sound, float factor);

/**
 * Rechannels the sound.
 * \param sound The sound to rechannel.
 * \param channels The new channel configuration.
 * \return The rechanneled sound.
 */
extern AUD_Sound* AUD_Sound_rechannel(AUD_Sound* sound, AUD_Channels channels);

/**
 * Reverses a sound. Make sure the sound source can be reversed.
 * \param sound The sound to reverse.
 * \return A handle of the reversed sound.
 */
extern AUD_Sound* AUD_Sound_reverse(AUD_Sound* sound);

/**
 * Sums the samples of a sound.
 * \param sound The sound to sum.
 * \return A handle of the summed sound.
 */
extern AUD_Sound* AUD_Sound_sum(AUD_Sound* sound);

/**
 * Turns a sound into a square wave by thresholding.
 * \param sound The sound to threshold.
 * \return A handle of the thresholded sound.
 */
extern AUD_Sound* AUD_Sound_threshold(AUD_Sound* sound);

/**
 * Changes the volume of a sound.
 * \param sound The sound to change.
 * \param volume The new volume of the sound. Should be in the range 0 to 1. Use higher values with caution.
 * \return A handle of the amplified sound.
 */
extern AUD_Sound* AUD_Sound_volume(AUD_Sound* sound, float volume);

/**
 * Joins two sound, which means playing them one after the other.
 * \param first The first sound.
 * \param second The second sound.
 * \return A handle of the joined sound.
 */
extern AUD_Sound* AUD_Sound_join(AUD_Sound* first, AUD_Sound* second);

/**
 * Mixes two sound, which means superposing the sound samples.
 * \param first The first sound.
 * \param second The second sound.
 * \return A handle of the mixed sound.
 */
extern AUD_Sound* AUD_Sound_mix(AUD_Sound* first, AUD_Sound* second);

/**
 * Ping pongs a sound.
 * \param sound The sound to ping pong.
 * \return A handle of the ping pong sound.
 */
extern AUD_Sound* AUD_Sound_pingpong(AUD_Sound* sound);

/**
 * Unloads a sound of any type.
 * \param sound The handle of the sound.
 */
extern void AUD_Sound_free(AUD_Sound* sound);

/**
 * Copies a sound.
 * \param sound Sound to copy.
 * \return Copied sound.
 */
extern AUD_Sound* AUD_Sound_copy(AUD_Sound* sound);

#ifdef __cplusplus
}
#endif
