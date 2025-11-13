#pragma once

#include <exception>
#include <iostream>

#include "portaudio.h"

#include "portaudio_exception.h"

class PortAudioStream {
private:
    // Raw stream object
    PaStream *m_stream;

    // no input channels
    const int m_numInputChannels;

    // stereo output
    const int m_numOutputChannels;
    
    // Sample rate (Hz)
    const double m_sampleRate;

    // 32 bit floating point output
    const PaSampleFormat m_sampleFormat;

    // The number of sample frames that PortAudio will request from the callback
    // Apps may want to use paFramesPerBufferUnspecified, which tells PortAudio
    // to pick the best, possibly changing, buffer size.
    const unsigned long m_framesPerBuffer;

    // Whether the stream is stopped
    bool m_stopped;

public:
    PortAudioStream(
        const int numInputChannels,
        const int numOutputChannels,
        const double sampleRate,
        const PaSampleFormat sampleFormat,
        const unsigned long framesPerBuffer
    )
        : m_stream(nullptr)
        , m_numInputChannels(numInputChannels)
        , m_numOutputChannels(numOutputChannels)
        , m_sampleFormat(sampleFormat)
        , m_sampleRate(sampleRate)
        , m_framesPerBuffer(framesPerBuffer)
        , m_stopped(true)
    {}

    ~PortAudioStream() noexcept {
        try {
            if (!m_stopped) stop();
            if (m_stream != nullptr) close();
        } catch (const PortAudioException& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    /**
     * Initialize the stream
     * 
     * @param data A pointer that will be passed to the callback
     */
    void open(
        PaStreamCallback const* callback,
        void* data
    ) {
        if (m_stream != nullptr)
            throw PortAudioException("Stream is already open");

        const PaError err = Pa_OpenDefaultStream(
            &m_stream,
            m_numInputChannels,
            m_numOutputChannels,
            m_sampleFormat,  
            m_sampleRate,
            m_framesPerBuffer,        
            callback,
            data
        );

        if (err != paNoError)
            throw PortAudioException(err);
    }

    void close() {
        // Check whether stream is already closed
        if (m_stream == nullptr) return;

        // Close stream
        const PaError err = Pa_CloseStream(m_stream);
        if (err != paNoError)
            throw PortAudioException(err);

        // Mark stream as closed
        m_stream = nullptr;
    }

    void start() {
        if (m_stream == nullptr)
            throw PortAudioException("Stream is not open");

        // Early exit if the stream is already started
        if (!m_stopped) return;

        // Start audio stream
        const PaError err = Pa_StartStream(m_stream);
        if (err != paNoError)
            throw PortAudioException(err);

        m_stopped = false;
    }

    void stop() {
        if (m_stream == nullptr)
            throw PortAudioException("Stream is not open");

        // Early exit if the stream is already stopped
        if (m_stopped) return;

        const PaError err = Pa_StopStream(m_stream);
        if (err != paNoError)
            throw PortAudioException(err);

        m_stopped = true;
    }
};