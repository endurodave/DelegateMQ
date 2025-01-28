/// @file
/// @brief Implement an active object pattern using delegates. 
/// An example of asynchronous method invocation.

#include "ActiveObject.h"
#include "DelegateLib.h"
#include <iostream>

using namespace DelegateLib;
using namespace std;

namespace Example
{
    class ActiveObject
    {
    public:
        ActiveObject() : m_thread("ActiveObject") {
            m_thread.CreateThread();
        }

        void SetValue(double v) {
            // Is caller executing on m_thread?
            if (m_thread.GetThreadId() != Thread::GetCurrentThreadId())
            {
                // Create an asynchronous delegate and reinvoke the function call on m_thread
                MakeDelegate(this, &ActiveObject::SetValue, m_thread).AsyncInvoke(v);
                return;
            }

            // m_value is always updated on m_thread
            m_value = v;
        }

        double GetValue() {
            // Is caller executing on m_thread?
            if (m_thread.GetThreadId() != Thread::GetCurrentThreadId())
            {
                // Create an asynchronous delegate and reinvoke the function call on m_thread
                return MakeDelegate(this, &ActiveObject::GetValue, m_thread, WAIT_INFINITE)();
            }

            // m_value is always returned on m_thread
            return m_value;
        }


        ~ActiveObject() {
            m_thread.ExitThread();
        }

    private:
        double m_value = 0.0;

        // Object has an internal thread
        Thread m_thread;
    };


    void ActiveObjectExample()
    {
        ActiveObject activeObject;
        activeObject.SetValue(1.0);
        activeObject.SetValue(2.0);
        double value = activeObject.GetValue();
    }
}
