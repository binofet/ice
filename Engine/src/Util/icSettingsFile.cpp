#include "Util/icSettingsFile.h"
#include <sstream>
#include <string>
#include <map>
static std::string trim(const std::string& s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return {};auto b=s.find_last_not_of(" \t\r\n");return s.substr(a,b-a+1);}
icSettingsFile::icSettingsFile():m_i32Num(0){}
void icSettingsFile::ClearValues(){m_i32Num=0;}
ICRESULT icSettingsFile::SetFile(const char* path){if(!path)return IC_FAIL_NPE;m_File=path;return IC_OK;}
ICRESULT icSettingsFile::Read(){return ReadFile(m_File.ToChar());}
ICRESULT icSettingsFile::Write(){return WriteFile(m_File.ToChar());}
ICRESULT icSettingsFile::ReadFile(const char* path){
    icFile file;if(ICEFAIL(file.Open(path,ICFMREAD_EXISTING)))return IC_FAIL_GEN;
    uint64 size;if(ICEFAIL(file.GetSize(&size))||size>1024*1024)return IC_FAIL_GEN;
    std::string data((size_t)size,'\0');size_t count;
    if(size&&ICEFAIL(file.Read(&data[0],data.size(),&count)))return IC_FAIL_GEN;
    ClearValues();ParseData(size?&data[0]:nullptr,(int)size);return IC_OK;
}
void icSettingsFile::ParseData(char* data,int size){
    if(!data||size<=0)return;
    std::istringstream stream(std::string(data,size));std::string line,prefix;
    while(std::getline(stream,line)){
        line=trim(line);if(line.empty()||line[0]=='#'||line[0]==';')continue;
        if(line.front()=='['&&line.back()==']'){prefix=trim(line.substr(1,line.size()-2));continue;}
        auto equals=line.find('=');if(equals==std::string::npos)continue;
        auto key=trim(line.substr(0,equals));auto value=trim(line.substr(equals+1));
        if(!prefix.empty())key=prefix+"."+key;AddPair(key.c_str(),value.c_str());
    }
}
ICRESULT icSettingsFile::AddPair(const char* key,const char* value){
    if(!key||!value||strlen(key)>=ICKEYLENGTH||strlen(value)>=ICVALUELENGTH)return IC_FAIL_GEN;
    for(int i=0;i<m_i32Num;++i)if(!strcmp(m_heap[i].key,key)){strcpy(m_heap[i].value,value);return IC_OK;}
    if(m_i32Num>=IC_SETTINGS_MAX)return IC_FAIL_GEN;
    strcpy(m_heap[m_i32Num].key,key);strcpy(m_heap[m_i32Num++].value,value);return IC_OK;
}
icString icSettingsFile::GetValue(const char* key,const char* fallback,bool add){
    if(!key)return icString();
    for(int i=0;i<m_i32Num;++i)if(!strcmp(m_heap[i].key,key))return icString(m_heap[i].value);
    if(fallback){if(add)AddPair(key,fallback);return icString(fallback);}return icString();
}
ICRESULT icSettingsFile::WriteFile(const char* path){
    std::map<std::string,std::map<std::string,std::string>> sections;
    for(int i=0;i<m_i32Num;++i){std::string key=m_heap[i].key;auto dot=key.find_last_of('.');
        sections[dot==std::string::npos?"":key.substr(0,dot)][dot==std::string::npos?key:key.substr(dot+1)]=m_heap[i].value;}
    std::ostringstream output;
    for(auto& section:sections){if(!section.first.empty())output<<"["<<section.first<<"]\n";
        for(auto& pair:section.second)output<<pair.first<<"="<<pair.second<<"\n";output<<"\n";}
    std::string data=output.str();icFile file;if(ICEFAIL(file.Open(path,ICFMWRITE_NEW)))return IC_FAIL_GEN;
    size_t written;return file.Write((void*)data.data(),data.size(),&written);
}
