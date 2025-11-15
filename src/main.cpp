#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <queue>

#include <errno.h>
#include <math.h>
#include <linux/input.h>

#include "portaudio.h"

#include "keyboard_event_handler.h"
#include "portaudio_manager.h"
#include "portaudio_stream.h"

#define SAMPLE_RATE (44100)

extern int errno;

struct MorseElement {
    // One-hot encoded length
    uint8_t len;

    // Morse code: 0 = dit, 1 = dah
    uint8_t code;
};

// Morse Code durations, in units of 1 / <SAMPLE_RATE>
const long DURATION_DIT = 5 * SAMPLE_RATE / 100;
const long DURATION_DAH = 3 * DURATION_DIT;
const long DURATION_INTER_ELEMENT_GAP = 1 * DURATION_DIT;
const long DURATION_INTRA_LETTER_GAP = 3 * DURATION_DIT; // 3 dits total
const long DURATION_INTRA_WORD_GAP = 7 * DURATION_DIT; // 7 dits total

static std::atomic_bool interrupted;

void signal_handler(int signal) {
    interrupted = true;
}

/**
 * Struct representing the current state of the Morse code output and the
 * time of the next state change. This works as long as DURATION_DIT is
 * larger than framesPerBuffer.
 * All times in units of 1 / <SAMPLE_RATE>
 */
struct paMorse {
    // Current time
    long t;

    // Time for the next state change
    long next_t;

    // Whether to output audio at the current time
    bool emit;

    long waveIndex;

    // Morse elements to output
    std::queue<MorseElement> elements;

    // Wave data to output
    std::vector<float> waveData;
};

/* This routine will be called by the PortAudio engine when audio is needed.
 * It may called at interrupt level on some machines so don't do anything
 * that could mess up the system like calling malloc() or free().
*/ 
static int patestCallback(
    const void *inputBuffer,
    void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData
) {
    // Prevent unused variable warning
    (void) inputBuffer;
    
    /* Cast data passed through stream to our structure. */
    paMorse* data = reinterpret_cast<paMorse*>(userData); 
    float* out = reinterpret_cast<float*>(outputBuffer);
    
    for (
        unsigned int i = 0, j = data->t;
        i < framesPerBuffer;
        i++, j++, data->waveIndex = (data->waveIndex + 1) % data->waveData.size()
    ) {
        if (j == data->next_t) {
            data->emit = false;
        } else if (j == data->next_t + DURATION_INTER_ELEMENT_GAP) {
            data->t = 0;
            data->emit = false;
            // Determine the length of the next element to play
            // Stop output if there are no more Morse elements to output
            if (data->elements.size() == 0) {
                data->emit = false;
                data->next_t = framesPerBuffer + 1;
                
            // Determine the next time there's a state change
            } else {                
                MorseElement& element = data->elements.front();
                data->waveIndex = 0;
                data->emit = element.len > 0;

                // Get duration (branch-free)
                const static long DURATIONS[] = {
                    DURATION_DIT,
                    DURATION_DAH,
                    DURATION_INTRA_LETTER_GAP,
                    DURATION_INTRA_WORD_GAP,
                };
                const size_t durationOffset = (element.len == 0) << 1;
                data->next_t = DURATIONS[durationOffset | (element.code & 0x1)];
                element.len >>= 1, element.code >>= 1;
                
                if (!data->emit) data->elements.pop();
            }
        }

        const float value = data->emit ? data->waveData[data->waveIndex] : 0.f;
        *(out++) = value; // left
        *(out++) = value; // right
    }

    data->t += framesPerBuffer;
    return 0;
}

int keyboard_morse(KeyboardEventHandler& keh, long frequency) {
    std::vector<float> waveData;
    for (long i = 0; i < DURATION_DAH; i++) {
        waveData.push_back(0.5 * sin(
            2.0 * M_PI * static_cast<double>(i * frequency) /
            static_cast<double>(SAMPLE_RATE)
        ));
    }

    // Populate data buffer
    paMorse data{
        .t = 0,
        .next_t = 0,
        .emit = false,
        .waveIndex = 0,
        .elements = std::queue<MorseElement>(),
        .waveData = std::move(waveData),
    };

    // Open an audio output stream
    PortAudioStream stream(0, 2, SAMPLE_RATE, paFloat32, 256);
    stream.open(patestCallback, &data);
    stream.start();

    // Sleep until interrupted
    keh.run([&](input_event kbd_input) {
        // Only handle initial keypress event
        if (kbd_input.type != EV_KEY || kbd_input.value != 1) return;
        
        auto& q = data.elements;
        switch (kbd_input.code) {
            case KEY_A: q.push({ 0b0010, 0b0010 }); break;
            case KEY_B: q.push({ 0b1000, 0b0001 }); break;
            case KEY_C: q.push({ 0b1000, 0b0101 }); break;
            case KEY_D: q.push({ 0b0100, 0b0001 }); break;
            case KEY_E: q.push({ 0b0001, 0b0000 }); break;
            case KEY_F: q.push({ 0b1000, 0b0100 }); break;                
            case KEY_G: q.push({ 0b0100, 0b0011 }); break;
            case KEY_H: q.push({ 0b1000, 0b0000 }); break;            
            case KEY_I: q.push({ 0b0010, 0b0000 }); break;
            case KEY_J: q.push({ 0b1000, 0b1110 }); break;
            case KEY_K: q.push({ 0b0100, 0b0101 }); break;
            case KEY_L: q.push({ 0b1000, 0b0010 }); break;
            case KEY_M: q.push({ 0b0010, 0b0011 }); break;
            case KEY_N: q.push({ 0b0010, 0b0001 }); break;
            case KEY_O: q.push({ 0b0100, 0b0111 }); break;
            case KEY_P: q.push({ 0b1000, 0b0110 }); break;
            case KEY_Q: q.push({ 0b1000, 0b1011 }); break;
            case KEY_R: q.push({ 0b0100, 0b0010 }); break;
            case KEY_S: q.push({ 0b0100, 0b0000 }); break;
            case KEY_T: q.push({ 0b0001, 0b0001 }); break;
            case KEY_U: q.push({ 0b0100, 0b0100 }); break;
            case KEY_V: q.push({ 0b1000, 0b1000 }); break;
            case KEY_W: q.push({ 0b0100, 0b0110 }); break;
            case KEY_X: q.push({ 0b1000, 0b1001 }); break;
            case KEY_Y: q.push({ 0b1000, 0b1101 }); break;
            case KEY_Z: q.push({ 0b1000, 0b0011 }); break;                
            default: break;
        }
    });

    return 0;
}

int main(const int argc, const char * const* argv) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <KEYBOARD_INPUT_FILE> <FREQ_Hz> " << std::endl;
        return 1;
    }

    // Parse inputs
    const char* path = argv[1];
    const long frequency = std::atol(argv[2]);

    // Install signal handler
    interrupted = false;
    std::signal(SIGINT, signal_handler);

    // Create keyboard event handler
    KeyboardEventHandler keh(path, interrupted);

    // Initialize PortAudio
    PortAudioManager manager;

    return keyboard_morse(keh, frequency);
}