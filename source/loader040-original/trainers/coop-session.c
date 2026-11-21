/* Owned secondary-character prototype. Modes 1–4 are bounded checks; mode 5
 * spawns automatically without added UI and stays until explicit cleanup or
 * the game session ends. This does not implement split-screen/full P2 play. */
typedef unsigned long long u64;
typedef unsigned int u32;
typedef int i32;
typedef unsigned char u8;
#include "coop-input.h"
#include "coop-camera.h"
#include "coop-native-handlers.h"
#include "coop-assist-handlers.h"
typedef struct { float x,y,z,w; } Vec4;
typedef struct {
    u64 *result; u32 count,pad0; u64 *args; u32 vectors,pad1;
    float *dest[4]; Vec4 temp[4];
} Context;
typedef struct {
    u64 magic,forward;
    volatile u32 state,stop,error,visits;
    u32 mode,duration,started,elapsed,last_frame;
    i32 primary_user,secondary_user,handle,primary_handle,read_result,close_result;
    u32 owned,p1,p2,model,model_owned,pad_reads,movement_ticks,jump_count,cleanup_count;
    u32 buttons,connected,armed,ground_ok;
    float position[3],offset[3],ground,heading,camera[3];
    u32 abi_valid;
    u64 raw_position[4],raw_offset[4];
    CoopInput input;
    u8 data[256];
    u32 phase,phase_started,last_task,moving,paused;
    float origin[3],basis_length_squared,origin_delta_squared;
    float p2_start[3],p2_position[3],distance_squared,max_distance_squared;
    u32 cam_handle,cam_owned,cam_requested,cam_updates,cam_toggles,cam_last_ms;
    float cam_position[3];CoopOrbit orbit;
    u32 cam_restores;float cam_ground,cam_readback[3];
    u32 cam_rendering,foreign_camera_seen,cam_visible,cam_activate_started;
    u32 audit_error,in_vehicle,startup_started,startup_retries;
    u32 p2_blip,blip_owned;
    u32 death_count,respawn_count,respawn_started,respawn_attempt;
    float death_position[3],respawn_position[3];
    u32 respawn_ground_checks,respawn_wait_reason;float death_heading;
    u32 return_to_p2_camera,collision_started,respawn_last_probe;
    u32 no_death,p1_original_invincible,p1_protection_owned,p1_original_model;
    u32 p1_protected,p2_protected;
    u32 last_pad_attempt,pad_reconnects,pad_waiting,automatic_spawns;
} Session;
_Static_assert(__builtin_offsetof(Session,position)==0x80,"Co-op position layout");
_Static_assert(__builtin_offsetof(Session,raw_position)==0xb0,"Co-op vector layout");
_Static_assert(__builtin_offsetof(Session,phase)==0x208,"Co-op phase layout");
_Static_assert(__builtin_offsetof(Session,cam_handle)==0x250,"Co-op camera layout");
_Static_assert(__builtin_offsetof(Session,no_death)==0x2f8,"Co-op protection layout");
_Static_assert(__builtin_offsetof(Session,last_pad_attempt)==0x310,"Live-character layout");
_Static_assert(sizeof(Session)==0x320,"Co-op mailbox size");
#ifdef GTA_COOP_TEST
#define COOP_SECTION(name)
#else
#define COOP_SECTION(name) __attribute__((section(name),used,visibility("hidden")))
#endif
COOP_SECTION(".coop.data")
volatile Session session;
typedef struct {
    u32 initialized,armed,held,suppressed,toggles,spawns,despawns,read_errors;
    u32 first_press_ms,waiting_second;
} PrimaryToggle;
_Static_assert(sizeof(PrimaryToggle)==40,"Primary toggle mailbox size");
#define PRIMARY_DOUBLE_R3_MS 400u
COOP_SECTION(".coop.toggle") volatile PrimaryToggle primary_toggle;
static u32 live_mode(void){return session.mode==5||session.mode==6;}
static void controls_reset_for_ped(void);
static void controls_release_vehicle_streaming(void);
static u32 road_runtime_owns(u32);
static u32 road_runtime_release(void);
static u32 oppressor_controls_release(void);
static u32 oppressor_controls_owns(u32);
static u32 player_assists_release(void);
static void player_assist_aim(u32,const float*);
static void player_motorcycle_tick(u32);
static u32 hover_controls_release(void);
static void plane_controls_release(void);
static u32 plane_runtime_release(void);
static u32 aircraft_defer_action(u32,u32);
static void aircraft_buttons_release(void);
static void plane_weapons_release(void);
static u32 aircraft_bomb_context(u32);
static u32 ensure_player2_blip(void);

#ifdef GTA_COOP_TEST
static u64 coop_test_native(u64,u32,u64*,u64*);
static u32 test_network,test_menu_open;
static i32 test_users[4],test_handles[4];
static i32 platform_users(i32*);
static i32 platform_open(i32);
static i32 platform_read(i32,void*);
static i32 platform_close(i32);
static u32 network(void){return test_network;}
static i32 game_user(u32 i){return test_users[i];}
static i32 game_handle(u32 i){return test_handles[i];}
static u32 resident_menu_open(void){return test_menu_open;}
static u32 primary_protected_bit(void);
#else
static u32 network(void){return *(volatile u8*)0x5abefb8ULL;}
static i32 game_user(u32 i){return ((volatile i32*)0x4361650ULL)[i];}
static i32 game_handle(u32 i){return ((volatile i32*)0x5ee4750ULL)[i];}
static u32 resident_menu_open(void){return *(volatile u32*)(session.forward+0x10608ULL);}
static u32 primary_protected_bit(void){
    u64 root=*(volatile u64*)0x5abefa8ULL,entity=root?*(volatile u64*)(root+8):0;
    return entity?(*(volatile u32*)(entity+0x158)&0x100)!=0:0;
}
static i32 platform_users(i32 *users){return ((i32(*)(i32*))0x37ba0a0ULL)(users);}
static i32 platform_open(i32 user){return ((i32(*)(i32,i32,i32,void*))0x37ba400ULL)(user,0,0,0);}
static i32 platform_read(i32 handle,void *data){return ((i32(*)(i32,void*))0x37ba410ULL)(handle,data);}
static i32 platform_close(i32 handle){return ((i32(*)(i32))0x37ba430ULL)(handle);}
#endif
static u64 floating(float f) { union { float f; u32 i; } v;v.f=f;return v.i; }
static float as_float(u64 n) { union { float f; u32 i; } v;v.i=(u32)n;return v.f; }
static u64 pointer(const void *p) {u64 a=(u64)p;__asm__ volatile("" : "+r"(a));return a;}
__attribute__((noinline)) static u64 call(u64 address,u32 count,u64 *args,u64 *out) {
#ifdef GTA_COOP_TEST
    return coop_test_native(address,count,args,out);
#else
    u64 local[4]={0},padded[32]={0}; Context c={0};
    for(u32 i=0;i<count&&i<32;i++)padded[i]=args[i];
    if(count>32)return 0;
    c.result=out?out:local;c.count=count;c.args=padded;
    ((void(*)(Context*))address)(&c);
    if(c.vectors<=4)for(u32 i=0;i<c.vectors;i++)if(c.dest[i]){
        c.dest[i][0]=c.temp[i].x;c.dest[i][2]=c.temp[i].y;c.dest[i][4]=c.temp[i].z;
    }
    return c.result[0];
#endif
}
#define N(name,...) call(COOP_H_##name,sizeof((u64[]){0,##__VA_ARGS__})/8-1,((u64[]){0,##__VA_ARGS__})+1,0)
#define V(name,out,...) call(COOP_H_##name,sizeof((u64[]){0,##__VA_ARGS__})/8-1,((u64[]){0,##__VA_ARGS__})+1,out)
#define F(n) floating((float)(n))
#define P(p) pointer(p)

static u32 model_hash(const char *s) {
    u32 h=0;for(;*s;s++){u8 c=(u8)*s;if(c>='A'&&c<='Z')c+=32;h+=c;h+=h<<10;h^=h>>6;}
    h+=h<<3;h^=h>>11;return h+(h<<15);
}
static void unpack(float p[3],const u64 raw[4]) {
    for(u32 i=0;i<3;i++)p[i]=as_float(raw[i]);
}
static u32 coords(u32 ped,float p[3]) {
    u64 out[4]={0};V(GET_ENTITY_COORDS,out,ped,1);unpack(p,out);
    return coop_position_valid(p);
}
static void fail(u32 error) {if(!session.error)session.error=error;session.stop=1;}
static void protect_primary(void) {
    if(!session.no_death)return;
    if(!session.p1_protection_owned){
        session.p1_original_model=N(GET_ENTITY_MODEL,session.p1);
        session.p1_original_invincible=!N(GET_ENTITY_CAN_BE_DAMAGED,session.p1);
        session.p1_protection_owned=1;
    }
    N(SET_ENTITY_INVINCIBLE,session.p1,1);
    session.p1_protected=!N(GET_ENTITY_CAN_BE_DAMAGED,session.p1);
    if(!session.p1_protected)fail(29);
}
static void restore_primary(void) {
    if(!session.p1_protection_owned)return;
    if(N(DOES_ENTITY_EXIST,session.p1)&&
       (u32)N(GET_ENTITY_MODEL,session.p1)==session.p1_original_model){
        N(SET_ENTITY_INVINCIBLE,session.p1,session.p1_original_invincible);
        session.p1_protected=!N(GET_ENTITY_CAN_BE_DAMAGED,session.p1);
        if(session.p1_protected!=session.p1_original_invincible)fail(31);
    }
    session.p1_protection_owned=0;
}
static u32 borrowed(i32 handle) {
    for(u32 i=0;i<4;i++)if(game_handle(i)==handle)return 1;
    return 0;
}
static u32 audit_coordinates(void) {
    session.audit_error=1;session.abi_valid=0;
    u32 p=N(PLAYER_PED_ID);session.p1=p;
    if(!p||!N(DOES_ENTITY_EXIST,p)||N(IS_ENTITY_DEAD,p,0))return 0;
    session.in_vehicle=N(IS_PED_IN_ANY_VEHICLE,p,0)!=0;
    u64 raw[4]={0},off[4]={0},rot[4]={0},zero[4]={0};float a[3],b[3],origin[3],ground=0;
    u32 beside=session.mode==6&&session.in_vehicle;
    V(GET_ENTITY_COORDS,raw,p,1);V(GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,off,p,F(!live_mode()||beside?3:0),F(live_mode()&&!beside?3:0),F(0));
    V(GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,zero,p,F(0),F(0),F(0));
    V(GET_GAMEPLAY_CAM_ROT,rot,2);unpack(a,raw);unpack(b,off);unpack(origin,zero);
    for(u32 i=0;i<4;i++){session.raw_position[i]=raw[i];session.raw_offset[i]=off[i];}
    for(u32 i=0;i<3;i++){session.position[i]=a[i];session.offset[i]=b[i];session.origin[i]=origin[i];session.camera[i]=as_float(rot[i]);}
    session.heading=as_float(N(GET_ENTITY_HEADING,p));
    session.audit_error=2;
    if(session.heading!=session.heading||coop_abs(session.heading)>36000)return 0;
    session.audit_error=3;
    if(!coop_position_valid(a)||!coop_position_valid(b)||!coop_position_valid(origin))return 0;
    /* GET_ENTITY_COORDS may use a ped's alternative transform (+0x1510),
     * whereas GET_OFFSET uses its own matrix. Validate against offset(0),
     * not against a potentially different entity-coordinate origin. */
    session.basis_length_squared=coop_distance_squared(origin,b);
    session.origin_delta_squared=coop_distance_squared(origin,a);
    session.audit_error=4;
    if(session.basis_length_squared<8.5f||session.basis_length_squared>9.5f)return 0;
    /* The two GTA position origins can differ by more than 3 m. Bound the
     * actual candidate spawn against player coordinates instead of rejecting
     * a valid transform solely because its origin differs. */
    session.audit_error=5;
    if(coop_distance_squared(a,b)>36)return 0;
    session.ground_ok=N(GET_GROUND_Z_FOR_3D_COORD,F(b[0]),F(b[1]),F(b[2]+2),P(&ground),0,0)!=0;
    session.ground=ground;
    session.abi_valid=1;session.audit_error=0;return 1;
}
static void camera_release_view(void) {
    if(!session.cam_owned)return;
    u32 current=N(GET_RENDERING_CAM);session.cam_rendering=current;
    if(current==session.cam_handle||(current==0xffffffffu&&session.cam_requested)){
        N(RENDER_SCRIPT_CAMS,0,0,0,1,0,0);session.cam_restores++;
    }
    if(N(DOES_CAM_EXIST,session.cam_handle))N(SET_CAM_ACTIVE,session.cam_handle,0);
    session.cam_requested=session.cam_visible=0;
}
static u32 camera_cleanup(void) {
    if(!session.cam_owned)return 1;
    camera_release_view();
    if(N(DOES_CAM_EXIST,session.cam_handle))N(DESTROY_CAM,session.cam_handle,0);
    if(N(DOES_CAM_EXIST,session.cam_handle))return 0;
    session.cam_owned=session.cam_handle=0;session.cam_rendering=N(GET_RENDERING_CAM);return 1;
}
static u32 character_cleanup(void) {
    if(!player_assists_release())return 0;
    if(!road_runtime_release())return 0;
    if(!oppressor_controls_release())return 0;
    if(!plane_runtime_release())return 0;
    plane_controls_release();
    aircraft_buttons_release();
    plane_weapons_release();
    if(!hover_controls_release())return 0;
    if(session.blip_owned){
        u32 blip=session.p2_blip;
        if(N(DOES_BLIP_EXIST,blip))N(REMOVE_BLIP,P(&blip));
        if(N(DOES_BLIP_EXIST,session.p2_blip))return 0;
        session.p2_blip=session.blip_owned=0;
    }
    if(session.p2){
        u32 p=session.p2;
        if(p!=(u32)N(PLAYER_PED_ID)&&N(DOES_ENTITY_EXIST,p)&&
           (u32)N(GET_ENTITY_MODEL,p)==session.model){
            u32 original=p;N(DELETE_PED,P(&p));
            if(N(DOES_ENTITY_EXIST,original))return 0;
        }
        session.p2=0;
    }
    return 1;
}
static void cleanup(void) {
    if(session.mode==6)controls_release_vehicle_streaming();
    if(!camera_cleanup()||!character_cleanup())return;
    restore_primary();session.p2_protected=0;
    session.return_to_p2_camera=0;
    if(session.model_owned){N(SET_MODEL_AS_NO_LONGER_NEEDED,session.model);session.model_owned=0;}
    if(session.owned&&session.handle>=0){
        /* Ownership can change if the game adopts an already-open pad. */
        if(!borrowed(session.handle)){
            session.close_result=platform_close(session.handle);
            if(session.close_result)return;
        }
        session.owned=0;
    }
    session.handle=-1;session.cleanup_count++;session.state=3;
}
static void text(const char *message,float y) {
    N(SET_TEXT_FONT,0);N(SET_TEXT_SCALE,F(0.32),F(0.32));
    N(SET_TEXT_COLOUR,255,255,255,255);N(SET_TEXT_CENTRE,1);
    N(BEGIN_TEXT_COMMAND_DISPLAY_TEXT,P("STRING"));
    N(ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME,P(message));
    N(END_TEXT_COMMAND_DISPLAY_TEXT,F(0.5),F(y),0);
}
static void draw(void) {
    if(live_mode())return;
    if(session.mode==4){
        text("Camera and no-death check | normal view returns automatically",0.075f);return;
    }
    N(DRAW_RECT,F(0.5),F(0.12),F(0.75),F(session.mode==2?0.13:0.09),0,0,0,180,0);
    text(session.mode==2?"Co-op camera development test | one visible view":"Co-op input development test | normal camera",0.075f);
    text(session.phase==0?"Waiting for stable Story Mode coordinates...":
         session.phase==1?(session.in_vehicle?"Player 1: exit the vehicle, then Controller 2 presses X":
         "Controller 2: release buttons, then X to create test character"):
         session.phase==2?"Loading stock character...":
         session.phase==4?"Player 2 died | respawning near the last location...":
         session.phase==5?"Preparing Player 2's respawn...":
         "Controller 2: left stick walks, X jumps, Circle ends test",0.11f);
    if(session.mode==2)text("Controller 2: R3 switches camera | right stick looks around",0.145f);
}
static void audit_inactive_camera(void) {
    /* No pad, character or visible camera takeover: validate the setter ABI
     * against an owned inactive camera before another interactive trial. */
    u32 before=N(GET_RENDERING_CAM),camera=N(CREATE_CAM,P("DEFAULT_SCRIPTED_CAMERA"),0);
    if(!camera||camera==0xffffffffu){fail(20);return;}
    session.cam_handle=camera;session.cam_owned=1;
    if(!N(DOES_CAM_EXIST,camera)){fail(20);return;}
    float position[3]={session.position[0]+1.25f,session.position[1]-2.5f,session.position[2]+3.75f};
    N(SET_CAM_COORD,camera,F(position[0]),F(position[1]),F(position[2]));
    u64 raw[4]={0};float actual[3];V(GET_CAM_COORD,raw,camera);unpack(actual,raw);
    for(u32 i=0;i<3;i++){session.cam_position[i]=position[i];session.cam_readback[i]=actual[i];}
    if(!coop_position_valid(actual)||coop_distance_squared(actual,position)>0.01f){fail(23);return;}
    if((u32)N(GET_RENDERING_CAM)!=before){fail(24);return;}
    session.cam_updates=1;
}
static u32 acquire_secondary(void) {
    i32 users[4]={-1,-1,-1,-1};
    if(session.primary_handle<0||platform_users(users))return 4;
    session.secondary_user=coop_secondary_user(users,session.primary_user);
    if(session.secondary_user<0)return 5;
    for(u32 i=0;i<4;i++)if(game_user(i)==session.secondary_user&&game_handle(i)>=0)session.handle=game_handle(i);
    if(session.handle<0){
        session.handle=platform_open(session.secondary_user);
        session.owned=session.handle>=0&&!borrowed(session.handle);
    }
    if(session.handle<0||session.handle==session.primary_handle){session.handle=-1;session.owned=0;return 6;}
    return 0;
}
static void init(void) {
    u32 time=N(GET_GAME_TIMER);
    if(!session.startup_retries){
        /* An off-state ownership handoff can retain an opened secondary pad.
         * A zeroed fresh image has no owned handle/user and must acquire one.
         * Never discard a verified retained handle merely because P2 is off. */
        u32 retained=session.secondary_user>0&&session.handle>=0&&session.handle!=game_handle(0)&&session.owned;
        if(!session.p2&&!retained){session.handle=-1;session.secondary_user=-1;}
        session.startup_started=time;
    }
    session.startup_retries++;
    if(network()){session.state=2;fail(1);return;}
    if(!audit_coordinates()){
        if(session.mode&&time-session.startup_started<8000){draw();return;}
        session.state=2;fail(2);return;
    }
    session.state=2;session.started=time;
    if(!session.mode){
        session.p1_original_invincible=!N(GET_ENTITY_CAN_BE_DAMAGED,session.p1);
        session.p1_protected=primary_protected_bit();
        if(session.p1_original_invincible!=session.p1_protected)fail(32);
        cleanup();return;
    }
    if(session.mode==3){audit_inactive_camera();cleanup();return;}
    if((session.mode!=1&&session.mode!=2&&session.mode!=4&&!live_mode())||
       (live_mode()?session.duration!=0:(session.duration<1000||session.duration>120000))){fail(3);return;}
    session.primary_user=game_user(0);
    session.primary_handle=game_handle(0);
    if(session.mode==4){protect_primary();session.phase=1;return;}
    if(session.mode==6&&session.handle==session.primary_handle){session.handle=-1;session.owned=0;}
    u32 pad_error=(session.mode==6&&session.p2&&session.handle>=0)?0:acquire_secondary();session.last_pad_attempt=time;
    if(pad_error&&!live_mode()){fail(pad_error);return;}
    session.pad_waiting=pad_error!=0;
    protect_primary();if(session.stop)return;
    if(session.mode==6&&session.p2){
        if(session.p2==session.p1||!N(DOES_ENTITY_EXIST,session.p2)||N(IS_ENTITY_DEAD,session.p2,0)||
           (u32)N(GET_ENTITY_MODEL,session.p2)!=session.model||session.model!=model_hash("a_m_y_business_01")){
            fail(33);return;
        }
        session.phase=3;session.last_task=time;session.input.armed=0;
        if(!ensure_player2_blip()){fail(26);return;}
    }else if(!(session.mode==6&&session.phase==6))session.phase=1;
}
static void begin_model(u32 now) {
    if((session.mode!=4&&session.mode!=6&&N(IS_PED_IN_ANY_VEHICLE,session.p1,0))||!audit_coordinates()||!session.ground_ok||
       coop_abs(session.ground-session.position[2])>3.0f){if(session.mode!=6)fail(7);return;}
    session.model=model_hash("a_m_y_business_01");
    if(!N(IS_MODEL_VALID,session.model)||!N(IS_MODEL_IN_CDIMAGE,session.model)||
       !N(IS_MODEL_A_PED,session.model)){fail(8);return;}
    N(REQUEST_MODEL,session.model);session.model_owned=1;session.phase=2;session.phase_started=now;
}
static u32 ensure_player2_blip(void) {
    if(session.mode==5)return 1;
    if(session.blip_owned&&N(DOES_BLIP_EXIST,session.p2_blip))return 1;
    u32 blip=N(ADD_BLIP_FOR_ENTITY,session.p2);
    if(!blip)return 0;
    session.p2_blip=blip;session.blip_owned=1;
    N(SET_BLIP_SPRITE,blip,1);N(SET_BLIP_COLOUR,blip,3);N(SET_BLIP_SCALE,blip,F(0.85));
    N(SET_BLIP_AS_SHORT_RANGE,blip,0);
    N(BEGIN_TEXT_COMMAND_SET_BLIP_NAME,P("STRING"));N(ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME,P("Player 2"));
    N(END_TEXT_COMMAND_SET_BLIP_NAME,blip);return 1;
}
static u32 spawn_character(const float position[3],float heading) {
    u32 p=N(CREATE_PED,4,session.model,F(position[0]),F(position[1]),F(position[2]),F(heading),0,0);
    if(!p||p==session.p1||!N(DOES_ENTITY_EXIST,p)){fail(10);return 0;}
    session.p2=p;if(session.mode==6)controls_reset_for_ped();N(SET_ENTITY_AS_MISSION_ENTITY,p,1,0);N(SET_BLOCKING_OF_NON_TEMPORARY_EVENTS,p,1);
    if(session.no_death||live_mode()){
        N(SET_ENTITY_INVINCIBLE,p,1);session.p2_protected=!N(GET_ENTITY_CAN_BE_DAMAGED,p);
        if(!session.p2_protected){fail(29);return 0;}
    }
    float pos[3];
    if(!coords(p,pos)||coop_distance_squared(pos,position)>9){fail(11);return 0;}
    for(u32 i=0;i<3;i++)session.p2_start[i]=session.p2_position[i]=pos[i];
    if(!ensure_player2_blip()){fail(26);return 0;}
    return 1;
}
static void create_character(u32 now) {
    if(!N(HAS_MODEL_LOADED,session.model)){
        if(now-session.phase_started>8000)fail(9);
        return;
    }
    if(!audit_coordinates()||!session.ground_ok||coop_abs(session.ground-session.position[2])>3.0f){if(session.mode!=6)fail(7);return;}
    float position[3]={session.offset[0],session.offset[1],session.ground+0.5f};
    if(!spawn_character(position,session.heading))return;
    if(live_mode())session.automatic_spawns++;
    session.phase=3;session.phase_started=now;session.last_task=now;session.input.armed=0;
}
static void begin_respawn(u32 now) {
    float death[3]={session.p2_position[0],session.p2_position[1],session.p2_position[2]};
    if(session.p2&&N(DOES_ENTITY_EXIST,session.p2)){
        if((u32)N(GET_ENTITY_MODEL,session.p2)!=session.model){fail(12);return;}
        if(!coords(session.p2,death)){fail(27);return;}
        session.death_heading=as_float(N(GET_ENTITY_HEADING,session.p2));
    }else session.death_heading=session.heading;
    if(!coop_position_valid(death)||session.death_heading!=session.death_heading||coop_abs(session.death_heading)>36000){fail(27);return;}
    for(u32 i=0;i<3;i++)session.death_position[i]=death[i];
    session.return_to_p2_camera=session.cam_requested;camera_release_view();
    session.death_count++;session.respawn_started=now;session.respawn_attempt=0;
    session.respawn_wait_reason=1;session.phase=4;session.moving=0;session.input.armed=0;
}
static void respawn_tick(u32 now) {
    if(session.phase==5){
        if(!session.p2||!N(DOES_ENTITY_EXIST,session.p2)||N(IS_ENTITY_DEAD,session.p2,0)){fail(28);return;}
        if(N(HAS_COLLISION_LOADED_AROUND_ENTITY,session.p2)){
            N(FREEZE_ENTITY_POSITION,session.p2,0);session.phase=3;session.respawn_count++;
            session.respawn_wait_reason=0;session.last_task=now;session.input.armed=0;
        }else if(now-session.collision_started>5000)fail(28);
        return;
    }
    if(now-session.respawn_started<2000)return;
    if(now-session.respawn_started>10000){fail(28);return;}
    if(!N(HAS_MODEL_LOADED,session.model)){N(REQUEST_MODEL,session.model);session.respawn_wait_reason=2;return;}
    if(session.respawn_attempt&&now-session.respawn_last_probe<150)return;
    session.respawn_last_probe=now;
    static const float offsets[9][2]={{0,0},{3,0},{-3,0},{0,3},{0,-3},{5,5},{-5,5},{5,-5},{-5,-5}};
    u32 candidate=session.respawn_attempt++%9;
    float death[3]={session.death_position[0],session.death_position[1],session.death_position[2]};
    float position[3]={death[0]+offsets[candidate][0],death[1]+offsets[candidate][1],death[2]+3},ground=0;
    N(REQUEST_COLLISION_AT_COORD,F(position[0]),F(position[1]),F(position[2]));session.respawn_ground_checks++;
    if(!N(GET_GROUND_Z_FOR_3D_COORD,F(position[0]),F(position[1]),F(position[2]),P(&ground),0,0)){
        session.respawn_wait_reason=3;return;
    }
    position[2]=ground+0.5f;
    if(!coop_position_valid(position)||coop_abs(ground-death[2])>80||coop_distance_squared(position,death)>10000){session.respawn_wait_reason=4;return;}
    if(!character_cleanup())return;
    if(!spawn_character(position,session.death_heading))return;
    float actual[3];
    if(!coords(session.p2,actual)||coop_distance_squared(actual,death)>10000){fail(27);return;}
    for(u32 i=0;i<3;i++)session.respawn_position[i]=actual[i];
    N(FREEZE_ENTITY_POSITION,session.p2,1);session.collision_started=now;
    session.phase=5;session.respawn_wait_reason=5;
}
static void camera_tick(u32 now) {
    if(!session.cam_owned){
        u32 camera=N(CREATE_CAM,P("DEFAULT_SCRIPTED_CAMERA"),0);
        if(!camera||camera==0xffffffffu){fail(20);return;}
        session.cam_handle=camera;session.cam_owned=1;
        if(!N(DOES_CAM_EXIST,camera)){fail(20);return;}
        session.orbit.yaw=coop_wrap_angle(session.heading*0.01745329252f);session.orbit.pitch=0.22f;
        session.cam_last_ms=now;
        N(SET_CAM_FOV,camera,F(60));
    }
    u32 camera=session.cam_handle;
    if(!N(DOES_CAM_EXIST,camera)){fail(21);return;}
    float dt=(now-session.cam_last_ms)*0.001f;session.cam_last_ms=now;
    if(session.input.armed)coop_orbit_update((CoopOrbit*)&session.orbit,
        coop_axis(session.data[6]),coop_axis(session.data[7]),dt);
    float center[3],position[3],ground=0;
    if(!coords(session.p2,center)){fail(22);return;}
    CoopOrbit orbit={session.orbit.yaw,session.orbit.pitch};coop_orbit_position(&orbit,center,position);
    if(!coop_position_valid(position)||!N(GET_GROUND_Z_FOR_3D_COORD,F(position[0]),F(position[1]),F(position[2]+2),P(&ground),0,0)||
       ground!=ground||coop_abs(ground-center[2])>4){fail(22);return;}
    if(position[2]<ground+0.7f)position[2]=ground+0.7f;
    if(!coop_position_valid(position)){fail(22);return;}
    session.cam_ground=ground;
    N(SET_CAM_COORD,camera,F(position[0]),F(position[1]),F(position[2]));
    N(POINT_CAM_AT_ENTITY,camera,session.p2,F(0),F(0),F(1),1);
    u64 raw[4]={0};float actual[3];V(GET_CAM_COORD,raw,camera);unpack(actual,raw);
    for(u32 i=0;i<3;i++){session.cam_position[i]=position[i];session.cam_readback[i]=actual[i];}
    if(!coop_position_valid(actual)||coop_distance_squared(actual,position)>0.01f){fail(23);return;}
    session.cam_updates++;
    u32 current=N(GET_RENDERING_CAM);
    if(session.cam_requested&&current!=camera&&current!=0xffffffffu){session.foreign_camera_seen=current;fail(24);return;}
    u32 restore=session.return_to_p2_camera&&!session.cam_requested;
    if(restore&&current!=0xffffffffu&&current!=camera){session.return_to_p2_camera=0;restore=0;}
    u32 automatic=session.mode==4&&!session.cam_toggles;
    if((session.input.pressed&4)||restore||automatic){
        if(session.cam_requested)camera_release_view();
        else{
            if(current!=0xffffffffu&&current!=camera){session.foreign_camera_seen=current;fail(24);return;}
            N(SET_CAM_ACTIVE,camera,1);N(RENDER_SCRIPT_CAMS,1,0,0,1,0,0);
            session.cam_requested=1;session.cam_activate_started=now;
        }
        if((session.input.pressed&4)||automatic)session.cam_toggles++;
        session.return_to_p2_camera=0;
    }
    current=N(GET_RENDERING_CAM);session.cam_rendering=current;session.cam_visible=current==camera;
    if(session.cam_requested&&!session.cam_visible&&now-session.cam_activate_started>1000){fail(25);return;}
}
#include "coop-player-controls.c"
#include "coop-hover-controls.c"
#include "coop-plane-controls.c"
#include "coop-aircraft-extras.c"
#include "coop-plane-weapons.c"
#include "coop-road-runtime.c"
#include "coop-oppressor2-runtime.c"
#include "coop-player-assists.c"
static u32 primary_toggle_tick(u32 now) {
    if(session.mode!=6)return 0;
    u8 raw[256]={0};
    i32 result=platform_read(session.primary_handle,raw);
    if(result||!raw[0x4c]){primary_toggle.armed=0;primary_toggle.waiting_second=0;primary_toggle.read_errors++;return 0;}
    u32 buttons=(u32)raw[0]|(u32)raw[1]<<8|(u32)raw[2]<<16|(u32)raw[3]<<24;
    u32 down=buttons&4u,previous=primary_toggle.held;primary_toggle.held=down;
    primary_toggle.initialized=1;
    if(session.paused){primary_toggle.armed=0;primary_toggle.waiting_second=0;primary_toggle.suppressed++;return 0;}
    /* Two rising R3 edges within 400 ms; a held button cannot repeat. Pause,
     * disconnect and expiration discard a partial gesture. Reading P1 does
     * not consume or replace his game input. L3 is not required. */
    if(primary_toggle.waiting_second&&now-primary_toggle.first_press_ms>PRIMARY_DOUBLE_R3_MS)primary_toggle.waiting_second=0;
    if(!down){primary_toggle.armed=1;return 0;}
    if(previous||!primary_toggle.armed)return 0;
    primary_toggle.armed=0;
    if(!primary_toggle.waiting_second){primary_toggle.first_press_ms=now;primary_toggle.waiting_second=1;return 0;}
    primary_toggle.waiting_second=0;primary_toggle.toggles++;
    if(session.phase==6){session.phase=1;primary_toggle.spawns++;}
    else session.phase=7; /* Cancel an existing character or pending spawn. */
    return 1;
}
static void manual_despawn(void) {
    controls_release_vehicle_streaming();
    if(!character_cleanup())return;
    if(session.model_owned){N(SET_MODEL_AS_NO_LONGER_NEEDED,session.model);session.model_owned=0;}
    controls_reset_for_ped();session.moving=0;session.p2_protected=0;
    session.input.armed=0;session.armed=0;session.respawn_attempt=0;
    session.return_to_p2_camera=0;session.phase=6;primary_toggle.despawns++;
}
/* P1 death/switching is a normal Story transition. Keep the script owner and
 * pad alive, release only our NPC, then bind to the new living primary ped.
 * Phase 8 is deliberately separate from P2's own local respawn state. */
static u32 primary_recovery_tick(u32 now,u32 current,u32 switching) {
    u32 changed=current!=session.p1;
    u32 dead=!current||!N(DOES_ENTITY_EXIST,current)||N(IS_ENTITY_DEAD,current,0);
    u32 pad_changed=game_handle(0)!=session.primary_handle;
    if(session.phase!=8&&!changed&&!dead&&!switching&&!pad_changed)return 0;
    if(session.phase!=8){
        player_controls.primary_resume_enabled=session.phase!=6&&session.phase!=7;
        player_controls.primary_recovery_reason=dead?1:changed?2:switching?3:4;
        session.phase=8;player_controls.primary_recoveries++;
    }
    player_controls.primary_wait_ticks++;
    primary_toggle.armed=primary_toggle.held=primary_toggle.waiting_second=0;
    session.input.armed=0;session.armed=0;
    controls_release_vehicle_streaming();
    if(!camera_cleanup()||!character_cleanup())return 1;
    controls_reset_for_ped();session.moving=0;session.p2_protected=0;
    if(session.model_owned){N(SET_MODEL_AS_NO_LONGER_NEEDED,session.model);session.model_owned=0;}
    /* No protection is normally owned in mode 6. Restore it before rebinding
     * if a diagnostic session explicitly enabled it. */
    restore_primary();if(session.stop)return 1;
    session.return_to_p2_camera=0;
    if(dead||switching||N(IS_PAUSE_MENU_ACTIVE)||resident_menu_open()||game_handle(0)<0)return 1;
    session.primary_handle=game_handle(0);
    if(session.handle==session.primary_handle){session.handle=-1;session.owned=0;session.pad_waiting=1;}
    /* audit_coordinates updates p1, and proves spawn coordinates/ground again. */
    if(!audit_coordinates()||!session.ground_ok||coop_abs(session.ground-session.position[2])>3)return 1;
    session.phase=player_controls.primary_resume_enabled?1:6;
    session.phase_started=now;session.last_task=now;
    player_controls.primary_recovery_reason=0;
    return 1;
}
static void control(u32 now) {
    u32 p=session.p2;float a[3],b[3];
    if(!p||!N(DOES_ENTITY_EXIST,p)||N(IS_ENTITY_DEAD,p,0)){begin_respawn(now);return;}
    if((u32)N(GET_ENTITY_MODEL,p)!=session.model){fail(12);return;}
    if(!coords(session.p1,a)||!coords(p,b)){fail(13);return;}
    if(session.mode==6)player_controls.primary_distance_squared=coop_distance_squared(a,b);
    if(session.mode!=6&&coop_distance_squared(a,b)>1600){
        if(!live_mode()){fail(13);return;}
        if(session.moving){N(CLEAR_PED_TASKS,p);session.moving=0;}return;
    }
    float start[3]={session.p2_start[0],session.p2_start[1],session.p2_start[2]};
    for(u32 i=0;i<3;i++)session.p2_position[i]=b[i];
    session.distance_squared=coop_distance_squared(start,b);
    if(session.distance_squared>session.max_distance_squared)session.max_distance_squared=session.distance_squared;
    if(session.mode==6){player_controls_tick(now,b);return;}
    if(session.input.pressed&0x4000){
        if(!N(IS_PED_FALLING,p)&&!N(IS_PED_JUMPING,p)){N(TASK_JUMP,p,1,0,0);session.jump_count++;session.moving=0;}
    }
    float x=session.input.x,y=session.input.y;
    if(!x&&!y){if(session.moving){N(CLEAR_PED_TASKS,p);session.moving=0;}return;}
    if(now-session.last_task<120||N(IS_PED_FALLING,p)||N(IS_PED_JUMPING,p))return;
    /* Mode 1 keeps the confirmed movement basis. Mode 2 moves relative to the
     * secondary camera, independently of P1's orientation. */
    u64 raw[4]={0},zero[4]={0};float target[3],origin[3];
    float heading=session.heading;
    if(session.mode==2){
        float movement[2];coop_orbit_movement(session.orbit.yaw,x,y,movement);
        target[0]=b[0]+movement[0];target[1]=b[1]+movement[1];target[2]=b[2];
        heading=as_float(N(GET_HEADING_FROM_VECTOR_2D,F(movement[0]),F(movement[1])));
    }else{
        V(GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,raw,session.p1,F(x*2),F(y*2),F(0));unpack(target,raw);
        V(GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,zero,session.p1,F(0),F(0),F(0));unpack(origin,zero);
        if(!coop_position_valid(target)||!coop_position_valid(origin)||coop_distance_squared(target,origin)>4.1f){fail(14);return;}
        for(u32 i=0;i<3;i++)target[i]=b[i]+target[i]-origin[i];
    }
    if(!coop_position_valid(target)||heading!=heading||coop_abs(heading)>36000){fail(14);return;}
    N(TASK_GO_STRAIGHT_TO_COORD,p,F(target[0]),F(target[1]),F(b[2]),F(2),250,F(heading),F(0.1));
    session.last_task=now;session.moving=1;session.movement_ticks++;
}
#include "coop-pose-probe.c"
static u32 payload_probe_busy(void);
#include "coop-vehicle-probe.c"
#include "coop-plane-payload-probe.c"
static void tick(void) {
    if(session.stop&&(session.state==1||session.state==2)){cleanup();return;}
    if(session.state==1){
        u32 frame=N(GET_FRAME_COUNT);if(frame==session.last_frame)return;session.last_frame=frame;
        init();if(session.state==2)session.last_frame=frame-1;
    }
    if(session.state!=2)return;
    if(session.stop){cleanup();return;}
    u32 frame=N(GET_FRAME_COUNT);if(frame==session.last_frame)return;session.last_frame=frame;
    u32 now=N(GET_GAME_TIMER);session.elapsed=now-session.started;
    if(!live_mode()&&session.elapsed>=session.duration){session.stop=1;cleanup();return;}
    if(network()||game_user(0)!=session.primary_user){fail(15);cleanup();return;}
    u32 current=N(PLAYER_PED_ID),switching=N(IS_PLAYER_SWITCH_IN_PROGRESS);
    if(session.mode==6){if(primary_recovery_tick(now,current,switching))return;}
    else if(current!=session.p1||switching||N(IS_ENTITY_DEAD,session.p1,0)||
       game_handle(0)!=session.primary_handle){fail(15);cleanup();return;}
    protect_primary();
    if(session.stop){cleanup();return;}
    if((session.no_death||live_mode())&&session.p2&&N(DOES_ENTITY_EXIST,session.p2)&&!N(IS_ENTITY_DEAD,session.p2,0)){
        N(SET_ENTITY_INVINCIBLE,session.p2,1);session.p2_protected=!N(GET_ENTITY_CAN_BE_DAMAGED,session.p2);
        if(!session.p2_protected){fail(29);cleanup();return;}
    }
    session.paused=N(IS_PAUSE_MENU_ACTIVE)||resident_menu_open();
    primary_toggle_tick(now);
    if(session.mode==6&&session.phase==7){manual_despawn();return;}
    if(session.mode==6&&session.phase==6)return;
    if(session.mode!=4){
        if(live_mode()&&session.handle<0&&now-session.last_pad_attempt>=1000){
            session.last_pad_attempt=now;
            if(!acquire_secondary()){session.pad_reconnects++;session.pad_waiting=0;}
        }
        session.read_result=session.handle>=0?platform_read(session.handle,(void*)session.data):-1;
        session.pad_reads++;session.connected=session.data[0x4c]!=0;
        coop_input_update((CoopInput*)&session.input,(const u8*)session.data,session.read_result==0,session.paused);
        session.buttons=session.input.held;session.armed=session.input.armed;
        if(session.read_result||!session.connected){
            if(!live_mode()){fail(16);cleanup();return;}
            session.pad_waiting=1;session.connected=0;
            if(session.handle>=0&&(!session.owned||borrowed(session.handle)||
                !(session.close_result=platform_close(session.handle)))){
                session.handle=-1;session.owned=0;
            }
        }else if(live_mode())session.pad_waiting=0;
        if(session.mode==6?0:live_mode()?((session.input.held&6)==6&&(session.input.pressed&6)):
            (session.input.pressed&0x2000)){session.stop=1;cleanup();return;}
    }else{
        /* Automated calibration never injects inputs or moves either character.
         * Cancel camera takeover if P1 starts moving. */
        float speed=as_float(N(GET_ENTITY_SPEED,session.p1));
        if(!(speed>=0&&speed<0.25f)){fail(30);cleanup();return;}
    }
    if(session.paused){if(session.mode==6&&session.p2)controls_stop_task();if(session.moving){N(CLEAR_PED_TASKS,session.p2);session.moving=0;}session.cam_last_ms=now;return;}
    if(session.phase==1){
        session.in_vehicle=N(IS_PED_IN_ANY_VEHICLE,session.p1,0)!=0;
        if(session.mode==4||session.mode==6||(!session.in_vehicle&&(live_mode()||(session.input.pressed&0x4000))))begin_model(now);
    }
    else if(session.phase==2)create_character(now);
    else if(session.phase==3)control(now);
    else if(session.phase==4||session.phase==5)respawn_tick(now);
    if(!session.stop&&(session.mode==2||session.mode==4)&&session.phase==3)camera_tick(now);
    if(session.stop)cleanup();else draw();
}
COOP_SECTION(".entry")
void _start(Context *context) {
    session.visits++;
    if(session.magic==0x315353504f4f43ULL){tick();player_vehicle_god_tick();oppressor_controls_tick();road_runtime_tick();hover_controls_tick();plane_controls_tick();aircraft_buttons_tick();plane_weapons_tick();primary_payload_equipment_tick();pose_probe_tick();vehicle_probe_tick();payload_probe_tick();}
    if(session.forward)((void(*)(Context*))session.forward)(context);
}
