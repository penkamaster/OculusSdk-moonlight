/************************************************************************************

+Filename    :   Settings.cpp
Content     :
Created     :	7/22/2015
Authors     :   Michael Grosse Huelsewiesche

Copyright   :   Copyright 2015 VRMatter All Rights reserved.

This source code is licensed under the GPL license found in the
LICENSE file in the StreamTheater/ directory.

*************************************************************************************/

/*#include "Kernel/OVR_String_Utils.h"

#include "CinemaApp.h"
#include "Native.h"
#include "SceneManager.h"
#include "GuiSys.h"*/

#include "Settings.h"

#include <fstream>
#include <string>
#include <Kernel/OVR_LogUtils.h>

#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_String.h"

namespace OculusCinema
{

void PrintFileToLog(const char* filename)
{
    std::ifstream file(filename);
    std::string str;
    LOG("%s ----------------------", filename);
    while (std::getline(file, str))
    {
        LOG("%s",str.c_str());
    }
    LOG("END ---------------------");
}

/*
 * Variable holder
 */
class Settings::IVariable {
public:
	virtual ~IVariable() { free(name); name = NULL; if(json) json->Release(); }
	virtual IVariable* Clone() = 0;
	// The three basic types of JSON values, since we don't know what type we're storing in this instance
	// Could have template<typename T> Load(T), but this way we don't really have to save type info to json
	virtual void LoadNumber(double num) = 0;
	virtual void LoadCStr(const char* str) = 0;
	virtual void LoadBool(bool b) = 0;
	virtual JSON* Serialize() = 0;
	virtual bool IsChanged() = 0;
	virtual void SaveValue() = 0;

public:
	char* name;
	JSON* json;
};


template<typename T>
class Settings::Variable: public Settings::IVariable
{
public:
	Variable(): initialValue(0) { name = NULL; json = NULL;}
	Variable(const char* varName, T* ptr)
	{
		name = strdup(varName);
		json = NULL;
		varPtr = ptr;
		initialValue = *ptr;
	}
	virtual ~Variable(){ free(name); name = NULL; if(json) json->Release(); }
	virtual IVariable* Clone()
	{
		Variable<T>* newVar = new Variable<T>();
		newVar->name = strdup(name);
		newVar->varPtr = varPtr;
		newVar->initialValue = initialValue;
		return newVar;
	}
	virtual void LoadNumber(double num) { initialValue = (T)num; *varPtr = (T)num; }
	virtual void LoadCStr(const char* str) { LOG("Loaded the wrong type! %s is not text.", name);}
	virtual void LoadBool(bool b) { initialValue = (T)b; *varPtr = (T)b;}
	virtual JSON* Serialize()
	{
		JSON* newJSON = JSON::CreateNumber((double)(*varPtr));
		newJSON->Name = name;
		json = NULL; // can't be sure we'll be pointing to the right thing soon
		return newJSON;
	}
	virtual bool IsChanged() { return ( *varPtr != initialValue ); }
	virtual void SaveValue() { initialValue = *varPtr; }

public:
	T* varPtr;
	T initialValue;
	typedef T type;
};

// Specialization for copying value of a c-string
template<>
class Settings::Variable<char*>: public Settings::IVariable
{
public:
	Variable(): initialValue(NULL) { name = NULL; json = NULL;}
	Variable(const char* varName, char** ptr)
	{
		name = strdup(varName);
		json = NULL;
		varPtr = ptr;
		initialValue = strdup(*ptr);
	}
	virtual ~Variable() { free(name); name = NULL; free(initialValue); if(json) json->Release(); }
	virtual IVariable* Clone()
	{
		Variable<char*>* newVar = new Variable<char*>();
		newVar->name = strdup(name);
		newVar->varPtr = varPtr;
		newVar->initialValue = strdup(initialValue);
		return newVar;
	}
	virtual void LoadNumber(double num) {LOG("Loaded the wrong type! %s is text.", name);}
	virtual void LoadCStr(const char* str)
	{
		if(initialValue)
		{
			free(initialValue);
		}
		initialValue = strdup(str);
		if(*varPtr)
		{
			free(*varPtr);
		}
		*varPtr = strdup(str);
	}
	virtual void LoadBool(bool b) { LOG("Loaded the wrong type! %s is text.", name);}
	virtual JSON* Serialize()
	{
		JSON* newJSON = JSON::CreateString(*varPtr);
		newJSON->Name = name;
		json = NULL; // can't be sure we'll be pointing to the right thing soon
		return newJSON;
	}
	virtual bool IsChanged() { return 0 != strcmp(*varPtr, initialValue); }
	virtual void SaveValue() {
		if(initialValue)
		{
			free(initialValue);
		}
		initialValue = strdup(*varPtr);
	}

public:
	char** varPtr;
	char* initialValue;
	typedef char* type;
};

// Specialization for OVR_Strings
template<>
class Settings::Variable<String>: public Settings::IVariable
{
public:
	Variable(): initialValue("") { name = NULL; json = NULL;}
	Variable(const char* varName, String* ptr)
	{
		name = strdup(varName);
		json = NULL;
		varPtr = ptr;
		initialValue = *ptr;
	}
	virtual ~Variable(){ free(name); name = NULL; if(json) json->Release(); }
	virtual IVariable* Clone()
	{
		Variable<String>* newVar = new Variable<String>();
		newVar->name = strdup(name);
		newVar->varPtr = varPtr;
		newVar->initialValue = initialValue;
		return newVar;
	}
	virtual void LoadNumber(double num) { LOG("Loaded the wrong type! %s is text.", name); }
	virtual void LoadCStr(const char* str) {
		initialValue.AssignString(str,strlen(str));
		varPtr->AssignString(str,strlen(str));
	}
	virtual void LoadBool(bool b) { LOG("Loaded the wrong type! %s is text.", name); }
	virtual JSON* Serialize()
	{
		JSON* newJSON = JSON::CreateString(varPtr->ToCStr());
		newJSON->Name = name;
		json = NULL; // can't be sure we'll be pointing to the right thing soon
		return newJSON;
	}
	virtual bool IsChanged() { return ( *varPtr != initialValue ); }
	virtual void SaveValue() { initialValue = *varPtr; }

public:
	String* varPtr;
	String initialValue;
	typedef String type;
};

/*
 * Template type helpers for GetVal
 * (Specializations for new types shouldn't have to go past this section)
 * ((unless you're adding arrays or objects))
 */
template<typename T> void SetHelper(T* source, T* toSet) { *toSet = *source; }
template<> 			 void SetHelper(char** source, char** toSet) { *toSet = strdup(*source); }
template<typename T> void JSONToTypeHelper(JSON* json, T* toSet)
{
	*toSet = (T) json->GetDoubleValue();
}
template<> void JSONToTypeHelper(JSON* json, bool* toSet)
{
	*toSet = json->GetBoolValue();
}
template<> void JSONToTypeHelper(JSON* json, char** toSet)
{
	*toSet = strdup(json->GetStringValue().ToCStr());
}
template<> void JSONToTypeHelper(JSON* json, String* toSet)
{
	*toSet = json->GetStringValue();
}


/***************************
 * Settings                *
 ***************************/
Settings::Settings() :
		settingsFileName(NULL),
		rootSettingsJSON(NULL),
		settingsJSON(NULL),
		variables()
{
	;
}

Settings::Settings(const char* filename) :
		settingsFileName(NULL),
		rootSettingsJSON(NULL),
		settingsJSON(NULL),
		variables()
{
	OpenOrCreate(filename);
}

Settings::~Settings()
{
	while(variables.GetSize() > 0)
	{
		IVariable* var = variables.Pop();
		delete(var);
	}

	if(rootSettingsJSON)
	{
		LOG("Releasing rootSettingsJSON");
		rootSettingsJSON->Release();
		rootSettingsJSON = NULL;
		LOG("Done releasing!");
	}
}

void Settings::OpenOrCreate(const char* filename)
{
	settingsFileName = strdup(filename);
	if(!(rootSettingsJSON = JSON::Load(filename)))
	{
		LOG("Creating new settings file: %s", filename);
		rootSettingsJSON = JSON::CreateObject();
		rootSettingsJSON->AddNumberItem("SettingsVersion",SETTINGS_VERSION);
		rootSettingsJSON->AddItem("Settings",JSON::CreateObject());
		if(!rootSettingsJSON->Save(filename))
		{
			LOG("Error creating settings file: %s", filename);
			return;
		}
	}
	else
	{
		LOG("Opening existing settings file: %s", filename);
		PrintFileToLog(filename);
	}

	// Any incompatible settings versions should be dealt with here!
	settingsJSON = rootSettingsJSON->GetItemByName("Settings");

	if(settingsJSON == NULL)
	{
		LOG("Error! Invalid settings file!");
	}
}

void Settings::CopyDefines(const Settings& source)
{
	for(int i=0;i<source.variables.GetSizeI();i++)
	{
		variables.PushBack(source.variables[i]->Clone());
	}
}

template<typename T> void Settings::Define(const char* varName, T* ptr)
{
	Variable<T>* newVar = new Variable<T>(varName, ptr);
	variables.PushBack(newVar);
}

template<typename T> bool Settings::GetVal(const char* varName, T* toSet)
{
	if(settingsJSON == NULL) return false;

	// Check all the variables first
	for(int i = 0; i < variables.GetSizeI(); i++)
	{
		if(strcmp(variables[i]->name, varName) == 0)
		{
			Variable<T>* var = dynamic_cast<Variable<T>*>(variables[i]);
			if(var == NULL)
			{
				return false;
			}
			SetHelper(var->varPtr, toSet);
			return true;
		}
	}

	// Still here?  It wasn't in the defined objects.
	JSON* valJSON = settingsJSON->GetItemByName(varName);
	if(valJSON)
	{
		JSONToTypeHelper<T>(valJSON, toSet);
		valJSON->Release();
		return true;
	}
	return false;
}

bool Settings::IsChanged()
{
	if(settingsJSON == NULL) return false;
	for(int i = 0; i < variables.GetSizeI(); i++)
	{
		IVariable* var = variables[i];
		var->SaveValue();
		if(var->IsChanged())
		{
			return true;
		}
	}
	return false;
}



void Settings::DeleteVar(const char* varName)
{
	if(settingsJSON == NULL) return;

	// Undefine the variable
	for(int i = 0; i < variables.GetSizeI(); i++)
	{
		if(strcmp(variables[i]->name, varName) == 0)
		{
			IVariable* toDel = variables[i];
			variables.RemoveAtUnordered(i);
			delete(toDel);
			break;
		}
	}

	JSON* valJSON = settingsJSON->GetItemByName(varName);
	if(valJSON)
	{
		valJSON->RemoveNode();
		valJSON->Release();
		rootSettingsJSON->Save(settingsFileName);
	}
}

void Settings::Load()
{
	if(settingsJSON == NULL) return;

	for(int i = 0; i < variables.GetSizeI(); i++)
	{
		IVariable* var = variables[i];
		JSON* varJSON = var->json;
		if(varJSON == NULL)
		{
			varJSON = settingsJSON->GetItemByName(var->name);
		}
		if(varJSON)
		{
			var->json = varJSON;
			switch(varJSON->Type)
			{
			case JSON_Bool:
				var->LoadBool(varJSON->GetBoolValue());
				break;
			case JSON_Number:
				var->LoadNumber(varJSON->GetDoubleValue());
				break;
			case JSON_String:
				var->LoadCStr(varJSON->GetStringValue().ToCStr());
				break;
			default:
				break;
			}
		}
	}
}

void Settings::SaveAll()
{
	if(settingsJSON == NULL) return;

	for(int i = 0; i < variables.GetSizeI(); i++)
	{
		IVariable* var = variables[i];
		JSON* varJSON = var->json;
		if(varJSON == NULL)
		{
			varJSON = settingsJSON->GetItemByName(var->name);
			var->json = varJSON;
		}
		if(varJSON == NULL)
		{
			settingsJSON->AddItem(var->name,var->Serialize());
		}
		else
		{
			varJSON->ReplaceNodeWith(var->Serialize());
		}
	}
	rootSettingsJSON->Save(settingsFileName);
}

void Settings::SaveChanged()
{
	if(settingsJSON == NULL) return;
	for(int i = 0; i < variables.GetSizeI(); i++)
	{
		IVariable* var = variables[i];
		if(!var->IsChanged())
		{
			continue;
		}
		var->SaveValue();

		JSON* varJSON = var->json;
		if(varJSON == NULL)
		{
			varJSON = settingsJSON->GetItemByName(var->name);
			var->json = varJSON;
		}
		if(varJSON == NULL)
		{
			settingsJSON->AddItem(var->name,var->Serialize());
		}
		else
		{
			varJSON->ReplaceNodeWith(var->Serialize());
		}
	}
	rootSettingsJSON->Save(settingsFileName);
	PrintFileToLog(settingsFileName);
}

void Settings::SaveOnly(const Array<const char*> &varNames)
{
	if(settingsJSON == NULL) return;

	for(int i = 0; i < variables.GetSizeI(); i++)
	{
		IVariable* var = variables[i];
		bool found = false;
		for(int namesIndex = 0; namesIndex < varNames.GetSizeI(); namesIndex++)
		{
			if(strcmp(var->name, varNames[namesIndex]) == 0)
			{
				found = true;
				break;
			}
		}
		if(!found)
		{
			continue;
		}
		var->SaveValue();
		JSON* varJSON = var->json;
		if(varJSON == NULL)
		{
			varJSON = settingsJSON->GetItemByName(var->name);
			var->json = varJSON;
		}
		if(varJSON == NULL)
		{
			settingsJSON->AddItem(var->name,var->Serialize());
		}
		else
		{
			varJSON->ReplaceNodeWith(var->Serialize());
		}
	}
	rootSettingsJSON->Save(settingsFileName);
}

void	Settings::SaveVarNames()
{
	if(settingsJSON == NULL) return;

	for(int i = 0; i < variables.GetSizeI(); i++)
	{
		IVariable* var = variables[i];
		JSON* varJSON = var->json;
		if(varJSON == NULL)
		{
			varJSON = settingsJSON->GetItemByName(var->name);
			var->json = varJSON;
		}
		if(varJSON == NULL)
		{
			settingsJSON->AddItem(var->name,JSON::CreateNull());
		}
	}
	rootSettingsJSON->Save(settingsFileName);
}

#ifndef NDEBUG
#include <assert.h>
void SettingsTest(String packageName)
{
	bool b = false;
	int i = 1;
	float f = 2.0f;
	double d = 3.0;
	char* cstr = strdup("cstring");
	String str("string");

	String appFileStoragePath = "/data/data/";
	appFileStoragePath += packageName;
	appFileStoragePath += "/files/";

    String FilePath = appFileStoragePath + "settingstest.json";

	LOG("Opening file");
	Settings* s = new Settings(FilePath);

	LOG("Deleting variables");
	s->DeleteVar("testbool");
	s->DeleteVar("testint");
	s->DeleteVar("testfloat");
	s->DeleteVar("testdouble");
	s->DeleteVar("testcstr");
	s->DeleteVar("teststr");

	LOG("Defining variables");
	s->Define("testbool",&b);
	s->Define("testint",&i);
	s->Define("testfloat",&f);
	s->Define("testdouble",&d);
	s->Define("testcstr",&cstr);
	s->Define("teststr",&str);

	LOG("Changing values");
	b = true;
	i = 5;
	cstr = strdup("changed cstr");

	LOG("Saving changes");
	s->SaveChanged();

	LOG("resetting values");
	b = false;
	i = 4;
	f = 6.0f;
	str.AssignString("changed String",strlen("changed String"));

	Array<const char*> onlysave;
	onlysave.PushBack("teststr");
	LOG("Saving only string value");
	s->SaveOnly(onlysave);

	LOG("Changing string value");
	cstr = strdup("changed cstr 2");
	str.AssignString("changed String 2",strlen("changed String 2"));

	LOG("Closing settings");
	delete(s);

	LOG("Opening file again");
	s = new Settings(FilePath);

	LOG("Defining stuff");
	s->Define("testint",&i);
	s->Define("testfloat",&f);
	s->Define("testdouble",&d);
	s->Define("testcstr",&cstr);
	s->Define("teststr",&str);

	LOG("Loading variables");
	s->Load();

	LOG("Checking values");

	assert( b == false ); // didn't define it
	assert( i == 5 ); // should revert
	assert( f == 6.0f ); // shouldn't have been saved
	assert( d == 3.0 ); // should stay same
	assert( strcmp(cstr,"changed cstr") == 0 ); // C-Strings revert ok?
	LOG("Test %s",str.ToCStr());
	assert( str == "changed String" ); // Saved on its own?
}
#endif

} // namespace 