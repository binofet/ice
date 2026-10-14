#ifndef WIN32
#include "Core/IO/icFile.h"
#include <limits>
icFile::icFile():m_fp(nullptr),m_u64FilePos(0),m_bStreaming(false),m_FileMode(0),m_ptrUser(nullptr),m_pVoidCallback(nullptr){}
icFile::~icFile(){Close();}
ICRESULT icFile::Open(const char* path,uchar mode){
    Close();if(!path)return IC_FAIL_NPE;
    if(mode&ICFM_ASYNC)return IC_NOT_IMPL;
    const bool write=(mode&ICFM_WRITE)!=0;
    const bool create=(mode&ICFM_CREATE_ALWAYS)!=0;
    const bool always=(mode&ICFM_OPEN_ALWAYS)!=0;
    m_fp=fopen(path,create?"w+b":write?"r+b":"rb");
    if(!m_fp&&always)m_fp=fopen(path,"w+b");
    if(!m_fp)return IC_FAIL_GEN;
    m_FileMode=mode;return IC_OK;
}
ICRESULT icFile::Close(){ICRESULT r=IC_OK;if(m_fp){if(fclose(m_fp)!=0)r=IC_FAIL_GEN;m_fp=nullptr;}m_FileMode=0;return r;}
ICRESULT icFile::Read(void* dst,size_t size,size_t* actual){if(actual)*actual=0;if(!m_fp||(!dst&&size))return IC_FAIL_NPE;size_t n=fread(dst,1,size,m_fp);if(actual)*actual=n;return n==size?IC_OK:IC_FAIL_GEN;}
ICRESULT icFile::Write(void* src,size_t size,size_t* actual){if(actual)*actual=0;if(!m_fp||(!src&&size))return IC_FAIL_NPE;size_t n=fwrite(src,1,size,m_fp);if(actual)*actual=n;return n==size?IC_OK:IC_FAIL_GEN;}
ICRESULT icFile::GetSize(uint64* size){if(!m_fp||!size)return IC_FAIL_NPE;off_t pos=ftello(m_fp);if(pos<0||fseeko(m_fp,0,SEEK_END)!=0)return IC_FAIL_GEN;off_t end=ftello(m_fp);int result=fseeko(m_fp,pos,SEEK_SET);if(end<0||result!=0)return IC_FAIL_GEN;*size=(uint64)end;return IC_OK;}
ICRESULT icFile::GetPos(uint64* pos){if(!m_fp||!pos)return IC_FAIL_NPE;off_t n=ftello(m_fp);if(n<0)return IC_FAIL_GEN;*pos=(uint64)n;return IC_OK;}
ICRESULT icFile::SetPos(uint64 pos){if(!m_fp||pos>(uint64)std::numeric_limits<off_t>::max())return IC_FAIL_GEN;return fseeko(m_fp,(off_t)pos,SEEK_SET)==0?IC_OK:IC_FAIL_GEN;}
ICRESULT icFile::ReadAsync(void*,size_t,void*,void(*)(void*,size_t)){return IC_NOT_IMPL;}
ICRESULT icFile::WriteAsync(void*,size_t,void*,void(*)(void*,size_t)){return IC_NOT_IMPL;}
ICRESULT icFile::StopAsync(){return IC_OK;}
#endif
