#ifndef __IC_ISCRIPTDOMAIN_H__
#define __IC_ISCRIPTDOMAIN_H__

#include "Core/icGlobals.h"
#include "Memory/icString.h"

namespace Ice { namespace Scripting
{
	/**
	 * Provides an interface to a scripting system.
	 */
	class IScriptDomain
	{
	public:
		virtual ~IScriptDomain() {};

		virtual void Update(const float fDeltaTime) { fDeltaTime; }

		virtual ICRESULT ExecuteFile(const char* szFilePath)
		{
			(void)szFilePath; return IC_NOT_IMPL;
		}

		virtual ICRESULT ExecuteBuffer(const char* szBuffer, size_t size)
		{
			(void)szBuffer; (void)size;
			return IC_NOT_IMPL;
		}

		virtual ICRESULT LoadBuffer(const char* szBuffer, size_t size)
		{
			(void)szBuffer;
			(void)size;
			return IC_NOT_IMPL;
		}

		virtual ICRESULT LoadFile(const char* szFilePath)
		{
			(void)szFilePath;
			return IC_NOT_IMPL;
		}

		virtual ICRESULT LoadFile(const class icFile* file)
		{
			(void)file;
			return IC_NOT_IMPL;
		}

		/*
		virtual ICRESULT GetValue(const char* path, int& value) { return IC_NOT_IMPL; }
		virtual ICRESULT SetValue(const char* path, const int value) { return IC_NOT_IMPL; }

		virtual ICRESULT GetValue(const char* path, float& value) { return IC_NOT_IMPL; }
		virtual ICRESULT SetValue(const char* path, const float value) { return IC_NOT_IMPL; }

		virtual ICRESULT GetValue(const char* path, void*& value) { return IC_NOT_IMPL; }
		virtual ICRESULT SetValue(const char* path, const void* value) { return IC_NOT_IMPL; }

		virtual ICRESULT GetValue(const char* path, icString& value) { return IC_NOT_IMPL; }
		virtual ICRESULT SetValue(const char* path, const icString& value) { return IC_NOT_IMPL; }
		*/
	};

} } // Ice.Scripting

#endif __IC_ISCRIPTDOMAIN_H__
