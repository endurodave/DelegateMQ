/// @file
/// @brief Implement the proactor design pattern using delegates.

#include "Proactor.h"
#include "DelegateLib.h"
#include "Thread.h"
#include <iostream>
#include <fstream>
#include <string>
#include <future>
#include <functional>
#include <memory>

using namespace DelegateLib;
using namespace std;

namespace Example
{
    // Abstract completion handler interface
    class CompletionHandler {
    public:
        virtual ~CompletionHandler() = default;
        virtual void handleCompletion(const std::string& result) = 0;
    };

    // Proactor class that initiates asynchronous operations
    class Proactor {
    public:
        Proactor() : m_fileThread("FileThread") { 
            m_fileThread.CreateThread();
        }

        ~Proactor() {
            m_fileThread.ExitThread();
        }

        // Asynchronous file read operation
        void asyncReadFile(const std::string& filename, std::shared_ptr<CompletionHandler> handler) {
            // Simulating an asynchronous file read operation using m_fileThread thread 
            MakeDelegate(this, &Proactor::readFile, m_fileThread).AsyncInvoke(filename, handler);

            // Alternative, but std::async uses a random thread from a thread poll
            //std::async(std::launch::async, &Proactor::readFile, this, filename, handler);
        }

    private:
        // Function to read a file (simulating an I/O operation)
        void readFile(const std::string& filename, std::shared_ptr<CompletionHandler> handler) {
            std::ifstream file(filename);
            if (file.is_open()) {
                std::string content((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
                file.close();

                // Once the operation is complete, call the completion handler
                handler->handleCompletion(content);
            }
            else {
                handler->handleCompletion("Error reading file: " + filename);
            }
        }

        // Private thread
        Thread m_fileThread;
    };

    // Concrete handler class that processes the completion of the operation
    class FileReadHandler : public CompletionHandler {
    public:
        void handleCompletion(const std::string& result) override {
            // Process the result of the asynchronous operation (e.g., print content).
            // Callback invoked from the m_fileThread context.
            std::cout << "File read completed with result: \n" << result << std::endl;
        }
    };

    void ProactorExample() {
        // Create a Proactor instance
        Proactor proactor;

        // Create a handler that will process the file read result
        auto handler = std::make_shared<FileReadHandler>();

        // Initiate an asynchronous file read operation
        proactor.asyncReadFile("example.txt", handler);

        // Simulate doing other work in the main thread
        std::cout << "Main thread doing other work..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));  // Simulating other work
    }
}
