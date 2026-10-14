#include "gameplay.h"
#include "AiInput.h"
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"failed line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static b2Body* disc(b2World& world,b2Vec2 pos,float radius,float density){
    b2BodyDef def;def.position=pos;def.isBullet=true;
    auto body=world.CreateBody(&def);b2CircleDef shape;shape.radius=radius;shape.density=density;shape.restitution=.94f;shape.friction=0;
    body->CreateShape(&shape);body->SetMassFromShapes();return body;
}
static bool cornerEscape(bool recovery){
    b2AABB box;box.lowerBound.Set(-10,-10);box.upperBound.Set(10,10);
    b2World world(box,b2Vec2(0,0),false);
    // The actual table's right rail and lower-right end wall.
    b2BodyDef def;def.position.Set(.55f,0);auto side=world.CreateBody(&def);
    b2PolygonDef wall;wall.SetAsBox(.2f,2.025f);wall.restitution=.94f;wall.friction=0;side->CreateShape(&wall);
    def.position.Set(1.0f/3,-.835f);auto end=world.CreateBody(&def);
    wall.SetAsBox(.175f,.2f);end->CreateShape(&wall);
    auto puck=disc(world,b2Vec2(.2975f,-.5825f),.0525f,.1f);
    gameplay::RailRecovery watchdog;
    for(int i=0;i<480;++i){
        if(recovery)puck->SetLinearVelocity(watchdog.update(puck->GetPosition(),puck->GetLinearVelocity(),gameplay::step));
        world.Step(gameplay::step,8);
        if(puck->GetPosition().x<.25f&&puck->GetPosition().y>-.5f)return true;
    }
    return false;
}
int main(){
    CHECK(!cornerEscape(false)); // Reproduce the square corner's stationary trap.
    CHECK(cornerEscape(true));
    gameplay::RailRecovery watchdog;
    b2Vec2 velocity(0,1);
    for(int i=0;i<121;++i)velocity=watchdog.update(b2Vec2(.295f,.3f),velocity,gameplay::step);
    CHECK(velocity.x<-.3f&&velocity.y==1); // A moving rail puck also escapes.
    watchdog.reset();velocity=b2Vec2(.5f,1);
    for(int i=0;i<240;++i)velocity=watchdog.update(b2Vec2(.295f,.3f),velocity,gameplay::step);
    CHECK(velocity.x==.5f); // Ordinary wall rebounds keep their trajectory.
    CHECK(std::fabs(gameplay::reboundX(.4f)-.195f)<.0001f);
    CHECK(std::fabs(gameplay::reboundX(-.4f)+.195f)<.0001f);
    CHECK(std::fabs(gameplay::reboundX(1.19f))<.0001f);
    b2AABB box;box.lowerBound.Set(-10,-10);box.upperBound.Set(10,10);
    b2World world(box,b2Vec2(0,0),false);
    auto puck=disc(world,b2Vec2(.22f,-.2f),.0525f,.1f);
    auto paddle=disc(world,b2Vec2(0,.55f),.05f,1);
    AiInput ai(1,puck,paddle);puck->SetLinearVelocity(b2Vec2(0,1));
    bool returned=false;
    for(int i=0;i<180;++i){
        CHECK(!ICEFAIL(ai.Process(nullptr,gameplay::step)));
        CHECK(paddle->GetLinearVelocity().Length()<=.951f);
        world.Step(gameplay::step,8);
        CHECK(std::isfinite(puck->GetPosition().x));
        if(puck->GetLinearVelocity().y<-.1f)returned=true;
    }
    CHECK(returned); // The controller must actually return a reachable incoming shot.
    // Fast shots bounce from a thin rail instead of passing through it.
    b2BodyDef railDef;railDef.position.Set(.39f,0);auto rail=world.CreateBody(&railDef);
    b2PolygonDef railShape;railShape.SetAsBox(.05f,1);railShape.restitution=.94f;rail->CreateShape(&railShape);
    puck->SetXForm(b2Vec2(0,-.3f),0);puck->SetLinearVelocity(b2Vec2(2.8f,0));
    bool bounced=false;for(int i=0;i<30;++i){world.Step(gameplay::step,8);if(puck->GetLinearVelocity().x<0)bounced=true;}
    CHECK(bounced);
    // Targets and repeated driving stay within the player's own half.
    auto player=disc(world,b2Vec2(0,-.43f),.05f,1);
    for(int i=0;i<240;++i){gameplay::drive(player,b2Vec2(20,20),0,gameplay::step,2.8f,45);world.Step(gameplay::step,8);}
    CHECK(player->GetPosition().x<.301f&&player->GetPosition().y<-.06f);
    std::puts("Gameplay checks passed: rebound prediction, AI return, CCD rail, paddle limits");
}
