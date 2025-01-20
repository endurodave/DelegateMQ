#ifndef _IINVOKER_H
#define _IINVOKER_H

/// @file
/// @brief Delegate inter-thread invoker base class. 

#include <memory>

namespace DelegateLib {

class DelegateMsg;

/// @brief Abstract base class to support asynchronous delegate function invoke
/// on destination thread of control. 
/// 
/// @details Inherit form this class and implement `Invoke()`. The implementation
/// typically posts a message into the destination thread message queue. The destination
/// thread receives the message and invokes the target bound function. Destination 
/// thread invoke example: 
/// 
/// // Get pointer to DelegateMsg data from queue msg data  
/// `auto delegateMsg = msg->GetData();`
///
/// // Invoke the delegate destination target function  
/// `delegateMsg->GetInvoker()->Invoke(delegateMsg);`
class IThreadInvoker
{
public:
	/// Called to invoke the bound target function by the destination thread of control.
	/// @param[in] msg The incoming delegate message.
	/// @return `true` if function was invoked; `false` if failed. 
	virtual bool Invoke(std::shared_ptr<DelegateMsg> msg) = 0;
};

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