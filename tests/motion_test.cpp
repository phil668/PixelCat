#include "../src/motion.h"
#include <cassert>
int main() {
    assert(pose(10,10,true).rise==0);
    assert(pose(10,10,true).scaleY==1);
    assert(pose(10.325,10,false).rise>11.9);
    assert(pose(11,10,false).rise==0);
    assert(clampOrigin(-200,-100,500,192)==-100);
    assert(clampOrigin(600,-100,500,192)==308);
    assert(clampOrigin(0,0,100,192)==0);
    Behavior b;
    b.phase=9; b.direction=-1;
    assert(b.advance(0.125,0,0,1000,192,false)==0);
    assert(b.direction==1);
    b.direction=1;
    assert(b.advance(0.125,808,0,1000,192,false)==808);
    assert(b.direction==-1);
    double clock=b.clock;
    assert(b.advance(1,500,0,1000,192,true)==500);
    assert(b.clock==clock);
    b.sinceInteraction=119.9;
    b.advance(0.125,500,0,1000,192,false);
    assert(b.sleeping && b.activity()==Activity::Sleep);
    assert(b.frame()>=2 && b.frame()<=3);
    b.interact();
    assert(!b.sleeping && b.sinceInteraction==0 && b.activity()==Activity::Idle);
    b.autoSleep=false; b.sinceInteraction=1000;
    b.advance(1,500,0,1000,192,false); assert(!b.sleeping);
    b.roaming=false; b.phase=9;
    assert(b.advance(0.125,500,0,1000,192,false)==500);
    b.nap(); assert(b.sleeping);
    assert(b.advance(2,500,0,1000,192,false)==500);
    b.interact(); b.roaming=true; b.phase=9;
    assert(b.advance(0.125,-1000,-1000,-200,192,false)>=-1000);
    b.playSpecial(0);
    assert(b.activity()==Activity::Special && !b.sleeping);
    double remaining=b.specialRemaining;
    assert(b.advance(1,200,0,1000,192,true)==200);
    assert(b.specialRemaining==remaining);
    for(int i=0;i<7;++i) assert(b.advance(1,200,0,1000,192,false)==200);
    assert(b.special==0);
    b.advance(1,200,0,1000,192,false);
    assert(b.special==-1 && b.activity()==Activity::Idle);
    b.nap(); b.playSpecial(1); assert(!b.sleeping && b.special==1);
    b.playSpecial(2); assert(b.special==2 && b.specialRemaining==8);
    b.interact(); assert(b.special==-1);
    b.playSpecial(2); b.nap(); assert(b.special==-1 && b.sleeping);
    b.playSpecial(99); assert(b.sleeping && b.special==-1);
    for (int index=0; index<SpecialCount; ++index) {
        b.playSpecial(index);
        assert(b.special==index && b.activity()==Activity::Special);
        for(int t=0;t<8;++t) b.advance(1,200,0,1000,192,false);
        assert(b.special==-1 && b.activity()==Activity::Idle);
    }
    b.playSpecial(SpecialCount); assert(b.special==-1);
}
