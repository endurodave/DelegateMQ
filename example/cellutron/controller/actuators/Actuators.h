#ifndef _ACTUATORS_H
#define _ACTUATORS_H

#include "DelegateMQ.h"

/// @brief Singleton class for controlling hardware actuators.
class Actuators {
public:
    static Actuators& GetInstance() {
        static Actuators instance;
        return instance;
    }

    void Initialize();
    void Shutdown();

    /// Set a valve state (Synchronous Blocking call)
    int SetValve(int id, bool open);

    /// Set a pump speed (Synchronous Blocking call)
    int SetPump(int id, int speed);

private:
    Actuators() = default;
    ~Actuators();

    Actuators(const Actuators&) = delete;
    Actuators& operator=(const Actuators&) = delete;

    int InternalSetValve(int id, bool open);
    int InternalSetPump(int id, int speed);

    // Use standardized thread name for Active Object subsystem
    Thread m_thread{"ActuatorThread"};
};

#endif
