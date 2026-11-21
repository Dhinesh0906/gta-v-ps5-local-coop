#ifndef COOP_LIVE_POLICY_H
#define COOP_LIVE_POLICY_H
#include <stdint.h>
enum live_session_action {LIVE_SESSION_KEEP,LIVE_SESSION_STOP,LIVE_SESSION_REARM,LIVE_SESSION_FAULT};
struct live_session_observation {
    uint32_t wanted,same,camera_state,camera_error,camera_busy;
    uint32_t output_state,output_error,output_busy,output_owned,pipeline_error;
};
static enum live_session_action live_session_plan(const struct live_session_observation *s){
    if(!s->wanted)return LIVE_SESSION_STOP;
    if(s->pipeline_error||(s->output_state==6&&s->output_error!=13))return LIVE_SESSION_FAULT;
    if(s->camera_state!=5)return s->same?LIVE_SESSION_KEEP:LIVE_SESSION_STOP;
    if(s->output_state!=5||s->camera_busy||s->output_busy||s->output_owned)return LIVE_SESSION_STOP;
    if(s->camera_error&&s->same)return LIVE_SESSION_FAULT;
    return LIVE_SESSION_REARM;
}
static int live_output_available(uint64_t tail,uint64_t completed,uint32_t current,uint32_t slot){
    return slot<3u&&(!tail||(completed>tail&&current!=slot+3u));
}
static int64_t fixed_position(uint32_t bits){
    uint32_t exponent=(bits>>23)&255u;uint64_t m=bits&0x7fffffu;
    if(exponent)m|=0x800000u;
    int shift=(int)exponent-134;
    if(shift>8)return INT64_MAX;
    uint64_t value=shift>=0?(m<<(unsigned)shift):(-shift<64?m>>(unsigned)-shift:0);
    return bits>>31?-(int64_t)value:(int64_t)value;
}
static int live_position_matches(uint32_t sample,uint32_t pose){
    if((sample&0x7fffffffu)>0x471c4000u||(pose&0x7fffffffu)>0x471c4000u)return 0;
    int64_t d=fixed_position(sample)-fixed_position(pose);if(d<0)d=-d;
    return d<=1310;
}
#endif
