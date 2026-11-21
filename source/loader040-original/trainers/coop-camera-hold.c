/* Bounded camera-identity probe. Creates an owned inactive camera, exposes
 * its view only on explicit request, restores gameplay on lease/cancel, and
 * forwards the complete current co-op owner. It never moves either actor. */
typedef unsigned long long u64;typedef unsigned int u32;typedef unsigned char u8;
#include "coop-native-handlers.h"
#include "coop-camera.h"
#ifdef COOP_CONTEXT_CAMERA
#ifdef CAMERA_HOLD_TEST
#define AWARE_CODE
#else
#define AWARE_CODE __attribute__((section(".aware.text"),noinline))
#endif
#include "coop-camera-handlers.h"
#include "coop-context-camera.h"
#endif
#ifndef CAMERA_HOLD_TEST
#include <coop-camera-hold-profile.h>
#endif
typedef struct {float x,y,z,w;} Vec4;
typedef struct {u64 *result;u32 count,pad;u64 *args;u32 vectors,pad1;float *dest[4];Vec4 temp[4];} Context;
typedef struct {u32 script_frame,render_frame;float position[3];u32 rendering;} CameraSample;
typedef struct {
    u64 magic,forward,owner;
    u32 state,error,cancel,busy;
    u32 duration,started,last_frame,p1,p2,cam,owned,visible;
    float target[3],position[3],readback[3];
    u32 original_rendering,before,after,restores,visits;
    float gameplay[3];u32 request_view,activated,active_frames,reserved[2];
    u32 history_head,history_count;CameraSample history[128];
    u32 follow_enabled,follow_updates,follow_errors,last_follow_frame;
    float follow_yaw,follow_pitch;
    u32 follow_reason;float follow_delta;
    u32 persistent,epoch;
} CameraHold;
_Static_assert(sizeof(CameraHold)==3280,"Camera hold ABI");
#ifdef CAMERA_HOLD_TEST
#define SECTION(s)
static u64 native(u64,u32,u64*,u64*);
static u32 owner_word(u32);
static u32 story_network(void);
#else
#define SECTION(s) __attribute__((section(s),used,visibility("hidden")))
static u32 owner_word(u32);
static u32 story_network(void){return *(volatile u8*)0x5abefb8ULL;}
#endif
SECTION(".hold.data") volatile CameraHold camera_hold;
#ifdef COOP_CONTEXT_CAMERA
typedef struct {
    u32 ped,cam,epoch,last_ms,last_look,vehicle,model,vehicle_class;
    u32 shape,shape_started,last_shape_ms,hits,shape_errors,updates;
    float length,height,previous_yaw,limit,last_fov;
    CoopCameraFraming framing;
    float ray_from[3],ray_to[3],limit_from[3],limit_to[3];
    u32 last_bind,look_mask,zoom_mask,previous_buttons,foot_view,vehicle_view,look_behind,view_changes;
    float previous_pitch,smooth_yaw,smooth_pitch,foot_fov,vehicle_fov,limit_current;
    float smooth_center[3];
    u32 fov_seeded,context,transitions;
    float flight_pitch,model_width;
    u32 flight_context,flight_updates;
    u32 recenter_updates,recenter_blocked;
} CameraAware;
_Static_assert(sizeof(CameraAware)<=816,"Camera extension must fit before constants");
SECTION(".hold.data") CameraAware camera_aware;
#ifndef CAMERA_HOLD_TEST
SECTION(".aware.layout") const u32 camera_aware_layout[]={
    sizeof(CameraAware),__builtin_offsetof(CameraAware,shape),__builtin_offsetof(CameraAware,hits),
    __builtin_offsetof(CameraAware,shape_errors),__builtin_offsetof(CameraAware,updates),
    __builtin_offsetof(CameraAware,framing),__builtin_offsetof(CameraAware,limit_current),
    __builtin_offsetof(CameraAware,look_behind),__builtin_offsetof(CameraAware,foot_view),
    __builtin_offsetof(CameraAware,vehicle_view),__builtin_offsetof(CameraAware,view_changes),
    __builtin_offsetof(CameraAware,transitions),__builtin_offsetof(CameraAware,flight_pitch),
    __builtin_offsetof(CameraAware,flight_context),__builtin_offsetof(CameraAware,flight_updates),
    __builtin_offsetof(CameraAware,recenter_updates),__builtin_offsetof(CameraAware,recenter_blocked)};
#endif
#endif
#ifndef CAMERA_HOLD_TEST
static u32 owner_word(u32 at){return *(volatile u32*)(camera_hold.owner+at);}
#endif
static u64 floating(float f){union{float f;u32 i;}a;a.f=f;return a.i;}
static float as_float(u64 v){union{float f;u32 i;}a;a.i=(u32)v;return a.f;}
static u64 pointer(const void *p){u64 v=(u64)p;__asm__ volatile("":"+r"(v));return v;}
#ifndef CAMERA_HOLD_TEST
#ifdef COOP_CONTEXT_CAMERA
__attribute__((noinline))
#endif
static u64 native(u64 address,u32 count,u64 *args,u64 *out){
    u64 local[4]={0},padded[32]={0};Context c={0};
    for(u32 n=0;n<count&&n<32;n++)padded[n]=args[n];
    if(count>32)return 0;c.result=out?out:local;c.count=count;c.args=padded;
    ((void(*)(Context*))address)(&c);
    if(c.vectors<=4)for(u32 n=0;n<c.vectors;n++)if(c.dest[n]){c.dest[n][0]=c.temp[n].x;c.dest[n][2]=c.temp[n].y;c.dest[n][4]=c.temp[n].z;}
    return c.result[0];
}
#endif
#define N(name,...) native(COOP_H_##name,sizeof((u64[]){0,##__VA_ARGS__})/8-1,((u64[]){0,##__VA_ARGS__})+1,0)
#define V(name,out,...) native(COOP_H_##name,sizeof((u64[]){0,##__VA_ARGS__})/8-1,((u64[]){0,##__VA_ARGS__})+1,out)
#define F(x) floating((float)(x))
#define P(x) pointer(x)
static float absf(float f){return f<0?-f:f;}
static u32 position(float *p){for(u32 n=0;n<3;n++)if(p[n]!=p[n]||absf(p[n])>40000)return 0;return 1;}
static void unpack(float *p,u64 *raw){for(u32 n=0;n<3;n++)p[n]=as_float(raw[n]);}
#ifdef COOP_CONTEXT_CAMERA
#ifdef CAMERA_HOLD_TEST
static void test_aware_owner_yaw(float yaw);
static void test_aware_owner_pitch(float pitch);
#endif
static void aware_owner_yaw(float yaw){
#ifndef CAMERA_HOLD_TEST
    *(volatile u32*)(camera_hold.owner+0x8274)=floating(yaw);
#else
    test_aware_owner_yaw(yaw);
#endif
}
static void aware_owner_pitch(float pitch){
#ifndef CAMERA_HOLD_TEST
    *(volatile u32*)(camera_hold.owner+0x8278)=floating(pitch);
#else
    test_aware_owner_pitch(pitch);
#endif
}
static u32 aware_binding(u32 action){
    u64 address=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,action,1);
#ifndef CAMERA_HOLD_TEST
    if(address!=0x548eb50ULL+(u64)action*64u)return 0;
#endif
    const char *s=(const char*)address;
    if(!s||s[0]!='b'||s[1]!='_'||s[2]<'0'||s[2]>'9')return 0;
    u32 id=s[2]-'0';
    if(s[3]){if(s[3]<'0'||s[3]>'9'||s[4])return 0;id=id*10u+s[3]-'0';}
    switch(id){
      case 4:return 0x10;case 5:return 0x40;case 6:return 0x80;case 7:return 0x20;
      case 16:return 2;case 25:return 4;case 39:return 0x100000;
      case 30:return 0x4000;case 31:return 0x2000;case 32:return 0x8000;case 33:return 0x1000;
      case 34:return 0x400;case 35:return 0x100;case 36:return 0x800;case 37:return 0x200;
      default:return 0;
    }
}
AWARE_CODE static void aware_poll_shape(const float from[3],const float to[3],u32 apply){
    CameraAware *s=&camera_aware;
    if(!s->shape)return;
    u32 hit=0,entity=0;u64 result[4]={0},normal[4]={0};
    u32 status=N(GET_SHAPE_TEST_RESULT,s->shape,P(&hit),P(result),P(normal),P(&entity));
    if(status==1)return;
    s->shape=0;
    if(status!=2){s->shape_errors++;return;}
    if(!apply||!coop_cam_same_ray(s->ray_from,s->ray_to,from,to))return;
    float at[3];unpack(at,result);
    s->limit=hit?coop_cam_hit_fraction(s->ray_from,s->ray_to,at):1.f;
    for(u32 n=0;n<3;n++){s->limit_from[n]=s->ray_from[n];s->limit_to[n]=s->ray_to[n];}
    if(hit)s->hits++;
}
AWARE_CODE static void aware_position(u32 now,CoopOrbit *orbit,u32 aim_target,float center[3],float p[3]){
    CameraAware *s=&camera_aware;
    u32 fresh=s->ped!=camera_hold.p2||s->cam!=camera_hold.cam||s->epoch!=camera_hold.epoch;
    if(fresh){
        /* Consume an outstanding job before starting another; do not leak a
         * handle on each spawn or vehicle transition. (AI-assisted) */
        aware_poll_shape(center,center,0);
        u32 shape=s->shape;
        *s=(CameraAware){0};s->shape=shape;s->ped=camera_hold.p2;s->cam=camera_hold.cam;s->epoch=camera_hold.epoch;
        s->length=4;s->height=1.5f;s->limit=s->limit_current=1;s->last_fov=-1;s->last_look=now;s->previous_yaw=orbit->yaw;
        s->previous_pitch=orbit->pitch;s->smooth_yaw=orbit->yaw;s->smooth_pitch=orbit->pitch;
        s->foot_fov=s->vehicle_fov=50;s->previous_buttons=owner_word(0x80f0);
        s->foot_view=N(GET_FOLLOW_PED_CAM_VIEW_MODE);s->vehicle_view=N(GET_FOLLOW_VEHICLE_CAM_VIEW_MODE);
        if(s->foot_view>2)s->foot_view=1;if(s->vehicle_view>2)s->vehicle_view=1;
        for(u32 n=0;n<3;n++)s->smooth_center[n]=center[n];
    }
    float dt=s->last_ms?coop_cam_clamp((now-s->last_ms)*.001f,0,.1f):.016f;
    s->last_ms=now;
    u32 vehicle=0;
    if(N(IS_PED_IN_ANY_VEHICLE,camera_hold.p2,0))vehicle=N(GET_VEHICLE_PED_IS_IN,camera_hold.p2,0);
    if(vehicle&&!N(DOES_ENTITY_EXIST,vehicle))vehicle=0;
    u32 context_change=vehicle!=s->vehicle;
    if(context_change){s->transitions++;s->limit=s->limit_current=1;s->last_look=now;}
    if(vehicle){
        u64 raw[4]={0};float at[3];V(GET_ENTITY_COORDS,raw,vehicle,1);unpack(at,raw);
        if(position(at))for(u32 n=0;n<3;n++)center[n]=at[n];
        u32 model=N(GET_ENTITY_MODEL,vehicle);
        if(model!=s->model||vehicle!=s->vehicle){
            u64 lo[4]={0},hi[4]={0};float low[3],high[3];
            N(GET_MODEL_DIMENSIONS,model,P(lo),P(hi));unpack(low,lo);unpack(high,hi);
            if(position(low)&&position(high)&&high[1]>low[1]&&high[2]>low[2]){
                s->length=coop_cam_clamp(high[1]-low[1],1,65);s->height=coop_cam_clamp(high[2]-low[2],.5f,15);
                s->model_width=coop_cam_clamp(high[0]-low[0],.5f,80);
            }else{s->length=4;s->height=1.5f;}
            s->model=model;s->vehicle_class=N(GET_VEHICLE_CLASS,vehicle);s->last_look=now;
        }
    }
    u32 held=owner_word(0x80f0),aiming=!vehicle&&(held&owner_word(0x8420))!=0;
    float delta=coop_wrap_angle(orbit->yaw-s->previous_yaw);
    float pitch_delta=orbit->pitch-s->previous_pitch;
    if(!s->last_bind||now-s->last_bind>1000||context_change){
        s->look_mask=aware_binding(vehicle?79:26);s->zoom_mask=aware_binding(0);s->last_bind=now;
    }
    s->look_behind=!aiming&&s->look_mask&&(held&s->look_mask)!=0;
    if((held&~s->previous_buttons&s->zoom_mask)&&!(held&8)){
        if(vehicle)s->vehicle_view=(s->vehicle_view+1)%3;else s->foot_view=(s->foot_view+1)%3;
        s->view_changes++;
    }
    s->previous_buttons=held;
    /* Recenter only the owned P2 orientation. Vehicle steering never uses
     * this yaw, and fresh look input always wins over the automatic turn. */
    u32 sticks=owner_word(0x810c),rx=(sticks>>16)&255u,ry=sticks>>24;
    u32 looking=rx<104||rx>152||ry<104||ry>152||coop_cam_abs(delta)>.0001f||coop_cam_abs(pitch_delta)>.0001f;
    u32 camera_blocked=looking||s->look_behind||aiming||(held&owner_word(0x8428));
    if(camera_blocked)s->last_look=now;
    float speed=as_float(N(GET_ENTITY_SPEED,vehicle?vehicle:camera_hold.p2));
    s->flight_context=vehicle&&(s->vehicle_class==15||s->vehicle_class==16||
        ((s->model==1483171323u||s->model==884483972u||s->model==2069146067u)&&N(IS_ENTITY_IN_AIR,vehicle)));
    float flight_target=.08f;
    if(s->flight_context){
        u64 raw[4]={0};float forward[3];V(GET_ENTITY_FORWARD_VECTOR,raw,vehicle);unpack(forward,raw);
        s->flight_pitch=coop_cam_flight_pitch(forward);s->flight_updates++;
        flight_target=coop_cam_clamp(.08f-s->flight_pitch*(s->vehicle_class==16?.9f:.6f),-1.05f,1.05f);
    }else s->flight_pitch=0;
    float forward=as_float(owner_word(0x8104)),strafe=as_float(owner_word(0x8100));
    if(coop_cam_should_recenter(vehicle,speed,forward,strafe,camera_blocked,now,s->last_look)){
        float heading=as_float(N(GET_ENTITY_HEADING,vehicle?vehicle:camera_hold.p2));
        if(coop_cam_finite(heading)&&coop_cam_abs(heading)<36000){
            orbit->yaw=coop_cam_angle_approach(orbit->yaw,heading*.01745329252f,dt,vehicle?(s->flight_context?.28f:.22f):.38f);
            aware_owner_yaw(orbit->yaw);
        }
        if(vehicle){orbit->pitch=coop_cam_approach(orbit->pitch,flight_target,dt,s->flight_context?.35f:.75f);aware_owner_pitch(orbit->pitch);}
        s->recenter_updates++;
    }else if(camera_blocked)s->recenter_blocked++;
    s->previous_yaw=orbit->yaw;s->previous_pitch=orbit->pitch;s->vehicle=vehicle;
    u32 view=vehicle?s->vehicle_view:s->foot_view;
    /* Seed each context from P1 only when P1 is in that same context, in
     * third person, and is not aiming. Subsequent P1 aiming/zoom never
     * changes P2's independent field of view. (AI-assisted) */
    u32 category=vehicle?2:1;
    if(!(s->fov_seeded&category)&&!camera_hold.visible&&!N(IS_AIM_CAM_ACTIVE)){
        u32 p1_vehicle=N(IS_PED_IN_ANY_VEHICLE,camera_hold.p1,0);
        u32 p1_view=p1_vehicle?N(GET_FOLLOW_VEHICLE_CAM_VIEW_MODE):N(GET_FOLLOW_PED_CAM_VIEW_MODE);
        if(!!p1_vehicle==!!vehicle&&p1_view<3){
            float native_fov=as_float(N(GET_GAMEPLAY_CAM_FOV));
            if(native_fov>=45&&native_fov<=70){
                if(vehicle)s->vehicle_fov=native_fov;else s->foot_fov=native_fov;
                s->fov_seeded|=category;
            }
        }
    }
    float fov=vehicle?s->vehicle_fov:s->foot_fov;
    float extent=s->length;
    if(s->flight_context&&s->model_width*.8f>extent)extent=s->model_width*.8f;
    CoopCameraFraming desired=coop_cam_framing(vehicle,s->vehicle_class,extent,s->height,speed,view,fov,aiming);
    if(fresh)s->framing=desired;else coop_cam_blend(&s->framing,&desired,dt);
    /* Smooth the framing while keeping the control/aim orientation owned by
     * the controller. Suppress positional lag after entering a vehicle or
     * teleporting, and keep the visible camera level during bike lean. */
    float displacement=0;for(u32 n=0;n<3;n++){float x=center[n]-s->smooth_center[n];displacement+=x*x;}
    for(u32 n=0;n<3;n++){
        float next=coop_cam_approach(s->smooth_center[n],center[n],dt,vehicle?.04f:.025f);
        s->smooth_center[n]=(fresh||context_change||displacement>400)?center[n]:next;
    }
    s->smooth_yaw=coop_cam_angle_approach(s->smooth_yaw,orbit->yaw,dt,aiming?.01f:.035f);
    s->smooth_pitch=coop_cam_approach(s->smooth_pitch,orbit->pitch,dt,aiming?.01f:.045f);
    float yaw=s->look_behind?coop_wrap_angle(s->smooth_yaw+3.141592654f):s->smooth_yaw;
    float pitch=coop_cam_clamp(s->smooth_pitch,-1.1f,1.1f),radius=s->framing.distance*coop_cos(pitch);
    p[0]=s->smooth_center[0]+coop_sin(yaw)*radius;
    p[1]=s->smooth_center[1]-coop_cos(yaw)*radius;
    p[2]=s->smooth_center[2]+s->framing.height+s->framing.distance*coop_sin(pitch);
    if(aiming&&!aim_target){p[0]+=.45f*coop_cos(orbit->yaw);p[1]+=.45f*coop_sin(orbit->yaw);}
    float focus[3]={center[0],center[1],center[2]+s->framing.focus};
    aware_poll_shape(focus,p,1);
    if(!coop_cam_same_ray(s->limit_from,s->limit_to,focus,p))s->limit=1;
    if(s->look_behind!=s->context){s->context=s->look_behind;s->limit=s->limit_current=1;}
    if(!s->shape&&now-s->last_shape_ms>=45){
        for(u32 n=0;n<3;n++){s->ray_from[n]=focus[n];s->ray_to[n]=p[n];}
        s->shape=N(START_SHAPE_TEST_SWEPT_SPHERE,F(focus[0]),F(focus[1]),F(focus[2]),F(p[0]),F(p[1]),F(p[2]),F(.25f),19,vehicle?vehicle:camera_hold.p2,7);
        s->shape_started=now;s->last_shape_ms=now;
        if(!s->shape)s->shape_errors++;
    }
    s->limit_current=coop_cam_collision_release(s->limit_current,s->limit,dt);
    float limit=s->limit_current;
    for(u32 n=0;n<3;n++)p[n]=focus[n]+(p[n]-focus[n])*limit;
    if(coop_cam_abs(s->framing.fov-s->last_fov)>.02f){N(SET_CAM_FOV,camera_hold.cam,F(s->framing.fov));s->last_fov=s->framing.fov;}
    N(SET_CAM_COORD,camera_hold.cam,F(p[0]),F(p[1]),F(p[2]));
    if(aim_target)N(POINT_CAM_AT_ENTITY,camera_hold.cam,aim_target,F(0),F(0),F(.4f),1);
    else if(aiming){
        float x=center[0]-100*coop_sin(orbit->yaw),y=center[1]+100*coop_cos(orbit->yaw),z=center[2]+.4f-100*coop_sin(orbit->pitch);
        N(POINT_CAM_AT_COORD,camera_hold.cam,F(x),F(y),F(z));
    }else N(POINT_CAM_AT_ENTITY,camera_hold.cam,vehicle?vehicle:camera_hold.p2,F(0),F(0),F(s->framing.focus),1);
    s->updates++;
}
#endif
static u32 record_camera(u32 frame,u32 rendering){
    u64 raw[4]={0};float p[3];
    if(rendering==0xffffffffu)V(GET_GAMEPLAY_CAM_COORD,raw);
    else if(rendering==camera_hold.cam)V(GET_CAM_COORD,raw,camera_hold.cam);
    else return 0;
    unpack(p,raw);if(!position(p))return 0;
    if(rendering==0xffffffffu)for(u32 n=0;n<3;n++)camera_hold.gameplay[n]=p[n];
    CameraSample sample={frame,frame,{p[0],p[1],p[2]},rendering};
#ifndef CAMERA_HOLD_TEST
    sample.render_frame=*(volatile u32*)CAMERA_RENDER_FRAME;
#endif
    u32 head=camera_hold.history_head;camera_hold.history[head&127u]=sample;
    __atomic_store_n(&camera_hold.history_head,head+1u,__ATOMIC_RELEASE);
    if(camera_hold.history_count<128)camera_hold.history_count++;
    return 1;
}
static void suspend_view(void){
    if(!camera_hold.owned)return;
    u32 cam=camera_hold.cam,current=N(GET_RENDERING_CAM);
    /* A script-camera activation can be requested but not acknowledged yet.
     * Cancel our pending request too, while preserving any foreign camera. */
    if(current==cam||(current==0xffffffffu&&camera_hold.visible)){
        N(RENDER_SCRIPT_CAMS,0,0,0,1,0,0);camera_hold.restores++;
    }
    if(N(DOES_CAM_EXIST,cam))N(SET_CAM_ACTIVE,cam,0);
    camera_hold.visible=0;camera_hold.after=N(GET_RENDERING_CAM);
}
static void release_camera(void){
    u32 cam=camera_hold.cam;
    if(camera_hold.owned){
        suspend_view();
        if(N(DOES_CAM_EXIST,cam)){N(SET_CAM_ACTIVE,cam,0);N(DESTROY_CAM,cam,0);}
        if(N(DOES_CAM_EXIST,cam)){camera_hold.state=4;return;}
    }
    camera_hold.cam=camera_hold.owned=camera_hold.visible=0;camera_hold.after=N(GET_RENDERING_CAM);camera_hold.state=5;
}
static u32 follow_camera(u32 frame){
    if(!camera_hold.follow_enabled||!camera_hold.owned||camera_hold.last_follow_frame==frame)return 1;
    u64 raw[4]={0};float center[3],p[3],back[3];V(GET_ENTITY_COORDS,raw,camera_hold.p2,1);unpack(center,raw);
    CoopOrbit orbit={as_float(owner_word(0x8274)),as_float(owner_word(0x8278))};
    for(u32 n=0;n<3;n++)camera_hold.target[n]=center[n];
    camera_hold.follow_yaw=orbit.yaw;camera_hold.follow_pitch=orbit.pitch;
    if(!position(center)||absf(center[0])>=15000||absf(center[1])>=15000||center[2]<=-100||center[2]>=2500){camera_hold.follow_reason=1;return 0;}
    if(orbit.yaw!=orbit.yaw||orbit.pitch!=orbit.pitch||absf(orbit.yaw)>3.142f||absf(orbit.pitch)>1.101f){camera_hold.follow_reason=2;return 0;}
    /* A targeting task alone does not centre a script-camera view. While the
     * owned on-foot controller has a valid L2 target, orbit behind P2 toward
     * that target and point the camera at the same chest coordinate used by
     * the gun task/reticle. Never change P1's view or P2's free-look input. */
    u32 aim_target=0,held=owner_word(0x80f0);
    if((owner_word(0x8f40)&2)&&!owner_word(0x8450)&&
       (held&owner_word(0x8420))&&!(held&owner_word(0x8428))){
        u32 target=owner_word(0x8f58);
        if(target&&target!=camera_hold.p1&&target!=camera_hold.p2&&N(DOES_ENTITY_EXIST,target)&&!N(IS_ENTITY_DEAD,target,0)){
            float at[3];V(GET_ENTITY_COORDS,raw,target,1);unpack(at,raw);
            float dx=at[0]-center[0],dy=at[1]-center[1],dz=at[2]-center[2],distance=dx*dx+dy*dy+dz*dz;
            if(position(at)&&distance>=1&&distance<=10000&&dx*dx+dy*dy>.01f){
                float heading=as_float(N(GET_HEADING_FROM_VECTOR_2D,F(dx),F(dy)));
                if(heading==heading&&absf(heading)<=36000){
                    orbit.yaw=coop_wrap_angle(heading*.01745329252f);aim_target=target;
                }
            }
        }
    }
    /* Read P2's existing orientation; never consume its pad a second time or
     * change P1's movement/camera. Keep the orbit above the pedestrian's feet. */
#ifdef COOP_CONTEXT_CAMERA
    if(camera_hold.follow_enabled!=3)aware_position(N(GET_GAME_TIMER),&orbit,aim_target,center,p);
    else {
#endif
    if(orbit.pitch<-.1f)orbit.pitch=-.1f;
    float radius=6.f*coop_cos(orbit.pitch);
    p[0]=center[0]+coop_sin(orbit.yaw)*radius;
    p[1]=center[1]-coop_cos(orbit.yaw)*radius;
    p[2]=center[2]+1.f+6.f*coop_sin(orbit.pitch);
    N(SET_CAM_COORD,camera_hold.cam,F(p[0]),F(p[1]),F(p[2]));
    /* The native ped origin is near body centre. A head-height offset aimed
     * this cropped viewport above the character and clipped the whole body. */
    N(POINT_CAM_AT_ENTITY,camera_hold.cam,aim_target?aim_target:camera_hold.p2,F(0),F(0),F(aim_target?.4f:0),1);
#ifdef COOP_CONTEXT_CAMERA
    }
#endif
    V(GET_CAM_COORD,raw,camera_hold.cam);unpack(back,raw);
    for(u32 n=0;n<3;n++){camera_hold.position[n]=p[n];camera_hold.readback[n]=back[n];}
    if(!position(back)){camera_hold.follow_reason=3;return 0;}
    float delta=0;
    for(u32 n=0;n<3;n++)if(absf(back[n]-p[n])>delta)delta=absf(back[n]-p[n]);
    camera_hold.follow_delta=delta;
    /* An active camera's returned transform may lag SET_CAM_COORD. Its actual
     * native transform, not the requested point, identifies render captures.
     * Keep a bounded separation guard and the strict initial inactive audit. */
    if(delta>25.f){camera_hold.follow_reason=4;return 0;}
    camera_hold.follow_reason=0;
    camera_hold.follow_yaw=orbit.yaw;camera_hold.follow_pitch=orbit.pitch;
    camera_hold.last_follow_frame=frame;camera_hold.follow_updates++;return 1;
}
static void camera_hold_tick(void){
#ifdef COOP_CONTEXT_CAMERA
    if((!camera_hold.state||camera_hold.state==5)&&camera_aware.shape){float nowhere[3]={0};aware_poll_shape(nowhere,nowhere,0);}
#endif
    if(!camera_hold.state||camera_hold.state==5)return;
    if(camera_hold.state==4){release_camera();return;}
    u32 now=N(GET_GAME_TIMER);
    if(camera_hold.cancel||!camera_hold.owner||owner_word(0x8010)!=2||owner_word(0x8018)||owner_word(0x8020)!=6||
       owner_word(0x8050)!=camera_hold.p1||owner_word(0x8054)!=camera_hold.p2||owner_word(0x8208)!=3||
       story_network()||!camera_hold.p2||
       !N(DOES_ENTITY_EXIST,camera_hold.p2)||N(IS_ENTITY_DEAD,camera_hold.p2,0)||
       (camera_hold.persistent?(camera_hold.persistent!=1||camera_hold.duration||!camera_hold.follow_enabled||(camera_hold.request_view&&camera_hold.request_view!=2)):
        (camera_hold.duration<500||camera_hold.duration>30000||
         (camera_hold.started&&now-camera_hold.started>=35000)||
         (camera_hold.activated&&now-camera_hold.activated>=camera_hold.duration)))){
        camera_hold.state=4;release_camera();return;
    }
    u32 frame=N(GET_FRAME_COUNT);if(frame==camera_hold.last_frame)return;camera_hold.last_frame=frame;
    if(owner_word(0x8218)||N(IS_PAUSE_MENU_ACTIVE)){
        if(camera_hold.state!=6){suspend_view();camera_hold.state=6;}
        return;
    }
    if(camera_hold.state==6){camera_hold.state=camera_hold.owned?7:1;camera_hold.reserved[1]=frame;}
    /* Allow the normal gameplay camera to update before alternating again. */
    if(camera_hold.state==7){if(frame-camera_hold.reserved[1]<3u)return;camera_hold.state=2;}
    if(camera_hold.state==1){
        camera_hold.started=now;camera_hold.original_rendering=N(GET_RENDERING_CAM);
        if(camera_hold.original_rendering!=0xffffffffu){camera_hold.error=1;release_camera();return;}
        u64 raw[4]={0};float p[3];V(GET_ENTITY_COORDS,raw,camera_hold.p2,1);unpack(p,raw);
        if(!position(p)){camera_hold.error=2;release_camera();return;}
        for(u32 n=0;n<3;n++)camera_hold.target[n]=p[n];p[1]-=6;p[2]+=3;
        u32 cam=N(CREATE_CAM,P("DEFAULT_SCRIPTED_CAMERA"),0);
        if(!cam||cam==0xffffffffu){camera_hold.error=3;release_camera();return;}
        camera_hold.cam=cam;camera_hold.owned=1;
        if(!N(DOES_CAM_EXIST,cam)){camera_hold.error=3;release_camera();return;}
        N(SET_CAM_FOV,cam,F(60));N(SET_CAM_COORD,cam,F(p[0]),F(p[1]),F(p[2]));
        N(POINT_CAM_AT_ENTITY,cam,camera_hold.p2,F(0),F(0),F(1),1);
        V(GET_CAM_COORD,raw,cam);float back[3];unpack(back,raw);
        for(u32 n=0;n<3;n++){camera_hold.position[n]=p[n];camera_hold.readback[n]=back[n];if(back[n]!=back[n]||absf(back[n]-p[n])>.02f)camera_hold.error=4;}
        if(camera_hold.error){release_camera();return;}
        camera_hold.state=2;
    }
    if(!follow_camera(frame)){camera_hold.follow_errors++;camera_hold.error=9;release_camera();return;}
    if(camera_hold.state==2){
        u32 rendering=N(GET_RENDERING_CAM);
        if(rendering!=0xffffffffu){camera_hold.error=6;release_camera();return;}
        if(!record_camera(frame,rendering)){camera_hold.error=5;release_camera();return;}
        if(!camera_hold.request_view)return;
        if(camera_hold.request_view>2){camera_hold.error=8;release_camera();return;}
        camera_hold.before=N(GET_RENDERING_CAM);
        if(camera_hold.before!=0xffffffffu){camera_hold.error=6;release_camera();return;}
        N(SET_CAM_ACTIVE,camera_hold.cam,1);N(RENDER_SCRIPT_CAMS,1,0,0,1,0,0);
        camera_hold.activated=now;camera_hold.visible=1;camera_hold.state=3;camera_hold.reserved[0]=1;camera_hold.reserved[1]=now;
        camera_hold.after=N(GET_RENDERING_CAM);
        if(camera_hold.after==camera_hold.cam){camera_hold.active_frames++;record_camera(frame,camera_hold.after);}
        return;
    }
    if(camera_hold.state==3){
        camera_hold.after=N(GET_RENDERING_CAM);
        if(camera_hold.request_view==2){
            u32 expected=camera_hold.visible?camera_hold.cam:0xffffffffu;
            if(camera_hold.after!=expected){
                if((camera_hold.after==camera_hold.cam||camera_hold.after==0xffffffffu)&&now-camera_hold.reserved[1]<500){
                    if(!record_camera(frame,camera_hold.after)){camera_hold.error=5;release_camera();}
                    return;
                }
                camera_hold.error=7;release_camera();return;
            }
            if(!record_camera(frame,camera_hold.after)){camera_hold.error=5;release_camera();return;}
            if(camera_hold.visible){N(RENDER_SCRIPT_CAMS,0,0,0,1,0,0);N(SET_CAM_ACTIVE,camera_hold.cam,0);camera_hold.visible=0;}
            else {N(SET_CAM_ACTIVE,camera_hold.cam,1);N(RENDER_SCRIPT_CAMS,1,0,0,1,0,0);camera_hold.visible=1;}
            camera_hold.after=N(GET_RENDERING_CAM);camera_hold.reserved[0]++;camera_hold.reserved[1]=now;
            camera_hold.active_frames++;
            return;
        }
        if(camera_hold.after==camera_hold.cam){camera_hold.active_frames++;return;}
        if(now-camera_hold.activated<500&&camera_hold.after==0xffffffffu)return;
        camera_hold.error=7;release_camera();
    }
}
SECTION(".entry") void _start(Context *context){
    camera_hold.visits++;
    if(camera_hold.magic==0x31444c4f484d4143ULL&&!__atomic_exchange_n(&camera_hold.busy,1,__ATOMIC_ACQUIRE)){
        camera_hold_tick();__atomic_store_n(&camera_hold.busy,0,__ATOMIC_RELEASE);
    }
#ifndef CAMERA_HOLD_TEST
    if(camera_hold.forward)((void(*)(Context*))camera_hold.forward)(context);
#else
    (void)context;
#endif
}
