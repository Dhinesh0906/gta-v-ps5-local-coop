/* Fixed-wing NPC input overrides consumed by exact-build CPlane::physics.
 * These are not car task actions or custom forces. Only the owned P2 pilot
 * can publish them. Defaults off until the console trial is enabled. */
#include "coop-plane-input.h"
#include "coop-plane-runtime.c"
typedef struct {
    u32 enabled,active,error,vehicle;u64 entity;
    float original[4],last[4];
    u32 ticks,restores,rejected,last_bind_ms,masks[4],axes[2],bindings_valid,last_frame,engine_starts,reserved;
} PlaneControls;
_Static_assert(sizeof(PlaneControls)==112,"Plane controls ABI");
COOP_SECTION(".coop.plane") volatile PlaneControls plane_controls;
#ifdef GTA_COOP_TEST
static u32 plane_capture(u32,u32,u64*,float*);
static u32 plane_apply(u32,u32,u64,const float*);
static void plane_restore(u32,u64,const float*,const float*);
#else
static u32 plane_fixture_table_matches(u32,u64);
static u64 plane_entity(u32 h,u32 ped){
    u64 v=hover_lookup(h);if(!v||(*(volatile u64*)v!=0x405ad50ULL&&!plane_fixture_table_matches(h,v)&&!plane_runtime_table_matches(h,v))||*(volatile u32*)(v+0xbf8)!=1||
       *(volatile u64*)(0x405ad50ULL+0x670)!=0x27204d0ULL)return 0;
    if(ped){
        u64 p=hover_lookup(ped);if(!p||ped==session.p1)return 0;
        i32 index=(i32)*(volatile signed char*)(v+0xc71);u32 seats=*(volatile u8*)(v+0xc70);
        if(index<0||(u32)index>=seats||*(volatile u64*)(v+0xc78+(u32)index*8)!=p)return 0;
    }
    return v;
}
static u32 plane_capture(u32 h,u32 ped,u64 *entity,float *original){
    u64 v=plane_entity(h,ped);if(!v)return 0;
    for(u32 n=0;n<4;n++){
        float f=((volatile float*)(v+0x1ec8))[n];if(f!=f||coop_abs(f)>10000)return 0;
        original[n]=f;
    }
    *entity=v;return 1;
}
static u32 plane_apply(u32 h,u32 ped,u64 expected,const float *values){
    u64 v=plane_entity(h,ped);if(!v||v!=expected)return 0;
    for(u32 n=0;n<4;n++)if(values[n]!=values[n]||coop_abs(values[n])>1)return 0;
    for(u32 n=0;n<4;n++)((volatile float*)(v+0x1ec8))[n]=values[n];
    return 1;
}
static void plane_restore(u32 h,u64 expected,const float *original,const float *last){
    u64 v=plane_entity(h,0);if(!v||v!=expected)return;
    for(u32 n=0;n<4;n++){
        /* A stock task or a new driver may already have replaced an input.
         * Restore only values still equal to this owner's last publication. */
        if(*(volatile u32*)(v+0x1ec8+n*4)==(u32)F(last[n]))
            *(volatile u32*)(v+0x1ec8+n*4)=(u32)F(original[n]);
    }
}
#endif
static void plane_controls_release(void){
    if(!plane_runtime_release())return;
    if(plane_controls.active){
        float original[4],last[4];for(u32 n=0;n<4;n++){original[n]=plane_controls.original[n];last[n]=plane_controls.last[n];}
        plane_restore(plane_controls.vehicle,plane_controls.entity,original,last);plane_controls.restores++;
    }
    plane_controls.active=plane_controls.vehicle=0;plane_controls.entity=0;
}
static u32 plane_axis_label(const char *s,u32 x){
    if(!s||s[0]!='b'||s[1]!='_')return 0;
    if(x&&s[2]=='1'&&s[3]=='9'&&!s[4])return 4;
    if(!x&&s[2]=='1'&&s[3]=='8'&&!s[4])return 5;
    if(x&&s[2]=='2'&&s[3]=='8'&&!s[4])return 6;
    if(!x&&s[2]=='2'&&s[3]=='7'&&!s[4])return 7;
    return 0;
}
#ifndef GTA_COOP_TEST
__attribute__((section(".coop.extensions"),noinline))
#endif
static u32 plane_bindings(u32 now){
    if(plane_controls.bindings_valid&&now-plane_controls.last_bind_ms<1000)return 1;
    static const u32 actions[6]={87,88,89,90,107,110};u32 masks[4],axes[2];
    for(u32 n=0;n<6;n++){
        u64 label=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,actions[n],1);
#ifndef GTA_COOP_TEST
        if(label!=0x548eb50ULL+(u64)actions[n]*64u)return 0;
#endif
        if(n<4){masks[n]=button_from_label((const char*)label);if(!masks[n])return 0;}
        else{axes[n-4]=plane_axis_label((const char*)label,n==4);if(!axes[n-4])return 0;}
    }
    for(u32 n=0;n<4;n++)plane_controls.masks[n]=masks[n];
    for(u32 n=0;n<2;n++)plane_controls.axes[n]=axes[n];
    plane_controls.last_bind_ms=now;plane_controls.bindings_valid=1;return 1;
}
static float plane_button(u32 mask){
    float value=(session.input.held&mask)?1:0;
    /* ScePad's analog triggers keep partial throttle/braking available. */
    if(mask==0x200&&session.data[9])value=(float)session.data[9]/255.0f;
    else if(mask==0x100&&session.data[8])value=(float)session.data[8]/255.0f;
    return value;
}
#ifdef GTA_COOP_TEST
static u32 plane_input_settings(CoopPlaneInputSettings*);
#else
__attribute__((section(".coop.extensions"),noinline))
static u32 plane_input_settings(CoopPlaneInputSettings *s){
    /* Same settings and per-action inversion as the primary plane task.
     * Read settings only; never copy P1's held input or history into P2. */
    s->deadzone=*(volatile float*)0x5ee4310ULL;s->flags=*(volatile u32*)0x5ee4314ULL;
    if(s->deadzone!=s->deadzone||s->deadzone<0||s->deadzone>=1)return 0;
    static const u32 actions[6]={87,88,89,90,107,110};
    for(u32 n=0;n<6;n++){
        s->multiplier[n]=*(volatile i32*)(0x57c1610ULL+0x1e68+(u64)actions[n]*72);
        if(s->multiplier[n]<-1||s->multiplier[n]>1)return 0;
    }
    return 1;
}
#endif
#ifdef GTA_COOP_TEST
__attribute__((noinline))
#else
__attribute__((section(".coop.extensions"),noinline))
#endif
static void plane_controls_tick(void){
    if(!plane_controls.enabled||session.state!=2||session.mode!=6||session.phase!=3||session.stop||session.error||
       session.paused||!session.connected||!session.input.armed||network()||!session.p2||session.p2==session.p1||
       !player_controls.p2_collision||player_controls.action==5){
        plane_controls_release();return;
    }
    /* The Story hook may run repeatedly in one rendered frame. Gate before
     * vehicle/binding natives, while still releasing immediately on pause,
     * disconnect or stop above. (AI-assisted) */
    if(plane_controls.last_frame==session.last_frame)return;
    plane_controls.last_frame=session.last_frame;
    if(!N(IS_PED_IN_ANY_VEHICLE,session.p2,0)){plane_controls_release();return;}
    u32 v=N(GET_VEHICLE_PED_IS_IN,session.p2,0);
    if(!v||v!=player_controls.vehicle||(u32)N(GET_VEHICLE_CLASS,v)!=16||
       (u32)N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)!=session.p2){plane_controls_release();return;}
    if(!plane_bindings(player_controls.last_ms)){plane_controls.error=1;plane_controls_release();return;}
    if(plane_controls.vehicle!=v){
        plane_controls_release();if(plane_controls.active)return;u64 entity=0;float original[4];
        if(!plane_capture(v,session.p2,&entity,original)){plane_controls.error=2;plane_controls.rejected++;return;}
        plane_controls.vehicle=v;plane_controls.entity=entity;
        for(u32 n=0;n<4;n++)plane_controls.original[n]=original[n];
    }
    CoopPlaneInputSettings settings;
    if(!plane_input_settings(&settings)){plane_controls.error=4;plane_controls_release();return;}
    float buttons[4];
    for(u32 n=0;n<4;n++){
        u32 mask=plane_controls.masks[n];
        float f=plane_read_scalar(plane_button(mask),mask==0x100||mask==0x200?11:9,settings.multiplier[n],&settings);
        buttons[n]=f<0?0:f;
    }
    /* The normal primary pilot task (0x25c7659 / 0x25c7671) stores signed
     * roll directly. Negating it to imitate the special-physics fallback
     * reverses normal fixed-wing flight. L1/R1 are the pilot task's action
     * 89/90 pair, not the unrelated right-stick gun axis 66. */
    float values[4]={buttons[3]-buttons[2],
        plane_read_axis(session.data[plane_controls.axes[1]],settings.multiplier[5],&settings),
        plane_read_axis(session.data[plane_controls.axes[0]],settings.multiplier[4],&settings),
        buttons[0]-buttons[1]};
    if(!plane_apply(v,session.p2,plane_controls.entity,values)){
        plane_controls.error=3;plane_controls.rejected++;plane_controls_release();return;
    }
    /* Publish four axes as one validated snapshot to the physics callback.
     * An odd sequence is never consumed. (AI-assisted) */
    __atomic_add_fetch(&plane_controls.reserved,1,__ATOMIC_ACQ_REL);
    for(u32 n=0;n<4;n++)plane_controls.last[n]=values[n];
    __atomic_add_fetch(&plane_controls.reserved,1,__ATOMIC_RELEASE);
    plane_controls.active=1;plane_controls.error=0;plane_controls.ticks++;
    if(plane_runtime.enabled&&!plane_runtime.active&&!plane_runtime_publish(v,plane_controls.entity))plane_runtime.rejected++;
    else if(!plane_runtime.enabled&&plane_runtime.active)plane_runtime_release();
    if(values[3]>0&&!N(GET_IS_VEHICLE_ENGINE_RUNNING,v)){
        N(SET_VEHICLE_ENGINE_ON,v,1,1,0);plane_controls.engine_starts++;
    }
}
