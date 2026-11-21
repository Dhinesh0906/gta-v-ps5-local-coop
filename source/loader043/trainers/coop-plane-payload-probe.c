/* Explicit, bounded payload audit. Only temporary NPC-owned projectiles are
 * queried/deleted. It never clears an area or another character's projectiles.
 * Cleanup stays in the game thread even if the host disconnects or cancels. */
typedef struct {
    u32 request,completed,state,error,kind,vehicle,pilot,pilot_model;
    u32 model_owned,started,stage_started,shots,observed,projectile;
    i32 ammo_before,ammo_after;
    u32 empty_since,created,deleted,pilots_created,pilots_deleted,cleanup_checks;
    PlaneWeaponAsset asset;
} PlanePayloadProbe;
_Static_assert(sizeof(PlanePayloadProbe)==108,"Payload probe ABI");
COOP_SECTION(".coop.payload_probe") volatile PlanePayloadProbe payload_probe;
#ifndef GTA_COOP_TEST
#define PP_EXT __attribute__((section(".coop.extensions"),noinline))
#else
#define PP_EXT
#endif
static PP_EXT u32 payload_probe_busy(void){
    return payload_probe.vehicle||payload_probe.pilot||payload_probe.projectile||
        (payload_probe.request&&payload_probe.request!=payload_probe.completed)||
        (payload_probe.state&&payload_probe.state!=6);
}
static PP_EXT u32 payload_probe_weapon(void){
    static const char names[][40]={"vehicle_weapon_bomb","vehicle_weapon_bomb_incendiary","vehicle_weapon_bomb_gas","vehicle_weapon_bomb_cluster","weapon_flaregun"};
    return payload_probe.kind<5?model_hash(names[payload_probe.kind]):0;
}
static PP_EXT u32 payload_probe_drain(u32 now){
    if(payload_probe.pilot&&N(DOES_ENTITY_EXIST,payload_probe.pilot)&&
       (payload_probe.pilot==session.p1||payload_probe.pilot==session.p2||
        (u32)N(GET_ENTITY_MODEL,payload_probe.pilot)!=payload_probe.pilot_model)){payload_probe.error=8;return 0;}
    if(payload_probe.projectile){
        u32 handle=payload_probe.projectile;
        if(N(DOES_ENTITY_EXIST,handle)){N(DELETE_ENTITY,P(&handle));return 0;}
        payload_probe.projectile=0;
    }
    if(payload_probe.pilot&&N(DOES_ENTITY_EXIST,payload_probe.pilot)){
        u64 coords[4]={0};u32 handle=0;
        if(N(GET_PROJECTILE_OF_PROJECTILE_TYPE_WITHIN_DISTANCE,payload_probe.pilot,payload_probe_weapon(),F(10000),P(coords),P(&handle),0)&&handle){
            if(handle==payload_probe.pilot||handle==payload_probe.vehicle||handle==session.p1||handle==session.p2){payload_probe.error=8;return 0;}
            payload_probe.projectile=handle;payload_probe.observed++;payload_probe.empty_since=0;
            N(SET_ENTITY_AS_MISSION_ENTITY,handle,1,1);N(DELETE_ENTITY,P(&handle));return 0;
        }
    }
    payload_probe.cleanup_checks++;
    if(!payload_probe.empty_since)payload_probe.empty_since=now;
    return now-payload_probe.empty_since>=300;
}
static PP_EXT void payload_probe_finish(void){
    plane_asset_release(&payload_probe.asset);
    if(payload_probe.model_owned){N(SET_MODEL_AS_NO_LONGER_NEEDED,model_hash("bombushka"));payload_probe.model_owned=0;}
    payload_probe.state=6;__atomic_store_n(&payload_probe.completed,payload_probe.request,__ATOMIC_RELEASE);
}
static PP_EXT void payload_probe_cleanup(u32 now){
    if(!payload_probe_drain(now))return;
    u32 p=payload_probe.pilot,v=payload_probe.vehicle;
    if(p&&N(DOES_ENTITY_EXIST,p)){
        if(p==session.p1||p==session.p2||(u32)N(GET_ENTITY_MODEL,p)!=payload_probe.pilot_model){payload_probe.error=8;return;}
        N(DELETE_PED,P(&p));if(N(DOES_ENTITY_EXIST,payload_probe.pilot))return;
    }
    if(payload_probe.pilot){payload_probe.pilot=0;payload_probe.pilots_deleted++;}
    if(v&&N(DOES_ENTITY_EXIST,v)){
        if((u32)N(GET_ENTITY_MODEL,v)!=model_hash("bombushka")||N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)){payload_probe.error=8;return;}
        N(DELETE_VEHICLE,P(&v));if(N(DOES_ENTITY_EXIST,payload_probe.vehicle))return;
    }
    if(payload_probe.vehicle){payload_probe.vehicle=0;payload_probe.deleted++;}
    payload_probe_finish();
}
static PP_EXT void payload_probe_tick(void){
    u32 now=N(GET_GAME_TIMER),request=__atomic_load_n(&payload_probe.request,__ATOMIC_ACQUIRE);
    if(payload_probe.state==5){payload_probe_cleanup(now);return;}
    if(!request||request==payload_probe.completed){
        if(payload_probe.vehicle||payload_probe.pilot||payload_probe.state==1){payload_probe.state=5;payload_probe.error=1;payload_probe_cleanup(now);}return;
    }
    if(session.state!=2||session.mode!=6||session.phase!=3||session.stop||session.paused||network()||
       (payload_probe.started&&payload_probe.state!=6&&now-payload_probe.started>12000)){
        payload_probe.error=1;payload_probe.state=5;payload_probe_cleanup(now);return;
    }
    if(payload_probe.state==0||payload_probe.state==6){
        u32 kind=payload_probe.kind;payload_probe=(PlanePayloadProbe){.request=request,.kind=kind,.state=1,.started=now};
        if(kind>4||vehicle_probe.vehicle||vehicle_probe.pilot||(vehicle_probe.request&&vehicle_probe.request!=vehicle_probe.completed)||!N(IS_MODEL_VALID,model_hash("bombushka"))){payload_probe.error=2;payload_probe_finish();return;}
        if(!N(HAS_MODEL_LOADED,model_hash("bombushka"))){N(REQUEST_MODEL,model_hash("bombushka"));payload_probe.model_owned=1;}
    }
    if(payload_probe.state==1){
        if(!N(HAS_MODEL_LOADED,model_hash("bombushka")))return;
        if(!session.model||!N(HAS_MODEL_LOADED,session.model)){payload_probe.error=2;payload_probe.state=5;return;}
        u32 v=N(CREATE_VEHICLE,model_hash("bombushka"),F(-4000),F(-4000),F(500),F(0),0,0,0);
        if(!v){payload_probe.error=3;payload_probe.state=5;return;}
        payload_probe.vehicle=v;payload_probe.created++;
        N(SET_ENTITY_AS_MISSION_ENTITY,v,1,1);N(FREEZE_ENTITY_POSITION,v,1);N(SET_ENTITY_VISIBLE,v,0,0);
        payload_probe.pilot_model=session.model;u32 p=N(CREATE_PED_INSIDE_VEHICLE,v,4,session.model,(u64)-1,0,0);
        if(!p||p==session.p1||p==session.p2){payload_probe.error=3;payload_probe.state=5;return;}
        payload_probe.pilot=p;payload_probe.pilots_created++;
        N(SET_ENTITY_AS_MISSION_ENTITY,p,1,1);N(SET_BLOCKING_OF_NON_TEMPORARY_EVENTS,p,1);N(SET_ENTITY_VISIBLE,p,0,0);
        payload_probe.state=2;payload_probe.stage_started=now;return;
    }
    u32 v=payload_probe.vehicle,p=payload_probe.pilot;
    if(!N(DOES_ENTITY_EXIST,v)||!N(DOES_ENTITY_EXIST,p)||
       (u32)N(GET_ENTITY_MODEL,v)!=model_hash("bombushka")||(u32)N(GET_ENTITY_MODEL,p)!=payload_probe.pilot_model||!plane_weapons_owner(v,p)){
        payload_probe.error=8;payload_probe.state=5;return;
    }
    if(payload_probe.state==2){
        if(now-payload_probe.stage_started<500)return;
        u32 slot=payload_probe.kind==4?1:9,mod=payload_probe.kind==4?1:payload_probe.kind;
        N(SET_VEHICLE_MOD_KIT,v,0);N(SET_VEHICLE_MOD,v,slot,mod,0);
        if((u32)N(GET_VEHICLE_MOD,v,slot)!=mod){payload_probe.error=4;payload_probe.state=5;return;}
        if(payload_probe.kind==4)N(SET_VEHICLE_COUNTERMEASURE_AMMO,v,2);else N(SET_VEHICLE_BOMB_AMMO,v,2);
        payload_probe.ammo_before=(i32)(payload_probe.kind==4?N(GET_VEHICLE_COUNTERMEASURE_AMMO,v):N(GET_VEHICLE_BOMB_AMMO,v));
        if(payload_probe.ammo_before!=2){payload_probe.error=4;payload_probe.state=5;return;}
        payload_probe.state=3;payload_probe.stage_started=now;
    }
    const PlaneWeaponModel *model=plane_weapon_model(model_hash("bombushka"));
    if(payload_probe.state==3){
        u32 fired=payload_probe.kind==4?plane_flare_pair(v,p,model,0,now,&payload_probe.asset):plane_bomb_shot(v,p,model,payload_probe.kind,now,&payload_probe.asset);
        if(!fired)return;
        payload_probe.shots=payload_probe.kind==4?2:1;
        if(payload_probe.kind==4)N(SET_VEHICLE_COUNTERMEASURE_AMMO,v,1);
        payload_probe.state=4;payload_probe.stage_started=now;
    }
    if(payload_probe.state==4){
        if(payload_probe.kind==4&&payload_probe.shots==2&&now-payload_probe.stage_started>=250){
            if(plane_flare_pair(v,p,model,1,now,&payload_probe.asset))payload_probe.shots=4;
        }
        u32 drained=payload_probe_drain(now),expected=payload_probe.kind==4?4:1;
        if(payload_probe.shots==expected&&payload_probe.observed==expected&&drained){
            payload_probe.ammo_after=(i32)(payload_probe.kind==4?N(GET_VEHICLE_COUNTERMEASURE_AMMO,v):N(GET_VEHICLE_BOMB_AMMO,v));
            if(payload_probe.ammo_after!=1)payload_probe.error=6;
            payload_probe.state=5;payload_probe_cleanup(now);
        }else if(now-payload_probe.stage_started>5000){payload_probe.error=7;payload_probe.state=5;payload_probe_cleanup(now);}
    }
}
#undef PP_EXT
