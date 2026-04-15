#include "DelegateMQ.h"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

#if defined(DMQ_DATABUS)

// Drive Timer::ProcessTimers() from a background thread for the duration of a test.
// Returns a stop function; call it to shut the driver down before leaving the scope.
static std::function<void()> StartTimerDriver()
{
    auto running = std::make_shared<std::atomic<bool>>(true);
    std::thread([running]() {
        while (running->load()) {
            Timer::ProcessTimers();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }).detach();

    return [running]() { running->store(false); };
}

int DataBusDeadlineTestMain()
{
    std::cout << "Starting DataBusDeadlineTest..." << std::endl;

    // 1. Normal delivery: continuous publishes keep the timer reset; onMissed never fires
    {
        dmq::DataBus::ResetForTesting();
        auto stopDriver = StartTimerDriver();

        std::atomic<int> deliveryCount{0};
        std::atomic<int> missedCount{0};

        dmq::DeadlineSubscription<int> sub{
            "deadline/topic",
            std::chrono::milliseconds(100),
            [&](const int&) { deliveryCount++; },
            [&]()           { missedCount++; }
        };

        // Publish well within the deadline window
        for (int i = 0; i < 5; i++) {
            dmq::DataBus::Publish<int>("deadline/topic", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        ASSERT_TRUE(deliveryCount == 5);
        ASSERT_TRUE(missedCount == 0);

        stopDriver();
    }

    // 2. Deadline miss: publishes stop; onMissed fires after the window elapses
    {
        dmq::DataBus::ResetForTesting();
        auto stopDriver = StartTimerDriver();

        std::atomic<int> missedCount{0};

        dmq::DeadlineSubscription<int> sub{
            "deadline/topic",
            std::chrono::milliseconds(50),
            [](const int&) {},
            [&]() { missedCount++; }
        };

        // One delivery resets the timer, then go silent
        dmq::DataBus::Publish<int>("deadline/topic", 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(120));

        ASSERT_TRUE(missedCount >= 1);

        stopDriver();
    }

    // 3. Initial arm: onMissed fires even if Publish is never called
    {
        dmq::DataBus::ResetForTesting();
        auto stopDriver = StartTimerDriver();

        std::atomic<int> missedCount{0};

        dmq::DeadlineSubscription<int> sub{
            "deadline/topic",
            std::chrono::milliseconds(50),
            [](const int&) {},
            [&]() { missedCount++; }
        };

        // No publish at all
        std::this_thread::sleep_for(std::chrono::milliseconds(120));

        ASSERT_TRUE(missedCount >= 1);

        stopDriver();
    }

    // 4. Recovery: after a miss, resuming publishes resets the timer and stops further misses
    {
        dmq::DataBus::ResetForTesting();
        auto stopDriver = StartTimerDriver();

        std::atomic<int> missedCount{0};

        dmq::DeadlineSubscription<int> sub{
            "deadline/topic",
            std::chrono::milliseconds(50),
            [](const int&) {},
            [&]() { missedCount++; }
        };

        // Let it miss once
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        int missesAfterSilence = missedCount.load();
        ASSERT_TRUE(missesAfterSilence >= 1);

        // Resume publishing — resets the timer each time
        for (int i = 0; i < 4; i++) {
            dmq::DataBus::Publish<int>("deadline/topic", i);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        int missesAfterRecovery = missedCount.load();
        ASSERT_TRUE(missesAfterRecovery == missesAfterSilence); // no new misses

        stopDriver();
    }

    // 5. Cleanup: onMissed does not fire after DeadlineSubscription is destroyed
    {
        dmq::DataBus::ResetForTesting();
        auto stopDriver = StartTimerDriver();

        std::atomic<int> missedCount{0};

        {
            dmq::DeadlineSubscription<int> sub{
                "deadline/topic",
                std::chrono::milliseconds(50),
                [](const int&) {},
                [&]() { missedCount++; }
            };
            // Let it arm but destroy before deadline fires
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } // sub destroyed: DataBus disconnected, timer stopped

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
        ASSERT_TRUE(missedCount == 0);

        stopDriver();
    }

    // 6. Async thread dispatch: both data and onMissed callbacks arrive on the worker thread
    {
        dmq::DataBus::ResetForTesting();
        auto stopDriver = StartTimerDriver();

        Thread workerThread("DeadlineWorker");
        workerThread.CreateThread();

        auto mainId = Thread::GetCurrentThreadId();
        std::atomic<bool> dataOnWorker{false};
        std::atomic<bool> missedOnWorker{false};

        dmq::DeadlineSubscription<int> sub{
            "deadline/topic",
            std::chrono::milliseconds(50),
            [&](const int&) {
                dataOnWorker = (Thread::GetCurrentThreadId() == workerThread.GetThreadId());
            },
            [&]() {
                missedOnWorker = (Thread::GetCurrentThreadId() == workerThread.GetThreadId());
            },
            &workerThread
        };

        dmq::DataBus::Publish<int>("deadline/topic", 42);

        // Wait for both callbacks to arrive on the worker thread
        int retries = 0;
        while ((!dataOnWorker || !missedOnWorker) && retries++ < 40)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

        ASSERT_TRUE(dataOnWorker == true);
        ASSERT_TRUE(missedOnWorker == true);

        workerThread.ExitThread();
        stopDriver();
    }

    std::cout << "DataBusDeadlineTest PASSED!" << std::endl;
    return 0;
}

#else

int DataBusDeadlineTestMain() { return 0; }

#endif
