/// @file
/// @brief Implements a complete Remote Delegate example using a mock transport layer.
///
/// @details
/// This example demonstrates the current DelegateMQ remote delegate pattern using
/// `RemoteChannel<Sig>` to send data between two active objects (`Sender` and `Receiver`)
/// running on separate threads.
///
/// **Key Components:**
/// * **MockTransport:** Implements `ITransport` — `Send()` stores the serialized payload
///   and `DmqHeader` in a shared buffer; `Receive()` retrieves them for the polling side.
/// * **Data:** User-defined type inheriting `serialize::I` — binary serialization
///   handled by `msg_serialize` via the predefined `Serializer<>`.
/// * **RemoteChannel<Sig>:** Single setup object per message signature — owns the
///   `Dispatcher`, stream, and serializer. `Bind()` wires everything in one call.
/// * **Active Objects:** Both `Sender` and `Receiver` run on their own `Thread` and
///   use `Timer` objects to drive periodic sending and polling.

#include "RemoteCommunication.h"
#include "DelegateMQ.h"
#include <iostream>
#include <mutex>
#include <chrono>

using namespace dmq;
using namespace dmq::os;
using namespace dmq::util;
using namespace dmq::transport;
using namespace std;

namespace Example
{
    // Data transferred between Sender and Receiver.
    // Inherits serialize::I so msg_serialize can binary-serialize it via Serializer<>.
    class Data : public serialize::I
    {
    public:
        int x = 0;
        int y = 0;
        std::string msg;

        virtual std::ostream& write(serialize& ms, std::ostream& os) override {
            ms.write(os, x);
            ms.write(os, y);
            ms.write(os, msg);
            return os;
        }

        virtual std::istream& read(serialize& ms, std::istream& is) override {
            ms.read(is, x);
            ms.read(is, y);
            ms.read(is, msg);
            return is;
        }
    };

    // Mock transport implementing ITransport.
    // Send() stores the payload + header in a shared buffer (thread-safe).
    // Receive() retrieves the stored payload and header, returning -1 if none available.
    class MockTransport : public ITransport
    {
    public:
        int Send(xostringstream& os, const DmqHeader& header) override {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_payload = os.str();
            m_header = header;
            m_hasData = true;
            return 0;
        }

        int Receive(xstringstream& is, DmqHeader& header) override {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (!m_hasData)
                return -1;
            is.str(m_payload);
            header = m_header;
            m_hasData = false;
            return 0;
        }

    private:
        std::mutex m_mutex;
        xstring m_payload;
        DmqHeader m_header;
        bool m_hasData = false;
    };

    // Sender is an active object that periodically sends Data to the remote.
    class Sender
    {
    public:
        Sender(MockTransport& transport, DelegateRemoteId id)
            : m_thread("Sender")
            , m_channel(transport, m_serializer)
        {
            // Sender-side bind: no-op target (only the signature matters for dispatch)
            // Bind a raw lambda (no std::function wrapper needed)
            m_channel.Bind([](Data msg) { std::cout << "Remote lambda data: " << msg.x << std::endl; }, id);

            m_thread.CreateThread();

            m_sendTimerConn = m_sendTimer.OnExpired.Connect(MakeDelegate(this, &Sender::Send, m_thread));
            m_sendTimer.Start(std::chrono::milliseconds(100));
        }

        ~Sender()
        {
            m_sendTimer.Stop();
            m_thread.ExitThread();
        }

        // Invoked on m_thread: serialize Data and dispatch to the remote
        void Send()
        {
            static int pos = 0;
            Data data;
            data.x = pos++;
            data.y = pos++;
            data.msg = "Hello";
            m_channel(data);
        }

    private:
        Thread m_thread;
        Timer m_sendTimer;
        dmq::ScopedConnection m_sendTimerConn;

        dmq::serialization::serializer::Serializer<void(Data)> m_serializer;
        dmq::RemoteChannel<void(Data)> m_channel;
    };

    // Receiver is an active object that polls the transport and dispatches incoming data.
    class Receiver
    {
    public:
        Receiver(MockTransport& transport, DelegateRemoteId id)
            : m_thread("Receiver")
            , m_transport(transport)
            , m_channel(transport, m_serializer)
        {
            // Receiver-side bind: wire DataUpdate as the remote target function
            m_channel.Bind(this, &Receiver::DataUpdate, id);

            m_thread.CreateThread();

            m_recvTimerConn = m_recvTimer.OnExpired.Connect(MakeDelegate(this, &Receiver::Poll, m_thread));
            m_recvTimer.Start(std::chrono::milliseconds(50));
        }

        ~Receiver()
        {
            m_recvTimer.Stop();
            m_thread.ExitThread();
        }

        // Invoked on m_thread: poll transport, deserialize, and invoke DataUpdate
        void Poll()
        {
            DmqHeader header;
            xstringstream is(std::ios::in | std::ios::out | std::ios::binary);
            if (m_transport.Receive(is, header) == 0)
                m_channel.GetEndpoint()->Invoke(is);
        }

        // Remote target function: called when Sender dispatches a Data value
        void DataUpdate(Data data)
        {
            cout << "Receiver::DataUpdate: " << data.x
                 << " " << data.y
                 << " " << data.msg << endl;
        }

    private:
        Thread m_thread;
        Timer m_recvTimer;
        dmq::ScopedConnection m_recvTimerConn;

        MockTransport& m_transport;
        dmq::serialization::serializer::Serializer<void(Data)> m_serializer;
        dmq::RemoteChannel<void(Data)> m_channel;
    };

    void RemoteCommunicationExample()
    {
        DelegateRemoteId id = 1;

        MockTransport transport;
        Sender sender(transport, id);
        Receiver receiver(transport, id);

        // Wait while sender and receiver communicate
        this_thread::sleep_for(chrono::seconds(2));
    }
}
