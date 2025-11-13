#pragma once

#include <exception>
#include <iostream>

#include "portaudio.h"

#include "portaudio_exception.h"

class PortAudioManager {
public:
    PortAudioManager() {
        PaError err = Pa_Initialize();
        if (err != paNoError)
            throw PortAudioException(err);
    }

    PortAudioManager(const PortAudioManager&) = delete;

    ~PortAudioManager() noexcept {
        const PaError err = Pa_Terminate();
        if (err != paNoError) {
            std::cerr
                << "Failed to terminate PortAudio: "
                << Pa_GetErrorText(err)
                << std::endl;
        }
    }
};