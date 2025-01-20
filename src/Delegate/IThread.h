#ifndef _ITHREAD_H
#define _ITHREAD_H

#include "DelegateMsg.h"

namespace DelegateLib {

/// @file
/// @brief A base class for a delegate enabled execution thread. 
/// 
/// @details Each platform specific implementation must inherit from IThread
/// and provide an implementation for `DispatchDelegate()`. The `DispatchDelegate()`
/// function is called by the source thread to initiate an asynchronous function call
/// onto the destination thread of control.
class IThread
{
public:
	/// Destructor
	virtual ~IThread() = default;

	/// Dispatch a DelegateMsg onto this thread. The implementer is responsible
	/// for getting the DelegateMsg into an OS message queue. Once DelegateMsg
	/// is on the correct thread of control, the DelegateInvoker::DelegateInvoke() function
	/// must be called to execute the callback. 
	/// @param[in] msg - a pointer to the callback message that must be created dynamically.
	/// @pre Caller *must* create the DelegateMsg argument dynamically.
	/// @post The destination thread calls DelegateInvoke().
	virtual void DispatchDelegate(std::shared_ptr<DelegateMsg> msg) = 0;
};

}

#endif
