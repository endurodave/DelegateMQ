#ifndef MAIN_H
#define MAIN_H

#include "DelegateMQ.h"
#include "SysData.h"
#include "SysDataNoLock.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <utility>
#include <future>

#ifdef DMQ_LOG
#ifdef _WIN32
// https://github.com/gabime/spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>  // MSVC sink
#endif
#endif

using namespace std;
using namespace dmq;
using namespace std::chrono;

/// The main file examples namespace
namespace Main
{
    Thread workerThread1("WorkerThread1");

    /// @brief Test client to get callbacks from SysData::SystemModeChangedDelgate and 
    /// SysDataNoLock::SystemModeChangedDelegate
    class SysDataClient : public std::enable_shared_from_this<SysDataClient>
    {
    public:
        // Constructor
        SysDataClient() :
            m_numberOfCallbacks(0)
        {
        }

        ~SysDataClient()
        {
            // WARNING: Do NOT attempt to unregister here using shared_from_this().
            // It will cause a crash (std::bad_weak_ptr).
        }

        // Call this immediately after creating the shared_ptr
        // See DETAILS.md section Object Lifetime and Async Delegates.
        void Init()
        {
            // Register for callbacks
            SysData::GetInstance().SystemModeChangedDelegate += MakeDelegate(shared_from_this(), &SysDataClient::CallbackFunction, workerThread1);
            SysDataNoLock::GetInstance().SystemModeChangedDelegate += MakeDelegate(shared_from_this(), &SysDataClient::CallbackFunction, workerThread1);
        }

        void Term() {
            // Unsubscribe safely while "this" is still valid
            SysData::GetInstance().SystemModeChangedDelegate -= MakeDelegate(shared_from_this(), &SysDataClient::CallbackFunction, workerThread1);
            SysDataNoLock::GetInstance().SystemModeChangedDelegate -= MakeDelegate(shared_from_this(), &SysDataClient::CallbackFunction, workerThread1);
        }

    private:
        void CallbackFunction(const SystemModeChanged& data)
        {
            m_numberOfCallbacks++;
            cout << "CallbackFunction " << data.CurrentSystemMode << endl;
        }

        std::atomic<int> m_numberOfCallbacks;

        // See USE_ALLOCATOR in DelegateOpt.h for info about optional fixed-block allocator use
        XALLOCATOR
    };

    struct TestStruct
    {
        int x;
        TestStruct() { x = 0; }
        TestStruct(const TestStruct& d) { x = d.x; }
        ~TestStruct() {}

        XALLOCATOR
    };

    struct TestStructNoCopy
    {
        TestStructNoCopy(int _x) { x = _x; }
        int x;

    private:
        // Prevent copying objects
        TestStructNoCopy(const TestStructNoCopy&) = delete;
        TestStructNoCopy& operator=(const TestStructNoCopy&) = delete;
    };

    void FreeFuncInt(int value)
    {
        cout << "FreeFuncInt " << value << endl;
    }

    int FreeFuncIntRetInt(int value)
    {
        cout << "FreeFuncIntRetInt " << value << endl;
        return value;
    }

    void FreeFuncPtrPtrTestStruct(TestStruct** value)
    {
        cout << "FreeFuncPtrPtrTestStruct " << (*value)->x << endl;
    }

    void FreeFuncDelegateArg(Delegate<void(int)>& delegate)
    {
        delegate(123);
    }

    class TestClass
    {
    public:
        TestClass() {}
        ~TestClass() {}

        void StaticFuncInt(int value)
        {
            cout << "StaticFuncInt " << value << endl;
        }

        void StaticFuncIntConst(int value) const
        {
            cout << "StaticFuncIntConst " << value << endl;
        }

        void MemberFuncInt(int value)
        {
            cout << "MemberFuncInt " << value << endl;
        }

        void MemberFuncIntConst(int value) const
        {
            cout << "MemberFuncIntConst " << value << endl;
        }

        void MemberFuncInt2(int value)
        {
            cout << "MemberFuncInt2 " << value << endl;
        }

        void MemberFunc(TestStruct* value)
        {
            cout << "MemberFunc " << value->x << endl;
        }

        void MemberFuncThreeArgs(const TestStruct& value, float f, int** i)
        {
            cout << "MemberFuncThreeArgs " << value.x << " " << f << " " << (**i) << endl;
        }

        void MemberFuncNoCopy(std::shared_ptr<TestStructNoCopy> value)
        {
            cout << "MemberFuncNoCopy " << value->x << endl;
        }

        void MemberFuncStdString(const std::string& s, int year)
        {
            cout << "MemberFuncStdString " << s.c_str() << " " << year << endl;
        }

        int MemberFuncStdStringRetInt(std::string& s)
        {
            s = "Hello world";
            return 2020;
        }

        static void StaticFunc(TestStruct* value)
        {
            cout << "StaticFunc " << value->x << endl;
        }

        int TestFunc()
        {
            cout << "TestFunc " << endl;
            return 987;
        }
        void TestFuncNoRet()
        {
            cout << "TestFuncNoRet " << endl;
        }

        TestStruct TestFuncUserTypeRet()
        {
            TestStruct t;
            t.x = 777;
            return t;
        }

        XALLOCATOR
    };

    void TimerExpiredCb(void)
    {
        static int count = 0;
        cout << "TimerExpiredCb " << count++ << endl;
    }

    class Base {
    public:
        virtual void display(std::string msg) { cout << "Base: " << msg << endl; }
        XALLOCATOR
    };

    class Derived : public Base {
    public:
        void display(std::string msg) override { cout << "Derviced: " << msg << endl; }
    };

    class Coordinates
    {
    public:
        int x = 0;
        int y = 0;
    };

    class CoordinatesHandler
    {
    public:
        static MulticastDelegateSafe<void(const std::shared_ptr<const Coordinates>)> CoordinatesChanged;

        void SetData(const Coordinates& data)
        {
            m_data = data;
            CoordinatesChanged(std::make_shared<const Coordinates>(m_data));
        }

    private:
        Coordinates m_data;
    };

    MulticastDelegateSafe<void(const std::shared_ptr<const Coordinates>)> CoordinatesHandler::CoordinatesChanged;

    void CoordinatesChangedCallback(const std::shared_ptr<const Coordinates> c)
    {
        cout << "New coordinates " << c->x << " " << c->y << endl;
    }

    class Test
    {
    public:
        void Func(int i) {}
        void Func2(int i) {}
    };

    void StartLogger()
    {
#ifdef DMQ_LOG
#ifdef _WIN32
        // Create the MSVC sink (multi-threaded)
        auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();

        // Optional: Set the log level for the sink
        msvc_sink->set_level(spdlog::level::debug);

        // Create a logger using the MSVC sink
        auto logger = std::make_shared<spdlog::logger>("msvc_logger", msvc_sink);

        // Set as default logger (optional)
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug); // Show debug and above
#endif
#endif
    }
}

#endif