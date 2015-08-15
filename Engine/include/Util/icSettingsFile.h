#ifndef __IC_SETTINGS_FILE_H__
#define __IC_SETTINGS_FILE_H__

#include "Core/icGlobals.h"
#include "Memory/icString.h"
#include "Core/IO/icFile.h"

#define ICKEYLENGTH 128
#define ICVALUELENGTH 128
#define IC_SETTINGS_MAX (64)

struct icKeyValue
{
    char key[ICKEYLENGTH];
    char value[ICVALUELENGTH];

    bool operator<(const icKeyValue& Rhs)
    {
        return strcmp(key,Rhs.key) < 0;
    };

    bool operator>(const icKeyValue& Rhs)
    {
        return strcmp(key,Rhs.key) > 0;
    };

    bool operator>=(const icKeyValue& Rhs)
    {
        return (strcmp(key,Rhs.key) >= 0);
    };

    bool operator<=(const icKeyValue& Rhs)
    {
        return (strcmp(key,Rhs.key) <= 0);
    }

    bool operator==(const icKeyValue& Rhs)
    {
        return (strcmp(key,Rhs.key) == 0);
    };

    bool operator!=(const icKeyValue& Rhs)
    {
        return (strcmp(key,Rhs.key) != 0);
    };
};

/*! Provides simple settings file parsing
**/
class icSettingsFile
{
public:
    //////////////////////////////////////////////////////////////////////////
    // LIFECYCLE
    icSettingsFile(void);

    void ClearValues(void);

    //////////////////////////////////////////////////////////////////////////
    // OPERATIONS
    ICRESULT SetFile(const char* szFile);
    ICRESULT Read(void);
    ICRESULT Write(void);

    ICRESULT ReadFile(const char* szFile);
    ICRESULT WriteFile(const char* szFile);

    //////////////////////////////////////////////////////////////////////////
    // ACCESS
    ICRESULT AddPair(const char* key, const char* val);
    icString GetValue(const char* key, const char* defVal = NULL,
                      bool autoAdd = true);

private:
    void ParseData(char* mem, int size);

    icString    m_File;
    icKeyValue  m_heap[IC_SETTINGS_MAX];
    int         m_i32Num;
};

#endif // __IC_SETTINGS_FILE_H__