#ifndef _SYS_DATA_TYPES_H
#define _SYS_DATA_TYPES_H

struct SystemMode
{
	enum Type
	{
		STARTING,
		NORMAL,
		SERVICE,
		SYS_INOP
	};
};

/// @brief Structure to hold system mode callback data. 
class SystemModeChanged
{
public:
	SystemModeChanged() : 
		PreviousSystemMode(SystemMode::STARTING), 
		CurrentSystemMode(SystemMode::STARTING)
	{
	}

	SystemMode::Type PreviousSystemMode;
	SystemMode::Type CurrentSystemMode;
};

#endif