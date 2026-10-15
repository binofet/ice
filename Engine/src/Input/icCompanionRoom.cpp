#include "Input/icCompanionRoom.h"
#include <algorithm>
#include <cmath>
icCompanionRoom::Player* icCompanionRoom::Find(std::uint64_t id){for(auto& p:m_players)if(p.id==id)return &p;return nullptr;}
std::uint64_t icCompanionRoom::Join(const std::string& name,double now){
 if(!std::isfinite(now)||now<0||name.size()>20)return 0;
 if(m_players.size()>=8){auto old=std::find_if(m_players.begin(),m_players.end(),[&](const Player& p){return !p.online&&p.id!=m_shooter&&p.id!=m_current;});if(old==m_players.end())return 0;m_players.erase(old);}
 Player p;p.id=m_next++;p.seen=now;p.name=name.empty()?"Player "+std::to_string(p.id):name;p.forfeited=m_phase==Phase::Playing;m_players.push_back(p);return p.id;
}
bool icCompanionRoom::Touch(std::uint64_t id,double now){auto* p=Find(id);if(!p||!std::isfinite(now)||now<0)return false;p->online=true;p->seen=now;return true;}
void icCompanionRoom::Leave(std::uint64_t id){auto* p=Find(id);if(!p)return;p->online=false;if(m_phase==Phase::Playing)p->forfeited=true;if(!m_shooter&&id==m_current)Advance();}
void icCompanionRoom::Poll(double now){if(!std::isfinite(now)||now<0)return;for(auto& p:m_players)if(p.online&&now-p.seen>10)Leave(p.id);}
std::uint64_t icCompanionRoom::Leader()const{for(const auto& p:m_players)if(p.online)return p.id;return 0;}
bool icCompanionRoom::Start(std::uint64_t leader){
 if(!leader||leader!=Leader()||m_phase==Phase::Playing)return false;
 m_current=m_shooter=0;++m_round;for(auto& p:m_players){p.score=p.balls=0;p.forfeited=!p.online;if(p.online&&!m_current)m_current=p.id;}
 if(!m_current)return false;m_phase=Phase::Playing;++m_turn;return true;
}
std::uint64_t icCompanionRoom::Current()const{return m_phase==Phase::Playing?m_current:0;}
bool icCompanionRoom::CanShoot(std::uint64_t id)const{if(m_phase!=Phase::Playing||m_shooter||!id||id!=m_current)return false;for(const auto& p:m_players)if(p.id==id)return p.online&&!p.forfeited&&p.balls<5;return false;}
bool icCompanionRoom::BeginShot(std::uint64_t id){if(!CanShoot(id))return false;m_shooter=id;return true;}
bool icCompanionRoom::FinishShot(std::uint64_t id,unsigned points){if(!id||id!=m_shooter||points>100)return false;auto* p=Find(id);if(!p)return false;p->score+=points;++p->balls;m_shooter=0;Advance();return true;}
void icCompanionRoom::Advance(){
 if(m_phase!=Phase::Playing||m_shooter)return;unsigned start=0;for(unsigned i=0;i<m_players.size();++i)if(m_players[i].id==m_current){start=i+1;break;}
 for(unsigned i=0;i<m_players.size();++i){auto& p=m_players[(start+i)%m_players.size()];if(p.online&&!p.forfeited&&p.balls<5){m_current=p.id;++m_turn;return;}}
 m_current=0;m_phase=Phase::Results;
}
void icCompanionRoom::Interrupt(){m_shooter=m_current=0;if(m_phase==Phase::Playing)m_phase=Phase::Results;for(auto& p:m_players)p.online=false;}
