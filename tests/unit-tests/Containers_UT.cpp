#include "DelegateLib.h"
#include "UnitTestCommon.h"
#include <iostream>
#include <set>
#include <cstring>
#include "Thread.h"

using namespace DelegateLib;
using namespace std;
using namespace UnitTestData;

static int lambda1Int = 0;
static int lambda2Int = 0;
static int funcInt = 0;
static int classInt = 0;

void TFreeFunc(int i) { funcInt = i; }

class TClass
{
public:
    void Func(int i) { classInt = i; }
    void Func2(int i) { classInt = i+1; }
};

static void UnicastDelegateTests()
{
    UnicastDelegate<void(int)> src;
    src = MakeDelegate(FreeFuncInt1);
    ASSERT_TRUE(src.Size() == 1);
    auto dest = std::move(src);
    ASSERT_TRUE(src.Size() == 0);
    ASSERT_TRUE(dest.Size() == 1);

    UnicastDelegate<void(int)> src2(dest);
    ASSERT_TRUE(src2.Size() == 1);
    ASSERT_TRUE(dest.Size() == 1);

    dest.Clear();
    src = MakeDelegate(&FreeFuncInt1);
    ASSERT_TRUE(src.Size() == 1);
    dest = std::move(src);
    ASSERT_TRUE(src.Size() == 0);
    ASSERT_TRUE(dest.Size() == 1);

    src = dest;
    ASSERT_TRUE(src.Size() == 1);
    ASSERT_TRUE(dest.Size() == 1);

    dest.Clear();
    ASSERT_TRUE(dest.Size() == 0);

    src = MakeDelegate(&FreeFuncInt1);
    ASSERT_TRUE(src.Size() == 1);
    src = nullptr;
    ASSERT_TRUE(src.Size() == 0);

    dest.Clear();
    src = MakeDelegate(&FreeFuncInt1);
    dest = src;
    ASSERT_TRUE(src.Size() == 1);
    ASSERT_TRUE(dest.Size() == 1);

    // Invoke target
    src(TEST_INT);
    src.Broadcast(TEST_INT);
}

static void MulticastDelegateTests()
{
    MulticastDelegate<void(int)> src;
    src += MakeDelegate(&FreeFuncInt1);
    ASSERT_TRUE(src.Size() == 1);
    auto dest = std::move(src);
    ASSERT_TRUE(src.Size() == 0);
    ASSERT_TRUE(dest.Size() == 1);

    MulticastDelegate<void(int)> src2(dest);
    ASSERT_TRUE(src2.Size() == 1);
    ASSERT_TRUE(dest.Size() == 1);

    dest.Clear();
    src += MakeDelegate(&FreeFuncInt1);
    dest = std::move(src);
    ASSERT_TRUE(src.Size() == 0);
    ASSERT_TRUE(dest.Size() == 1);
    
    src = dest;
    ASSERT_TRUE(src.Size() == 1);
    ASSERT_TRUE(dest.Size() == 1);

    dest.Clear();
    ASSERT_TRUE(dest.Size() == 0);

    src += MakeDelegate(&FreeFuncInt1);
    ASSERT_TRUE(src.Size() == 2);
    src = nullptr;
    ASSERT_TRUE(src.Size() == 0);

    dest.Clear();
    src.PushBack(MakeDelegate(&FreeFuncInt1));
    dest = src;
    ASSERT_TRUE(src.Size() == 1);
    ASSERT_TRUE(dest.Size() == 1);

    src += DelegateFree<void(int)>();
    src += DelegateFree<void(int)>();
    ASSERT_TRUE(src.Size() == 3);
    src.Remove(DelegateFree<void(int)>());
    ASSERT_TRUE(src.Size() == 2);

    // Invoke all targets
    src(TEST_INT);
    src.Broadcast(TEST_INT);

    TClass testClass;
    std::function lambda1 = [](int i) { lambda1Int = i; };
    std::function lambda2 = [](int i) { lambda2Int = i; };

    funcInt = 0, classInt = 0, lambda1Int = 0, lambda2Int = 0;
    src.Clear();
    src += MakeDelegate(&TFreeFunc);
    src += MakeDelegate(&testClass, &TClass::Func);
    src += MakeDelegate(lambda1);
    src += MakeDelegate(lambda2);
    src -= MakeDelegate(lambda1);
    src(TEST_INT);
    ASSERT_TRUE(funcInt == TEST_INT);
    ASSERT_TRUE(classInt == TEST_INT);
    ASSERT_TRUE(lambda1Int == 0);
    ASSERT_TRUE(lambda2Int == TEST_INT);
    ASSERT_TRUE(src.Size() == 3);

    funcInt = 0, classInt = 0, lambda1Int = 0, lambda2Int = 0;
    src.Clear();
    src += MakeDelegate(&TFreeFunc);
    src += MakeDelegate(&testClass, &TClass::Func);
    src += MakeDelegate(lambda1);
    src += MakeDelegate(lambda2);
    src -= MakeDelegate(lambda2);
    src(TEST_INT);
    ASSERT_TRUE(funcInt == TEST_INT);
    ASSERT_TRUE(classInt == TEST_INT);
    ASSERT_TRUE(lambda1Int == TEST_INT);
    ASSERT_TRUE(lambda2Int == 0);
    ASSERT_TRUE(src.Size() == 3);

    funcInt = 0, classInt = 0, lambda1Int = 0, lambda2Int = 0;
    src.Clear();
    src += MakeDelegate(&TFreeFunc);
    src += MakeDelegate(&testClass, &TClass::Func);
    src += MakeDelegate(lambda1);
    src += MakeDelegate(lambda2);
    src -= MakeDelegate(&testClass, &TClass::Func);
    src(TEST_INT);
    src -= MakeDelegate(&testClass, &TClass::Func2);  // Nothing to remove; not registered
    ASSERT_TRUE(src.Size() == 3);
    ASSERT_TRUE(funcInt == TEST_INT);
    ASSERT_TRUE(classInt == 0);
    ASSERT_TRUE(lambda1Int == TEST_INT);
    ASSERT_TRUE(lambda2Int == TEST_INT);
    ASSERT_TRUE(src.Size() == 3);

    funcInt = 0, classInt = 0, lambda1Int = 0, lambda2Int = 0;
    src.Clear();
    src += MakeDelegate(&TFreeFunc);
    src += MakeDelegate(&testClass, &TClass::Func);
    src += MakeDelegate(lambda1);
    src += MakeDelegate(lambda2);
    src -= MakeDelegate(&TFreeFunc);
    src(TEST_INT);
    ASSERT_TRUE(funcInt == 0);
    ASSERT_TRUE(classInt == TEST_INT);
    ASSERT_TRUE(lambda1Int == TEST_INT);
    ASSERT_TRUE(lambda2Int == TEST_INT);
    ASSERT_TRUE(src.Size() == 3);

    src -= MakeDelegate(&FreeFuncInt1);
    ASSERT_TRUE(src.Size() == 3);

#if 0
    // Example shows std::function target limitations. Not a normal usage case.
    // Use MakeDelegate() to create delegates works correctly with delegate 
    // containers.
    src.Clear();
    TClass t1, t2;
    std::function<void(int)> f1 = std::bind(&TClass::Func, &t1, std::placeholders::_1);
    std::function<void(int)> f2 = std::bind(&TClass::Func2, &t2, std::placeholders::_1);
    src += MakeDelegate(f1);
    src += MakeDelegate(f2);
    src -= MakeDelegate(f2);   // Should remove f2, not f1!
    src(TEST_INT);
    ASSERT_TRUE(classInt == TEST_INT);
#endif
}

static void MulticastDelegateSafeTests()
{
    MulticastDelegateSafe<void(int)> src;
    MulticastDelegateSafe<void(int)> dest;
    src += MakeDelegate(&FreeFuncInt1);
    ASSERT_TRUE(src.Size() == 1);
    ASSERT_TRUE(dest.Size() == 0);

    src.Clear();
    dest.Clear();
    src += MakeDelegate(&FreeFuncInt1);
    dest = std::move(src);
    ASSERT_TRUE(src.Size() == 0);
    ASSERT_TRUE(dest.Size() == 1);

    src = dest;
    ASSERT_TRUE(src.Size() == 1);
    ASSERT_TRUE(dest.Size() == 1);

    dest.Clear();
    ASSERT_TRUE(dest.Size() == 0);

    src += MakeDelegate(&FreeFuncInt1);
    ASSERT_TRUE(src.Size() == 2);
    src = nullptr;
    ASSERT_TRUE(src.Size() == 0);

    dest.Clear();
    src.PushBack(MakeDelegate(&FreeFuncInt1));
    dest = src;
    ASSERT_TRUE(src.Size() == 1);
    ASSERT_TRUE(dest.Size() == 1);

    src += DelegateFree<void(int)>();
    src += DelegateFree<void(int)>();
    ASSERT_TRUE(src.Size() == 3);
    src.Remove(DelegateFree<void(int)>());
    ASSERT_TRUE(src.Size() == 2);

    // Invoke all targets
    src(TEST_INT);
    src.Broadcast(TEST_INT);
}

void Containers_UT()
{
    UnicastDelegateTests();
    MulticastDelegateTests();
    MulticastDelegateSafeTests();
}