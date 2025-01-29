#ifndef _IINVOKER_H
#define _IINVOKER_H

/// @file
/// @brief Delegate inter-thread invoker base class. 

#include <memory>

namespace DelegateMQ {

class DelegateMsg;

/// @brief Abstract base class to support asynchronous delegate function invoke
/// on destination thread of control. Used internally by the delegate library, 
/// not user application code. 
class IThreadInvoker
{
public:
	/// Called to invoke the bound target function by the destination thread of control.
	/// @param[in] msg The incoming delegate message.
	/// @return `true` if function was invoked; `false` if failed. 
	virtual bool Invoke(std::shared_ptr<DelegateMsg> msg) = 0;
};

/// @brief Abstract base class to support remote delegate function invoke
/// to a remote system. Used internally by the delegate library, not
/// user application code.
class IRemoteInvoker
{
public: 
    /// Called to invoke the bound target function by the remote system. 
    /// @param[in] is The incoming remote message stream. 
    /// @return `true` if function was invoked; `false` if failed. 
    virtual bool Invoke(std::istream& is) = 0;
};

}

#endif