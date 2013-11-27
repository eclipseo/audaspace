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

#include "devices/OpenALDevice.h"
#include "ISound.h"
#include "IReader.h"
#include "respec/ConverterReader.h"

#include <cstring>
#include <limits>
#include <mutex>

#ifdef WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

AUD_NAMESPACE_BEGIN

/*struct OpenALBuffered
{
	/// The factory.
	ISound* factory;

	/// The OpenAL buffer.
	ALuint buffer;
};*/


/******************************************************************************/
/*********************** OpenALHandle Handle Code *************************/
/******************************************************************************/

static const char* genbuffer_error = "OpenALDevice: Buffer couldn't be "
									 "generated.";
static const char* gensource_error = "OpenALDevice: Source couldn't be "
									 "generated.";
static const char* queue_error = "OpenALDevice: Buffer couldn't be "
								 "queued to the source.";
static const char* bufferdata_error = "OpenALDevice: Buffer couldn't be "
									  "filled with data.";

bool OpenALDevice::OpenALHandle::pause(bool keep)
{
	if(m_status)
	{
		std::lock_guard<ILockable> lock(*m_device);

		if(m_status == STATUS_PLAYING)
		{
			for(auto it = m_device->m_playingSounds.begin(); it != m_device->m_playingSounds.end(); it++)
			{
				if(it->get() == this)
				{
					std::shared_ptr<OpenALHandle> This = *it;

					m_device->m_playingSounds.erase(it);
					m_device->m_pausedSounds.push_back(This);

					alSourcePause(m_source);

					m_status = keep ? STATUS_STOPPED : STATUS_PAUSED;

					return true;
				}
			}
		}
	}

	return false;}

OpenALDevice::OpenALHandle::OpenALHandle(OpenALDevice* device, ALenum format, std::shared_ptr<IReader> reader, bool keep) :
	m_isBuffered(false), m_reader(reader), m_keep(keep), m_format(format), m_current(0),
	m_eos(false), m_loopcount(0), m_stop(NULL), m_stop_data(NULL), m_status(STATUS_PLAYING),
	m_device(device)
{
	DeviceSpecs specs = m_device->m_specs;
	specs.specs = m_reader->getSpecs();

	// OpenAL playback code
	alGenBuffers(CYCLE_BUFFERS, m_buffers);
	if(alGetError() != AL_NO_ERROR)
		AUD_THROW(ERROR_OPENAL, genbuffer_error);

	try
	{
		m_device->m_buffer.assureSize(m_device->m_buffersize * AUD_DEVICE_SAMPLE_SIZE(specs));
		int length;
		bool eos;

		for(int i = 0; i < CYCLE_BUFFERS; i++)
		{
			length = m_device->m_buffersize;
			reader->read(length, eos, m_device->m_buffer.getBuffer());

			if(length == 0)
			{
				// AUD_XXX: TODO: don't fill all buffers and enqueue them later
				length = 1;
				memset(m_device->m_buffer.getBuffer(), 0, length * AUD_DEVICE_SAMPLE_SIZE(specs));
			}

			alBufferData(m_buffers[i], m_format, m_device->m_buffer.getBuffer(),
						 length * AUD_DEVICE_SAMPLE_SIZE(specs),
						 specs.rate);
			if(alGetError() != AL_NO_ERROR)
				AUD_THROW(ERROR_OPENAL, bufferdata_error);
		}

		alGenSources(1, &m_source);
		if(alGetError() != AL_NO_ERROR)
			AUD_THROW(ERROR_OPENAL, gensource_error);

		try
		{
			alSourceQueueBuffers(m_source, CYCLE_BUFFERS, m_buffers);
			if(alGetError() != AL_NO_ERROR)
				AUD_THROW(ERROR_OPENAL, queue_error);
		}
		catch(Exception&)
		{
			alDeleteSources(1, &m_source);
			throw;
		}
	}
	catch(Exception&)
	{
		alDeleteBuffers(CYCLE_BUFFERS, m_buffers);
		throw;
	}
	alSourcei(m_source, AL_SOURCE_RELATIVE, 1);
}

bool OpenALDevice::OpenALHandle::pause()
{
	return pause(false);
}

bool OpenALDevice::OpenALHandle::resume()
{
	if(m_status)
	{
		std::lock_guard<ILockable> lock(*m_device);

		if(m_status == STATUS_PAUSED)
		{
			for(auto it = m_device->m_pausedSounds.begin(); it != m_device->m_pausedSounds.end(); it++)
			{
				if(it->get() == this)
				{
					std::shared_ptr<OpenALHandle> This = *it;

					m_device->m_pausedSounds.erase(it);
					m_device->m_playingSounds.push_back(This);

					m_device->start();
					m_status = STATUS_PLAYING;

					return true;
				}
			}
		}
	}

	return false;
}

bool OpenALDevice::OpenALHandle::stop()
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_status = STATUS_INVALID;

	alDeleteSources(1, &m_source);
	if(!m_isBuffered)
		alDeleteBuffers(CYCLE_BUFFERS, m_buffers);

	for(auto it = m_device->m_playingSounds.begin(); it != m_device->m_playingSounds.end(); it++)
	{
		if(it->get() == this)
		{
			std::shared_ptr<OpenALHandle> This = *it;

			m_device->m_playingSounds.erase(it);

			return true;
		}
	}

	for(auto it = m_device->m_pausedSounds.begin(); it != m_device->m_pausedSounds.end(); it++)
	{
		if(it->get() == this)
		{
			m_device->m_pausedSounds.erase(it);
			return true;
		}
	}

	return false;
}

bool OpenALDevice::OpenALHandle::getKeep()
{
	if(m_status)
		return m_keep;

	return false;
}

bool OpenALDevice::OpenALHandle::setKeep(bool keep)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_keep = keep;

	return true;
}

bool OpenALDevice::OpenALHandle::seek(float position)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	if(m_isBuffered)
		alSourcef(m_source, AL_SEC_OFFSET, position);
	else
	{
		m_reader->seek((int)(position * m_reader->getSpecs().rate));
		m_eos = false;

		ALint info;

		alGetSourcei(m_source, AL_SOURCE_STATE, &info);

		if(info != AL_PLAYING)
		{
			if(info == AL_PAUSED)
				alSourceStop(m_source);

			alSourcei(m_source, AL_BUFFER, 0);
			m_current = 0;

			ALenum err;
			if((err = alGetError()) == AL_NO_ERROR)
			{
				int length;
				DeviceSpecs specs = m_device->m_specs;
				specs.specs = m_reader->getSpecs();
				m_device->m_buffer.assureSize(m_device->m_buffersize * AUD_DEVICE_SAMPLE_SIZE(specs));

				for(int i = 0; i < CYCLE_BUFFERS; i++)
				{
					length = m_device->m_buffersize;
					m_reader->read(length, m_eos, m_device->m_buffer.getBuffer());

					if(length == 0)
					{
						// AUD_XXX: TODO: don't fill all buffers and enqueue them later
						length = 1;
						memset(m_device->m_buffer.getBuffer(), 0, length * AUD_DEVICE_SAMPLE_SIZE(specs));
					}

					alBufferData(m_buffers[i], m_format, m_device->m_buffer.getBuffer(),
								 length * AUD_DEVICE_SAMPLE_SIZE(specs), specs.rate);

					if(alGetError() != AL_NO_ERROR)
						break;
				}

				if(m_loopcount != 0)
					m_eos = false;

				alSourceQueueBuffers(m_source, CYCLE_BUFFERS, m_buffers);
			}

			alSourceRewind(m_source);
		}
	}

	if(m_status == STATUS_STOPPED)
		m_status = STATUS_PAUSED;

	return true;
}

float OpenALDevice::OpenALHandle::getPosition()
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return 0.0f;

	float position = 0.0f;

	alGetSourcef(m_source, AL_SEC_OFFSET, &position);

	if(!m_isBuffered)
	{
		Specs specs = m_reader->getSpecs();
		position += (m_reader->getPosition() - m_device->m_buffersize *
					 CYCLE_BUFFERS) / (float)specs.rate;
	}

	return position;
}

Status OpenALDevice::OpenALHandle::getStatus()
{
	return m_status;
}

float OpenALDevice::OpenALHandle::getVolume()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_GAIN, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setVolume(float volume)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_GAIN, volume);

	return true;
}

float OpenALDevice::OpenALHandle::getPitch()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_PITCH, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setPitch(float pitch)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_PITCH, pitch);

	return true;
}

int OpenALDevice::OpenALHandle::getLoopCount()
{
	if(!m_status)
		return 0;
	return m_loopcount;
}

bool OpenALDevice::OpenALHandle::setLoopCount(int count)
{
	if(!m_status)
		return false;

	if(m_status == STATUS_STOPPED && (count > m_loopcount || count < 0))
		m_status = STATUS_PAUSED;

	m_loopcount = count;

	return true;
}

bool OpenALDevice::OpenALHandle::setStopCallback(stopCallback callback, void* data)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	m_stop = callback;
	m_stop_data = data;

	return true;
}

/******************************************************************************/
/********************* OpenALHandle 3DHandle Code *************************/
/******************************************************************************/

Vector3 OpenALDevice::OpenALHandle::getSourceLocation()
{
	Vector3 result = Vector3(0, 0, 0);

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	ALfloat p[3];
	alGetSourcefv(m_source, AL_POSITION, p);

	result = Vector3(p[0], p[1], p[2]);

	return result;
}

bool OpenALDevice::OpenALHandle::setSourceLocation(const Vector3& location)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcefv(m_source, AL_POSITION, (ALfloat*)location.get());

	return true;
}

Vector3 OpenALDevice::OpenALHandle::getSourceVelocity()
{
	Vector3 result = Vector3(0, 0, 0);

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	ALfloat v[3];
	alGetSourcefv(m_source, AL_VELOCITY, v);

	result = Vector3(v[0], v[1], v[2]);

	return result;
}

bool OpenALDevice::OpenALHandle::setSourceVelocity(const Vector3& velocity)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcefv(m_source, AL_VELOCITY, (ALfloat*)velocity.get());

	return true;
}

Quaternion OpenALDevice::OpenALHandle::getSourceOrientation()
{
	return m_orientation;
}

bool OpenALDevice::OpenALHandle::setSourceOrientation(const Quaternion& orientation)
{
	ALfloat direction[3];
	direction[0] = -2 * (orientation.w() * orientation.y() +
						 orientation.x() * orientation.z());
	direction[1] = 2 * (orientation.x() * orientation.w() -
						orientation.z() * orientation.y());
	direction[2] = 2 * (orientation.x() * orientation.x() +
						orientation.y() * orientation.y()) - 1;

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcefv(m_source, AL_DIRECTION, direction);

	m_orientation = orientation;

	return true;
}

bool OpenALDevice::OpenALHandle::isRelative()
{
	int result;

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alGetSourcei(m_source, AL_SOURCE_RELATIVE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setRelative(bool relative)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcei(m_source, AL_SOURCE_RELATIVE, relative);

	return true;
}

float OpenALDevice::OpenALHandle::getVolumeMaximum()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_MAX_GAIN, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setVolumeMaximum(float volume)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_MAX_GAIN, volume);

	return true;
}

float OpenALDevice::OpenALHandle::getVolumeMinimum()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_MIN_GAIN, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setVolumeMinimum(float volume)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_MIN_GAIN, volume);

	return true;
}

float OpenALDevice::OpenALHandle::getDistanceMaximum()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_MAX_DISTANCE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setDistanceMaximum(float distance)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_MAX_DISTANCE, distance);

	return true;
}

float OpenALDevice::OpenALHandle::getDistanceReference()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_REFERENCE_DISTANCE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setDistanceReference(float distance)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_REFERENCE_DISTANCE, distance);

	return true;
}

float OpenALDevice::OpenALHandle::getAttenuation()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_ROLLOFF_FACTOR, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setAttenuation(float factor)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_ROLLOFF_FACTOR, factor);

	return true;
}

float OpenALDevice::OpenALHandle::getConeAngleOuter()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_CONE_OUTER_ANGLE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setConeAngleOuter(float angle)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_CONE_OUTER_ANGLE, angle);

	return true;
}

float OpenALDevice::OpenALHandle::getConeAngleInner()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_CONE_INNER_ANGLE, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setConeAngleInner(float angle)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_CONE_INNER_ANGLE, angle);

	return true;
}

float OpenALDevice::OpenALHandle::getConeVolumeOuter()
{
	float result = std::numeric_limits<float>::quiet_NaN();

	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return result;

	alGetSourcef(m_source, AL_CONE_OUTER_GAIN, &result);

	return result;
}

bool OpenALDevice::OpenALHandle::setConeVolumeOuter(float volume)
{
	if(!m_status)
		return false;

	std::lock_guard<ILockable> lock(*m_device);

	if(!m_status)
		return false;

	alSourcef(m_source, AL_CONE_OUTER_GAIN, volume);

	return true;
}

/******************************************************************************/
/**************************** Threading Code **********************************/
/******************************************************************************/

static void *openalRunThread(void *device)
{
	OpenALDevice* dev = (OpenALDevice*)device;
	dev->updateStreams();
	return NULL;
}

void OpenALDevice::start(bool join)
{
	std::lock_guard<ILockable> lock(*this);

	if(!m_playing)
	{
		if(join)
			pthread_join(m_thread, NULL);

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

		pthread_create(&m_thread, &attr, openalRunThread, this);

		pthread_attr_destroy(&attr);

		m_playing = true;
	}
}

void OpenALDevice::updateStreams()
{
	std::shared_ptr<OpenALHandle> sound;

	int length;

	ALint info;
	DeviceSpecs specs = m_specs;
	ALCenum cerr;
	std::list<std::shared_ptr<OpenALHandle> > stopSounds;
	std::list<std::shared_ptr<OpenALHandle> > pauseSounds;
	std::list<std::shared_ptr<OpenALHandle> >::iterator it;

	while(1)
	{
		lock();

		alcSuspendContext(m_context);
		cerr = alcGetError(m_device);
		if(cerr == ALC_NO_ERROR)
		{
			// for all sounds
			for(it = m_playingSounds.begin(); it != m_playingSounds.end(); it++)
			{
				sound = *it;

				// is it a streamed sound?
				if(!sound->m_isBuffered)
				{
					// check for buffer refilling
					alGetSourcei(sound->m_source, AL_BUFFERS_PROCESSED, &info);

					if(info)
					{
						specs.specs = sound->m_reader->getSpecs();
						m_buffer.assureSize(m_buffersize * AUD_DEVICE_SAMPLE_SIZE(specs));

						// for all empty buffers
						while(info--)
						{
							// if there's still data to play back
							if(!sound->m_eos)
							{
								// read data
								length = m_buffersize;
								sound->m_reader->read(length, sound->m_eos, m_buffer.getBuffer());

								// looping necessary?
								if(length == 0 && sound->m_loopcount)
								{
									if(sound->m_loopcount > 0)
										sound->m_loopcount--;

									sound->m_reader->seek(0);

									length = m_buffersize;
									sound->m_reader->read(length, sound->m_eos, m_buffer.getBuffer());
								}

								if(sound->m_loopcount != 0)
									sound->m_eos = false;

								// read nothing?
								if(length == 0)
								{
									break;
								}

								// unqueue buffer (warning: this might fail for slow early returning sources (none exist so far) if the buffer was not queued due to recent changes - has to be tested)
								alSourceUnqueueBuffers(sound->m_source, 1, &sound->m_buffers[sound->m_current]);
								ALenum err;
								if((err = alGetError()) != AL_NO_ERROR)
								{
									sound->m_eos = true;
									break;
								}

								// fill with new data
								alBufferData(sound->m_buffers[sound->m_current],
											 sound->m_format,
											 m_buffer.getBuffer(), length *
											 AUD_DEVICE_SAMPLE_SIZE(specs),
											 specs.rate);

								if((err = alGetError()) != AL_NO_ERROR)
								{
									sound->m_eos = true;
									break;
								}

								// and queue again
								alSourceQueueBuffers(sound->m_source, 1,
												&sound->m_buffers[sound->m_current]);
								if(alGetError() != AL_NO_ERROR)
								{
									sound->m_eos = true;
									break;
								}

								sound->m_current = (sound->m_current+1) %
												 OpenALHandle::CYCLE_BUFFERS;
							}
							else
								break;
						}
					}
				}

				// check if the sound has been stopped
				alGetSourcei(sound->m_source, AL_SOURCE_STATE, &info);

				if(info != AL_PLAYING)
				{
					// if it really stopped
					if(sound->m_eos)
					{
						if(sound->m_stop)
							sound->m_stop(sound->m_stop_data);

						// pause or
						if(sound->m_keep)
							pauseSounds.push_back(sound);
						// stop
						else
							stopSounds.push_back(sound);
					}
					// continue playing
					else
						alSourcePlay(sound->m_source);
				}
			}

			for(it = pauseSounds.begin(); it != pauseSounds.end(); it++)
				(*it)->pause(true);

			for(it = stopSounds.begin(); it != stopSounds.end(); it++)
				(*it)->stop();

			pauseSounds.clear();
			stopSounds.clear();

			alcProcessContext(m_context);
		}

		// stop thread
		if(m_playingSounds.empty() || (cerr != ALC_NO_ERROR))
		{
			m_playing = false;
			unlock();
			pthread_exit(NULL);
		}

		unlock();

#ifdef WIN32
		Sleep(20);
#else
		usleep(20000);
#endif
	}
}

/******************************************************************************/
/**************************** IDevice Code ************************************/
/******************************************************************************/

static const char* open_error = "OpenALDevice: Device couldn't be opened.";

OpenALDevice::OpenALDevice(DeviceSpecs specs, int buffersize)
{
	// cannot determine how many channels or which format OpenAL uses, but
	// it at least is able to play 16 bit stereo audio
	specs.format = FORMAT_S16;

#if 0
	if(alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT") == AL_TRUE)
	{
		ALCchar* devices = const_cast<ALCchar*>(alcGetString(NULL, ALC_DEVICE_SPECIFIER));
		printf("OpenAL devices (standard is: %s):\n", alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));

		while(*devices)
		{
			printf("%s\n", devices);
			devices += strlen(devices) + 1;
		}
	}
#endif

	m_device = alcOpenDevice(NULL);

	if(!m_device)
		AUD_THROW(ERROR_OPENAL, open_error);

	// at least try to set the frequency
	ALCint attribs[] = { ALC_FREQUENCY, (ALCint)specs.rate, 0 };
	ALCint* attributes = attribs;
	if(specs.rate == RATE_INVALID)
		attributes = NULL;

	m_context = alcCreateContext(m_device, attributes);
	alcMakeContextCurrent(m_context);

	alcGetIntegerv(m_device, ALC_FREQUENCY, 1, (ALCint*)&specs.rate);

	// check for specific formats and channel counts to be played back
	if(alIsExtensionPresent("AL_EXT_FLOAT32") == AL_TRUE)
		specs.format = FORMAT_FLOAT32;

	m_useMC = alIsExtensionPresent("AL_EXT_MCFORMATS") == AL_TRUE;

	if((!m_useMC && specs.channels > CHANNELS_STEREO) ||
			specs.channels == CHANNELS_STEREO_LFE ||
			specs.channels == CHANNELS_SURROUND5)
		specs.channels = CHANNELS_STEREO;

	alGetError();
	alcGetError(m_device);

	m_specs = specs;
	m_buffersize = buffersize;
	m_playing = false;

//	m_bufferedFactories = new std::list<OpenALBuffered*>();

	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_mutex, &attr);

	pthread_mutexattr_destroy(&attr);

	start(false);
}

OpenALDevice::~OpenALDevice()
{
	lock();
	alcSuspendContext(m_context);

	while(!m_playingSounds.empty())
		m_playingSounds.front()->stop();

	while(!m_pausedSounds.empty())
		m_pausedSounds.front()->stop();


	// delete all buffered factories
	/*while(!m_bufferedFactories->empty())
	{
		alDeleteBuffers(1, &(*(m_bufferedFactories->begin()))->buffer);
		delete *m_bufferedFactories->begin();
		m_bufferedFactories->erase(m_bufferedFactories->begin());
	}*/

	alcProcessContext(m_context);

	// wait for the thread to stop
	unlock();
	pthread_join(m_thread, NULL);

	//delete m_bufferedFactories;

	// quit OpenAL
	alcMakeContextCurrent(NULL);
	alcDestroyContext(m_context);
	alcCloseDevice(m_device);

	pthread_mutex_destroy(&m_mutex);
}

DeviceSpecs OpenALDevice::getSpecs() const
{
	return m_specs;
}

bool OpenALDevice::getFormat(ALenum &format, Specs specs)
{
	bool valid = true;
	format = 0;

	switch(m_specs.format)
	{
	case FORMAT_S16:
		switch(specs.channels)
		{
		case CHANNELS_MONO:
			format = AL_FORMAT_MONO16;
			break;
		case CHANNELS_STEREO:
			format = AL_FORMAT_STEREO16;
			break;
		case CHANNELS_SURROUND4:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_QUAD16");
				break;
			}
		case CHANNELS_SURROUND51:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_51CHN16");
				break;
			}
		case CHANNELS_SURROUND61:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_61CHN16");
				break;
			}
		case CHANNELS_SURROUND71:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_71CHN16");
				break;
			}
		default:
			valid = false;
		}
		break;
	case FORMAT_FLOAT32:
		switch(specs.channels)
		{
		case CHANNELS_MONO:
			format = alGetEnumValue("AL_FORMAT_MONO_FLOAT32");
			break;
		case CHANNELS_STEREO:
			format = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
			break;
		case CHANNELS_SURROUND4:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_QUAD32");
				break;
			}
		case CHANNELS_SURROUND51:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_51CHN32");
				break;
			}
		case CHANNELS_SURROUND61:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_61CHN32");
				break;
			}
		case CHANNELS_SURROUND71:
			if(m_useMC)
			{
				format = alGetEnumValue("AL_FORMAT_71CHN32");
				break;
			}
		default:
			valid = false;
		}
		break;
	default:
		valid = false;
	}

	if(!format)
		valid = false;

	return valid;
}

std::shared_ptr<IHandle> OpenALDevice::play(std::shared_ptr<IReader> reader, bool keep)
{
	Specs specs = reader->getSpecs();

	// check format
	if(specs.channels == CHANNELS_INVALID)
		return std::shared_ptr<IHandle>();

	if(m_specs.format != FORMAT_FLOAT32)
		reader = std::shared_ptr<IReader>(new ConverterReader(reader, m_specs));

	ALenum format;

	if(!getFormat(format, specs))
		return std::shared_ptr<IHandle>();

	std::lock_guard<ILockable> lock(*this);

	alcSuspendContext(m_context);

	std::shared_ptr<OpenALDevice::OpenALHandle> sound;

	try
	{
		// create the handle
		sound = std::shared_ptr<OpenALDevice::OpenALHandle>(new OpenALDevice::OpenALHandle(this, format, reader, keep));
	}
	catch(Exception&)
	{
		alcProcessContext(m_context);
		throw;
	}

	alcProcessContext(m_context);

	// play sound
	m_playingSounds.push_back(sound);

	start();

	return std::shared_ptr<IHandle>(sound);
}

std::shared_ptr<IHandle> OpenALDevice::play(std::shared_ptr<ISound> factory, bool keep)
{
	/* AUD_XXX disabled
	OpenALHandle* sound = NULL;

	lock();

	try
	{
		// check if it is a buffered factory
		for(auto i = m_bufferedFactories->begin();
			i != m_bufferedFactories->end(); i++)
		{
			if((*i)->factory == factory)
			{
				// create the handle
				sound = new OpenALHandle;
				sound->keep = keep;
				sound->current = -1;
				sound->isBuffered = true;
				sound->eos = true;
				sound->loopcount = 0;
				sound->stop = NULL;
				sound->stop_data = NULL;

				alcSuspendContext(m_context);

				// OpenAL playback code
				try
				{
					alGenSources(1, &sound->source);
					if(alGetError() != AL_NO_ERROR)
						AUD_THROW(ERROR_OPENAL, gensource_error);

					try
					{
						alSourcei(sound->source, AL_BUFFER, (*i)->buffer);
						if(alGetError() != AL_NO_ERROR)
							AUD_THROW(ERROR_OPENAL, queue_error);
					}
					catch(Exception&)
					{
						alDeleteSources(1, &sound->source);
						throw;
					}
				}
				catch(Exception&)
				{
					delete sound;
					alcProcessContext(m_context);
					throw;
				}

				// play sound
				m_playingSounds->push_back(sound);

				alSourcei(sound->source, AL_SOURCE_RELATIVE, 1);
				start();

				alcProcessContext(m_context);
			}
		}
	}
	catch(Exception&)
	{
		unlock();
		throw;
	}

	unlock();

	if(sound)
		return sound;*/

	return play(factory->createReader(), keep);
}

void OpenALDevice::stopAll()
{
	std::lock_guard<ILockable> lock(*this);

	alcSuspendContext(m_context);

	while(!m_playingSounds.empty())
		m_playingSounds.front()->stop();

	while(!m_pausedSounds.empty())
		m_pausedSounds.front()->stop();

	alcProcessContext(m_context);
}

void OpenALDevice::lock()
{
	pthread_mutex_lock(&m_mutex);
}

void OpenALDevice::unlock()
{
	pthread_mutex_unlock(&m_mutex);
}

float OpenALDevice::getVolume() const
{
	float result;
	alGetListenerf(AL_GAIN, &result);
	return result;
}

void OpenALDevice::setVolume(float volume)
{
	alListenerf(AL_GAIN, volume);
}

/* AUD_XXX Temorary disabled

bool OpenALDevice::bufferFactory(void *value)
{
	bool result = false;
	ISound* factory = (ISound*) value;

	// load the factory into an OpenAL buffer
	if(factory)
	{
		// check if the factory is already buffered
		lock();
		for(auto i = m_bufferedFactories->begin();
			i != m_bufferedFactories->end(); i++)
		{
			if((*i)->factory == factory)
			{
				result = true;
				break;
			}
		}
		unlock();
		if(result)
			return result;

		IReader* reader = factory->createReader();

		if(reader == NULL)
			return false;

		DeviceSpecs specs = m_specs;
		specs.specs = reader->getSpecs();

		if(m_specs.format != FORMAT_FLOAT32)
			reader = new ConverterReader(reader, m_specs);

		ALenum format;

		if(!getFormat(format, specs.specs))
		{
			return false;
		}

		// load into a buffer
		lock();
		alcSuspendContext(m_context);

		OpenALBuffered* bf = new OpenALBuffered;
		bf->factory = factory;

		try
		{
			alGenBuffers(1, &bf->buffer);
			if(alGetError() != AL_NO_ERROR)
				AUD_THROW(ERROR_OPENAL);

			try
			{
				sample_t* buf;
				int length = reader->getLength();

				reader->read(length, buf);
				alBufferData(bf->buffer, format, buf,
							 length * AUD_DEVICE_SAMPLE_SIZE(specs),
							 specs.rate);
				if(alGetError() != AL_NO_ERROR)
					AUD_THROW(ERROR_OPENAL);
			}
			catch(Exception&)
			{
				alDeleteBuffers(1, &bf->buffer);
				throw;
			}
		}
		catch(Exception&)
		{
			delete bf;
			alcProcessContext(m_context);
			unlock();
			return false;
		}

		m_bufferedFactories->push_back(bf);

		alcProcessContext(m_context);
		unlock();
	}
	else
	{
		// stop all playing and paused buffered sources
		lock();
		alcSuspendContext(m_context);

		OpenALHandle* sound;
		auto it = m_playingSounds->begin();
		while(it != m_playingSounds->end())
		{
			sound = *it;
			++it;

			if(sound->isBuffered)
				stop(sound);
		}
		alcProcessContext(m_context);

		while(!m_bufferedFactories->empty())
		{
			alDeleteBuffers(1,
							&(*(m_bufferedFactories->begin()))->buffer);
			delete *m_bufferedFactories->begin();
			m_bufferedFactories->erase(m_bufferedFactories->begin());
		}
		unlock();
	}

	return true;
}*/

/******************************************************************************/
/**************************** 3D Device Code **********************************/
/******************************************************************************/

Vector3 OpenALDevice::getListenerLocation() const
{
	ALfloat p[3];
	alGetListenerfv(AL_POSITION, p);
	return Vector3(p[0], p[1], p[2]);
}

void OpenALDevice::setListenerLocation(const Vector3& location)
{
	alListenerfv(AL_POSITION, (ALfloat*)location.get());
}

Vector3 OpenALDevice::getListenerVelocity() const
{
	ALfloat v[3];
	alGetListenerfv(AL_VELOCITY, v);
	return Vector3(v[0], v[1], v[2]);
}

void OpenALDevice::setListenerVelocity(const Vector3& velocity)
{
	alListenerfv(AL_VELOCITY, (ALfloat*)velocity.get());
}

Quaternion OpenALDevice::getListenerOrientation() const
{
	return m_orientation;
}

void OpenALDevice::setListenerOrientation(const Quaternion& orientation)
{
	ALfloat direction[6];
	direction[0] = -2 * (orientation.w() * orientation.y() +
						 orientation.x() * orientation.z());
	direction[1] = 2 * (orientation.x() * orientation.w() -
						orientation.z() * orientation.y());
	direction[2] = 2 * (orientation.x() * orientation.x() +
						orientation.y() * orientation.y()) - 1;
	direction[3] = 2 * (orientation.x() * orientation.y() -
						orientation.w() * orientation.z());
	direction[4] = 1 - 2 * (orientation.x() * orientation.x() +
							orientation.z() * orientation.z());
	direction[5] = 2 * (orientation.w() * orientation.x() +
						orientation.y() * orientation.z());
	alListenerfv(AL_ORIENTATION, direction);
	m_orientation = orientation;
}

float OpenALDevice::getSpeedOfSound() const
{
	return alGetFloat(AL_SPEED_OF_SOUND);
}

void OpenALDevice::setSpeedOfSound(float speed)
{
	alSpeedOfSound(speed);
}

float OpenALDevice::getDopplerFactor() const
{
	return alGetFloat(AL_DOPPLER_FACTOR);
}

void OpenALDevice::setDopplerFactor(float factor)
{
	alDopplerFactor(factor);
}

DistanceModel OpenALDevice::getDistanceModel() const
{
	switch(alGetInteger(AL_DISTANCE_MODEL))
	{
	case AL_INVERSE_DISTANCE:
		return DISTANCE_MODEL_INVERSE;
	case AL_INVERSE_DISTANCE_CLAMPED:
		return DISTANCE_MODEL_INVERSE_CLAMPED;
	case AL_LINEAR_DISTANCE:
		return DISTANCE_MODEL_LINEAR;
	case AL_LINEAR_DISTANCE_CLAMPED:
		return DISTANCE_MODEL_LINEAR_CLAMPED;
	case AL_EXPONENT_DISTANCE:
		return DISTANCE_MODEL_EXPONENT;
	case AL_EXPONENT_DISTANCE_CLAMPED:
		return DISTANCE_MODEL_EXPONENT_CLAMPED;
	default:
		return DISTANCE_MODEL_INVALID;
	}
}

void OpenALDevice::setDistanceModel(DistanceModel model)
{
	switch(model)
	{
	case DISTANCE_MODEL_INVERSE:
		alDistanceModel(AL_INVERSE_DISTANCE);
		break;
	case DISTANCE_MODEL_INVERSE_CLAMPED:
		alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
		break;
	case DISTANCE_MODEL_LINEAR:
		alDistanceModel(AL_LINEAR_DISTANCE);
		break;
	case DISTANCE_MODEL_LINEAR_CLAMPED:
		alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
		break;
	case DISTANCE_MODEL_EXPONENT:
		alDistanceModel(AL_EXPONENT_DISTANCE);
		break;
	case DISTANCE_MODEL_EXPONENT_CLAMPED:
		alDistanceModel(AL_EXPONENT_DISTANCE_CLAMPED);
		break;
	default:
		alDistanceModel(AL_NONE);
	}
}

AUD_NAMESPACE_END
