#pragma once
#include "coop-camera.h"
/* Contextual third-person framing; does not clone a native player camera. (AI-assisted) */
typedef struct {float distance,height,focus,fov;} CoopCameraFraming;
static float coop_cam_clamp(float x,float lo,float hi){return x<lo?lo:x>hi?hi:x;}
static inline float coop_cam_abs(float x){return x<0?-x:x;}
static int coop_cam_finite(float x){return x==x&&x>-100000&&x<100000;}
static float coop_cam_approach(float value,float target,float dt,float tau){
    if(!(dt>0))return value;
    dt=coop_cam_clamp(dt,0,.1f);
    return value+(target-value)*(dt/(tau+dt));
}
static CoopCameraFraming coop_cam_framing(unsigned in_vehicle,unsigned vehicle_class,
    float length,float height,float speed,unsigned view,float native_fov,unsigned aiming){
    float zoom=view==0?.8f:view==1?1.f:1.18f;
    if(!coop_cam_finite(length))length=4;
    if(!coop_cam_finite(height))height=1.5f;
    if(!coop_cam_finite(speed))speed=0;
    if(!coop_cam_finite(native_fov)||native_fov<20||native_fov>110)native_fov=60;
    length=coop_cam_clamp(length,1,65);height=coop_cam_clamp(height,.5f,15);
    speed=coop_cam_clamp(speed,0,150);
    CoopCameraFraming p={5.f*zoom,.9f,.15f,native_fov};
    if(!in_vehicle){
        p.distance+=coop_cam_clamp(speed-3,0,5)*.10f;
        if(aiming){p.distance=2.4f;p.height=.65f;p.focus=.4f;p.fov=coop_cam_clamp(native_fov-5,25,85);}
        return p;
    }
    p.distance=coop_cam_clamp(length*.8f+2.5f,4.5f,22)*zoom;
    p.height=1.f+height*.35f;p.focus=height*.20f;
    if(vehicle_class==8||vehicle_class==13){p.distance=4.5f*zoom;p.height=1.1f;p.focus=.35f;}
    if(vehicle_class==15){p.distance=coop_cam_clamp(length*.9f+3,8,40)*zoom;p.height=2.2f;p.focus=.5f;}
    if(vehicle_class==16){p.distance=coop_cam_clamp(length+3,9,55)*zoom;p.height=2.5f;p.focus=.5f;}
    if(vehicle_class==14){p.distance=coop_cam_clamp(length*.8f+3,6,24)*zoom;p.height=1.8f;p.focus=.4f;}
    p.distance+=coop_cam_clamp(speed/45,0,1.5f);
    p.fov=coop_cam_clamp(native_fov+speed*.055f,25,95);
    return p;
}
static void coop_cam_blend(CoopCameraFraming *p,const CoopCameraFraming *target,float dt){
    p->distance=coop_cam_approach(p->distance,target->distance,dt,.18f);
    p->height=coop_cam_approach(p->height,target->height,dt,.18f);
    p->focus=coop_cam_approach(p->focus,target->focus,dt,.18f);
    p->fov=coop_cam_approach(p->fov,target->fov,dt,.20f);
}
/* A result belongs to the exact ray origin/direction it sampled. Stale hits
 * must not pull the camera toward an old wall after turning or teleporting. */
static int coop_cam_same_ray(const float a[3],const float b[3],const float c[3],const float d[3]){
    float moved=0,dot=0,norm_a=0,norm_b=0;
    for(unsigned n=0;n<3;n++){
        float delta=a[n]-c[n],x=b[n]-a[n],y=d[n]-c[n];
        moved+=delta*delta;dot+=x*y;norm_a+=x*x;norm_b+=y*y;
    }
    return moved<1&&dot>0&&norm_a>.01f&&norm_b>.01f&&dot*dot>norm_a*norm_b*.94f;
}
static float coop_cam_hit_fraction(const float from[3],const float to[3],const float hit[3]){
    float dot=0,norm=0;
    for(unsigned n=0;n<3;n++){
        if(!coop_cam_finite(hit[n]))return 1;
        float delta=to[n]-from[n];dot+=(hit[n]-from[n])*delta;norm+=delta*delta;
    }
    if(norm<.01f)return 1;
    return coop_cam_clamp(dot/norm-.04f,.08f,1);
}
/* Wall avoidance moves inward immediately, then eases outward after the
 * obstruction clears. Camera recovery must never step through the wall. (AI-assisted) */
static inline float coop_cam_collision_release(float current,float target,float dt){
    current=coop_cam_clamp(current,.08f,1);target=coop_cam_clamp(target,.08f,1);
    return target<current?target:coop_cam_approach(current,target,dt,.24f);
}
static inline float coop_cam_angle_approach(float value,float target,float dt,float tau){
    float delta=coop_wrap_angle(target-value);
    return coop_wrap_angle(value+delta*(coop_cam_clamp(dt,0,.1f)/(tau+coop_cam_clamp(dt,0,.1f))));
}
/* Follow walking forward as well as vehicles, without rotating the walking
 * basis during strafing, backwards movement, aiming or active free look. (AI-assisted) */
static inline int coop_cam_should_recenter(unsigned vehicle,float speed,float forward,float strafe,
    unsigned blocked,unsigned now,unsigned last_look){
    if(blocked||!coop_cam_finite(speed)||now-last_look<(vehicle?550u:850u))return 0;
    return vehicle?speed>.25f:(speed>.35f&&forward>.35f&&coop_cam_abs(strafe)<.35f);
}
/* Flight follows the entity's actual forward vector; angle extraction has
 * no guessed memory offsets or platform maths-library calls. (AI-assisted) */
static inline float coop_cam_flight_pitch(const float forward[3]){
    for(unsigned n=0;n<3;n++)if(!coop_cam_finite(forward[n])||coop_cam_abs(forward[n])>2)return 0;
    float h2=forward[0]*forward[0]+forward[1]*forward[1],z=forward[2];
    if(h2+z*z<.1f)return 0;
    if(h2<.00001f)return z<0?-1.1f:1.1f;
    float horizontal=1;
    for(unsigned n=0;n<8;n++)horizontal=(horizontal+h2/horizontal)*.5f;
    float x=coop_cam_abs(z)/horizontal,offset=0;
    if(x>1){x=1/x;offset=1.570796327f;}
    float angle=.785398163f*x-x*(x-1)*(.2447f+.0663f*x);
    if(offset)angle=offset-angle;
    return coop_cam_clamp(z<0?-angle:angle,-1.1f,1.1f);
}
