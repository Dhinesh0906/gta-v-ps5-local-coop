#pragma once
/* Freestanding orbit math. Each secondary view owns its orientation. */
typedef struct { float yaw,pitch; } CoopOrbit;
static __attribute__((unused)) float coop_wrap_angle(float angle) {
    if(!(angle>-10000&&angle<10000))return 0;
    while(angle>3.141592654f)angle-=6.283185307f;
    while(angle<-3.141592654f)angle+=6.283185307f;
    return angle;
}
static __attribute__((unused)) float coop_sin(float angle) {
    float x=coop_wrap_angle(angle),s=x*x;
    return x*(1+s*(-1.0f/6+s*(1.0f/120+s*(-1.0f/5040+s*(1.0f/362880+
        s*(-1.0f/39916800+s/6227020800.0f))))));
}
static __attribute__((unused)) float coop_cos(float angle) {return coop_sin(angle+1.570796327f);}
static __attribute__((unused)) void coop_orbit_update(CoopOrbit *orbit,float right,float down,float dt) {
    if(!(dt>0))return;
    if(dt>0.1f)dt=0.1f;
    orbit->yaw=coop_wrap_angle(orbit->yaw+right*dt*2.2f);
    orbit->pitch+=down*dt*1.5f;
    if(orbit->pitch<-0.15f)orbit->pitch=-0.15f;
    if(orbit->pitch>1.1f)orbit->pitch=1.1f;
}
static __attribute__((unused)) void coop_orbit_position(const CoopOrbit *orbit,const float center[3],float out[3]) {
    float radius=4*coop_cos(orbit->pitch);
    out[0]=center[0]+coop_sin(orbit->yaw)*radius;
    out[1]=center[1]-coop_cos(orbit->yaw)*radius;
    out[2]=center[2]+1.3f+4*coop_sin(orbit->pitch);
}
static __attribute__((unused)) void coop_orbit_movement(float yaw,float right,float forward,float out[2]) {
    float sine=coop_sin(yaw),cosine=coop_cos(yaw);
    out[0]=2*(right*cosine-forward*sine);
    out[1]=2*(right*sine+forward*cosine);
}
