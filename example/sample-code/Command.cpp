/// @file
/// @brief Implement the command pattern using delegates.

#include "Command.h"
#include "DelegateMQ.h"
#include <iostream>

using namespace dmq;
using namespace std;

namespace Example
{
    // Command Interface
    class Command {
    public:
        virtual void execute() = 0;
        virtual ~Command() = default;
    };

    // Receiver class (a simple task example)
    class Light {
    public:
        void turnOn() { std::cout << "Light is ON\n"; }
        void turnOff() { std::cout << "Light is OFF\n"; }
    };

    // Concrete Command
    class LightOnCommand : public Command {
    private:
        // Delegate to hold the command target function
        UnicastDelegate<void(void)> command;

    public:
        LightOnCommand(Light* light) {
            // Store target function within the delegate container
            command = MakeDelegate(light, &Light::turnOn);

            // Alternatively, use an async delegate to invoke command
            //command = MakeDelegate(light, &Light::turnOn, workerThread);
        }
        void execute() override { command(); }
    };

    // Invoker (uses command objects)
    class RemoteControl {
    public:
        void submit(Command* command) {
            command->execute();
        }
    };

    void CommandExample()
    {
        Light light;
        LightOnCommand lightOnCommand(&light);
        RemoteControl remote;

        // Sending the function object (command) to the invoker
        remote.submit(&lightOnCommand);
    }
}
