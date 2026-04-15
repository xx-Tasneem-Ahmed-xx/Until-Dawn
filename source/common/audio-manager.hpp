#pragma once

#include <AL/al.h>
#include <AL/alc.h>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <cstdint>

namespace our
{
    // Simple WAV file loader for OpenAL
    class AudioManager
    {
    private:
        ALCdevice *device = nullptr;
        ALCcontext *context = nullptr;
        std::map<std::string, ALuint> buffers;
        std::map<std::string, ALuint> sources;

        // Helper method to ensure context is current before any AL operation
        void ensureContextCurrent() const
        {
            if (context)
            {
                ALCcontext *current = alcGetCurrentContext();
                if (current != context)
                {
                    alcMakeContextCurrent(context);
                }
            }
        }

        // Private constructor for Singleton pattern
        AudioManager()
        {
            // Try to get available devices
            const ALCchar *devices = alcGetString(nullptr, ALC_DEVICE_SPECIFIER);
            if (devices)
            {
                std::cout << "[AudioManager] Available devices:" << std::endl;
                const ALCchar *device_ptr = devices;
                while (device_ptr && *device_ptr)
                {
                    std::cout << "  - " << device_ptr << std::endl;
                    device_ptr += std::strlen(device_ptr) + 1;
                }
            }

            // Initialize OpenAL - try multiple device strategies
            const char *deviceNames[] = {nullptr, "OpenAL Soft", "default", "pulse", "alsa", "null"};

            for (const char *deviceName : deviceNames)
            {
                device = alcOpenDevice(deviceName);
                if (device)
                {
                    if (deviceName)
                        std::cout << "[AudioManager] Opened device: " << deviceName << std::endl;
                    else
                        std::cout << "[AudioManager] Opened default device" << std::endl;
                    break;
                }
                else
                {
                    ALCenum error = alcGetError(nullptr);
                    if (deviceName)
                        std::cout << "[AudioManager] Failed to open '" << deviceName << "' (error: " << error << ")" << std::endl;
                    else
                        std::cout << "[AudioManager] Failed to open default device (error: " << error << ")" << std::endl;
                }
            }

            if (!device)
            {
                std::cerr << "[AudioManager] Failed to open any audio device" << std::endl;
                std::cerr << "[AudioManager] Audio will be disabled - game will run without sound" << std::endl;
                std::cerr << "[AudioManager] Tip: Try running with: ALSOFT_DRIVERS=null ./bin/GAME_APPLICATION" << std::endl;
                return;
            }

            context = alcCreateContext(device, nullptr);
            if (!context)
            {
                std::cerr << "[AudioManager] Failed to create audio context" << std::endl;
                alcCloseDevice(device);
                device = nullptr;
                return;
            }

            if (!alcMakeContextCurrent(context))
            {
                std::cerr << "[AudioManager] Failed to make context current" << std::endl;
                alcDestroyContext(context);
                alcCloseDevice(device);
                device = nullptr;
                context = nullptr;
                return;
            }

            std::cout << "[AudioManager] Initialized successfully" << std::endl;
        }

        // Delete copy constructor and assignment operator for Singleton
        AudioManager(const AudioManager &) = delete;
        AudioManager &operator=(const AudioManager &) = delete;

    public:
        // Static method to get the singleton instance
        static AudioManager &getInstance()
        {
            static AudioManager instance; // Guaranteed to be destroyed, instantiated on first use.
            return instance;
        }

        // Clean up finished sources (call periodically in your game loop)
        void cleanupFinishedSources()
        {
            if (!isInitialized())
                return;

            ensureContextCurrent();
            std::vector<std::string> sourcesToDelete;

            for (auto &pair : sources)
            {
                ALint state;
                alGetSourcei(pair.second, AL_SOURCE_STATE, &state);

                if (state != AL_PLAYING)
                {
                    // Source has finished playing, mark for deletion
                    sourcesToDelete.push_back(pair.first);
                }
            }

            // Delete finished sources
            for (const auto &key : sourcesToDelete)
            {
                alDeleteSources(1, &sources[key]);
                sources.erase(key);
            }
        }

        ~AudioManager()
        {
            if (context)
            {
                alcMakeContextCurrent(context);
            }

            // Clean up sources
            for (auto &pair : sources)
            {
                alDeleteSources(1, &pair.second);
            }

            // Clean up buffers
            for (auto &pair : buffers)
            {
                alDeleteBuffers(1, &pair.second);
            }

            // Clean up context
            if (context)
            {
                alcMakeContextCurrent(nullptr);
                alcDestroyContext(context);
            }

            // Close device
            if (device)
            {
                alcCloseDevice(device);
            }

            std::cout << "[AudioManager] Cleaned up successfully" << std::endl;
        }

        // Check if the audio manager is initialized and ready
        bool isInitialized() const
        {
            return device != nullptr && context != nullptr;
        }

        // Load a WAV file and store it in a buffer
        bool loadWAV(const std::string &filename)
        {
            if (!isInitialized())
            {
                std::cerr << "[AudioManager] Not initialized. Cannot load WAV: " << filename << std::endl;
                return false;
            }

            ensureContextCurrent();
            alGetError(); // flush any pre-existing error state

            if (buffers.find(filename) != buffers.end())
            {
                std::cout << "[AudioManager] WAV already loaded: " << filename << std::endl;
                return true;
            }

            ALuint buffer = 0;
            std::ifstream wavFile(filename, std::ios::binary);
            if (!wavFile)
            {
                std::cerr << "[AudioManager] Failed to open WAV file: " << filename << std::endl;
                return false;
            }

            auto readBytes = [&wavFile](char *destination, std::streamsize count) -> bool
            {
                wavFile.read(destination, count);
                return wavFile.good();
            };

            char riff[4] = {};
            char wave[4] = {};
            uint32_t riffChunkSize = 0;

            if (!readBytes(riff, 4) || !readBytes(reinterpret_cast<char *>(&riffChunkSize), 4) || !readBytes(wave, 4))
            {
                std::cerr << "[AudioManager] Invalid WAV header: " << filename << std::endl;
                return false;
            }

            if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0)
            {
                std::cerr << "[AudioManager] Not a valid RIFF/WAVE file: " << filename << std::endl;
                return false;
            }

            uint16_t audioFormat = 0;
            uint16_t numChannels = 0;
            uint32_t sampleRate = 0;
            uint16_t bitsPerSample = 0;
            std::vector<uint8_t> audioData;
            bool foundFmt = false;
            bool foundData = false;

            while (wavFile && (!foundFmt || !foundData))
            {
                char chunkId[4] = {};
                uint32_t chunkSize = 0;

                if (!readBytes(chunkId, 4) || !readBytes(reinterpret_cast<char *>(&chunkSize), 4))
                {
                    break;
                }

                if (std::strncmp(chunkId, "fmt ", 4) == 0)
                {
                    if (chunkSize < 16)
                    {
                        std::cerr << "[AudioManager] Invalid fmt chunk in WAV: " << filename << std::endl;
                        return false;
                    }

                    uint32_t byteRate = 0;
                    uint16_t blockAlign = 0;
                    if (!readBytes(reinterpret_cast<char *>(&audioFormat), 2) ||
                        !readBytes(reinterpret_cast<char *>(&numChannels), 2) ||
                        !readBytes(reinterpret_cast<char *>(&sampleRate), 4) ||
                        !readBytes(reinterpret_cast<char *>(&byteRate), 4) ||
                        !readBytes(reinterpret_cast<char *>(&blockAlign), 2) ||
                        !readBytes(reinterpret_cast<char *>(&bitsPerSample), 2))
                    {
                        std::cerr << "[AudioManager] Failed to read fmt chunk in WAV: " << filename << std::endl;
                        return false;
                    }

                    if (chunkSize > 16)
                    {
                        wavFile.seekg(chunkSize - 16, std::ios::cur);
                    }

                    foundFmt = true;
                }
                else if (std::strncmp(chunkId, "data", 4) == 0)
                {
                    audioData.resize(chunkSize);
                    if (chunkSize > 0)
                    {
                        wavFile.read(reinterpret_cast<char *>(audioData.data()), static_cast<std::streamsize>(chunkSize));
                        if (!wavFile.good())
                        {
                            std::cerr << "[AudioManager] Failed to read audio data from WAV: " << filename << std::endl;
                            return false;
                        }
                    }
                    foundData = true;
                }
                else
                {
                    wavFile.seekg(chunkSize, std::ios::cur);
                }

                if (chunkSize % 2 == 1)
                {
                    wavFile.seekg(1, std::ios::cur);
                }
            }

            if (!foundFmt || !foundData)
            {
                std::cerr << "[AudioManager] WAV is missing fmt or data chunk: " << filename << std::endl;
                return false;
            }

            if (audioFormat != 1)
            {
                std::cerr << "[AudioManager] Unsupported WAV encoding (only PCM supported): " << filename << std::endl;
                return false;
            }

            // Determine format
            ALenum format;
            if (numChannels == 1 && bitsPerSample == 8)
                format = AL_FORMAT_MONO8;
            else if (numChannels == 1 && bitsPerSample == 16)
                format = AL_FORMAT_MONO16;
            else if (numChannels == 2 && bitsPerSample == 8)
                format = AL_FORMAT_STEREO8;
            else if (numChannels == 2 && bitsPerSample == 16)
                format = AL_FORMAT_STEREO16;
            else
            {
                std::cerr << "[AudioManager] Unsupported WAV format" << std::endl;
                return false;
            }

            // Create OpenAL buffer
            alGetError(); // clear stale error state before creating buffer
            alGenBuffers(1, &buffer);
            ALenum error = alGetError();
            if (error != AL_NO_ERROR || buffer == 0)
            {
                std::cerr << "[AudioManager] Error generating buffer: " << error << std::endl;
                return false;
            }

            alBufferData(buffer, format, audioData.data(), static_cast<ALsizei>(audioData.size()), static_cast<ALsizei>(sampleRate));
            error = alGetError();
            if (error != AL_NO_ERROR)
            {
                std::cerr << "[AudioManager] Error filling buffer: " << error << std::endl;
                alDeleteBuffers(1, &buffer);
                return false;
            }

            buffers[filename] = buffer;
            std::cout << "[AudioManager] Loaded WAV: " << filename << std::endl;
            return true;
        }

        // Play a sound effect
        bool playSound(const std::string &filename)
        {
            if (!isInitialized())
            {
                std::cerr << "[AudioManager] Not initialized. Cannot play sound: " << filename << std::endl;
                return false;
            }

            ensureContextCurrent();
            alGetError(); // flush any pre-existing error state

            if (buffers.find(filename) == buffers.end())
            {
                if (!loadWAV(filename))
                {
                    std::cerr << "[AudioManager] Failed to load WAV file: " << filename << std::endl;
                    return false;
                }
            }

            alGetError(); // clear any error from load path before source creation

            // Create a new source for this sound
            ALuint source = 0;
            alGenSources(1, &source);

            // Check for OpenAL errors
            ALenum error = alGetError();
            if (error != AL_NO_ERROR)
            {
                std::cerr << "[AudioManager] Error generating source: " << error << std::endl;
                return false;
            }

            if (source == 0)
            {
                std::cerr << "[AudioManager] Failed to generate valid source" << std::endl;
                return false;
            }

            // Attach buffer to source
            alSourcei(source, AL_BUFFER, buffers[filename]);
            error = alGetError();
            if (error != AL_NO_ERROR)
            {
                std::cerr << "[AudioManager] Error setting buffer: " << error << std::endl;
                alDeleteSources(1, &source);
                return false;
            }

            // Set source properties for better audio playback
            alSourcef(source, AL_PITCH, 1.0f);
            alSourcef(source, AL_GAIN, 1.0f);
            alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);

            // Play the source
            alSourcePlay(source);
            error = alGetError();
            if (error != AL_NO_ERROR)
            {
                std::cerr << "[AudioManager] Error playing source: " << error << std::endl;
                alDeleteSources(1, &source);
                return false;
            }

            // Store source for later cleanup
            // Use a unique key to support multiple simultaneous sounds
            static int soundCounter = 0;
            std::string sourceKey = filename + "_" + std::to_string(soundCounter++);
            sources[sourceKey] = source;

            std::cout << "[AudioManager] Playing sound: " << filename << std::endl;
            return true;
        }
    };
}