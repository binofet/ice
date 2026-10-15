#include "Input/icCompanionRoom.h"
#include <cstdio>
#include <cstdlib>
static void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
int main(){icCompanionRoom room;auto a=room.Join("Alice",1),b=room.Join("Bob",1);check(a&&b&&a!=b,"distinct player identities");check(!room.Start(b)&&room.Start(a),"only lobby leader starts");
 check(room.CanShoot(a)&&!room.CanShoot(b),"server owns turn");auto turn=room.Turn();check(room.BeginShot(a)&&!room.BeginShot(a)&&!room.BeginShot(b),"one ball at a time");
 check(!room.FinishShot(b,100)&&room.FinishShot(a,50),"score belongs to shooter");check(room.Current()==b&&room.Turn()!=turn,"round-robin next turn");
 auto c=room.Join("Late",2);check(c&&!room.CanShoot(c)&&room.Players().back().forfeited,"late join waits for next round");
 check(room.BeginShot(b),"second player launches");room.Leave(b);check(room.FinishShot(b,20),"disconnected in-flight player keeps score");check(room.Current()==a,"forfeited player skipped");
 for(int i=0;i<4;++i){check(room.BeginShot(a)&&room.FinishShot(a,10),"remaining turns");}check(room.State()==icCompanionRoom::Phase::Results,"round ends after five balls per eligible player");check(room.Players()[0].score==90&&room.Players()[0].balls==5&&room.Players()[1].score==20,"independent totals");
 check(room.Touch(b,3)&&room.Start(a),"reconnected player can join next round");check(room.Players()[0].score==0&&!room.Players()[1].forfeited&&!room.Players()[2].forfeited,"round reset includes late arrivals");
 room.Touch(b,20);room.Touch(c,20);room.Poll(20);check(room.Current()==b,"timed-out current player cannot stall table");room.Interrupt();check(!room.Current()&&!room.InFlight()&&!room.CanShoot(b),"host suspension stops round");
 icCompanionRoom capacity;for(int i=0;i<8;++i)check(capacity.Join("Player",1)!=0,"eight seats");check(!capacity.Join("Ninth",1),"capacity bound");capacity.Leave(1);check(capacity.Join("Replacement",2)!=0,"offline seat reusable");
 std::puts("Companion room turns, scores, disconnects and capacity passed");}
