
#include "Util/icFileName.h"

#include <string.h>

/*! Default c'tor
**/
icFileName::icFileName( void )
{
	m_Mem[0]='\0';
	m_Drive = NULL;
	m_Path = NULL;
	m_FileName = NULL;
	m_Extension = NULL;
}// END FUCTION icFileName( void )


/*! Default d'tor
**/
icFileName::~icFileName( void )
{
}// END FUNCTION ~icFileName( void )


/*! Parses the given filename
 *
 *	Finds the drive name, path, filename, and file extension
 *
 *	@param[in]	szFileName	Fullpath filename
 *	@param		length		Length of the string, if 0 we find it
 *	@returns	ICRESULT	Result of parse
 *
**/
ICRESULT icFileName::ParseFileName( const char* szFileName, uint length )
{
	assert(szFileName);

	m_Mem[0] = '\0';
	m_Drive = NULL;
	m_Path = NULL;
	m_FileName = NULL;
	m_Extension = NULL;

	if (!length)
	{
		while (szFileName[length] != '\0')
			length++;
	}

	// shortest possible filename is x.x
	if (length >= 3)
	{
		char* pMem = m_Mem;
		uint np=0;
#ifdef WIN32
		// Get the Drive
		if (szFileName[1] == ':' &&
			szFileName[2] == '/' ||
			szFileName[2] == '\\' )
		{
			memcpy(m_Mem, szFileName, 3);
			m_Drive = pMem;
			pMem[3] = '\0';
			np++;
			pMem += 4;
		}
		uint lSlash = length-1;
		// Get the Path and filename
		while (!m_Path && lSlash > 1)
		{
			if (szFileName[lSlash] == '/' || szFileName[lSlash] == '\\')
			{
				memcpy(pMem,&szFileName[(pMem-m_Mem)-np],length-(pMem-m_Mem));
				m_Path = pMem;
				pMem += lSlash-(pMem-m_Mem)+np+1;
				pMem[0] = '\0';
				pMem++;
				m_FileName = pMem;
				memcpy(pMem, &szFileName[lSlash+1], length-lSlash);
			}
			lSlash--;
		}

		if (!m_FileName)
		{
			memcpy(pMem, &szFileName[(pMem-m_Mem)-np], length-(pMem-m_Mem));
			m_FileName = pMem;
			lSlash = 0;
		}

		uint extPos = length-1;
		// Get the extension
		while ( !m_Extension && extPos > 0)
		{
			if (szFileName[extPos--] == '.')
			{
				pMem += strlen(m_FileName);
                pMem -= length-extPos-1;
				pMem[0] = '\0';
				pMem++;
				memcpy(pMem, &szFileName[extPos+1], length-extPos+1);
				m_Extension = pMem;
				pMem[length-extPos+1] = '\0';
			}
		}
	}

	return IC_OK;

#else
#if (defined(LINUX) || defined(OSX))
        }
        return IC_NOT_IMPL;
#endif
        }
        return IC_NOT_IMPL;
#endif
}


/*! Parses the given filename
 *
 *	Finds the drive name, path, filename, and file extension
 *
 *	@param[in]	sFileName	Fullpath filename
 *	@returns	ICRESULT	Result of parse
 *
**/
ICRESULT icFileName::ParseFileName( const icString& sFileName )
{
	return ParseFileName( sFileName.ToChar(), sFileName.Length() );
}// END FUNCTION ParseFileName( const icString& sFileName )

