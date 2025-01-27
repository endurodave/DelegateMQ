#include "DelegateLib.h"
#include "UnitTestCommon.h"
#include <iostream>
#include "Thread.h"
#include <random>
#include <chrono>
#include <cstring>

using namespace DelegateLib;
using namespace std;
using namespace UnitTestData;

static Thread workerThread1("DelegateThreads1_UT");
static Thread workerThread2("DelegateThreads2_UT");

static std::mutex m_lock;
static const int LOOPS = 10;
static const int CNT_MAX = 7;
static int callerCnt[CNT_MAX] = { 0 };

static void Wait()
{
    // Wait for tests to complete. Test complete when thread queues empty.
    while (workerThread1.GetQueueSize() != 0 &&
        workerThread2.GetQueueSize() != 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

static std::chrono::milliseconds getRandomTime() 
{
    // Create a random number generator and a uniform distribution
    std::random_device rd;  // Non-deterministic random number generator
    std::mt19937 gen(rd()); // Mersenne Twister engine initialized with rd
    std::uniform_int_distribution<> dis(0, 10); // Uniform distribution between 0 and 10

    // Generate a random number (between 0 and 10 milliseconds)
    int random_ms = dis(gen);

    // Return the random number as a chrono::milliseconds object
    return std::chrono::milliseconds(random_ms);
}

static void FreeThreadSafe(std::chrono::milliseconds delay, int idx)
{
    const std::lock_guard<std::mutex> lock(m_lock);
    callerCnt[idx]++;
    std::this_thread::sleep_for(delay);
}

static std::function<void(std::chrono::milliseconds, int)> LambdaThreadSafe = [](std::chrono::milliseconds delay, int idx)
{
    const std::lock_guard<std::mutex> lock(m_lock);
    callerCnt[idx]++;
    std::this_thread::sleep_for(delay);
};

class TestClass
{
public:
    void MemberThreadSafe(std::chrono::milliseconds delay, int idx)
    {
        const std::lock_guard<std::mutex> lock(m_lock);
        callerCnt[idx]++;
        std::this_thread::sleep_for(delay);
    }
};

static void FreeTests()
{
    std::memset(callerCnt, 0, sizeof(callerCnt));

    auto delegateSync1 = MakeDelegate(&FreeThreadSafe);
    auto delegateSync2 = MakeDelegate(&FreeThreadSafe);
    auto delegateAsync1 = MakeDelegate(&FreeThreadSafe, workerThread1);
    auto delegateAsync2 = MakeDelegate(&FreeThreadSafe, workerThread2);
    auto delegateAsyncWait1 = MakeDelegate(&FreeThreadSafe, workerThread1, std::chrono::milliseconds(500));
    auto delegateAsyncWait2 = MakeDelegate(&FreeThreadSafe, workerThread1, std::chrono::milliseconds(500));

    MulticastDelegateSafe<void(std::chrono::milliseconds, int)> container;
    container += delegateSync1;
    container += delegateSync2;
    container += delegateAsync1;
    container += delegateAsync2;
    container += delegateAsyncWait1;
    container += delegateAsyncWait2;

    int cnt = 0;
    int cnt2 = 0;
    while (cnt++ < LOOPS)
    {
        delegateSync1(getRandomTime(), 0);
        delegateSync2(getRandomTime(), 1);

        auto retVal1 = delegateAsyncWait1.AsyncInvoke(getRandomTime(), 4);
        ASSERT_TRUE(retVal1.has_value());
        auto retVal2 = delegateAsyncWait2.AsyncInvoke(getRandomTime(), 5);
        ASSERT_TRUE(retVal2.has_value());

        while (cnt2++ < 5)
        {
            delegateAsync1(getRandomTime(), 2);
            delegateAsync2(getRandomTime(), 3);
        }
        cnt2 = 0;
        container(getRandomTime(), 6);
    }

    Wait();

    ASSERT_TRUE(callerCnt[0] == LOOPS);
    ASSERT_TRUE(callerCnt[1] == LOOPS);
    ASSERT_TRUE(callerCnt[2] == LOOPS * 5);
    ASSERT_TRUE(callerCnt[3] == LOOPS * 5);
    ASSERT_TRUE(callerCnt[4] == LOOPS);
    ASSERT_TRUE(callerCnt[5] == LOOPS);
    ASSERT_TRUE(callerCnt[6] == LOOPS * 6);
    std::cout << "FreeTests() complete!" << std::endl;
}

static void MemberTests()
{
    std::memset(callerCnt, 0, sizeof(callerCnt));
    TestClass testClass;

    auto delegateSync1 = MakeDelegate(&testClass, &TestClass::MemberThreadSafe);
    auto delegateSync2 = MakeDelegate(&testClass, &TestClass::MemberThreadSafe);
    auto delegateAsync1 = MakeDelegate(&testClass, &TestClass::MemberThreadSafe, workerThread1);
    auto delegateAsync2 = MakeDelegate(&testClass, &TestClass::MemberThreadSafe, workerThread2);
    auto delegateAsyncWait1 = MakeDelegate(&testClass, &TestClass::MemberThreadSafe, workerThread1, std::chrono::milliseconds(500));
    auto delegateAsyncWait2 = MakeDelegate(&testClass, &TestClass::MemberThreadSafe, workerThread1, std::chrono::milliseconds(500));

    MulticastDelegateSafe<void(std::chrono::milliseconds, int)> container;
    container += delegateSync1;
    container += delegateSync2;
    container += delegateAsync1;
    container += delegateAsync2;
    container += delegateAsyncWait1;
    container += delegateAsyncWait2;

    int cnt = 0;
    int cnt2 = 0;
    while (cnt++ < LOOPS)
    {
        delegateSync1(getRandomTime(), 0);
        delegateSync2(getRandomTime(), 1);

        auto retVal1 = delegateAsyncWait1.AsyncInvoke(getRandomTime(), 4);
        ASSERT_TRUE(retVal1.has_value());
        auto retVal2 = delegateAsyncWait2.AsyncInvoke(getRandomTime(), 5);
        ASSERT_TRUE(retVal2.has_value());

        while (cnt2++ < 5)
        {
            delegateAsync1(getRandomTime(), 2);
            delegateAsync2(getRandomTime(), 3);
        }
        cnt2 = 0;
        container(getRandomTime(), 6);
    }

    Wait();

    ASSERT_TRUE(callerCnt[0] == LOOPS);
    ASSERT_TRUE(callerCnt[1] == LOOPS);
    ASSERT_TRUE(callerCnt[2] == LOOPS * 5);
    ASSERT_TRUE(callerCnt[3] == LOOPS * 5);
    ASSERT_TRUE(callerCnt[4] == LOOPS);
    ASSERT_TRUE(callerCnt[5] == LOOPS);
    ASSERT_TRUE(callerCnt[6] == LOOPS * 6);
    std::cout << "MemberTests() complete!" << std::endl;
}

static void MemberSpTests()
{
    std::memset(callerCnt, 0, sizeof(callerCnt));
    auto testClass = std::make_shared<TestClass>();

    auto delegateSync1 = MakeDelegate(testClass, &TestClass::MemberThreadSafe);
    auto delegateSync2 = MakeDelegate(testClass, &TestClass::MemberThreadSafe);
    auto delegateAsync1 = MakeDelegate(testClass, &TestClass::MemberThreadSafe, workerThread1);
    auto delegateAsync2 = MakeDelegate(testClass, &TestClass::MemberThreadSafe, workerThread2);
    auto delegateAsyncWait1 = MakeDelegate(testClass, &TestClass::MemberThreadSafe, workerThread1, std::chrono::milliseconds(500));
    auto delegateAsyncWait2 = MakeDelegate(testClass, &TestClass::MemberThreadSafe, workerThread1, std::chrono::milliseconds(500));

    MulticastDelegateSafe<void(std::chrono::milliseconds, int)> container;
    container += delegateSync1;
    container += delegateSync2;
    container += delegateAsync1;
    container += delegateAsync2;
    container += delegateAsyncWait1;
    container += delegateAsyncWait2;

    int cnt = 0;
    int cnt2 = 0;
    while (cnt++ < LOOPS)
    {
        delegateSync1(getRandomTime(), 0);
        delegateSync2(getRandomTime(), 1);

        auto retVal1 = delegateAsyncWait1.AsyncInvoke(getRandomTime(), 4);
        ASSERT_TRUE(retVal1.has_value());
        auto retVal2 = delegateAsyncWait2.AsyncInvoke(getRandomTime(), 5);
        ASSERT_TRUE(retVal2.has_value());

        while (cnt2++ < 5)
        {
            delegateAsync1(getRandomTime(), 2);
            delegateAsync2(getRandomTime(), 3);
        }
        cnt2 = 0;
        container(getRandomTime(), 6);
    }

    Wait();

    ASSERT_TRUE(callerCnt[0] == LOOPS);
    ASSERT_TRUE(callerCnt[1] == LOOPS);
    ASSERT_TRUE(callerCnt[2] == LOOPS * 5);
    ASSERT_TRUE(callerCnt[3] == LOOPS * 5);
    ASSERT_TRUE(callerCnt[4] == LOOPS);
    ASSERT_TRUE(callerCnt[5] == LOOPS);
    ASSERT_TRUE(callerCnt[6] == LOOPS * 6);
    std::cout << "MemberSpTests() complete!" << std::endl;
}

static void FunctionTests()
{
    std::memset(callerCnt, 0, sizeof(callerCnt));

    auto delegateSync1 = MakeDelegate(LambdaThreadSafe);
    auto delegateSync2 = MakeDelegate(LambdaThreadSafe);
    auto delegateAsync1 = MakeDelegate(LambdaThreadSafe, workerThread1);
    auto delegateAsync2 = MakeDelegate(LambdaThreadSafe, workerThread2);
    auto delegateAsyncWait1 = MakeDelegate(LambdaThreadSafe, workerThread1, std::chrono::milliseconds(500));
    auto delegateAsyncWait2 = MakeDelegate(LambdaThreadSafe, workerThread1, std::chrono::milliseconds(500));

    MulticastDelegateSafe<void(std::chrono::milliseconds, int)> container;
    container += delegateSync1;
    container += delegateSync2;
    container += delegateAsync1;
    container += delegateAsync2;
    container += delegateAsyncWait1;
    container += delegateAsyncWait2;

    int cnt = 0;
    int cnt2 = 0;
    while (cnt++ < LOOPS)
    {
        delegateSync1(getRandomTime(), 0);
        delegateSync2(getRandomTime(), 1);

        auto retVal1 = delegateAsyncWait1.AsyncInvoke(getRandomTime(), 4);
        ASSERT_TRUE(retVal1.has_value());
        auto retVal2 = delegateAsyncWait2.AsyncInvoke(getRandomTime(), 5);
        ASSERT_TRUE(retVal2.has_value());

        while (cnt2++ < 5)
        {
            delegateAsync1(getRandomTime(), 2);
            delegateAsync2(getRandomTime(), 3);
        }
        cnt2 = 0;
        container(getRandomTime(), 6);
    }

    Wait();

    ASSERT_TRUE(callerCnt[0] == LOOPS);
    ASSERT_TRUE(callerCnt[1] == LOOPS);
    ASSERT_TRUE(callerCnt[2] == LOOPS * 5);
    ASSERT_TRUE(callerCnt[3] == LOOPS * 5);
    ASSERT_TRUE(callerCnt[4] == LOOPS);
    ASSERT_TRUE(callerCnt[5] == LOOPS);
    ASSERT_TRUE(callerCnt[6] == LOOPS * 6);
    std::cout << "FunctionTests() complete!" << std::endl;
}

void DelegateThreads_UT()
{
    workerThread1.CreateThread();
    workerThread2.CreateThread();

    FreeTests();
    MemberTests();
    MemberSpTests();
    FunctionTests();

    workerThread1.ExitThread();
    workerThread2.ExitThread();
}