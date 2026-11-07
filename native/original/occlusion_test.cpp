#include "occlusion.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
using namespace disney::original;
static void require(bool condition,const char* name) {
    if(!condition){std::cerr<<"FAIL: "<<name<<'\n';std::exit(1);}
}
static Occluder hidden() {
    Occluder out;
    // Intersection of x<-1, y<-1, z<-1, x+y<-2 and y+z<-2.
    out.planes={Plane{1,0,0,1},Plane{0,1,0,1},Plane{0,0,1,1},
                Plane{1,1,0,2},Plane{0,1,1,2}};
    return out;
}
int main() {
    require(!isOccluded({}, {0,0,0},1,7),"empty original occluder list");
    std::vector<Occluder> list{hidden()};
    require(isOccluded(list,{-10,-10,-10},1,7),"sphere behind all five planes");
    require(list[0].hitCount==7,"original counter increment");
    require(!isOccluded(list,{0,-10,-10},1,7),"one separating plane keeps sphere visible");
    require(list[0].hitCount==7,"visible sphere does not increment counter");
    // The equality boundary is deliberately visible in the original branch.
    require(!isOccluded(list,{-2,-10,-10},1,7),"tangent sphere uses <=, not <");
    require(isOccluded(list,{-2.001f,-10,-10},1,0),"just beyond tangent sphere");
    require(list[0].hitCount==7,"zero increment");
    list={hidden(),hidden(),hidden()};
    list[0].planes[4]={0,0,0,0};
    require(isOccluded(list,{-10,-10,-10},1,3),"later occluder is considered");
    require(list[0].hitCount==0 && list[1].hitCount==3 && list[2].hitCount==0,
            "only first matching original occluder is updated");
    list={hidden()};list[0].hitCount=std::numeric_limits<std::uint32_t>::max();
    require(isOccluded(list,{-10,-10,-10},1,2),"counter wrap result");
    require(list[0].hitCount==1,"counter wraps exactly like ADDU");
    for(auto& plane:list[0].planes) plane={16777216.0f,1.0f,-16777216.0f,-0.5f};
    require(isOccluded(list,{1,1,1},0,0),"original float operation ordering preserved");
    std::cout<<"Original FUN_001624c0: empty, five planes, visibility, tangency, "
               "first match, counter wrap and float ordering passed\n";
}
