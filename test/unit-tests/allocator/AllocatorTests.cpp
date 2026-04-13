#include "DelegateMQ.h"

#ifdef DMQ_ALLOCATOR

#include "UnitTestCommon.h"
#include "extras/allocator/Allocator.h"
#include "extras/allocator/xallocator.h"
#include "extras/allocator/stl_allocator.h"
#include "extras/allocator/xmake_shared.h"
#include <vector>
#include <map>
#include <list>
#include <set>

using namespace std;

// A class to test DECLARE_ALLOCATOR/IMPLEMENT_ALLOCATOR
class MyClass {
    DECLARE_ALLOCATOR
public:
    int x;
};

// Allocate 10 blocks of MyClass size from the heap
IMPLEMENT_ALLOCATOR(MyClass, 10, NULL)

// A class to test XALLOCATOR
class MyXClass {
    XALLOCATOR
public:
    int x;
};

void AllocatorTests()
{
    const size_t minBlockSize = sizeof(long*);

    // Test Allocator class
    {
        size_t requestSize = 32;
        Allocator allocator(requestSize, 5, NULL, "TestAllocator");
        ASSERT_TRUE(allocator.GetBlockSize() == (requestSize < minBlockSize ? minBlockSize : requestSize));
        ASSERT_TRUE(allocator.GetBlockCount() == 0);
        ASSERT_TRUE(allocator.GetBlocksInUse() == 0);

        void* p1 = allocator.Allocate(requestSize);
        ASSERT_TRUE(p1 != NULL);
        ASSERT_TRUE(allocator.GetBlocksInUse() == 1);
        ASSERT_TRUE(allocator.GetAllocations() == 1);

        void* p2 = allocator.Allocate(requestSize);
        ASSERT_TRUE(p2 != NULL);
        ASSERT_TRUE(allocator.GetBlocksInUse() == 2);

        allocator.Deallocate(p1);
        ASSERT_TRUE(allocator.GetBlocksInUse() == 1);
        ASSERT_TRUE(allocator.GetDeallocations() == 1);

        allocator.Deallocate(p2);
        ASSERT_TRUE(allocator.GetBlocksInUse() == 0);
    }

    // Test AllocatorPool
    {
        AllocatorPool<int, 5> pool;
        size_t expectedSize = sizeof(int) < minBlockSize ? minBlockSize : sizeof(int);
        ASSERT_TRUE(pool.GetBlockSize() == expectedSize);
        
        int* p1 = (int*)pool.Allocate(sizeof(int));
        ASSERT_TRUE(p1 != NULL);
        
        pool.Deallocate(p1);
    }

    // Test DECLARE_ALLOCATOR/IMPLEMENT_ALLOCATOR macros
    {
        MyClass* p1 = new MyClass();
        ASSERT_TRUE(p1 != NULL);
        delete p1;
    }

    // Test xallocator (C API)
    {
        void* p1 = xmalloc(10);
        ASSERT_TRUE(p1 != NULL);
        
        // p1 should handle up to 10 bytes (or block size power of 2)
        // xrealloc to smaller or same size
        void* p2 = xrealloc(p1, 5);
        ASSERT_TRUE(p2 != NULL);
        
        // xrealloc to larger size
        void* p3 = xrealloc(p2, 100);
        ASSERT_TRUE(p3 != NULL);

        // xrealloc(NULL, size) should be like xmalloc
        void* p4 = xrealloc(NULL, 50);
        ASSERT_TRUE(p4 != NULL);

        // xrealloc(p, 0) should be like xfree
        void* p5 = xrealloc(p4, 0);
        ASSERT_TRUE(p5 == NULL);
        
        xfree(p3);
    }

    // Test stl_allocator with list
    {
        std::list<int, stl_allocator<int>> l;
        l.push_back(1);
        l.push_back(2);
        l.push_back(3);
        ASSERT_TRUE(l.size() == 3);
        l.pop_front();
        ASSERT_TRUE(l.size() == 2);
    }

    // Test stl_allocator with set
    {
        std::set<int, std::less<int>, stl_allocator<int>> s;
        s.insert(10);
        s.insert(20);
        ASSERT_TRUE(s.size() == 2);
    }

    // Test XALLOCATOR macro
    {
        MyXClass* p1 = new MyXClass();
        ASSERT_TRUE(p1 != NULL);
        delete p1;
    }

    // Test stl_allocator with vector
    {
        std::vector<int, stl_allocator<int>> v;
        v.push_back(1);
        v.push_back(2);
        ASSERT_TRUE(v.size() == 2);
        v.clear();
    }

    // Test stl_allocator with map
    {
        std::map<int, int, std::less<int>, stl_allocator<std::pair<const int, int>>> m;
        m[1] = 10;
        m[2] = 20;
        ASSERT_TRUE(m.size() == 2);
    }

    // Test xmake_shared
    {
        auto sp = xmake_shared<int>(123);
        ASSERT_TRUE(sp != NULL);
        ASSERT_TRUE(*sp == 123);
    }
}

#endif
