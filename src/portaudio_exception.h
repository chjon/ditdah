#pragma once

#include <exception>
#include <string>

class PortAudioException: public std::runtime_error {
public:
    PortAudioException(const std::string& s)
        : std::runtime_error(s)
    {}

    PortAudioException(const PaError err)
        : std::runtime_error(Pa_GetErrorText(err))
    {}
};