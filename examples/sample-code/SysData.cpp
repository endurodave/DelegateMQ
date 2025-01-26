#include "SysData.h"

//----------------------------------------------------------------------------
// GetInstance
//----------------------------------------------------------------------------
SysData& SysData::GetInstance()
{
	static SysData instance;
	return instance;
}

//----------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------
SysData::SysData() :
	m_systemMode(SystemMode::STARTING)
{
}

//----------------------------------------------------------------------------
// Destructor
//----------------------------------------------------------------------------
SysData::~SysData()
{
}

//----------------------------------------------------------------------------
// SetSystemMode
//----------------------------------------------------------------------------
void SysData::SetSystemMode(SystemMode::Type systemMode)
{
	const std::lock_guard<std::mutex> lock(m_lock);

	// Create the callback data
	SystemModeChanged callbackData;
	callbackData.PreviousSystemMode = m_systemMode;
	callbackData.CurrentSystemMode = systemMode;

	// Update the system mode
	m_systemMode = systemMode;

	// Callback all registered subscribers
	SystemModeChangedDelegate(callbackData);
}