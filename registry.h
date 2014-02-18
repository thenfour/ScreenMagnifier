

#pragma once


#include <windows.h>
#include <string>
#include "blob.h"

using namespace LibCC;

class RegistryValue
{
public:
  RegistryValue(HKEY hKey, const std::string& name);
	RegistryValue(HKEY hKey);
	~RegistryValue();

	DWORD GetSize() const;

	template<typename el, bool l, typename tr>
	bool GetBinary(Blob<el, l, tr>& buf, DWORD& size) const
	{
		bool r = false;

		// get the size
		if(ERROR_SUCCESS == RegQueryValueEx(m_hKey, m_Name, 0, 0, 0, &size))
		{
			buf.Alloc(size + 1);
			buf.Lock();
			if(ERROR_SUCCESS == RegQueryValueEx(m_hKey, m_Name, 0, 0, reinterpret_cast<BYTE*>(buf.GetLockedBuffer()), &size))
			{
				r = true;
			}
			buf.Unlock();
		}
		return r;
	}

	bool GetBinary(void* buf, IN OUT DWORD& size) const;
	bool GetBinary2(void* buf, IN DWORD size) const;
	bool SetBinary(const void* Buffer, DWORD Length) const;

	bool GetString(std::string& s) const;
  RegistryValue& operator = (const std::string& s);
	bool SetString(const std::string& s) const;

	bool SetDWORD(DWORD dw) const;
	bool GetDWORD(DWORD& dw) const;
	RegistryValue& operator = (const DWORD& s);

	bool SetBool(bool b) const;
	bool GetBool(bool& b) const;
	RegistryValue& operator = (const bool& s);

private:
	HKEY m_hKey;
	TCHAR* m_Name;
};

class RegistryKey
{
public:
	RegistryKey();
	RegistryKey(bool Create, bool WriteAccess, HKEY key, const std::string& subkey);
	RegistryKey(bool Create, bool WriteAccess, const std::string& key, const std::string& subkey);
	RegistryKey(bool Create, bool WriteAccess, const std::string& key);
	~RegistryKey();

	bool Open(bool Create, bool WriteAccess, HKEY key, const std::string& subkey);
	bool Open(bool Create, bool WriteAccess, const std::string& key, const std::string& subkey);
	bool Open(bool Create, bool WriteAccess, const std::string& subkey);

	RegistryValue Value(const std::string& name) const;
	RegistryValue DefaultValue() const;
	RegistryValue operator [] (const std::string& name) const;

private:
	HKEY m_hKey;
};


