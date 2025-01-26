#ifndef _SYS_DATA_NO_LOCK_H
#define _SYS_DATA_NO_LOCK_H

#include "DelegateLib.h"
#include "SysDataTypes.h"

using namespace DelegateLib;

/// @brief SysData stores common data accessible by any system thread. This class
/// is thread-safe.
class SysDataNoLock
{
public:
	/// Clients register with MulticastDelegateSafe to get callbacks when system mode changes
	MulticastDelegateSafe<void(const SystemModeChanged&)> SystemModeChangedDelegate;

	/// Get singleton instance of this class
	static SysDataNoLock& GetInstance();

	/// Sets the system mode and notify registered clients via SystemModeChangedDelegate.
	/// @param[in] systemMode - the new system mode. 
	void SetSystemMode(SystemMode::Type systemMode);	

	/// Sets the system mode and notify registered clients via a temporary stack created
	/// asynchronous delegate. 
	/// @param[in] systemMode - The new system mode. 
	void SetSystemModeAsyncAPI(SystemMode::Type systemMode);	

	/// Sets the system mode and notify registered clients via a temporary stack created
	/// asynchronous delegate. This version blocks (waits) until the delegate callback
	/// is invoked and returns the previous system mode value. 
	/// @param[in] systemMode - The new system mode. 
	/// @return The previous system mode. 
	SystemMode::Type SetSystemModeAsyncWaitAPI(SystemMode::Type systemMode);

private:
	SysDataNoLock();
	~SysDataNoLock();
	SysDataNoLock(const SysDataNoLock&) = delete;

	/// Private callback to get the SetSystemMode call onto a common thread
	MulticastDelegateSafe<void(SystemMode::Type)> SetSystemModeDelegate; 

	/// Sets the system mode and notify registered clients via SystemModeChangedDelegate.
	/// @param[in] systemMode - the new system mode. 
	void SetSystemModePrivate(SystemMode::Type);	

	/// The current system mode data
	SystemMode::Type m_systemMode;
};

#endif