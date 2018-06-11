/************************************************************************************

Filename    :   Settings.h
Content     :
Created     :	7/22/2015
Authors     :   Michael Grosse Huelsewiesche

Copyright   :   Copyright 2015 VRMatter All Rights reserved.

This source code is licensed under the GPL license found in the
LICENSE file in the StreamTheater/ directory.

*************************************************************************************/

#if !defined( GenericSettings_h )
#define GenericSettings_h

#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_JSON.h"

namespace OculusCinema {

using namespace OVR;

class Settings
{
public:
	static constexpr double SETTINGS_VERSION = 1.0;
	Settings();
	Settings(const char* filename);
	~Settings();

	void OpenOrCreate(const char* filename);

	// Copy the variable definitions from Settings that are already initialized
	// Allows for multiple levels of saveable settings, e.g. defaults and theaters
	void CopyDefines(const Settings& source);

	// Associates a pointer to a variable with a name to save and load the value
	// Please make sure all variables outlive the Settings object!
	template<typename T>
	void Define(const char* varName, T* ptr);

	// Get a variable's value from the settings file
	// Doesn't need to be defined, but call Load() first
	// returns true if the variable exists, false if missing, wrong type, or unloaded
	// (slow, please avoid if possible, C-strings are duplicated so remember to clean up)
	template<typename T>
	bool GetVal(const char* varName, T* toSet);

	bool IsChanged();

	// Remove a variable from the saved settings file
	// (automatically saved with no other changes)
	void DeleteVar(const char* varName);

	// Load any defined values into the specified variables
	void Load();

	// Save all values to settings
	void SaveAll();

	// Only add values if they're different from when they were defined or the last Load()
	void SaveChanged();

	// Update or create only the values of these variables
	void SaveOnly(const Array<const char*> &varNames);

	// Save out any currently defined variable names if they don't exist but without data
	// Allows users to edit variables that might not be changeable, without forcing a default value
	void SaveVarNames();

private:
	class IVariable;
	template<typename T> class Variable;

private:
	char* settingsFileName;
	JSON* rootSettingsJSON;
	JSON* settingsJSON;
	Array<IVariable*> variables;

};

#ifndef NDEBUG
void SettingsTest(String packageName);
#endif

} // namespace VRMatterStreamTheater

#endif