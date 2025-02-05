/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.

#include "DelegateMQ.h"
#include <iostream>
#include <sstream>
#include <chrono>

using namespace DelegateMQ;
using namespace std;

// The "transport" is simplely a shared std::stringstream
class Transport
{
public:
    static void Send(std::stringstream& os) {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_ss << os.rdbuf();
    }
    static std::stringstream& Receive() {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_ss;
    }
private:
    // Simulate send/recv data using a stream
    static std::stringstream m_ss;
    static std::mutex m_mutex;
};

std::stringstream Transport::m_ss(ios::in | ios::out | ios::binary);
std::mutex Transport::m_mutex;

// Dispatcher sends data to the transport for transmission to the receiver
class Dispatcher : public IDispatcher
{
public:
    // Send argument data to the transport
    virtual int Dispatch(std::ostream& os, DelegateRemoteId id) {
        std::stringstream ss(ios::in | ios::out | ios::binary);
        ss << os.rdbuf();
        Transport::Send(ss);
        return 0;
    }
};

// make_serialized serializes each remote function argument
template<typename... Ts>
void make_serialized(std::ostream& os) { }

template<typename Arg1, typename... Args>
void make_serialized(std::ostream& os, Arg1& arg1, Args... args) {
    os << arg1;
    make_serialized(os, args...);
}

template<typename Arg1, typename... Args>
void make_serialized(std::ostream& os, Arg1* arg1, Args... args) {
    os << *arg1;
    make_serialized(os, args...);
}

template<typename Arg1, typename... Args>
void make_serialized(std::ostream& os, Arg1** arg1, Args... args) {
    static_assert(!std::is_pointer_v<Arg1>, "Pointer-to-pointer argument not supported");
}

// make_unserialized serializes each remote function argument
template<typename... Ts>
void make_unserialized(std::istream& is) { }

template<typename Arg1, typename... Args>
void make_unserialized(std::istream& is, Arg1& arg1, Args... args) {
    is >> arg1;
    make_unserialized(is, args...);
}

template<typename Arg1, typename... Args>
void make_unserialized(std::istream& is, Arg1* arg1, Args... args) {
    is >> *arg1;
    make_unserialized(is, args...);
}

template<typename Arg1, typename... Args>
void make_unserialized(std::istream& is, Arg1** arg1, Args... args) {
    static_assert(!std::is_pointer_v<Arg1>, "Pointer-to-pointer argument not supported");
}

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public ISerializer<RetType(Args...)>
{
public:
    virtual std::ostream& Write(std::ostream& os, Args... args) override {
        make_serialized(os, args...);
        return os;
    }

    virtual std::istream& Read(std::istream& is, Args&... args) override {
        make_unserialized(is, args...);
        return is;
    }
};

class Data
{
public:
    int x;
    int y;
    std::string msg;
};

// Simple serialization of Data. Use any serialization scheme as necessary. 
std::ostream& operator<<(std::ostream& os, const Data& data) {
    os << data.x << endl;
    os << data.y << endl;
    os << data.msg << endl;
    return os;
}

// Simple unserialization of Data. Use any unserialization scheme as necessary. 
std::istream& operator>>(std::istream& is, Data& data) {
    is >> data.x;
    is >> data.y;
    is >> data.msg;
    return is;
}

// Sender is an active object with a thread. The thread sends data to the 
// remote every time the timer expires. 
class Sender
{
public:
    Sender(DelegateRemoteId id) :
        m_thread("Sender"),
        m_sendDelegate(id),
        m_args(ios::in | ios::out | ios::binary)
    {
        // Set the delegate interfaces
        m_sendDelegate.SetStream(&m_args);
        m_sendDelegate.SetSerializer(&m_serializer);
        m_sendDelegate.SetDispatcher(&m_dispatcher);

        m_thread.CreateThread();

        // Start a timer to send data
        m_sendTimer.Expired = MakeDelegate(this, &Sender::Send, m_thread);
        m_sendTimer.Start(std::chrono::milliseconds(50));
    }

    ~Sender()
    {
        m_sendTimer.Stop();
        m_thread.ExitThread();
    }

    // Send data to the remote
    void Send()
    {
        Data data;
        data.x = 1;
        data.y = 2;
        data.msg = "Hello!";

        m_sendDelegate(data);
    }

private:
    Thread m_thread;
    Timer m_sendTimer;

    xostringstream m_args;
    Dispatcher m_dispatcher;
    Serializer<void(Data&)> m_serializer;

    // Sender remote delegate
    DelegateMemberRemote<Sender, void(Data&)> m_sendDelegate;
};

// Receiver receives data from the Sender 
class Receiver
{
public:
    Receiver(DelegateRemoteId id) :
        m_thread("Receiver"),
        m_args(ios::in | ios::out | ios::binary)
    {
        // Set the delegate interfaces
        m_recvDelegate.SetStream(&m_args);
        m_recvDelegate.SetSerializer(&m_serializer);
        m_recvDelegate = MakeDelegate(this, &Receiver::DataUpdate, id);

        m_thread.CreateThread();

        // Start a timer to poll data
        m_recvTimer.Expired = MakeDelegate(this, &Receiver::Poll, m_thread);
        m_recvTimer.Start(std::chrono::milliseconds(50));
    }
    ~Receiver()
    {
        m_thread.ExitThread();
        m_recvDelegate = nullptr;
    }

    // Poll called periodically on m_thread context
    void Poll()
    {
        // Get incoming data
        auto& arg_data = Transport::Receive();

        // Incoming remote delegate data arrived?
        if (!arg_data.str().empty())
        {
            // Invoke the receiver target function with the sender's argument data
            m_recvDelegate.Invoke(arg_data);
        }
    }

    // Receiver target function called when sender remote delegate is invoked
    void DataUpdate(Data& data)
    {
        cout << "Receiver::DataUpdate: " << data.x;
        cout << " " << data.y;
        cout << " " << data.msg;
        cout << endl;
    }

private:
    Thread m_thread;
    Timer m_recvTimer;

    xostringstream m_args;
    Serializer<void(Data&)> m_serializer;

    // Receiver remote delegate
    DelegateMemberRemote<Receiver, void(Data&)> m_recvDelegate;
};

int main() 
{
    DelegateRemoteId id = 1;

    Sender sender(id);
    Receiver receiver(id);

    // Wait here while sender and receiver communicate
    this_thread::sleep_for(chrono::seconds(2));
    return 0;
}