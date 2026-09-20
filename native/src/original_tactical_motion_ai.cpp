#include "zhongyuan/original_state.hpp"
#include <algorithm>
#include <array>
namespace zhongyuan {
// Continuation after the role's B614 strategy attempt returned without a
// command. This does not consume RNG, repeat strategy selection, or advance
// A2DF's scan. Unported branches are explicit, resumable boundaries.
Json OriginalState::tactical_ai_motion(int target,int slot,int points,const Json &status,bool nearby,int *scratch21,int side,int enemy_commander) const {
    if((side!=0&&side!=128)||(side&&(enemy_commander<16||enemy_commander>=160))||target<0||target>=30||slot<0||slot>=(side?11:12)||points<0||points>40||!status.is_array()||status.size()!=24)
        return {{"error","无效电脑移动决策状态"}};
    for(const auto &v:status)if(!v.is_number_integer()||v<0||v>255)return {{"error","无效战术状态效果"}};
    const int base=side?0xdc2:0xdaa,stride=side?3:2,enemy_base=side?0xdaa:0xdc2,enemy_stride=side?2:3;
    const int friendly=side?48:16,hostile=side?16:48;
    const int id=bytes_[base+stride*slot],current=bytes_[base+1+stride*slot];
    if(id>=241||current<16||current>=160||(bytes_[0xe1a+current]&63)!=(friendly|slot))return {{"error","无效守军位置或占位"}};
    for(int cell=0;cell<256;++cell){const int token=bytes_[0xe1a+cell];if((token&48)==hostile){
        const int enemy=token&15;if(enemy>=(side?12:11)||bytes_[enemy_base+enemy_stride*enemy]>=241)return {{"error","无效敌军占位"}};
    }}
    const int role=side?1:bytes_[0xdec+2*slot],fort=rom_.deployment_tables(target)["defenders"][0];
    Json result={{"kind","pending"},{"branch","role"},{"slot",slot},{"role",role},{"position",current},{"fort",fort},{"direction",-1},{"target",-1}};
    auto held=[&](const char *kind,const char *branch){result["kind"]=kind;result["branch"]=branch;return result;};
    const std::array<int,4> delta={-1,-16,1,16},directions={2,0,3,1}; // L,U,R,D -> NES direction byte
    auto valid=[&](int cell,int d){return d==0?(cell&15)>0:d==1?cell>=32:d==2?(cell&15)<15:cell<144;};
    auto distance=[](int a,int b){return std::abs((a&15)-(b&15))+std::abs((a>>4)-(b>>4));};
    auto toward=[&](int goal){int mask=0;for(int d=0;d<4;++d){
        if(d==0&&(goal&15)<(current&15))mask|=1;
        if(d==1&&(goal>>4)<(current>>4))mask|=2;
        if(d==2&&(goal&15)>(current&15))mask|=4;
        if(d==3&&(goal>>4)>(current>>4))mask|=8;
    }return mask;};
    const int officer=bytes_[0x438+8*id+6],flags=status[(side?12:0)+slot];
    auto cost=[&](int cell){return rom_.tactical_ai_step_cost(officer,bytes_[0xf1a+cell]);};
    auto movable=[&](int mask){for(int d=0;d<4;++d){const int cell=(current+delta[d])&255;
        if((bytes_[0xe1a+cell]&16)||(flags&3)||((flags&12)&&bytes_[0xf1a+current]==1)||points<cost(cell))mask&=~(1<<d);
    }return mask;};
    auto strength=[&](int cell){const int token=bytes_[0xe1a+cell];if((token&48)!=hostile)return 255;
        const int b=0x438+8*bytes_[enemy_base+enemy_stride*(token&15)];const int n=(bytes_[b+3]>>2)*((bytes_[b+6]&15)+1);return n>255?254:n;
    };
    auto command=[&](const char *kind,int d){result["kind"]=kind;result["direction"]=directions[d];result["destination"]=(current+delta[d])&255;
        result["command"]=(slot<<4)|(std::string(kind)=="attack"?1:0);result["argument"]=directions[d];return result;};
    if(role==15)return held("scan","fort_guard");
    auto pursue=[&](int commander){
        result["branch"]="pursuit";result["target"]=commander;
        const int toward_mask=toward(commander),move_mask=movable(toward_mask);
        if(move_mask){
            // A4DA: a single bit bypasses B10E. Multiple bits use strict
            // minimum cost with L,U,R,D ties and 0xff as the sentinel.
            int best=-1,score=255;for(int d=0;d<4;++d)if(move_mask&(1<<d)){
                if((move_mask&(move_mask-1))==0)return command("move",d);
                if(valid(current,d)&&cost((current+delta[d])&255)<score){best=d;score=cost((current+delta[d])&255);}
            }
            return best<0?held("scan","pursuit"):command("move",best);
        }
        int mask=toward_mask;for(int d=0;d<4;++d)if((bytes_[0xe1a+((current+delta[d])&255)]&48)==friendly)mask&=~(1<<d);
        int best=-1,score=255;for(int d=0;d<4;++d)if(mask&(1<<d)){
            if((mask&(mask-1))==0){best=d;break;}
            const int value=strength((current+delta[d])&255);if(value<score){score=value;best=d;}
        }
        if(best<0||!valid(current,best)||(bytes_[0xe1a+((current+delta[best])&255)]&48)!=hostile)return held("scan","pursuit");
        return command("attack",best);
    };
    if((role&3)==0){
        int commander=side?enemy_commander:-1;for(int i=10;!side&&i>=0;--i)if(bytes_[0xdc2+3*i]!=255&&(bytes_[0xdc4+3*i]&128)){if(bytes_[0xdc2+3*i]>=241)return {{"error","无效进攻主将"}};commander=bytes_[0xdc3+3*i];break;}
        if(commander<16||commander>=160)return {{"error","没有有效进攻主将"}};
        return pursue(commander);
    }
    if((nearby||side)&&(role&1)){
        result["branch"]="nearby";
        // AF90 literally compares the direction mask, not the position, to
        // A0 for the lower boundary. Up/down remain in the initial mask.
        const int all=(current&15)==0?14:(current&15)==15?11:15;
        int unblocked=all;
        for(int d=0;d<4;++d)if((bytes_[0xe1a+((current+delta[d])&255)]&48)==friendly)unblocked&=~(1<<d);
        if(!unblocked)return held("scan","nearby");
        // B215 searches 24 entries; A700's score loop reads four more bytes
        // beyond that table. Preserve those literal offsets through ROM data.
        auto cell_at=[&](int index){const auto v=rom_.tactical_nearby_offset(index);
            const int y=(current>>4)+v[0],x=(current&15)+v[1];return y<1||y>=10||x<0||x>=16?-1:y*16+x;};
        auto score_at=[&](int index){const int cell=cell_at(index);return cell<0?255:strength(cell);};
        auto search=[&](int start){for(int i=start;i<24;++i){const int cell=cell_at(i);if(cell>=0&&(bytes_[0xe1a+cell]&48)==hostile)return i;}return 24;};
        int found=search(0);
        if(found==24){
            if((role&3)==3){auto out=pursue(fort);out["branch"]="nearby_fort_fallback";return out;}
        int commander=side?enemy_commander:-1;for(int i=10;!side&&i>=0;--i)if(bytes_[0xdc2+3*i]!=255&&(bytes_[0xdc4+3*i]&128)){if(bytes_[0xdc2+3*i]>=241)return {{"error","无效进攻主将"}};commander=bytes_[0xdc3+3*i];break;}
        if(commander<16||commander>=160)return {{"error","没有有效进攻主将"}};
            auto out=pursue(commander);out["branch"]="nearby_commander_fallback";return out;
        }
        if(found<4){
            if(scratch21)*scratch21=score_at(3); // B29E then A5D6: fourth adjacent score.
            int best=-1,value=255;for(int i=found;i<4;++i)if(score_at(i)<value){best=i;value=score_at(i);}
            const int to_dir[]={1,2,3,0};
            if(best>=0){const int d=to_dir[best];if(valid(current,d))return command("attack",d);}
        }
        const int mask=movable(all);if(!mask)return held("scan","nearby");
        found=search(4);if(found==24)return held("scan","nearby");
        const int masks[]={2,4,8,1,6,12,9,3};
        // Each ring chooses the weakest eligible enemy; strict comparisons
        // retain table order. Movement then uses the first L/U/R/D mask bit.
        for(int ring=found<12?0:found<20?1:2;ring<3;++ring){
            const int start=ring==0?4:ring==1?12:20;
            int best=-1,value=255;
            // B29E initializes $21; each score ring writes its fourth score
            // there, followed by the ring-specific direction mask filter.
            const int scratch_mask=ring==0?1:ring==1?12:3;
            if(scratch21)*scratch21=(mask&scratch_mask)?score_at(start+3):255;
            for(int i=0;i<8;++i){
                const int directions=ring==0?masks[i]:ring==1?masks[4+i/2]:masks[4+i%4];
                // A713-A739 mask only the first four of the final eight.
                if(!(directions&mask)&&(ring!=2||i<4))continue;
                const int score=score_at(start+i);if(score<value){best=i;value=score;}
            }
            if(best<0)continue;
            const int candidates=mask&(ring==0?masks[best]:ring==1?masks[4+best/2]:masks[4+best%4]);
            if(!candidates){if(ring==2)return held("scan","nearby");continue;}
            for(int d=0;d<4;++d)if(candidates&(1<<d))return command("move",d);
        }
        return held("scan","nearby");
    }
    if((role&3)!=2)return result;
    if((bytes_[0xe1a+fort]&48)==hostile)return held("pending","occupied_fort");
    if(bytes_[0xded+2*slot]!=255)return held("pending","cached_target");
    result["branch"]="fort_approach";
    if(distance(current,fort)==1){
        result["branch"]="fort_defense";int best=-1,score=255;
        for(int d=0;d<4;++d)if(valid(current,d)){const int value=strength(current+delta[d]);if(value<score){score=value;best=d;}}
        return best<0?held("pending","fort_flank"):command("attack",best);
    }
    if(scratch21)*scratch21=(fort&15)?fort-1:255; // A80E/A851 left-fort candidate.
    int goal=-1,score=255;for(int d:{1,2,3,0})if(valid(fort,d)){const int cell=fort+delta[d],value=distance(current,cell);if(value<score){goal=cell;score=value;}}
    result["target"]=goal;if(goal<0)return held("scan","fort_approach");
    const int mask=movable(toward(goal));for(int d=0;d<4;++d)if(mask&(1<<d))return command("move",d);
    return held("scan","fort_approach");
}
}
