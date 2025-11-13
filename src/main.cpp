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

enum MorseElement {
    DIT = 0,
    DAH = 1,
    LETTER_BREAK,
    WORD_BREAK,
};

// Morse Code durations, in units of 1 / <SAMPLE_RATE>
const long DURATION_DIT = 10 * SAMPLE_RATE / 100;
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

    // Frequency to emit
    long freq;

    // Morse elements to output
    std::queue<MorseElement> elements;
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
    /* Cast data passed through stream to our structure. */
    paMorse* data = reinterpret_cast<paMorse*>(userData); 
    float* out = reinterpret_cast<float*>(outputBuffer);
    (void) inputBuffer; /* Prevent unused variable warning. */
    
    for (unsigned int i = 0, j = data->t; i < framesPerBuffer; i++, j++) {
        if (j == data->next_t) {
            data->emit = false;
        } else if (j == data->next_t + DURATION_INTER_ELEMENT_GAP) {
            j = data->t = 0;
            data->emit = false;
            // Determine the length of the next element to play
            // Stop output if there are no more Morse elements to output
            if (data->elements.size() == 0) {
                data->emit = false;
                data->next_t = framesPerBuffer + 1;
                
            // Determine the next time there's a state change
            } else {                
                const MorseElement element = data->elements.front();
                data->elements.pop();

                data->emit = false;
                switch (element) {
                    case MorseElement::DIT:
                        data->emit = true;
                        data->next_t = DURATION_DIT;
                        break;
                    case MorseElement::DAH:
                        data->emit = true;
                        data->next_t = DURATION_DAH;
                        break;
                    case MorseElement::LETTER_BREAK:
                        data->next_t = DURATION_INTRA_LETTER_GAP;
                        break;
                    case MorseElement::WORD_BREAK:
                        data->next_t = DURATION_INTRA_WORD_GAP;
                        break;
                    default:
                        break;
                }
            }
        }

        float value = 0.5 * sin(
            2.0 * M_PI * static_cast<double>(j * data->freq) /
            static_cast<double>(SAMPLE_RATE)
        );

        if (!data->emit)
            value = 0;

        *(out++) = value; // left
        *(out++) = value; // right
    }

    data->t += framesPerBuffer;
    return 0;
}

int keyboard_morse(KeyboardEventHandler& keh, long frequency) {
    // Populate data buffer
    paMorse data{
        .t = 0,
        .next_t = 0,
        .emit = false,
        .freq = frequency,
        .elements = std::queue<MorseElement>(),
    };

    // Open an audio output stream
    PortAudioStream stream(0, 2, SAMPLE_RATE, paFloat32, 256);
    stream.open(patestCallback, &data);
    stream.start();

    // Sleep until interrupted
    keh.run([&](input_event kbd_input) {
        // Only handle initial keypress event
        if (kbd_input.type != EV_KEY || kbd_input.value != 1) return;
        
        switch (kbd_input.code) {
            case KEY_A:
                data.elements.push(MorseElement::DIT);
                data.elements.push(MorseElement::DAH);
                data.elements.push(MorseElement::LETTER_BREAK);
                break;

            case KEY_B:
                data.elements.push(MorseElement::DAH);
                data.elements.push(MorseElement::DIT);
                data.elements.push(MorseElement::DIT);
                data.elements.push(MorseElement::DIT);
                data.elements.push(MorseElement::LETTER_BREAK);
                break;

            case KEY_C:
                data.elements.push(MorseElement::DAH);
                data.elements.push(MorseElement::DIT);
                data.elements.push(MorseElement::DAH);
                data.elements.push(MorseElement::DIT);
                data.elements.push(MorseElement::LETTER_BREAK);
                break;

            case KEY_D:
                data.elements.push(MorseElement::DAH);
                data.elements.push(MorseElement::DIT);
                data.elements.push(MorseElement::DIT);
                data.elements.push(MorseElement::LETTER_BREAK);

            default:
                break;
        }
    });

    return 0;
}

int main(const int argc, const char * const* argv) {
    // Requires sudo setcap 'cap_dac_override=ep' argv[0]

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