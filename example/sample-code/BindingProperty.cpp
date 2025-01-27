/// @file
/// @brief Implement the binding property design pattern using delegates. 

#include "BindingProperty.h"
#include "DelegateLib.h"
#include "Thread.h"
#include "AsyncInvoke.h"
#include <iostream>
#include <functional>
#include <string>

using namespace DelegateLib;
using namespace std;

namespace Example
{
    // A simple observer class that binds properties
    class Property {
    private:
        int value;
        MulticastDelegateSafe<void(int)> callbacks;

    public:
        Property(int initial_value) : value(initial_value) {}

        // Set the value of the property and call the bound callback function
        void set(int new_value) {
            if (value != new_value) {
                value = new_value;
                if (callbacks) {
                    callbacks(value);  // Notify listeners
                }
            }
        }

        // Get the value of the property
        int get() const {
            return value;
        }

        // Bind a callback function to be called whenever the value changes
        void bind(Delegate<void(int)>& cb) {
            callbacks += cb;
        }

        void bind(Delegate<void(int)>&& cb) {
            callbacks += std::move(cb);
        }
    };

    // A simple class that demonstrates property binding
    class PropertyBinding {
    private:
        Thread workerThread;
        Property property1;
        Property property2;
        Property property3;
        std::mutex lock;

    public:
        PropertyBinding() : workerThread("Example"), property1(0), property2(0), property3(0) {
            workerThread.CreateThread();

            std::function<void(int)> lambda2 = [this](int new_value) {
                // Whenever property1 changes, set property2 to double the value
                property2.set(new_value * 2);
            };

            // Bind property2 to property1's changes (synchonous binding)
            property1.bind(MakeDelegate(lambda2));  

            std::function<void(int)> lambda3 = [this](int new_value) {
                const std::lock_guard<std::mutex> lk(lock);
                // Whenever property1 changes, set property3 to triple the value
                property3.set(new_value * 3);
            };

            // Bind property3 to property1's changes on workerThread thread context
            // (asynchronous binding). e.g. technique useful if a property change needs 
            // a callback on say the GUI thread.
            property1.bind(MakeDelegate(lambda3, workerThread, WAIT_INFINITE));
        }

        void setProperty1(int value) {
            property1.set(value);
        }

        int getProperty1() const {
            return property1.get();
        }

        int getProperty2() const {
            return property2.get();
        }

        int getProperty3() {
            // Protect property3 from cross-threading
            const std::lock_guard<std::mutex> lk(lock);
            return property3.get();
        }
    };

    void BindingPropertyExample()
    {
        PropertyBinding example;

        // Set property1 and automatically property2 and property3 will be updated
        std::cout << "Initial values: Property1 = " << example.getProperty1()
            << ", Property2 = " << example.getProperty2()
            << ", Property3 = " << example.getProperty3() << std::endl;

        example.setProperty1(5);
        std::cout << "After setting Property1 to 5: Property1 = " << example.getProperty1()
            << ", Property2 = " << example.getProperty2()
            << ", Property3 = " << example.getProperty3() << std::endl;

        example.setProperty1(10);
        std::cout << "After setting Property1 to 10: Property1 = " << example.getProperty1()
            << ", Property2 = " << example.getProperty2()
            << ", Property3 = " << example.getProperty3() << std::endl;
    }
}
