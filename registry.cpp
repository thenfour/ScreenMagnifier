

#include "stdafx.h"
#include "registry.h"
#include <algorithm>
#include <locale>
#include <ctype.h>

template<typename Ch, typename Traits, typename Alloc>
void StringToLower(std::basic_string<Ch, Traits, Alloc> &s)
{
  std::transform(s.begin(), s.end(), s.begin(), tolower);  
}

template<typename Tl, typename Tr>
inline int StringCompareI(const Tl *left, const Tr *right)
{
    while(*left && *right)
    {
        if(!CharEqualsI(*left, *right))
        {
            return (CharLessThanI(*left, *right) ? -1 : +1);
        }
        left++;
        right++;
    }

    return (0);
}

template<typename LCh, typename LTraits, typename LAlloc, typename RCh, typename RTraits, typename RAlloc>
inline bool StringEqualsI(const std::basic_string<LCh, LTraits, LAlloc>& l, const std::basic_string<RCh, RTraits, RAlloc>& r)
{
  return 0 == StringCompareI(l.c_str(), r.c_str());
}

/*
    Takes a string reg key name and attempts to convert it to a real HKEY.
    This is only for the registry hive names HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER, etc...
    s can have stuff after it too (e.g. "HKEY_LOCAL_MACHINE\\Software")
    Shortened versions also recognized (e.g "HKLM" instead of "HKEY_LOCAL_MACHINE")

    returns 0 for failure (unrecognized key name)
*/
inline HKEY InterpretRegHiveName(const std::string& s, std::string& therest)
{
    HKEY r = 0;
    std::string sKey;
    std::string::size_type nSep = s.find_first_of("\\/");
    if(nSep == std::string::npos)
    {
        // no separator... maybe the whole thing is the hive name
        sKey = s;
        therest.clear();
    }
    else
    {
        sKey = s.substr(0, nSep);
        therest = s.substr(nSep + 1);// BUG: could potentially go past the end of the str.
    }

    StringToLower(sKey);

    if((sKey == "hklm") || (sKey == "hkey_local_machine"))
    {
        r = HKEY_LOCAL_MACHINE;
    }
    else if((sKey == "hkcu") || (sKey == "hkey_current_user"))
    {
        r = HKEY_CURRENT_USER;
    }
    else if((sKey == "hkcr") || (sKey == "hkey_classes_root"))
    {
        r = HKEY_CLASSES_ROOT;
    }
    else if((sKey == "hku") || (sKey == "hkey_users"))
    {
        r = HKEY_CLASSES_ROOT;
    }

    return r;
}



RegistryValue::RegistryValue(HKEY hKey, const std::string& name) :
	m_hKey(hKey)
{
	m_Name = _tcsdup(name.c_str());
}

RegistryValue::RegistryValue(HKEY hKey) :
	m_hKey(hKey),
	m_Name(0)
{
}

RegistryValue::~RegistryValue()
{
	if(m_Name)
	{
		free(m_Name);
	}
}

DWORD RegistryValue::GetSize() const
{
	DWORD ret(0);
	RegQueryValueEx(m_hKey, m_Name, 0, 0, 0, &ret);
	return ret;
}

bool RegistryValue::SetBinary(const void* Buffer, DWORD Length) const
{
	bool r = false;
	if(ERROR_SUCCESS == RegSetValueEx(m_hKey, m_Name, 0, REG_BINARY, reinterpret_cast<const BYTE*>(Buffer), Length))
	{
		r = true;
	}
	return r;
}

bool RegistryValue::GetBinary(void* buf, IN OUT DWORD& size) const
{
	bool r = false;
	if(ERROR_SUCCESS == RegQueryValueEx(m_hKey, m_Name, 0, 0, reinterpret_cast<BYTE*>(buf), &size))
	{
		r = true;
	}
	return r;
}

bool RegistryValue::GetBinary2(void* buf, IN DWORD size) const
{
	return GetBinary(buf, size);
}

bool RegistryValue::GetString(std::string& s) const
{
	bool r = false;
	DWORD type;
	DWORD size;

	// get the size
	if(ERROR_SUCCESS == RegQueryValueEx(m_hKey, m_Name, 0, &type, 0, &size))
	{
		if((type == REG_SZ) || (type == REG_MULTI_SZ) || (type == REG_EXPAND_SZ))
		{
      Blob<TCHAR, false> b;
      b.Alloc(size + 1);
      if(ERROR_SUCCESS == RegQueryValueEx(m_hKey, m_Name, 0, 0, reinterpret_cast<BYTE*>(b.GetWritableBuffer()), &size))
			{
        s = b.GetBuffer();
				r = true;
			}
		}
	}
	return r;
}

bool RegistryValue::SetString(const std::string& s) const
{
	bool r = false;
	if(ERROR_SUCCESS == RegSetValueEx(m_hKey, m_Name, 0, REG_SZ, reinterpret_cast<const BYTE*>(s.c_str()), sizeof(TCHAR) * ((DWORD)s.size() + 1)))
	{
		r = true;
	}
	return r;
}

bool RegistryValue::SetDWORD(DWORD dw) const
{
	bool r = false;
	if(ERROR_SUCCESS == RegSetValueEx(m_hKey, m_Name, 0, REG_DWORD, reinterpret_cast<BYTE*>(&dw), sizeof(DWORD)))
	{
		r = true;
	}
	return r;
}

bool RegistryValue::GetDWORD(DWORD& dw) const
{
	bool r = false;
	DWORD type;
	DWORD val;
	DWORD size = sizeof(val);
	if(ERROR_SUCCESS == RegQueryValueEx(m_hKey, m_Name, 0, &type, reinterpret_cast<BYTE*>(&val), &size))
	{
		if(type == REG_DWORD)
		{
			dw = val;
			r = true;
		}
	}
	return r;
}

bool RegistryValue::SetBool(bool b) const
{
	bool r = false;
	DWORD val = b ? 1 : 0;
	if(ERROR_SUCCESS == RegSetValueEx(m_hKey, m_Name, 0, REG_DWORD, reinterpret_cast<BYTE*>(&val), sizeof(val)))
	{
		r = true;
	}
	return r;
}

bool RegistryValue::GetBool(bool& b) const
{
	bool r = false;
	DWORD val;
	if(GetDWORD(val))
	{
		b = (val == 1);
		r = true;
	}
	return r;
}

RegistryValue& RegistryValue::operator = (const std::string& s)
{
	SetString(s);
	return *this;
}

RegistryValue& RegistryValue::operator = (const DWORD& s)
{
	SetDWORD(s);
	return *this;
}

RegistryValue& RegistryValue::operator = (const bool& s)
{
	SetBool(s);
	return *this;
}




RegistryKey::RegistryKey() :
	m_hKey(0)
{
}

RegistryKey::RegistryKey(bool Create, bool WriteAccess, HKEY key, const std::string& subkey) :
	m_hKey(0)
{
	Open(Create, WriteAccess, key, subkey);
}

RegistryKey::RegistryKey(bool Create, bool WriteAccess, const std::string& key, const std::string& subkey) :
	m_hKey(0)
{
	Open(Create, WriteAccess, key, subkey);
}

RegistryKey::RegistryKey(bool Create, bool WriteAccess, const std::string& key) :
	m_hKey(0)
{
	Open(Create, WriteAccess, key);
}

RegistryKey::~RegistryKey()
{
	if(m_hKey)
	{
		RegCloseKey(m_hKey);
	}
}

bool RegistryKey::Open(bool Create, bool WriteAccess, HKEY key, const std::string& subkey)
{
	bool r = false;

	if(m_hKey)
	{
		RegCloseKey(m_hKey);
	}

	REGSAM access = WriteAccess ? KEY_WRITE | KEY_READ : KEY_READ;

	if(Create)
	{
		r = (ERROR_SUCCESS == RegCreateKeyEx(key, subkey.c_str(), 0, 0, 0, access, 0, &m_hKey, 0));
	}
	else
	{
		r = (ERROR_SUCCESS == RegOpenKeyEx(key, subkey.c_str(), 0, access, &m_hKey));
	}
	return r;
}

bool RegistryKey::Open(bool Create, bool WriteAccess, const std::string& key, const std::string& subkey)
{
  std::string therest;
	HKEY hHive = InterpretRegHiveName(key, therest);
	return Open(Create, WriteAccess, hHive, subkey);
}

bool RegistryKey::Open(bool Create, bool WriteAccess, const std::string& subkey)
{
  std::string therest;
	HKEY hHive = InterpretRegHiveName(subkey, therest);
	return Open(Create, WriteAccess, hHive, therest);
}

RegistryValue RegistryKey::Value(const std::string& name) const
{
	return RegistryValue(m_hKey, name);
}

RegistryValue RegistryKey::DefaultValue() const
{
	return RegistryValue(m_hKey);
}

RegistryValue RegistryKey::operator [] (const std::string& name) const
{
	return Value(name);
}



