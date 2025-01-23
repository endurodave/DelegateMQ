/// @file main.cpp
/// @see https://github.com/endurodave/cpp-async-delegate
/// David Lafreniere, 2025.
/// 
/// @brief ZeroMQ with remote delegates examples. 

#include "DelegateLib.h"
#include "WorkerThreadStd.h"
#include "Timer.h"
#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <sstream>

using namespace DelegateLib;
using namespace std;

    // The "transport" is simplely a shared std::stringstream
class Transport
{
public:
    enum class Type 
    {
        SUB,  // Subscriber
        PUB   // Publisher
    };

    int Create(Type type)
    {
        m_zmqContext = zmq_ctx_new();

        if (type == Type::PUB)
        {
            // Create a PUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_PUB);

            // Bind the socket to a TCP port
            int rc = zmq_bind(m_zmq, "tcp://*:5555");
            if (rc != 0) {
                perror("zmq_bind failed");  // TODO
                return 1;
            }
        }
        else
        {
            // Create a SUB socket
            m_zmq = zmq_socket(m_zmqContext, ZMQ_SUB);

            int rc = zmq_connect(m_zmq, "tcp://localhost:5555");
            if (rc != 0) {
                perror("zmq_connect failed");
                return 1;
            }

            // Subscribe to all messages
            zmq_setsockopt(m_zmq, ZMQ_SUBSCRIBE, "", 0);
        }

        // Allow time for subscribers to connect
        this_thread::sleep_for(chrono::seconds(1)); // TODO: move somewhere else
    }

    void Close()
    {
        // Close the subscriber socket and context
        zmq_close(m_zmq);
        m_zmq = nullptr;
        zmq_ctx_destroy(m_zmqContext);
        m_zmqContext = nullptr;
    }

    void Send(std::stringstream& os) {
        std::lock_guard<std::mutex> lk(m_mutex);

        size_t length = os.str().length();
        if (os.bad() || os.fail() || length <= 0)
            return;

        // Send delegate argument data using ZeroMQ
        zmq_send(m_zmq, os.str().c_str(), length, 0);
    }

    std::stringstream Receive() {
        std::lock_guard<std::mutex> lk(m_mutex);

        std::stringstream is(ios::in | ios::out | ios::binary);

        char buffer[256];
        int size = zmq_recv(m_zmq, buffer, 255, 0);
        if (size == -1) {
            return is;
        }

        // Write the received data into the stringstream
        is.write(buffer, size);
        is.seekg(0);  
        return is;
    }

private:
    // TODO: don't use mutex; use async delegate?
    std::mutex m_mutex;

    void* m_zmqContext = nullptr;
    void* m_zmq = nullptr;
};

// Dispatcher sends data to the transport for transmission to the receiver
class Dispatcher : public IDispatcher
{
public:
    Dispatcher() = default;
    ~Dispatcher()
    {
        if (m_transport)
            m_transport->Close();
        m_transport = nullptr;
    }

    void SetTransport(Transport* transport)
    {
        m_transport = transport;
    }

    // Send argument data to the transport
    virtual int Dispatch(std::ostream& os) {
        std::stringstream ss(ios::in | ios::out | ios::binary);
        ss << os.rdbuf();
        if (m_transport)
            m_transport->Send(ss);
        return 0;
    }

private:
    Transport* m_transport = nullptr;   // TODO: use delegate
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
    static_assert(false, "Pointer-to-pointer argument not supported");
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
    static_assert(false, "Pointer-to-pointer argument not supported");
}

template <class R>
struct Serializer; // Not defined

// Serialize all target function argument data
template<class RetType, class... Args>
class Serializer<RetType(Args...)> : public ISerializer<RetType(Args...)>
{
public:
    virtual std::ostream& write(std::ostream& os, Args... args) override {
        make_serialized(os, args...);
        return os;
    }

    virtual std::istream& read(std::istream& is, Args&... args) override {
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

        // Set the transport
        m_transport.Create(Transport::Type::PUB);
        m_dispatcher.SetTransport(&m_transport);
        
        // Create the sender thread
        m_thread.CreateThread();
    }

    ~Sender() { Stop(); }

    void Start()
    {
        // Start a timer to send data
        m_sendTimer.Expired = MakeDelegate(this, &Sender::Send, m_thread);
        m_sendTimer.Start(std::chrono::milliseconds(50));
    }

    void Stop()
    {
        m_sendTimer.Stop();
        m_thread.ExitThread();
        m_transport.Close();
    }

    // Send data to the remote
    void Send()
    {
        Data data;
        data.x = x++;
        data.y = y++;
        data.msg = "Data:";

        m_sendDelegate(data);
    }

private:
    WorkerThread m_thread;
    Timer m_sendTimer;

    xostringstream m_args;
    Dispatcher m_dispatcher;
    Transport m_transport;
    Serializer<void(Data&)> m_serializer;

    // Sender remote delegate
    DelegateMemberRemote<Sender, void(Data&)> m_sendDelegate;

    int x = 0;
    int y = 0;
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

        // Set the transport
        m_transport.Create(Transport::Type::SUB);

        // Create the receiver thread
        m_thread.CreateThread();
    }

    ~Receiver()
    {
        m_thread.ExitThread();
        m_recvDelegate = nullptr;
    }

    void Start()
    {
        // Start a timer to poll data
        m_recvTimer.Expired = MakeDelegate(this, &Receiver::Poll, m_thread);
        m_recvTimer.Start(std::chrono::milliseconds(50));
    }

    void Stop()
    {
        m_recvTimer.Stop();
        m_thread.ExitThread();
        m_recvTimer.Stop();
    }

private:
    // Poll called periodically on m_thread context
    void Poll()
    {
        // Get incoming data
        auto& arg_data = m_transport.Receive();

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
        cout << data.msg << " " << data.y << " " << data.y << endl;
    }

    WorkerThread m_thread;
    Timer m_recvTimer;

    xostringstream m_args;
    Transport m_transport;
    Serializer<void(Data&)> m_serializer;

    // Receiver remote delegate
    DelegateMemberRemote<Receiver, void(Data&)> m_recvDelegate;
};

int main() {
    const DelegateRemoteId id = 1;
    Sender sender(id);
    Receiver receiver(id);

    sender.Start();
    receiver.Start();

    // Let sender and receiver communicate
    this_thread::sleep_for(chrono::seconds(5));

    receiver.Stop();
    sender.Stop();

    return 0;
}