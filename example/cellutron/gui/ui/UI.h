#ifndef _UI_H
#define _UI_H

#include "DelegateMQ.h"

/// @brief Singleton class for the User Interface.
class UI {
public:
    static UI& GetInstance() {
        static UI instance;
        return instance;
    }

    /// Start the UI event loop (blocking).
    void Start();

    /// Shutdown the UI.
    void Shutdown();

private:
    UI() = default;
    ~UI();

    UI(const UI&) = delete;
    UI& operator=(const UI&) = delete;

    // Use standardized thread name for Active Object subsystem
    Thread m_thread{"UIThread"};
};

#endif
