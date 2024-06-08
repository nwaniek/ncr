#pragma once

// some debugging flags
#define DEBUG			true
#define	DEBUG_VERBOSE	false

#if DEBUG_VERBOSE
	#define NCR_ENABLE_LOG_LEVEL_VERBOSE
#elif DEBUG
	#define NCR_ENABLE_LOG_LEVEL_DEBUG
#else
	#define NCR_ENABLE_LOG_LEVEL_WARNING
#endif

// we want to allow empty final states
#define NCR_DFA_ALLOW_EMPTY_FINAL_STATE_SET true

// we want to have HDF5 functionality in NCR's cvar system
// #define NCR_CVAR_ENABLE_HDF5
