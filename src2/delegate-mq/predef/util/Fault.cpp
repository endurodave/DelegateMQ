#include "Fault.h"
#include <assert.h>
#include <iostream>
#if WIN32
	#include "windows.h"
#endif

using namespace std;

//----------------------------------------------------------------------------
// FaultHandler
//----------------------------------------------------------------------------
void FaultHandler(const char* file, unsigned short line)
{
#if WIN32
	// If you hit this line, it means one of the ASSERT macros failed.
    DebugBreak();
#endif

    cout << "FaultHandler called. Application terminated." << endl;
    cout << "File: " << file << " Line: " << line << endl;

	assert(0);
}
