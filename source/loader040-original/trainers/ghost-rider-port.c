// Experimental native Ghost Rider gameplay for PPSA04264 01.011.001.
// Included by bridge.c only in GTA_GHOST_PORT builds. Never install this
// intermediate build as the complete JulioNIB mod: custom props, sounds,
// transformation, and several attack variants are still being implemented.
#include "ghost-rider-port-handlers.h"

#define GR_CODE __attribute__((section(".ghost.text"), used))
#define GR(name, ...) nc(GR_H_##name, sizeof((u64[]){0, ##__VA_ARGS__})/8-1, ((u64[]){0, ##__VA_ARGS__})+1, 0)
#define GRV(name, out, ...) nc(GR_H_##name, sizeof((u64[]){0, ##__VA_ARGS__})/8-1, ((u64[]){0, ##__VA_ARGS__})+1, out)

typedef struct {float x,y,z;} GhostVec;
typedef struct {u32 hit,entity;GhostVec point,normal;} GhostHit;
typedef struct {
    u32 initialized, open, page, row, enabled, powers, keep_player;
    u32 mode, chain_mode, stare_mode, attack_held, attack_started;
    u32 fx_ready_core, fx_ready_martin, fx_ready_agency, fx_ready_oddjob;
    u32 head_fx, head_ring_fx, head_ember_fx[3], hand_fx[2];
    u32 wheel_fx[4], wheel_outer_fx[4], fx_ped, fx_vehicle;
    u32 bike_job, bike_started, bike, last_vehicle;
    u32 transformed_vehicle, transformed_model;
    int original_primary, original_secondary;
    u32 fire_trail, speed_kills, water_ride, water_owned, wall_ride, low_fx, last_fire_trail, last_speed_kill;
    u32 last_flame, target, target_started, projectile_active, projectile_started;
    int rope;u32 rope_target, rope_started;
    u32 projectile_level, last_projectile_fx;
    GhostVec projectile_pos, projectile_dir;
    u32 previous_attack, previous_aim, previous_mode;
    u32 last_weapon_impact, last_damage_bone, last_health, last_landing;
    u32 was_airborne;float fall_speed;
    GhostVec weapon_impact;
    u32 custom_ped, model_job, model_target, previous_model, applied_model;
    u32 proof_ped, proof_model;int original_proofs[8];
} GhostState;
static GhostState ghost;

#include "ghost-native-runtime.c"

static GhostVec GR_CODE gr_vec(float x,float y,float z){GhostVec v={x,y,z};return v;}
static GhostVec GR_CODE gr_add(GhostVec a,GhostVec b){return gr_vec(a.x+b.x,a.y+b.y,a.z+b.z);}
static GhostVec GR_CODE gr_scale(GhostVec a,float s){return gr_vec(a.x*s,a.y*s,a.z*s);}
static float GR_CODE gr_abs(float a){return a<0?-a:a;}
static float GR_CODE gr_sqrt(float a){
    if(a<=0)return 0;
    float r=a>1?a:1;
    for(u32 i=0;i<8;i++)r=.5f*(r+a/r);
    return r;
}
static float GR_CODE gr_sin(float a){
    // Camera rotation is in degrees. Reduce to [-pi, pi], then use a
    // polynomial so the flat freestanding bridge needs no libm linkage.
    const float pi=3.14159265358979323846f,twopi=6.2831853071795864769f;
    while(a>pi)a-=twopi;while(a<-pi)a+=twopi;
    float a2=a*a;
    return a*(1-a2*(1.0f/6-a2*(1.0f/120-a2*(1.0f/5040-a2/362880))));
}
static float GR_CODE gr_cos(float a){return gr_sin(1.5707963267948966192f-a);}
static GhostVec GR_CODE gr_from_result(u64 *r){return gr_vec(as_float(r[0]),as_float(r[1]),as_float(r[2]));}
static GhostVec GR_CODE gr_coords(u32 entity){u64 r[4]={0};GRV(GET_ENTITY_COORDS,r,entity,1);return gr_from_result(r);}
static GhostVec GR_CODE gr_camera_pos(void){u64 r[4]={0};GRV(GET_GAMEPLAY_CAM_COORD,r);return gr_from_result(r);}
static GhostVec GR_CODE gr_camera_direction(void){
    u64 r[4]={0};GRV(GET_GAMEPLAY_CAM_ROT,r,2);
    float pitch=as_float(r[0])*0.017453292519943295f;
    float yaw=as_float(r[2])*0.017453292519943295f;
    float cp=gr_cos(pitch);
    return gr_vec(-gr_sin(yaw)*cp,gr_cos(yaw)*cp,gr_sin(pitch));
}
static GhostHit GR_CODE gr_trace_mask(GhostVec a,GhostVec b,u32 ignored,u32 flags){
    int hit=0,entity=0;float end[6]={0},normal[6]={0};
    GhostHit result={0,0,b,{0,0,0}};
    u32 handle=GR(START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE,
                  F(a.x),F(a.y),F(a.z),F(b.x),F(b.y),F(b.z),flags,ignored,7);
    if(!handle)return result;
    if(GR(GET_SHAPE_TEST_RESULT,handle,P(&hit),P(end),P(normal),P(&entity))!=2||!hit)return result;
    result.hit=1;result.entity=(u32)entity;
    result.point=gr_vec(end[0],end[2],end[4]);
    result.normal=gr_vec(normal[0],normal[2],normal[4]);
    return result;
}
static GhostHit GR_CODE gr_trace(GhostVec a,GhostVec b){
    u32 v=car();return gr_trace_mask(a,b,v?v:ped(),511);
}
static void GR_CODE gr_fx(const char *asset,const char *name,GhostVec p,float scale){
    GR(USE_PARTICLE_FX_ASSET,P(asset));
    GR(START_PARTICLE_FX_NON_LOOPED_AT_COORD,P(name),F(p.x),F(p.y),F(p.z),
       F(0),F(0),F(0),F(scale),0,0,0);
}
static void GR_CODE gr_stop_fx(u32 *handle){if(*handle){GR(STOP_PARTICLE_FX_LOOPED,*handle,0);*handle=0;}}
static void GR_CODE gr_clear_visuals(void){
    gr_stop_fx(&ghost.head_fx);
    gr_stop_fx(&ghost.head_ring_fx);
    for(u32 i=0;i<3;i++)gr_stop_fx(&ghost.head_ember_fx[i]);
    for(u32 i=0;i<2;i++)gr_stop_fx(&ghost.hand_fx[i]);
    for(u32 i=0;i<4;i++){gr_stop_fx(&ghost.wheel_fx[i]);gr_stop_fx(&ghost.wheel_outer_fx[i]);}
    ghost.fx_ped=ghost.fx_vehicle=0;
}
static void GR_CODE gr_restore_vehicle(void){
    u32 v=ghost.transformed_vehicle;
    if(v&&GR(DOES_ENTITY_EXIST,v)&&
       GR(GET_ENTITY_MODEL,v)==ghost.transformed_model)
        GR(SET_VEHICLE_COLOURS,v,ghost.original_primary,ghost.original_secondary);
    ghost.transformed_vehicle=ghost.transformed_model=0;
}
static void GR_CODE gr_transform_vehicle(u32 v){
    if(v==ghost.transformed_vehicle)return;
    gr_restore_vehicle();
    if(!v||!GR(DOES_ENTITY_EXIST,v))return;
    int primary=0,secondary=0;
    GR(GET_VEHICLE_COLOURS,v,P(&primary),P(&secondary));
    ghost.transformed_vehicle=v;
    ghost.transformed_model=GR(GET_ENTITY_MODEL,v);
    ghost.original_primary=primary;ghost.original_secondary=secondary;
    GR(SET_VEHICLE_COLOURS,v,0,0);
}
static void GR_CODE gr_water_tick(u32 v){
    if(v&&ghost.water_ride){
        if(!menu.flags[WATER]&&!menu.job){
            toggle(WATER);
            if(menu.flags[WATER])ghost.water_owned=1;
        }
    }else if(ghost.water_owned){
        if(menu.job==4){N(SET_MODEL_AS_NO_LONGER_NEEDED,menu.model);menu.job=0;}
        if(menu.flags[WATER])toggle(WATER);
        ghost.water_owned=0;
    }
}
static u32 GR_CODE gr_get_proofs(u32 p,int *flags){
    return GR(GET_ENTITY_PROOFS,p,P(&flags[0]),P(&flags[1]),P(&flags[2]),
              P(&flags[3]),P(&flags[4]),P(&flags[5]),P(&flags[6]),P(&flags[7]));
}
static void GR_CODE gr_set_proofs(u32 p,const int *flags){
    GR(SET_ENTITY_PROOFS,p,flags[0],flags[1],flags[2],flags[3],
       flags[4],flags[5],flags[6],flags[7]);
}
static void GR_CODE gr_restore_proofs(void){
    u32 p=ghost.proof_ped;
    if(p&&GR(DOES_ENTITY_EXIST,p)&&GR(GET_ENTITY_MODEL,p)==ghost.proof_model){
        int flags[8]={0};
        if(gr_get_proofs(p,flags)){
            flags[1]=ghost.original_proofs[1];
            flags[2]=ghost.original_proofs[2];
            gr_set_proofs(p,flags);
        }
    }
    ghost.proof_ped=ghost.proof_model=0;
}
static void GR_CODE gr_proof_tick(u32 p){
    if(p==ghost.proof_ped)return;
    gr_restore_proofs();
    if(!p||!GR(DOES_ENTITY_EXIST,p))return;
    int flags[8]={0};if(!gr_get_proofs(p,flags))return;
    ghost.proof_ped=p;ghost.proof_model=GR(GET_ENTITY_MODEL,p);
    for(u32 i=0;i<8;i++)ghost.original_proofs[i]=flags[i];
    flags[1]=flags[2]=1;
    gr_set_proofs(p,flags);
}
static void GR_CODE gr_chain_visual_clear(void){
    if(ghost.rope>=0){
        if(GR(DOES_ROPE_EXIST,ghost.rope))GR(DELETE_ROPE,P(&ghost.rope));
        ghost.rope=-1;
    }
    ghost.rope_target=0;
}
static void GR_CODE gr_chain_visual_start(u32 p,u32 target){
    gr_chain_visual_clear();
    GhostVec start=gr_coords(p),end=gr_coords(target);
    start.z+=.9f;end.z+=.4f;
    GhostVec span=gr_vec(end.x-start.x,end.y-start.y,end.z-start.z);
    float length=gr_sqrt(span.x*span.x+span.y*span.y+span.z*span.z);
    if(length<1||length>35)return;
    GR(ROPE_LOAD_TEXTURES);
    int rope=(int)GR(ADD_ROPE,F(start.x),F(start.y),F(start.z),F(0),F(0),F(0),
                     F(length+1),4,F(length+1),F(0),F(1),0,0,0,F(1),0,0);
    if(rope<0||!GR(DOES_ROPE_EXIST,rope))return;
    GR(ATTACH_ENTITIES_TO_ROPE,rope,p,target,
       F(start.x),F(start.y),F(start.z),F(end.x),F(end.y),F(end.z),
       F(length),0,0,0,0);
    ghost.rope=rope;ghost.rope_target=target;ghost.rope_started=now();
}
static void GR_CODE gr_chain_visual_tick(void){
    if(ghost.rope>=0&&(now()-ghost.rope_started>2200||
       !GR(DOES_ENTITY_EXIST,ghost.rope_target)))gr_chain_visual_clear();
}
static void GR_CODE gr_request_fx(void){
    if(!ghost.fx_ready_core){GR(REQUEST_NAMED_PTFX_ASSET,P("core"));ghost.fx_ready_core=GR(HAS_NAMED_PTFX_ASSET_LOADED,P("core"))!=0;}
    if(!ghost.fx_ready_martin){GR(REQUEST_NAMED_PTFX_ASSET,P("scr_martin1"));ghost.fx_ready_martin=GR(HAS_NAMED_PTFX_ASSET_LOADED,P("scr_martin1"))!=0;}
    if(!ghost.fx_ready_agency){GR(REQUEST_NAMED_PTFX_ASSET,P("cut_agencyheist"));ghost.fx_ready_agency=GR(HAS_NAMED_PTFX_ASSET_LOADED,P("cut_agencyheist"))!=0;}
    if(!ghost.fx_ready_oddjob){GR(REQUEST_NAMED_PTFX_ASSET,P("scr_oddjobtraffickingair"));ghost.fx_ready_oddjob=GR(HAS_NAMED_PTFX_ASSET_LOADED,P("scr_oddjobtraffickingair"))!=0;}
}
static void GR_CODE gr_head_fire(u32 p){
    if(ghost.fx_ped!=p){gr_stop_fx(&ghost.head_fx);gr_stop_fx(&ghost.head_ring_fx);
        for(u32 i=0;i<3;i++)gr_stop_fx(&ghost.head_ember_fx[i]);
        for(u32 i=0;i<2;i++)gr_stop_fx(&ghost.hand_fx[i]);ghost.fx_ped=p;
    }
    if(!ghost.fx_ready_core)return;
    u32 head=GR(GET_PED_BONE_INDEX,p,31086);
    if(!ghost.head_fx){GR(USE_PARTICLE_FX_ASSET,P("core"));
        ghost.head_fx=GR(START_PARTICLE_FX_LOOPED_ON_PED_BONE,P("ent_sht_flame"),p,
                         F(0),F(0),F(0),F(0),F(90),F(110),head,F(0.5f),0,0,0);}
    if(!ghost.head_ring_fx&&!ghost.low_fx){GR(USE_PARTICLE_FX_ASSET,P("core"));
        ghost.head_ring_fx=GR(START_PARTICLE_FX_LOOPED_ON_PED_BONE,P("ent_amb_fire_ring"),p,
                             F(0),F(0),F(0),F(0),F(0),F(0),head,F(.001f),0,0,0);}
    if(ghost.fx_ready_martin&&!ghost.low_fx){const u32 hands[]={60309,28422};
        for(u32 i=0;i<2;i++)if(!ghost.hand_fx[i]){
            u32 bone=GR(GET_PED_BONE_INDEX,p,hands[i]);
            GR(USE_PARTICLE_FX_ASSET,P("scr_martin1"));
            ghost.hand_fx[i]=GR(START_PARTICLE_FX_LOOPED_ON_PED_BONE,P("scr_sol1_fire_drip"),p,
                F(-.015f),F(.035f),F(0),F(0),F(0),F(-90),bone,F(1.5f),0,0,0);
        }
    }
    if(ghost.fx_ready_oddjob&&!ghost.low_fx){
        const float offsets[3][3]={{.2f,0,.35f},{.2f,.15f,-.15f},{.2f,-.15f,-.15f}};
        for(u32 i=0;i<3;i++)if(!ghost.head_ember_fx[i]){
            GR(USE_PARTICLE_FX_ASSET,P("scr_oddjobtraffickingair"));
            ghost.head_ember_fx[i]=GR(START_PARTICLE_FX_LOOPED_ON_PED_BONE,
                P("scr_ojdg4_boat_wreck_fire"),p,F(offsets[i][0]),F(offsets[i][1]),F(offsets[i][2]),
                F(0),F(90),F(0),head,F(.0005f),0,0,0);
        }
    }
}
static void GR_CODE gr_wheel_fire(u32 v){
    if(v!=ghost.fx_vehicle){for(u32 i=0;i<4;i++){
        gr_stop_fx(&ghost.wheel_fx[i]);gr_stop_fx(&ghost.wheel_outer_fx[i]);}
        ghost.fx_vehicle=v;
    }
    if(!v||!ghost.fx_ready_core)return;
    const char *bones[]={"wheel_lf","wheel_rf","wheel_lr","wheel_rr"};
    for(u32 i=0;i<4;i++){
        u32 bone=GR(GET_ENTITY_BONE_INDEX_BY_NAME,v,P(bones[i]));
        if((int)bone<0)continue;
        if(!ghost.wheel_fx[i]){GR(USE_PARTICLE_FX_ASSET,P("core"));
            ghost.wheel_fx[i]=GR(START_PARTICLE_FX_LOOPED_ON_ENTITY_BONE,
                P("ent_ray_heli_aprtmnt_s_fire"),v,F(-.045f),F(0),F(0),F(0),F(0),F(0),bone,F(.3f),0,0,0);}
        if(ghost.fx_ready_oddjob&&!ghost.low_fx&&!ghost.wheel_outer_fx[i]){
            GR(USE_PARTICLE_FX_ASSET,P("scr_oddjobtraffickingair"));
            ghost.wheel_outer_fx[i]=GR(START_PARTICLE_FX_LOOPED_ON_ENTITY_BONE,
                P("scr_ojdg4_train_fire"),v,F(-.045f),F(0),F(0),F(0),F(0),F(0),bone,F(.25f),0,0,0);
        }
    }
}
static void GR_CODE gr_turn_powers(u32 enabled){
    if(ghost.powers==enabled)return;
    ghost.powers=enabled;ghost.attack_held=ghost.projectile_active=0;
    mailbox.ghost_diag[0]=enabled;
    if(!enabled){
        gr_clear_visuals();gr_chain_visual_clear();gr_restore_vehicle();gr_water_tick(0);gr_restore_proofs();ghost.target=0;
        if(ghost.applied_model&&ghost.previous_model){
            ghost.model_target=ghost.previous_model;ghost.model_job=1;
        }
    }else{
        gr_request_fx();
        if(!ghost.keep_player){
            ghost.previous_model=GR(GET_ENTITY_MODEL,ped());
            ghost.model_target=hash(ghost.custom_ped?"g_m_y_lost_01":"mp_m_freemode_01");
            ghost.model_job=1;
        }
    }
}
static void GR_CODE gr_model_tick(void){
    if(!ghost.model_job)return;
    u32 model=ghost.model_target;
    if(!model||!GR(IS_MODEL_IN_CDIMAGE,model)){ghost.model_job=0;return;}
    GR(REQUEST_MODEL,model);
    if(!GR(HAS_MODEL_LOADED,model))return;
    gr_restore_proofs();
    N(SET_PLAYER_MODEL,0,model);
    N(SET_PED_DEFAULT_COMPONENT_VARIATION,ped());
    N(SET_MODEL_AS_NO_LONGER_NEEDED,model);
    ghost.applied_model=(model!=ghost.previous_model);
    ghost.model_job=0;
}
static u32 GR_CODE gr_charge_level(u32 duration){return duration<700?1:duration<1500?2:3;}
static const char *GR_CODE gr_mode_name(u32 mode){
    switch(mode){case 0:return "FIREBALL";case 1:return "CHAIN";
      case 2:return "PENANCE STARE";default:return "FLAMETHROWER";}
}
static void GR_CODE gr_fireball_start(u32 duration){
    mailbox.ghost_diag[4]++;
    GhostVec cam=gr_camera_pos(),dir=gr_camera_direction();
    ghost.projectile_pos=gr_add(cam,gr_scale(dir,1.5f));
    ghost.projectile_dir=dir;ghost.projectile_level=gr_charge_level(duration);
    ghost.projectile_started=now();ghost.last_projectile_fx=0;ghost.projectile_active=1;
}
static void GR_CODE gr_fireball_tick(void){
    if(!ghost.projectile_active)return;
    u32 t=now();if(t-ghost.projectile_started>2500){ghost.projectile_active=0;return;}
    float step=as_float(N(GET_FRAME_TIME))*36.0f;
    if(step<.1f)step=.1f;if(step>3.0f)step=3.0f;
    GhostVec next=gr_add(ghost.projectile_pos,gr_scale(ghost.projectile_dir,step));
    GhostHit hit=gr_trace(ghost.projectile_pos,next);
    if(hit.hit){
        u32 level=ghost.projectile_level;
        GR(ADD_EXPLOSION,F(hit.point.x),F(hit.point.y),F(hit.point.z),12,F(1.0f+level*.65f),1,0,F(.2f),0);
        if(hit.entity&&GR(DOES_ENTITY_EXIST,hit.entity))GR(START_ENTITY_FIRE,hit.entity);
        ghost.projectile_active=0;
    }else ghost.projectile_pos=next;
    if(ghost.fx_ready_core&&(!ghost.last_projectile_fx||
       t-ghost.last_projectile_fx>=(ghost.low_fx?120u:45u))){
        gr_fx("core","ent_sht_flame",hit.point,.38f+.18f*ghost.projectile_level);
        ghost.last_projectile_fx=t;
    }
}
static void GR_CODE gr_flamethrower_tick(void){
    u32 t=now();if(t-ghost.last_flame<(ghost.low_fx?170u:90u))return;ghost.last_flame=t;
    GhostVec start=gr_camera_pos(),dir=gr_camera_direction();
    GhostVec end=gr_add(start,gr_scale(dir,10.0f));
    GhostHit hit=gr_trace(start,end);
    if(ghost.fx_ready_core){
        for(u32 i=1;i<=(ghost.low_fx?1u:3u);i++)
            gr_fx("core","ent_sht_flame",gr_add(start,gr_scale(dir,(float)i*2.2f)),.42f);
    }
    if(hit.entity&&GR(DOES_ENTITY_EXIST,hit.entity)){
        GR(START_ENTITY_FIRE,hit.entity);
        GR(ADD_EXPLOSION,F(hit.point.x),F(hit.point.y),F(hit.point.z),12,F(.12f),0,1,F(0),0);
    }
}
static void GR_CODE gr_chain_attack(u32 duration,u32 aimed){
    u32 level=gr_charge_level(duration);
    if(ghost.chain_mode==3&&!aimed&&ghost.target&&now()-ghost.target_started<5000&&
       GR(DOES_ENTITY_EXIST,ghost.target)){
        GhostVec direction=gr_camera_direction();
        GR(APPLY_FORCE_TO_ENTITY,ghost.target,1,F(direction.x*(18+level*8)),
           F(direction.y*(18+level*8)),F(6+level*2),F(0),F(0),F(0),0,0,1,1,0,1);
        ghost.target=0;gr_chain_visual_clear();return;
    }
    GhostVec start=gr_camera_pos(),dir=gr_camera_direction();
    GhostVec end=gr_add(start,gr_scale(dir,28.0f));GhostHit hit=gr_trace(start,end);
    u32 target=hit.entity;
    if(!target||target==ped()||!GR(DOES_ENTITY_EXIST,target))return;
    ghost.target=target;ghost.target_started=now();
    gr_chain_visual_start(ped(),target);
    if(aimed||ghost.chain_mode==3){
        GR(APPLY_FORCE_TO_ENTITY,target,1,F(-dir.x*(8+level*7)),F(-dir.y*(8+level*7)),F(7+level*3),
           F(0),F(0),F(0),0,0,1,1,0,1);
    }else if(ghost.chain_mode==1){
        GR(APPLY_FORCE_TO_ENTITY,target,1,F(dir.x*(15+level*8)),F(dir.y*(15+level*8)),F(5+level*2),
           F(0),F(0),F(0),0,0,1,1,0,1);
    }else if(ghost.chain_mode==2){GR(START_ENTITY_FIRE,target);}
    else {
        GR(ADD_EXPLOSION,F(hit.point.x),F(hit.point.y),F(hit.point.z),12,F(.6f+level*.35f),1,0,F(.15f),0);
        GR(APPLY_FORCE_TO_ENTITY,target,1,F(dir.x*9),F(dir.y*9),F(4+level*2),
           F(0),F(0),F(0),0,0,1,1,0,1);
    }
    if(ghost.fx_ready_core)gr_fx("core","ent_sht_flame",hit.point,.35f);
}
static void GR_CODE gr_penance_attack(u32 duration){
    GhostVec start=gr_camera_pos(),dir=gr_camera_direction();
    GhostVec end=gr_add(start,gr_scale(dir,10.0f));GhostHit hit=gr_trace(start,end);
    u32 target=hit.entity;
    if(!target||target==ped()||!GR(DOES_ENTITY_EXIST,target)||GR(GET_ENTITY_TYPE,target)!=1)return;
    u32 level=gr_charge_level(duration);
    if(ghost.fx_ready_core)gr_fx("core","ent_sht_flame",hit.point,.65f);
    if(GR(HAS_ANIM_DICT_LOADED,P("stungun@standing"))){
        GR(TASK_PLAY_ANIM,target,P("stungun@standing"),P("damage"),F(8),F(-8),1500,49,F(0),0,0,0);
    }else GR(REQUEST_ANIM_DICT,P("stungun@standing"));
    GR(START_ENTITY_FIRE,target);
    u32 damage=ghost.stare_mode==0?(level==3?125:level==2?70:30):
               ghost.stare_mode==1?(level==3?175:level==2?110:55):250;
    GR(APPLY_DAMAGE_TO_PED,target,damage,0,0,hash("weapon_molotov"));
    if(ghost.stare_mode==2){
        GR(ADD_EXPLOSION,F(hit.point.x),F(hit.point.y),F(hit.point.z),12,F(.65f),1,0,F(.2f),0);
    }
    ghost.target=target;ghost.target_started=now();
}
static void GR_CODE gr_weapon_tick(u32 p){
    if(!GR(IS_PED_SHOOTING,p))return;
    u32 weapon=GR(GET_SELECTED_PED_WEAPON,p);
    u32 group=GR(GET_WEAPONTYPE_GROUP,weapon);
    if(group==hash("group_melee")||group==hash("group_heavy")||
       group==hash("group_thrown")||group==hash("group_unarmed"))return;
    float out[6]={0};
    if(!GR(GET_PED_LAST_WEAPON_IMPACT_COORD,p,P(out)))return;
    GhostVec impact=gr_vec(out[0],out[2],out[4]);
    if(gr_abs(impact.x-ghost.weapon_impact.x)<.25f&&
       gr_abs(impact.y-ghost.weapon_impact.y)<.25f&&
       gr_abs(impact.z-ghost.weapon_impact.z)<.25f&&
       now()-ghost.last_weapon_impact<250)return;
    ghost.last_weapon_impact=now();ghost.weapon_impact=impact;
    if(ghost.fx_ready_core)gr_fx("core","ent_sht_flame",impact,
                                 group==hash("group_shotgun")?.65f:.28f);
    GhostHit struck=gr_trace(gr_camera_pos(),impact);
    if(struck.entity&&GR(DOES_ENTITY_EXIST,struck.entity))GR(START_ENTITY_FIRE,struck.entity);
    if(group==hash("group_shotgun"))
        GR(ADD_EXPLOSION,F(impact.x),F(impact.y),F(impact.z),12,F(.34f),1,0,F(.1f),0);
}
static void GR_CODE gr_injury_tick(u32 p){
    u32 health=GR(GET_ENTITY_HEALTH,p);
    if(ghost.last_health&&health<ghost.last_health&&now()-ghost.last_damage_bone>300){
        int bone=0;
        if(GR(GET_PED_LAST_DAMAGE_BONE,p,P(&bone))&&ghost.fx_ready_core){
            u32 index=GR(GET_PED_BONE_INDEX,p,bone);
            GR(USE_PARTICLE_FX_ASSET,P("core"));
            GR(START_PARTICLE_FX_NON_LOOPED_ON_PED_BONE,P("ent_sht_flame"),p,
               F(0),F(0),F(0),F(0),F(0),F(0),index,F(.35f),0,0,0);
            ghost.last_damage_bone=now();
        }
    }
    ghost.last_health=health;
}
static void GR_CODE gr_wall_tick(u32 v){
    if(!ghost.wall_ride||!v||!GR(IS_DISABLED_CONTROL_PRESSED,0,71)||
       as_float(GR(GET_ENTITY_SPEED,v))<5.0f)return;
    u64 f[4]={0},velocity[4]={0};GRV(GET_ENTITY_FORWARD_VECTOR,f,v);
    GhostVec forward=gr_from_result(f),start=gr_coords(v);
    start.z+=.55f;
    GhostHit wall=gr_trace_mask(start,gr_add(start,gr_scale(forward,2.7f)),v,1);
    float facing=forward.x*wall.normal.x+forward.y*wall.normal.y;
    if(!wall.hit||gr_abs(wall.normal.z)>.30f||facing>-.50f)return;
    GRV(GET_ENTITY_VELOCITY,velocity,v);
    float dt=as_float(N(GET_FRAME_TIME)),vz=as_float(velocity[2]);
    if(dt<0||dt>.1f)return;
    if(vz<9.0f)vz+=dt*19.0f;
    GR(SET_ENTITY_VELOCITY,v,F(as_float(velocity[0])-wall.normal.x*dt*5.0f),
       F(as_float(velocity[1])-wall.normal.y*dt*5.0f),F(vz));
}
static void GR_CODE gr_bike_tick(u32 p,u32 v){
    if(ghost.bike_job){
        u32 h=hash("daemon");
        if(ghost.bike_job==1){GR(REQUEST_MODEL,h);ghost.bike_job=2;ghost.bike_started=now();}
        else if(GR(HAS_MODEL_LOADED,h)){
            u64 at[4]={0};V(GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,at,p,F(0),F(5),F(1));
            GhostVec source=gr_coords(p);
            float x=as_float(at[0]),y=as_float(at[1]),z=as_float(at[2]),ground=0;
            mailbox.ghost_diag[8]=(u32)at[0];mailbox.ghost_diag[9]=(u32)at[1];
            mailbox.ghost_diag[10]=(u32)at[2];mailbox.ghost_diag[11]=F(source.z);
            if(!(x>-12000&&x<12000&&y>-12000&&y<12000&&
                 z>-200&&z<1500&&gr_abs(z-source.z)<4)){
                mailbox.ghost_diag[12]=1;
                say("Bike ground/collision unavailable; spawn cancelled");
                ghost.bike_job=0;return;
            }
            N(REQUEST_COLLISION_AT_COORD,F(x),F(y),F(z));
            if(!N(GET_GROUND_Z_FOR_3D_COORD,F(x),F(y),F(z+10),P(&ground),0,0)||
               gr_abs(z-ground)>=4){
                if(now()-ghost.bike_started<3000)return;
                mailbox.ghost_diag[12]=1;
                say("Bike ground/collision unavailable; spawn cancelled");
                ghost.bike_job=0;return;
            }
            mailbox.ghost_diag[12]=2;mailbox.ghost_diag[13]=F(ground);
            if(menu.owned_count>=3)release_oldest_spawn(car());
            u32 bike=GR(CREATE_VEHICLE,h,F(x),F(y),F(ground+1.5f),N(GET_ENTITY_HEADING,p),0,0,0);
            if(!bike&&release_oldest_spawn(car()))
                bike=GR(CREATE_VEHICLE,h,F(x),F(y),F(ground+1.5f),N(GET_ENTITY_HEADING,p),0,0,0);
            if(bike){
                if(protect_spawned_vehicle(bike)){
                    u32 placed=N(SET_VEHICLE_ON_GROUND_PROPERLY,bike,F(5));
                    GhostVec actual=gr_coords(bike);
                    mailbox.ghost_diag[14]=F(actual.z);
                    if(placed&&gr_abs(actual.z-ground)<4){
                        ghost.bike=bike;remember_spawn(bike,h);
                        mailbox.ghost_diag[12]=3;
                        say("Bike nearby - check it, then enter manually");
                    }else{mailbox.ghost_diag[12]=4;GR(DELETE_VEHICLE,P(&bike));say("Bike placement failed; spawn cancelled");}
                }else GR(DELETE_VEHICLE,P(&bike));
            }else mailbox.ghost_diag[12]=5;
            ghost.bike_job=0;
        }else if(now()-ghost.bike_started>6000)ghost.bike_job=0;
    }
    if(v!=ghost.last_vehicle){for(u32 i=0;i<4;i++){
            gr_stop_fx(&ghost.wheel_fx[i]);gr_stop_fx(&ghost.wheel_outer_fx[i]);}
        ghost.fx_vehicle=0;ghost.last_vehicle=v;ghost.was_airborne=0;ghost.fall_speed=0;
    }
    gr_transform_vehicle(v);
    gr_water_tick(v);
    if(!v)return;
    gr_wheel_fire(v);
    gr_wall_tick(v);
    if(GR(IS_DISABLED_CONTROL_PRESSED,0,21)||GR(IS_DISABLED_CONTROL_PRESSED,0,73)){
        float speed=as_float(GR(GET_ENTITY_SPEED,v));
        if(speed<85.0f)GR(SET_VEHICLE_FORWARD_SPEED,v,F(speed+as_float(N(GET_FRAME_TIME))*22.0f));
    }
    if(GR(IS_DISABLED_CONTROL_PRESSED,0,72)){
        float speed=as_float(GR(GET_ENTITY_SPEED,v));
        if(speed>2.0f)GR(SET_VEHICLE_FORWARD_SPEED,v,F(speed-as_float(N(GET_FRAME_TIME))*15.0f));
    }
    u32 t=now();
    u32 airborne=GR(IS_ENTITY_IN_AIR,v);
    if(airborne){u64 motion[4]={0};GRV(GET_ENTITY_VELOCITY,motion,v);
        float vertical=as_float(motion[2]);if(vertical<ghost.fall_speed)ghost.fall_speed=vertical;
    }else if(ghost.was_airborne&&ghost.fall_speed< -6.0f&&t-ghost.last_landing>1200){
        GhostVec at=gr_coords(v);
        GR(ADD_EXPLOSION,F(at.x),F(at.y),F(at.z-.5f),12,F(.35f),1,0,F(.10f),0);
        ghost.last_landing=t;ghost.fall_speed=0;
    }
    ghost.was_airborne=airborne;
    if(ghost.fire_trail&&ghost.fx_ready_core&&t-ghost.last_fire_trail>(ghost.low_fx?350u:170u)){
        GhostVec at=gr_coords(v);at.z-=.5f;
        gr_fx("core","ent_sht_flame",at,.40f);ghost.last_fire_trail=t;
    }
    if(ghost.speed_kills&&t-ghost.last_speed_kill>500&&as_float(GR(GET_ENTITY_SPEED,v))>22.0f){
        GhostVec at=gr_coords(v);u32 victim=0;
        GR(GET_CLOSEST_PED,F(at.x),F(at.y),F(at.z),F(4.0f),1,1,P(&victim),0,0,-1);
        if(victim&&victim!=p&&!GR(IS_PED_DEAD_OR_DYING,victim,1)){
            GR(START_ENTITY_FIRE,victim);ghost.last_speed_kill=t;
        }
    }
}
static const char *GR_CODE gr_label(u32 page,u32 row){
    if(page){switch(row){case 0:return "Speed kills";case 1:return "Vehicle fire trail";
       case 2:return "Ride on water";case 3:return "Wall ride";
       case 4:return "Low FX mode";case 5:return "Chain attack";
       case 6:return "Penance attack";case 7:return "Use Ghost Rider model";
       default:return "Back";}}
    switch(row){case 0:return "Enabled";case 1:return "Toggle powers";
      case 2:return "Toggle powers (keep player)";case 3:return "Call bike";
      case 4:return "Attack mode";case 5:return "Options";default:return "Close";}
}
static void GR_CODE gr_render(void){
    u32 count=ghost.page?9:7;
    rect(.78f,.29f,.34f,.057f,32,17,20,230);
    text(.63f,.267f,.37f,ghost.page?"GHOST RIDER OPTIONS":"GHOST RIDER",0);
    for(u32 i=0;i<count;i++){
        float y=.329f+(float)i*.046f;
        rect(.78f,y+.013f,.34f,.044f,ghost.row==i?90:20,ghost.row==i?35:24,ghost.row==i?27:26,ghost.row==i?235:205);
        text(.63f,y-.004f,.29f,gr_label(ghost.page,i),ghost.row==i);
    }
    if(!ghost.page){
        text(.63f,.657f,.24f,ghost.powers?"Powers: ON":"Powers: OFF",ghost.powers);
        text(.63f,.686f,.24f,gr_mode_name(ghost.mode),1);
    }else{
        const char *chain=ghost.chain_mode==0?"Smash":ghost.chain_mode==1?"Launch":
                          ghost.chain_mode==2?"Burn":"Grab/throw";
        const char *stare=ghost.stare_mode==0?"Soul burn":ghost.stare_mode==1?"Neck burn":"Ash slash";
        text(.63f,.776f,.23f,chain,1);text(.63f,.804f,.23f,stare,1);
    }
    text(.63f,ghost.page?.855f:.73f,.25f,"L1 + Left: close  |  X: select",0);
}
static void GR_CODE gr_charge_display(void){
    if(!ghost.attack_held||ghost.mode==3)return;
    u32 level=gr_charge_level(now()-ghost.attack_started);
    u32 r=level==1?225:level==2?245:225;
    u32 g=level==1?190:level==2?110:30;
    rect(.5f,.825f,.19f,.020f,30,25,20,180);
    rect(.5f-.075f+(float)level*.025f,.825f,(float)level*.050f,.014f,r,g,20,240);
    text(.405f,.791f,.27f,gr_mode_name(ghost.mode),0);
}
static void GR_CODE gr_activate(void){
    if(!ghost.page){
        switch(ghost.row){
        case 0:ghost.enabled=!ghost.enabled;if(!ghost.enabled)gr_turn_powers(0);break;
        case 1:if(ghost.enabled){ghost.keep_player=0;gr_turn_powers(!ghost.powers);}break;
        case 2:if(ghost.enabled){ghost.keep_player=1;gr_turn_powers(!ghost.powers);}break;
        case 3:if(ghost.powers)ghost.bike_job=1;break;
        case 4:ghost.mode=(ghost.mode+1)%4;break;
        case 5:ghost.page=1;ghost.row=0;break;
        default:ghost.open=0;break;
        }
    }else{
        switch(ghost.row){
        case 0:ghost.speed_kills=!ghost.speed_kills;break;
        case 1:ghost.fire_trail=!ghost.fire_trail;break;
        case 2:ghost.water_ride=!ghost.water_ride;break;
        case 3:ghost.wall_ride=!ghost.wall_ride;break;
        case 4:ghost.low_fx=!ghost.low_fx;gr_clear_visuals();break;
        case 5:ghost.chain_mode=(ghost.chain_mode+1)%4;break;
        case 6:ghost.stare_mode=(ghost.stare_mode+1)%3;break;
        case 7:ghost.custom_ped=!ghost.custom_ped;break;
        default:ghost.page=0;ghost.row=0;break;
        }
    }
}
static u32 GR_CODE ghost_menu_open(void){return ghost.open;}
static void GR_CODE ghost_suspend(u32 network_transition){
    if(!ghost.initialized)return;
    ghost.open=0;ghost.attack_held=ghost.previous_attack=0;
    ghost.projectile_active=0;ghost.bike_job=0;
    if(network_transition){
        // No native gameplay writes after Story Mode exits. GTA tears down
        // its own entities/particles at the transition; forget their handles.
        ghost.powers=0;ghost.head_fx=ghost.head_ring_fx=ghost.fx_ped=ghost.fx_vehicle=0;
        for(u32 i=0;i<3;i++)ghost.head_ember_fx[i]=0;
        for(u32 i=0;i<2;i++)ghost.hand_fx[i]=0;
        for(u32 i=0;i<4;i++)ghost.wheel_fx[i]=ghost.wheel_outer_fx[i]=0;
        ghost.transformed_vehicle=ghost.transformed_model=0;
        ghost.proof_ped=ghost.proof_model=0;
        ghost.water_owned=0;
        ghost.previous_model=ghost.applied_model=ghost.model_job=0;
        ghost.target=0;
        ghost.rope=-1;ghost.rope_target=0;
    }else gr_turn_powers(0);
}
static void GR_CODE ghost_tick(u32 keys,u32 previous){
    if(!ghost.initialized){ghost.initialized=ghost.enabled=ghost.fire_trail=ghost.speed_kills=ghost.wall_ride=ghost.custom_ped=1;ghost.rope=-1;}
    mailbox.ghost_diag[1]=ghost.open;mailbox.ghost_diag[2]=ghost.mode;
    mailbox.ghost_diag[3]=ghost.fx_ready_core;
    mailbox.ghost_diag[5]=ghost.bike_job;mailbox.ghost_diag[6]=ghost.powers;
    gr_model_tick();
    u32 edge=keys&~previous;
    if((keys&1)&&(edge&16)){
        ghost.open=!ghost.open;ghost.row=ghost.page=0;
        if(ghost.open){menu.open=0;menu.confirm=0;preview_end();}
    }
    gr_input_tick(car(),ghost.open||menu.open||!ghost.enabled||!ghost.powers);
    if(ghost.open){
        if(edge&64){if(ghost.page){ghost.page=ghost.row=0;}else ghost.open=0;}
        else {
            u32 count=ghost.page?9:7;
            if(edge&4)ghost.row=(ghost.row+count-1)%count;
            if(edge&8)ghost.row=(ghost.row+1)%count;
            if(edge&32)gr_activate();
        }
        block_menu_buttons();gr_render();return;
    }
    if(!ghost.enabled||!ghost.powers){ghost.attack_held=ghost.previous_attack=0;gr_clear_visuals();return;}
    gr_request_fx();u32 p=ped(),v=car();if(!p)return;
    gr_proof_tick(p);gr_head_fire(p);gr_bike_tick(p,v);gr_fireball_tick();gr_chain_visual_tick();
    gr_weapon_tick(p);gr_injury_tick(p);
    if(!(keys&1)&&(edge&2)){ghost.mode=(ghost.mode+1)%4;say(gr_mode_name(ghost.mode));}
    if(menu.open){ghost.attack_held=ghost.previous_attack=0;return;}
    u32 attack=(gr_input.held&GR_ATTACK)!=0;
    u32 aimed=(gr_input.held&GR_AIM)!=0;
    mailbox.ghost_diag[7]=attack|(aimed<<1)|(ghost.attack_held<<2);
    if(gr_input.pressed&GR_ATTACK){ghost.attack_started=now();ghost.attack_held=1;}
    if(attack&&ghost.mode==3)gr_flamethrower_tick();
    if((gr_input.released&GR_ATTACK)&&ghost.attack_held){
        mailbox.ghost_diag[15]++;
        u32 duration=now()-ghost.attack_started;
        switch(ghost.mode){
        case 0:gr_fireball_start(duration);break;
        case 1:gr_chain_attack(duration,aimed);break;
        case 2:gr_penance_attack(duration);break;
        default:break;
        }
        ghost.attack_held=0;
    }
    ghost.previous_attack=attack;ghost.previous_aim=aimed;
    gr_charge_display();
}
