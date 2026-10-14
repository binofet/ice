#ifndef ICE_PLAYGROUND_GAME_MODEL_H
#define ICE_PLAYGROUND_GAME_MODEL_H
#include <algorithm>
#include <cmath>
#include <vector>
namespace playground {
struct Vec { float x=0,z=0; Vec(){} Vec(float a,float b):x(a),z(b){} };
inline float distance(Vec a,Vec b){float x=a.x-b.x,z=a.z-b.z;return std::sqrt(x*x+z*z);}
inline float clamp(float x,float a,float b){return std::max(a,std::min(b,x));}
struct Input {float forward=0,turn=0;bool action=false,sneak=false;};
struct Cargo {Vec pos,velocity; bool delivered=false; Cargo(Vec p,Vec v,bool d):pos(p),velocity(v),delivered(d){} };
struct Simulation {
    int mode; Vec player,velocity; float angle=0,time=0,alarm=0; int score=0,checkpoint=0,laps=0,attached=-1;
    bool won=false,lost=false; std::vector<Cargo> cargo;std::vector<Vec> guards;
    bool voxels[16][4][16]={}; int cursorX=8,cursorZ=8,layer=1;unsigned ticks=0;
    explicit Simulation(int m):mode(m){reset();}
    void reset(){
        player=Vec();velocity=Vec();angle=time=alarm=0;score=checkpoint=laps=0;attached=-1;won=lost=false;ticks=0;
        cargo.clear(); guards.clear();
        if(mode==0){player=Vec(0,8);cargo.push_back({Vec(-8,-5),Vec(),false});cargo.push_back({Vec(9,4),Vec(),false});cargo.push_back({Vec(3,-9),Vec(),false});}
        if(mode==1){player=Vec(-11,10);guards={Vec(-8,0),Vec(8,-6)};}
        if(mode==2){player=Vec(-10,10);angle=0;}
        if(mode==3){for(int x=0;x<16;++x)for(int y=0;y<4;++y)for(int z=0;z<16;++z)voxels[x][y][z]=y==0||(y==1&&x>2&&x<6&&z>3&&z<8);cursorX=cursorZ=8;layer=1;}
    }
    static bool wall(Vec p){return (std::fabs(p.x+4)<1.2f&&p.z>-6&&p.z<6)||(std::fabs(p.x-4)<1.2f&&p.z>-9&&p.z<3);}
    Vec target() const {static Vec points[]={Vec(-10,-10),Vec(10,-10),Vec(10,10),Vec(-10,10)};return points[checkpoint%4];}
    void update(float dt,Input input){
        if(won||lost)return;time+=dt;++ticks;
        if(mode==0){
            angle+=input.turn*2.3f*dt;velocity.x+=std::sin(angle)*input.forward*8*dt;velocity.z-=std::cos(angle)*input.forward*8*dt;
            player.x+=velocity.x*dt;player.z+=velocity.z*dt;
            if(std::fabs(player.x)>14){player.x=clamp(player.x,-14,14);velocity.x*=-.5f;}
            if(std::fabs(player.z)>14){player.z=clamp(player.z,-14,14);velocity.z*=-.5f;}
            if(input.action){if(attached>=0)attached=-1;else for(size_t i=0;i<cargo.size();++i)if(!cargo[i].delivered&&distance(player,cargo[i].pos)<3){attached=(int)i;break;}}
            for(size_t i=0;i<cargo.size();++i){auto& c=cargo[i];if(c.delivered)continue;
                if((int)i==attached){c.velocity.x+=(player.x-c.pos.x)*6*dt;c.velocity.z+=(player.z-c.pos.z)*6*dt;c.velocity.x*=std::exp(-2*dt);c.velocity.z*=std::exp(-2*dt);}
                c.pos.x+=c.velocity.x*dt;c.pos.z+=c.velocity.z*dt;
                if(distance(c.pos,Vec())<2.5f&&distance(c.velocity,Vec())<3){c.delivered=true;++score;if(attached==(int)i)attached=-1;}
            }won=score==3;
        } else if(mode==1){
            Vec next(player.x+input.turn*(input.sneak?2:5)*dt,player.z-input.forward*(input.sneak?2:5)*dt);
            next.x=clamp(next.x,-12,12);next.z=clamp(next.z,-12,12);if(!wall(next))player=next;
            guards[0]=Vec(-8+std::sin(time*.6f)*2, std::cos(time*.6f)*8);guards[1]=Vec(8+std::cos(time*.7f)*2,std::sin(time*.7f)*8);
            bool seen=false;for(auto g:guards){float d=distance(g,player);if(d<(input.sneak?3:6)){
                bool blocked=false;for(int j=1;j<20;++j){float t=j/20.f;if(wall(Vec(g.x+(player.x-g.x)*t,g.z+(player.z-g.z)*t)))blocked=true;}
                seen|=!blocked;}}
            alarm=clamp(alarm+(seen?dt: -dt*.5f),0,2);lost=alarm>=2;
            if(!score&&distance(player,Vec(11,-11))<2)score=1;
            won=score&&distance(player,Vec(-11,10))<2;
        } else if(mode==2){
            float speed=velocity.x;speed+=input.forward*14*dt;speed*=std::exp(-.45f*dt);speed=clamp(speed,-5,18);
            angle+=input.turn*speed*.16f*dt;Vec next(player.x+std::sin(angle)*speed*dt,player.z-std::cos(angle)*speed*dt);
            bool track=std::fabs(next.x)<13&&std::fabs(next.z)<13&&(std::fabs(next.x)>7||std::fabs(next.z)>7);
            if(track)player=next;else speed*=std::exp(-6*dt);velocity.x=speed;
            if(distance(player,target())<2.5f){++checkpoint;if(checkpoint%4==0)++laps;}won=laps>=3;
        } else if(mode==3){if(input.action){voxels[cursorX][layer][cursorZ]=!voxels[cursorX][layer][cursorZ];++score;}}
    }
};
}
#endif
