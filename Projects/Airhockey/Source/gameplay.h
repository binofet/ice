#ifndef AIRHOCKEY_GAMEPLAY_H
#define AIRHOCKEY_GAMEPLAY_H
#include "../Box2D/Box2D.h"
#include <algorithm>
#include <cmath>
namespace gameplay {
constexpr float step = 1.0f / 120.0f;
constexpr float puckLimit = 2.8f;
inline float clamp(float x,float lo,float hi){return std::max(lo,std::min(x,hi));}
inline b2Vec2 limit(b2Vec2 v,float speed){float length=v.Length();return length>speed?(speed/length)*v:v;}
inline b2Vec2 bounds(b2Vec2 target,int player){
    target.x=clamp(target.x,-0.295f,0.295f);
    target.y=player==0?clamp(target.y,-0.575f,-0.065f):clamp(target.y,0.065f,0.69f);
    return target;
}
// Mirror the unfolded trajectory back into the interval between the side rails.
inline float reboundX(float x,float halfWidth=0.2975f){
    float width=2*halfWidth;
    float phase=std::fmod(x+halfWidth,2*width);if(phase<0)phase+=2*width;
    return (phase<=width?phase:2*width-phase)-halfWidth;
}
inline b2Vec2 aiTarget(b2Vec2 puck,b2Vec2 velocity,b2Vec2 paddle){
    if(puck.y<0.02f){
        float x=velocity.y>0.05f?reboundX(puck.x+velocity.x*clamp((0.53f-puck.y)/velocity.y,0.0f,2.0f)):puck.x*0.4f;
        return bounds(b2Vec2(x,0.56f),1);
    }
    // Line the shot up toward the opponent's goal rather than down the rail.
    b2Vec2 shot = b2Vec2(-0.025f,-0.74f)-puck;
    shot.Normalize();
    b2Vec2 behind = bounds(puck-0.13f*shot,1);
    if((behind-paddle).LengthSquared()<0.0025f)
        return bounds(puck+0.18f*shot,1);
    return behind;
}
// Arcade recovery for trajectories that remain parallel to a side rail. A square
// corner cannot be struck inward when the paddle is confined to the same rail.
class RailRecovery {
    float elapsed = 0;
public:
    void reset(){elapsed=0;}
    b2Vec2 update(b2Vec2 position,b2Vec2 velocity,float dt){
        bool onRail=std::fabs(position.x)>0.275f;
        bool pinnedCorner=onRail&&(position.y<-.55f||position.y>.68f);
        bool parallel=onRail&&std::fabs(velocity.x)<.12f;
        elapsed=(pinnedCorner||parallel)?elapsed+dt:0;
        if(elapsed>=1.0f){
            velocity.x=position.x>0?-.35f:.35f;
            if(pinnedCorner&&std::fabs(velocity.y)<.25f)
                velocity.y=position.y<0?.3f:-.3f;
            elapsed=0;
        }
        return velocity;
    }
};
inline void drive(b2Body* paddle,b2Vec2 target,int player,float dt,float speed,float acceleration){
    b2Vec2 position=paddle->GetPosition(),clamped=bounds(position,player);
    if((position-clamped).LengthSquared()>0.000001f)paddle->SetXForm(clamped,0);
    b2Vec2 desired=limit((1.0f/dt)*(bounds(target,player)-clamped),speed);
    b2Vec2 velocity=paddle->GetLinearVelocity();
    paddle->SetLinearVelocity(velocity+limit(desired-velocity,acceleration*dt));
    paddle->SetAngularVelocity(0);
}
}
#endif
