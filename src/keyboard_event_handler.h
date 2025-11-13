#pragma once

#include <atomic>
#include <exception>
#include <fstream>
#include <functional>
#include <iostream>
#include <linux/input.h>
#include <sstream>

class KeyboardEventHandler {
public:
    FILE* m_event_file;
    std::atomic_bool& m_interrupted;

public:
    KeyboardEventHandler(
        const char* event_file_path,
        std::atomic_bool& g_interrupted
    )
        : m_event_file(nullptr)
        , m_interrupted(g_interrupted)
    {
        m_event_file = fopen(event_file_path, "r");
        if (m_event_file == nullptr) {
            throw std::runtime_error((std::stringstream()
                << "Failed to open file with path '"
                << event_file_path << "'"
            ).str());
        }
    }

    ~KeyboardEventHandler() noexcept {
        if (fclose(m_event_file) == -1)
            std::cerr << "Failed to close keyboard input file" << std::endl;
    }

    void run(std::function<void(input_event&)> event_handler) {
        input_event kbd_input;
        while (!m_interrupted) {
            if (fread(&kbd_input, sizeof(input_event), 1, m_event_file) == -1) {
                throw std::runtime_error("Failed to read event");
            } else {
                event_handler(kbd_input);
            }
        }
    }
};