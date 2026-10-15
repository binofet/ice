#include "Input/icCompanion.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <cstdio>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace {
const std::size_t maxEvents = 18000;
float clamp(float v,float lo,float hi) { return std::max(lo,std::min(hi,v)); }
bool valid(const icDeviceInput& e) {
    bool held=e.kind==icInputKind::Pointer||e.kind==icInputKind::Key;
    bool phase=held?e.phase!=icInputPhase::Sample:e.phase==icInputPhase::Sample;
    return phase && e.device && std::isfinite(e.seconds) && e.seconds >= 0 && std::isfinite(e.x) &&
        std::isfinite(e.y) && std::isfinite(e.z) && std::isfinite(e.w) &&
        (unsigned)e.kind <= (unsigned)icInputKind::Attitude && (unsigned)e.phase <= (unsigned)icInputPhase::Sample;
}
bool valid(const icGestureConfig& c) {
    float g=c.gravityX*c.gravityX+c.gravityY*c.gravityY+c.gravityZ*c.gravityZ;
    return (unsigned)c.mode <= 1 && std::isfinite(g) && (!c.calibrated || (g >= 64 && g <= 121));
}
bool same(const icShotIntent& a,const icShotIntent& b) {
    return a.source==b.source && std::abs(a.aim-b.aim)<1e-5 && std::abs(a.power-b.power)<1e-5 && std::abs(a.seconds-b.seconds)<1e-8;
}
}
void icShotGesture::Reset(const icGestureConfig& config) {
    m_config=valid(config)?config:icGestureConfig();m_samples.clear();m_gyro=0;Cancel();
}
void icShotGesture::Cancel() {
    m_armed=false;m_aim=m_touchPower=m_filtered=m_peak=0;m_lastMotion=m_peakTime=m_lastPointer=-1;
}
icCalibrationResult icShotGesture::Calibrate() {
    if(m_samples.size()<20 || m_samples.back().seconds-m_samples.front().seconds<.25) return icCalibrationResult::NeedSamples;
    double x=0,y=0,z=0;for(const auto& e:m_samples){x+=e.x;y+=e.y;z+=e.z;}
    x/=m_samples.size();y/=m_samples.size();z/=m_samples.size();double variance=0;
    for(const auto& e:m_samples)variance+=(e.x-x)*(e.x-x)+(e.y-y)*(e.y-y)+(e.z-z)*(e.z-z);
    double gravity=x*x+y*y+z*z;
    if(variance/m_samples.size()>.16 || gravity<64 || gravity>121 || m_gyro>.4f) return icCalibrationResult::Moving;
    Cancel();m_config.gravityX=x;m_config.gravityY=y;m_config.gravityZ=z;m_config.calibrated=true;
    return icCalibrationResult::Ready;
}
float icShotGesture::Power() const { return m_config.mode==icShotMode::Touch?m_touchPower:clamp((m_peak-1.0f)/14.0f,0,1); }
bool icShotGesture::Consume(const icDeviceInput& e,icShotIntent& shot) {
    if(!valid(e))return false;
    if(e.kind==icInputKind::AngularVelocity && e.phase==icInputPhase::Sample) {
        m_gyro=std::hypot(e.x,std::hypot(e.y,e.z));return false;
    }
    if(e.kind==icInputKind::Acceleration && e.phase==icInputPhase::Sample) {
        if(!m_armed) {
            if(!m_samples.empty()&&e.seconds<m_samples.back().seconds)return false;
            m_samples.push_back(e);
            while(m_samples.size()>90 || e.seconds-m_samples.front().seconds>.75)m_samples.pop_front();
        } else if(m_config.mode==icShotMode::Motion && e.seconds>=m_contact.seconds) {
            if(e.seconds-m_contact.seconds>3){Cancel();return false;}
            if(m_lastMotion>=0&&e.seconds<=m_lastMotion)return false;
            float dt=m_lastMotion<0?1.f/60:clamp(e.seconds-m_lastMotion,0,.1f);
            float x=e.x-m_config.gravityX,y=e.y-m_config.gravityY,z=e.z-m_config.gravityZ;
            float magnitude=clamp(std::hypot(x,std::hypot(y,z)),0,100);
            m_filtered+=(magnitude-m_filtered)*(1-std::exp(-dt/.035f));
            m_peak*=std::exp(-dt*2);
            if(m_filtered>=m_peak){m_peak=m_filtered;m_peakTime=e.seconds;}
            m_lastMotion=e.seconds;
        }
        return false;
    }
    if(e.kind!=icInputKind::Pointer)return false;
    if(e.phase==icInputPhase::Begin) {
        if(m_armed || (m_config.mode==icShotMode::Motion&&!m_config.calibrated))return false;
        Cancel();m_armed=true;m_contact=e;m_startY=e.y;m_lastPointer=e.seconds;
    }
    if(!m_armed || e.device!=m_contact.device || e.surface!=m_contact.surface || e.contact!=m_contact.contact || e.seconds<m_lastPointer)return false;
    m_lastPointer=e.seconds;
    if(e.phase==icInputPhase::Cancel){Cancel();return false;}
    m_aim=clamp((e.x-.5f)*2,-1,1);m_touchPower=clamp((m_startY-e.y)*1.8f,0,1);
    if(e.phase!=icInputPhase::End)return false;
    double duration=e.seconds-m_contact.seconds;float power=Power();
    bool fresh=m_config.mode==icShotMode::Touch?duration<=8:
        duration<=3 && m_lastMotion>=m_contact.seconds && e.seconds-m_lastMotion<=.25 && e.seconds-m_peakTime<=.6;
    shot.aim=m_aim;shot.power=power;shot.seconds=e.seconds;shot.source=m_config.mode;
    Cancel();return fresh&&power>=.08f;
}
void icCompanionSession::Consume(const icDeviceInput& e,bool record) {
    icShotIntent shot;
    if(m_gesture.Consume(e,shot)){m_shots.push_back(shot);if(record)m_take.expected.push_back(shot);}
}
void icCompanionSession::OnInput(const icDeviceInput& e) {
    if(m_replaying || !valid(e))return;
    bool record=m_recording;
    if(record){
        if(m_take.events.size()==maxEvents || (!m_take.events.empty()&&std::abs(e.seconds-m_take.events.front().seconds)>300)){m_recording=false;record=false;}
        else m_take.events.push_back(e);
    }
    Consume(e,record);
}
void icCompanionSession::SetMode(icShotMode mode) {
    Suspend();auto config=m_gesture.Config();config.mode=mode;m_gesture.Reset(config);
}
icCalibrationResult icCompanionSession::Calibrate() { Suspend();return m_gesture.Calibrate(); }
void icCompanionSession::StartRecording() {
    Suspend();m_take=icCompanionTake();m_take.config=m_gesture.Config();m_gesture.Reset(m_take.config);
    m_shots.clear();m_recording=true;m_replayMatched=false;
}
bool icCompanionSession::StartReplay() {
    Suspend();if(m_take.events.empty())return false;
    m_gesture.Reset(m_take.config);m_shots.clear();m_replayIndex=0;m_replayTime=m_deadline=0;
    m_replaying=true;m_replayMatched=false;return true;
}
void icCompanionSession::AdvanceReplay(double seconds) {
    if(!m_replaying || !std::isfinite(seconds) || seconds<0)return;
    m_replayTime+=seconds;
    while(m_replayIndex<m_take.events.size() && m_deadline<=m_replayTime) {
        Consume(m_take.events[m_replayIndex++],false);
        if(m_replayIndex<m_take.events.size())m_deadline=std::max(m_deadline,m_take.events[m_replayIndex].seconds-m_take.events.front().seconds);
    }
    if(m_replayIndex==m_take.events.size()) {
        m_replaying=false;m_replayMatched=m_shots.size()==m_take.expected.size();
        for(std::size_t i=0;m_replayMatched&&i<m_shots.size();++i)m_replayMatched=same(m_shots[i],m_take.expected[i]);
        m_gesture.Cancel();
    }
}
void icCompanionSession::Deactivate() { Suspend();auto config=m_gesture.Config();config.calibrated=false;m_gesture.Reset(config); }
void icCompanionSession::Suspend() { m_recording=false;m_replaying=false;m_gesture.Cancel(); }
bool icCompanionTake::Save(const std::string& path) const {
    if(!valid(config)||events.size()>maxEvents||expected.size()>events.size())return false;
    for(const auto& e:events)if(!valid(e)||std::abs(e.seconds-events.front().seconds)>300)return false;
    for(const auto& s:expected)if((unsigned)s.source>1||!std::isfinite(s.aim)||!std::isfinite(s.power)||!std::isfinite(s.seconds)||s.seconds<0||std::abs(s.aim)>1||s.power<.08f||s.power>1)return false;
    // The app supplies a writable sandbox path. Commit complete takes by rename.
    std::string temporary=path+".tmp";std::ofstream file(temporary,std::ios::trunc);file.imbue(std::locale::classic());
    file<<std::setprecision(std::numeric_limits<double>::max_digits10);
    file<<"ICE_COMPANION 1\n"<<(int)config.mode<<' '<<config.calibrated<<' '<<config.gravityX<<' '<<config.gravityY<<' '<<config.gravityZ<<'\n';
    file<<events.size()<<' '<<expected.size()<<'\n';
    for(const auto& e:events)file<<e.device<<' '<<e.surface<<' '<<e.contact<<' '<<e.seconds<<' '<<(int)e.kind<<' '<<(int)e.phase<<' '<<e.x<<' '<<e.y<<' '<<e.z<<' '<<e.w<<'\n';
    for(const auto& s:expected)file<<(int)s.source<<' '<<s.aim<<' '<<s.power<<' '<<s.seconds<<'\n';
    file.flush();bool good=file.good();file.close();good=good&&!file.fail();
    #ifdef _WIN32
    if(good&&MoveFileExA(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
#else
    if(good&&std::rename(temporary.c_str(),path.c_str())==0)return true;
#endif
    std::remove(temporary.c_str());return false;
}
bool icCompanionTake::Load(const std::string& path) {
    std::ifstream file(path);file.imbue(std::locale::classic());
    if(!file)return false;file.seekg(0,std::ios::end);auto bytes=file.tellg();
    if(bytes<0||bytes>8*1024*1024)return false;file.seekg(0);
    std::string magic;int version,mode,calibrated;std::size_t count,shots;icCompanionTake candidate;
    if(!(file>>magic>>version)||magic!="ICE_COMPANION"||version!=1)return false;
    if(!(file>>mode>>calibrated>>candidate.config.gravityX>>candidate.config.gravityY>>candidate.config.gravityZ)||mode<0||mode>1||calibrated<0||calibrated>1)return false;
    candidate.config.mode=(icShotMode)mode;candidate.config.calibrated=calibrated!=0;
    if(!valid(candidate.config)||!(file>>count>>shots)||count>maxEvents||shots>count)return false;
    for(std::size_t i=0;i<count;++i){icDeviceInput e;int kind,phase;
        if(!(file>>e.device>>e.surface>>e.contact>>e.seconds>>kind>>phase>>e.x>>e.y>>e.z>>e.w)||kind<0||kind>4||phase<0||phase>4)return false;
        e.kind=(icInputKind)kind;e.phase=(icInputPhase)phase;if(!valid(e))return false;candidate.events.push_back(e);
    }
    for(std::size_t i=0;i<shots;++i){icShotIntent s;int source;
        if(!(file>>source>>s.aim>>s.power>>s.seconds)||source<0||source>1||!std::isfinite(s.aim)||!std::isfinite(s.power)||!std::isfinite(s.seconds)||s.seconds<0||std::abs(s.aim)>1||s.power<.08f||s.power>1)return false;
        s.source=(icShotMode)source;candidate.expected.push_back(s);
    }
    std::string extra;if(file>>extra)return false;
    // A take is a bounded diagnostic session, not an unbounded wait on hostile timestamps.
    if(!candidate.events.empty())for(const auto& e:candidate.events)if(std::abs(e.seconds-candidate.events.front().seconds)>300)return false;
    *this=std::move(candidate);return true;
}
