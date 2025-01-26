#ifndef _SYS_DATA_H
#define _SYS_DATA_H

#include "DelegateLib.h"
#include <mutex>
#include "SysDataTypes.h"

using namespace DelegateLib;

/// @brief SysData stores common data accessible by any system thread. This class
/// is thread-safe.
class SysData
{
public:
	/// Clients register with MulticastDelegateSafe to get callbacks when system mode changes
    MulticastDelegateSafe<void(const SystemModeChanged&)> SystemModeChangedDelegate;

	/// Get singleton instance of this class
	static SysData& GetInstance();

	/// Sets the system mode and notify registered clients via SystemModeChangedDelegate.
	/// @param[in] systemMode - the new system mode. 
	void SetSystemMode(SystemMode::Type systemMode);	

private:
	SysData();
	~SysData();

	/// The current system mode data
	SystemMode::Type m_systemMode;

	/// Lock to make the class thread-safe
	std::mutex m_lock;
};

#endif