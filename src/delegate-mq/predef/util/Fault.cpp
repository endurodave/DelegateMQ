#include "Fault.h"
#include <assert.h>
#include <iostream>
#include "DelegateMQ.h"

#if WIN32
	#include "windows.h"
#endif

using namespace std;

//----------------------------------------------------------------------------
// FaultHandler
//----------------------------------------------------------------------------
void FaultHandler(const char* file, unsigned short line)
{
    // @TODO: Update fault handling if necessary. 
    
    // 1. PRINT FIRST (Flush to ensure it appears in CI logs)
    cout << "FaultHandler called. Application terminated." << endl;
    cout << "File: " << file << " Line: " << line << endl;
    LOG_ERROR("FaultHandler File={} Line={}", file, line);

    // 2. Break only if interactive or specifically desired
#if WIN32
    // Optional: Only break if a debugger is actually present to avoid CI crashes
    if (IsDebuggerPresent()) {
        DebugBreak();
    }
#endif

    // 3. Force exit
    abort(); // Better than assert(0) for a clean exit code
}
