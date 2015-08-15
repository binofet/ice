
#include "Util/icSettingsFile.h"

#include "Memory/icMinHeap.h"


/*! Function trims all leading/trailing whitespace, newlines, and comments
 *
 *  Note, this function modifies the string as it removes trailing newlines
 *
 *  @param          p               NULL terminated string to trim
 *  @param          comment         Comment character
 *  @returns        char*           Trimmed string
**/
char* icTrimWhiteComments(char* p, char comment)
{
    // remove leading white space, carriage returns
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    // remove comments
    while (*p == comment)
    {
        char* p2 = strchr(p,'\n\r');
        if (!p2) p2 = strchr(p,'\n');
        if (p2) p = p2;
    }
    // remove any white space found after comments
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    // remove trailing whitespace, carriage returns
    char* p2 = strchr(p,'\r');
    if (!p2) p2 = strchr(p,'\n');
    if (p2) p[p2-p] = '\0';
    return p;
}// END FUNCTION icTrimWhiteComments(char* p, char comment)


/*! Function trims all leading whitespace, newlines, and comments
 *
 *  @param          p               NULL terminated string to trim
 *  @param          comment         Comment character
 *  @returns        char*           Trimmed string
**/
char* icTrimWaste(char* p, char comment)
{
    // remove leading white space, carriage returns
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    // remove comments
    while (*p == comment)
    {
        char* p2 = strchr(p,'\r');
        if (!p2)
            p2 = strchr(p,'\n');
        if (p2) p = p2;
    }
    // remove any white space found after comments
    while (*p == ' ' || *p == '\n' || *p == '\r') p++;
    return p;
}// END FUNCTION icTrimWaste(char* p, char comment)


/*! default c'tor
**/
icSettingsFile::icSettingsFile(void)
{
    m_i32Num = 0;
}// END FUNCTION icSettingsFile(void)


/*! Clears all key-value pairs
**/
void icSettingsFile::ClearValues(void)
{
    m_i32Num = 0;
}// END FUNCTION ClearValues(void)


/*! Sets the settings file
 *
 *  @param      szFile      Full path + filename of the settings file
 *  @returns    ICRESULT    Success/failure of operation
**/
ICRESULT icSettingsFile::SetFile(const char* szFile)
{
    m_File = szFile;
    return IC_OK;
}// END FUNCTION SetFile(const char* szFile)


/*! Reads the settings file in (must call SetFile First!)
 *
 *  @returns        ICRESULT        Success/failure of Read
**/
ICRESULT icSettingsFile::Read(void)
{
    return ReadFile(m_File.ToChar());
}// END FUNCTION Read(void)


/*! Writes the current settings to file
 *
 *  @returns        ICRESULT        Success/failure of write
**/
ICRESULT icSettingsFile::Write(void)
{
    return WriteFile(m_File.ToChar());
}// END FUNCTION Write(void)


/*! Reads the settings file in
 *
 *  @param          szFile          Full path + filename of settings file
 *  @returns        ICRESULT        Success/failure of Read
**/
ICRESULT icSettingsFile::ReadFile(const char* szFile)
{
    icFile file;
    if (ICEFAIL(file.Open(szFile, ICFMREAD_CREATE)))
        return IC_FAIL_GEN;

    uint64 size=0;

    file.GetSize(&size);

    if (size)
    {
        char* mem = (char*)malloc((size_t)size + 1);

        if (mem)
        {
            size_t read;
            if (ICEOK(file.Read(mem,(size_t)size,&read)) && read == size)
            {
                mem[size] = 0;
                ParseData(mem, (int)size);
                free(mem);
                file.Close();
                return IC_OK;
            }
            free(mem);
            file.Close();
            return IC_FAIL_GEN;
        }
        return IC_FAIL_OUT_OF_MEM;
    }
    return IC_FAIL_GEN;
}// END FUNCTION ReadFile(const char* szFile)


/*! Writes the current settings to file
 *
 *  @param          szFile          Full path + filename to settings file
 *  @returns        ICRESULT        Success/failure of write
**/
ICRESULT icSettingsFile::WriteFile(const char* szFile)
{
    icFile file;
    if(ICEFAIL(file.Open(szFile,ICFMWRITE_NEW)))
        return IC_FAIL_GEN;

    char tempBuff[256];

    int num=0;

    icString headers[IC_SETTINGS_MAX];
 
    size_t curpos=0;

    // while we have data pairs to write
    while (m_i32Num > 0)
    {
        size_t written;

        // get the current (key,value) pair
        icKeyValue cur = icMinHeapDequeue(m_heap,m_i32Num--);

        icString key = cur.key;
        icString val = cur.value;

        // check to see if this prefix has been written
        int pre_index = key.FindLast('.');
        icString prefix = key.Remove(pre_index);
        int index = icMinHeapFind(headers,num,prefix);
        if (index < 0)
        {
#if _MSC_VER>=1400
            // write the prefix
            if (curpos == 0)
                sprintf_s(tempBuff,256,"[%s]\n",prefix.ToChar());
            else
                sprintf_s(tempBuff,256,"\n[%s]\n",prefix.ToChar());
#else
            // write the prefix
            if (curpos == 0)
                sprintf(tempBuff,"[%s]\n",prefix.ToChar());
            else
                sprintf(tempBuff,"\n[%s]\n",prefix.ToChar());
#endif

            size_t len = strlen(tempBuff);
            curpos += len;
            file.Write(tempBuff,len,&written);
            icMinHeapInsert(headers,num++,prefix);
        }

        // write the (key,value) pair
        key = key.Remove(0,prefix.Length());
#if _MSC_VER>=1400
        sprintf_s(tempBuff,256,"%s=%s\n",key.ToChar(),val.ToChar());
#else
        sprintf(tempBuff,"%s=%s\n",key.ToChar(),val.ToChar());
#endif
        size_t len = strlen(tempBuff);
        curpos += len;
        file.Write(tempBuff,len,&written);
    }

    // we have to put the data back, so we read what we just wrote
    // TODO: is this too hackish
    return ReadFile(szFile);
}// END FUNCTION WriteFile(const char* szFile)


/*! Adds a key-value pair to the settings table
 *
 *  @param      key         Key (max length = ICKEYLENGTH)
 *  @param      val         Value (max length = ICVALUELENGTH)
 *  @returns    ICRESULT    Success/failure of AddPair
**/
ICRESULT icSettingsFile::AddPair(const char* key, const char* val)
{
    if (m_i32Num < IC_SETTINGS_MAX)
    {
        icKeyValue temp;
#if _MSC_VER>=1400
        strcpy_s(temp.key,128,key);
        strcpy_s(temp.value,128,val);
#else
        strcpy(temp.key,key);
        strcpy(temp.value,val);
#endif
        int index = icMinHeapFind(m_heap,m_i32Num, temp);
        if (index < 0)
            icMinHeapInsert(m_heap,m_i32Num++,temp);
        else
            icWarning("Duplicated key,value pair added");
        return IC_OK;
    }
    return IC_FAIL_GEN;
}// END FUNCTION AddPair(const char* key, const char* val)


/*! Gets the value for the specified key
 *
 *  @param      key         Key to search for
 *  @returns    icString    Value associated with key, "" if not found
**/
icString icSettingsFile::GetValue(const char* key, const char* defVal,
                                  bool autoAdd)
{
    icString ret;
    icKeyValue temp;
#if _MSC_VER>=1400
    strcpy_s(temp.key,128,key);
#else
    strcpy(temp.key,key);
#endif
    int index = icMinHeapFind(m_heap,m_i32Num,temp);
    if (index >= 0)
    {
        ret = m_heap[index].value;
    }
    else if (defVal)
    {
        ret = defVal;
        if (autoAdd)
            AddPair(key, ret.ToChar());
    }
    return ret;
}// END FUNCTION GetValue(const char* key, const char* defVal, bool autoAdd)


/*! Parses the data, adding (key,value) pairs
 *
 *
 *
 *
 *
 *
 *
 *  @param          mem         Pointer to (key,value) memory data
**/
void icSettingsFile::ParseData(char* mem, int size)
{
    char* p = mem;
    p = icTrimWaste(p, '#');

    icString key;
    icString value;

    char temp[256] = {0};
    char temp2[256] = {0};

    icString pre_fix;
    do
    {
        char* p2 = NULL;
        // Identify Prefix Headers
        // [This.Is.A.Header]
        if (p[0] == '[')
        {
            p2 = strchr(p,']') + 1;
            int len = p2-p;
            memcpy(temp,p,len);
            p += len;

            // remove the brackets, add a period to the pre_fix string
            icString temp_pre_fix = &temp[1];
            pre_fix = temp_pre_fix.Remove(len-2);
            pre_fix += ".";
        }
        else
        {
            // we are looking for a (key,value) pair here
            // Format is in the next comment line
            // Key=Value
            int len=0;
            p2 = strchr(p,'\r');
            if (!p2)
                p2 = strchr(p,'\n');

            // this is ugly and hairy
            if (p2)
            {
                len = p2 - p;
                p2 = icTrimWaste(p2, '#');
            }
            else
                len = strlen(p);

            memcpy(temp2,p,len);
            temp2[len] = '\0';
            char* ptemp = temp2;
            ptemp = icTrimWhiteComments(ptemp, '#');

            // make sure we have an equals '=' for this to be a valid
            // (key,value) pair
            char* equals = strchr(ptemp,'=');
            if (equals)
            {
                int keylen = len - strlen(equals);
                memcpy(temp, p, keylen);
                temp[keylen] = '\0';
                memcpy(temp2, equals+1, len-keylen);
                temp2[len-keylen] = '\0';
                key = pre_fix + temp;
                value = temp2;

                AddPair(key.ToChar(),value.ToChar());
            }
            else
            {
                // this is an invalid (key,value) pair
                // skip it
                icWarning("Invalid line format detected in setting file");
            }
            if (p2)
                p = p2;
            else
                p += len;
        }
        p = icTrimWaste(p, '#');
    } while (p < mem + size);
}// END FUNCTION ParseData(char* mem)