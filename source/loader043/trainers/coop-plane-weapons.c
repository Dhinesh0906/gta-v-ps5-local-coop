/* Offline aircraft payload adapter. Behavior parameters come from the pinned
 * script reference; every native address/ABI is the exact PS5 executable.
 * Only the current owned P2 pilot can request production shots. */
#include "coop-plane-weapon-reference.h"
typedef struct {u32 weapon,model,weapon_owned,model_owned,requested;} PlaneWeaponAsset;
typedef struct {
    u32 enabled,vehicle,error,last_frame,fire_mask,counter_mask,last_bind_ms,bindings_valid;
    CoopFlightPress fire,counter;
    u32 last_bomb,last_flare,bomb_time_valid,flare_time_valid;
    u32 bomb_left,next_bomb,flare_stage,flare_started;
    u32 bombs_sent,flares_sent,bursts_sent,blocked,auto_equip,equipped_vehicle;
    i32 bomb_mod,counter_mod,bomb_ammo,counter_ammo;
    PlaneWeaponAsset bomb_asset,flare_asset;
} PlaneWeapons;
_Static_assert(sizeof(PlaneWeapons)==176,"Plane weapons ABI");
COOP_SECTION(".coop.plane_weapons") volatile PlaneWeapons plane_weapons;
#ifndef GTA_COOP_TEST
#define PW_EXT __attribute__((section(".coop.extensions"),noinline))
#else
#define PW_EXT
#endif
static PW_EXT const PlaneWeaponModel *plane_weapon_model(u32 hash){
    for(u32 n=0;n<sizeof(plane_weapon_models)/sizeof(plane_weapon_models[0]);n++)if(plane_weapon_models[n].model==hash)return &plane_weapon_models[n];
    return 0;
}
static PW_EXT void plane_asset_release(volatile PlaneWeaponAsset *a){
    if(a->weapon_owned)N(REMOVE_WEAPON_ASSET,a->weapon);
    if(a->model_owned)N(SET_MODEL_AS_NO_LONGER_NEEDED,a->model);
    *a=(PlaneWeaponAsset){0};
}
static PW_EXT void plane_weapons_release(void){
    plane_asset_release(&plane_weapons.bomb_asset);plane_asset_release(&plane_weapons.flare_asset);
    plane_weapons.vehicle=plane_weapons.bomb_left=plane_weapons.flare_stage=plane_weapons.equipped_vehicle=0;
    plane_weapons.fire=(CoopFlightPress){0};plane_weapons.counter=(CoopFlightPress){0};
}
static PW_EXT u32 plane_asset_ready(volatile PlaneWeaponAsset *a,u32 weapon,u32 model,u32 now){
    if(a->weapon!=weapon||a->model!=model){plane_asset_release(a);a->weapon=weapon;a->model=model;a->requested=now;}
    if(!N(IS_WEAPON_VALID,weapon)||!N(IS_MODEL_VALID,model)||!N(IS_MODEL_IN_CDIMAGE,model))return 0;
    u32 w=N(HAS_WEAPON_ASSET_LOADED,weapon)!=0,m=N(HAS_MODEL_LOADED,model)!=0;
    if(!w&&!a->weapon_owned){N(REQUEST_WEAPON_ASSET,weapon,31,0);a->weapon_owned=1;}
    if(!m&&!a->model_owned){N(REQUEST_MODEL,model);a->model_owned=1;}
    return w&&m;
}
static PW_EXT u32 plane_weapons_owner(u32 v,u32 ped){
    if(!v||!ped||ped==session.p1||(u32)N(GET_VEHICLE_CLASS,v)!=16||
       (u32)N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)!=ped)return 0;
#ifndef GTA_COOP_TEST
    return plane_entity(v,ped)!=0;
#else
    return 1;
#endif
}
static PW_EXT u32 aircraft_bomb_context(u32 v){
    if(!plane_weapons.enabled||!plane_controls.enabled||!plane_weapons_owner(v,session.p2))return 0;
    const PlaneWeaponModel *p=plane_weapon_model(N(GET_ENTITY_MODEL,v));
    i32 mod=(i32)N(GET_VEHICLE_MOD,v,9);
    return p&&p->bomb_t>=0&&mod>=0&&mod<=3&&N(GET_ARE_BOMB_BAY_DOORS_OPEN,v);
}
static PW_EXT u32 plane_world_offset(u32 v,float x,float y,float z,float *out){
    u64 raw[4]={0};V(GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS,raw,v,F(x),F(y),F(z));unpack(out,raw);
    return coop_position_valid(out);
}
static PW_EXT void plane_payload_shot(u32 ped,u32 hash,const float *a,const float *b,u32 flare){
    /* All 21 slots match the script's projectile call. No shared P1 input,
     * generic explosion, custom force or substitute particle projectile. */
    N(SHOOT_SINGLE_BULLET_BETWEEN_COORDS_IGNORE_ENTITY_NEW,F(a[0]),F(a[1]),F(a[2]),F(b[0]),F(b[1]),F(b[2]),
      0,flare,hash,ped,1,1,F(-1),0,0,0,0,1,1,0,0);
}
static PW_EXT u32 plane_bomb_shot(u32 v,u32 ped,const PlaneWeaponModel *p,u32 kind,u32 now,volatile PlaneWeaponAsset *asset){
    static const char weapons[][40]={"vehicle_weapon_bomb","vehicle_weapon_bomb_incendiary","vehicle_weapon_bomb_gas","vehicle_weapon_bomb_cluster"};
    static const char models[][20]={"w_smug_bomb_01","w_smug_bomb_04","w_smug_bomb_03","w_smug_bomb_02"};
    if(kind>3||!p||p->bomb_t<0||!plane_weapons_owner(v,ped))return 0;
    u32 hash=model_hash(weapons[kind]);
    if(!plane_asset_ready(asset,hash,model_hash(models[kind]),now))return 0;
    i32 ammo=(i32)N(GET_VEHICLE_BOMB_AMMO,v);if(ammo==0||ammo<-1||ammo>1000)return 0;
    u64 lo[4]={0},hi[4]={0};N(GET_MODEL_DIMENSIONS,p->model,P(lo),P(hi));
    float min[3],max[3],a[3],b[3];unpack(min,lo);unpack(max,hi);
    for(u32 n=0;n<3;n++)if(min[n]!=min[n]||max[n]!=max[n]||max[n]<=min[n]||max[n]-min[n]>500)return 0;
    float x=(min[0]+max[0])*0.5f,dy=min[1]-max[1];
    if(!plane_world_offset(v,x,max[1]+dy*p->bomb_t,min[2],a)||
       !plane_world_offset(v,x,max[1]+dy*(p->bomb_t-0.0001f),min[2],b))return 0;
    a[2]+=0.4f;b[2]+=0.2f;plane_payload_shot(ped,hash,a,b,0);
    if(ammo>0)N(SET_VEHICLE_BOMB_AMMO,v,ammo-1);
    return 1;
}
static PW_EXT u32 plane_flare_pair(u32 v,u32 ped,const PlaneWeaponModel *p,u32 outer,u32 now,volatile PlaneWeaponAsset *asset){
    if(!p||!p->flares||!plane_weapons_owner(v,ped)||(i32)N(GET_VEHICLE_MOD,v,1)!=1)return 0;
    u32 hash=model_hash("weapon_flaregun");
    if(!plane_asset_ready(asset,hash,model_hash("w_pi_flaregun_shell"),now))return 0;
    for(u32 n=0;n<2;n++){
        float sign=n?1:-1,a[3],b[3];
        if(!plane_world_offset(v,sign*(outer?p->outer:p->inner),p->flare_y,p->flare_z,a)||
           !plane_world_offset(v,sign*(outer?6:3),p->target_y,-0.2f,b))return 0;
        plane_payload_shot(ped,hash,a,b,1);
    }
    return 1;
}
static PW_EXT u32 plane_weapon_bindings(u32 now){
    if(plane_weapons.bindings_valid&&now-plane_weapons.last_bind_ms<1000)return 1;
    static const u32 actions[2]={COOP_INPUT_VEH_FLY_ATTACK,COOP_INPUT_VEH_FLY_COUNTER};u32 masks[2];
    for(u32 n=0;n<2;n++){
        u64 label=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,actions[n],1);
#ifndef GTA_COOP_TEST
        if(label!=0x548eb50ULL+(u64)actions[n]*64)return 0;
#endif
        masks[n]=button_from_label((const char*)label);if(!masks[n])return 0;
    }
    if(plane_weapons.bindings_valid&&(masks[0]!=plane_weapons.fire_mask||masks[1]!=plane_weapons.counter_mask))plane_weapons_release();
    plane_weapons.fire_mask=masks[0];plane_weapons.counter_mask=masks[1];plane_weapons.bindings_valid=1;plane_weapons.last_bind_ms=now;return 1;
}
/* Equipment only: no input/physics ownership is required by the shared
 * native mod operations. Callers must validate their own current driver. */
static PW_EXT u32 plane_equip_missing(u32 v,const PlaneWeaponModel *p){
    if(!p)return 0;
    u32 bombs=p->bomb_t>=0&&(i32)N(GET_VEHICLE_MOD,v,9)<0&&(i32)N(GET_NUM_VEHICLE_MODS,v,9)>0;
    u32 flares=p->flares&&(i32)N(GET_VEHICLE_MOD,v,1)<0&&(i32)N(GET_NUM_VEHICLE_MODS,v,1)>1;
    if(!bombs&&!flares)return 0;
    N(SET_VEHICLE_MOD_KIT,v,0);u32 changed=0;
    if(bombs){N(SET_VEHICLE_MOD,v,9,0,0);if((i32)N(GET_VEHICLE_MOD,v,9)==0){N(SET_VEHICLE_BOMB_AMMO,v,50);changed|=1;}}
    if(flares){N(SET_VEHICLE_MOD,v,1,1,0);if((i32)N(GET_VEHICLE_MOD,v,1)==1){N(SET_VEHICLE_COUNTERMEASURE_AMMO,v,20);changed|=2;}}
    return changed;
}
static PW_EXT void plane_weapon_equip_missing(u32 v,const PlaneWeaponModel *p){
    if(!plane_weapons.auto_equip||plane_weapons.equipped_vehicle==v||!p)return;
    plane_weapons.equipped_vehicle=v;plane_equip_missing(v,p);
}
typedef struct {u32 enabled,vehicle,model,changes,checks,last_frame;u64 entity;} PrimaryPayloadEquipment;
_Static_assert(sizeof(PrimaryPayloadEquipment)==32,"Primary equipment ABI");
COOP_SECTION(".coop.primary_payload_equipment") volatile PrimaryPayloadEquipment primary_payload_equipment;
static PW_EXT void primary_payload_equipment_tick(void){
    u32 p=session.p1;
    if(!primary_payload_equipment.enabled||session.state!=2||session.mode!=6||session.stop||session.error||session.paused||network()||!p){
        primary_payload_equipment.vehicle=primary_payload_equipment.model=0;primary_payload_equipment.entity=0;return;
    }
    /* Shared frame gate precedes all primary equipment natives. (AI-assisted) */
    if(primary_payload_equipment.last_frame==session.last_frame)return;primary_payload_equipment.last_frame=session.last_frame;
    if(p!=(u32)N(PLAYER_PED_ID)||N(IS_ENTITY_DEAD,p,0)||!N(IS_PED_IN_ANY_VEHICLE,p,0)){
        primary_payload_equipment.vehicle=primary_payload_equipment.model=0;primary_payload_equipment.entity=0;return;
    }
    u32 v=N(GET_VEHICLE_PED_IS_IN,p,0);
    if(!v||(u32)N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)!=p||(u32)N(GET_VEHICLE_CLASS,v)!=16){
        primary_payload_equipment.vehicle=primary_payload_equipment.model=0;primary_payload_equipment.entity=0;return;
    }
#ifndef GTA_COOP_TEST
    u64 entity=plane_entity(v,0);if(!entity)return;
#else
    u64 entity=0x22345000;
#endif
    u32 model=N(GET_ENTITY_MODEL,v);
    if(primary_payload_equipment.vehicle==v&&primary_payload_equipment.model==model&&primary_payload_equipment.entity==entity)return;
    primary_payload_equipment.vehicle=v;primary_payload_equipment.model=model;primary_payload_equipment.entity=entity;
    primary_payload_equipment.checks++;primary_payload_equipment.changes+=plane_equip_missing(v,plane_weapon_model(model))!=0;
}
static PW_EXT void plane_weapons_tick(void){
    if(!plane_weapons.enabled||!plane_controls.enabled||!plane_controls.active||session.state!=2||session.mode!=6||session.phase!=3||
       session.stop||session.error||session.paused||!session.connected||!session.input.armed||network()||
       !player_controls.p2_collision||player_controls.action==5){plane_weapons_release();return;}
    /* Payload ownership and loadout queries run once per game frame.
     * Cancellation above can still drain assets immediately. (AI-assisted) */
    if(plane_weapons.last_frame==session.last_frame)return;plane_weapons.last_frame=session.last_frame;
    u32 v=plane_controls.vehicle,now=player_controls.last_ms;
    if(!plane_weapons_owner(v,session.p2)||v!=player_controls.vehicle){plane_weapons_release();return;}
    if(!plane_weapon_bindings(now)){plane_weapons.error=1;plane_weapons_release();return;}
    if(plane_weapons.vehicle!=v){plane_weapons_release();plane_weapons.vehicle=v;}
    const PlaneWeaponModel *p=plane_weapon_model(N(GET_ENTITY_MODEL,v));plane_weapon_equip_missing(v,p);
    plane_weapons.bomb_mod=(i32)N(GET_VEHICLE_MOD,v,9);plane_weapons.counter_mod=(i32)N(GET_VEHICLE_MOD,v,1);
    plane_weapons.bomb_ammo=(i32)N(GET_VEHICLE_BOMB_AMMO,v);plane_weapons.counter_ammo=(i32)N(GET_VEHICLE_COUNTERMEASURE_AMMO,v);
    CoopFlightPress press=plane_weapons.fire;u32 fire=flight_press_tick(&press,now,(session.input.held&plane_weapons.fire_mask)!=0,500);plane_weapons.fire=press;
    press=plane_weapons.counter;u32 counter=flight_press_tick(&press,now,(session.input.held&plane_weapons.counter_mask)!=0,500);plane_weapons.counter=press;
    u32 airborne=N(IS_ENTITY_IN_AIR,v)&&as_float(N(GET_ENTITY_HEIGHT_ABOVE_GROUND,v))>=1.5f;
    u32 context=airborne&&aircraft_bomb_context(v),ready=!plane_weapons.bomb_time_valid||now-plane_weapons.last_bomb>=750;
    if(!context)plane_weapons.bomb_left=0;
    if(context&&!plane_weapons.bomb_left&&ready&&(fire==FLIGHT_PRESS_TAP||fire==FLIGHT_PRESS_HOLD||
       (plane_weapons.fire.down&&plane_weapons.fire.used))){
        plane_weapons.bomb_left=fire==FLIGHT_PRESS_TAP?1:5;plane_weapons.next_bomb=now;
    }
    if(plane_weapons.bomb_left&&(i32)(now-plane_weapons.next_bomb)>=0){
        if(plane_bomb_shot(v,session.p2,p,(u32)plane_weapons.bomb_mod,now,&plane_weapons.bomb_asset)){
            plane_weapons.bombs_sent++;plane_weapons.last_bomb=now;plane_weapons.bomb_time_valid=1;
            plane_weapons.bomb_left--;plane_weapons.next_bomb=now+200;
        }else if(plane_weapons.bomb_ammo==0||now-plane_weapons.bomb_asset.requested>5000){plane_weapons.bomb_left=0;plane_weapons.blocked++;}
    }
    if(counter==FLIGHT_PRESS_TAP&&airborne&&p&&p->flares&&plane_weapons.counter_mod==1&&!plane_weapons.flare_stage&&
       (!plane_weapons.flare_time_valid||now-plane_weapons.last_flare>=2500))plane_weapons.flare_stage=1;
    if(!airborne||plane_weapons.counter_mod!=1)plane_weapons.flare_stage=0;
    if(plane_weapons.flare_stage==1){
        i32 ammo=plane_weapons.counter_ammo;
        if(ammo==0||ammo<-1||ammo>1000){plane_weapons.flare_stage=0;plane_weapons.blocked++;}
        else if(plane_flare_pair(v,session.p2,p,0,now,&plane_weapons.flare_asset)){
            plane_weapons.flares_sent+=2;
            if(ammo>0)N(SET_VEHICLE_COUNTERMEASURE_AMMO,v,ammo-1);
            plane_weapons.flare_started=plane_weapons.last_flare=now;plane_weapons.flare_time_valid=1;plane_weapons.flare_stage=2;
        }else if(now-plane_weapons.flare_asset.requested>5000){plane_weapons.flare_stage=0;plane_weapons.blocked++;}
    }
    if(plane_weapons.flare_stage==2&&now-plane_weapons.flare_started>=250&&plane_flare_pair(v,session.p2,p,1,now,&plane_weapons.flare_asset)){
        plane_weapons.flares_sent+=2;plane_weapons.flare_stage=0;plane_weapons.last_flare=now;plane_weapons.bursts_sent++;
    }
    if(plane_weapons.flare_stage==2&&now-plane_weapons.flare_started>5000){plane_weapons.flare_stage=0;plane_weapons.blocked++;}
    plane_weapons.error=0;
}
#undef PW_EXT
