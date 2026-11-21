/* Bounded owned-vehicle audit. Explicit request only, default off. Mode 0
 * snapshots an empty, frozen vehicle and deletes it in the same tick. Mode 1
 * briefly runs a checked, privately copied physics method on an empty Deluxo.
 * Modes 2/3 apply bounded plane inputs, with an owned temporary pilot in mode 3.
 * Cleanup restores its table before deletion; no player or game-text writes.
 * Delayed loading/cleanup stays owned here across host disconnects.
 */
typedef struct {
    u32 request,completed,state,error,model,vehicle,started,model_owned;
    u64 table,physics,shared_method;
    u32 physical_type,vehicle_class,created,deleted;
    u32 mode,shadowed,started_frame,restored;
    u64 physics_calls,hover_calls;
    u64 plane_entity;float plane_original[4],plane_values[4];
    float rotation_start[3],rotation_end[3],velocity_start[3],velocity_end[3];
    u32 test_axis;float test_value;u32 control_updates,extra_stage;
    u32 pilot,pilot_model,engine_start,engine_end,pilots_created,pilots_deleted;
    u32 extra_kind,extra_requests;float extra_before,extra_after;u32 extra_expected,extra_verified;
} CoopVehicleProbe;
#include "coop-plane-catalogue.h"
#include "coop-plane-late-layout.h"
_Static_assert(sizeof(CoopVehicleProbe)==256,"Vehicle probe ABI");
COOP_SECTION(".coop.vehicle_probe") volatile CoopVehicleProbe vehicle_probe;
#ifdef GTA_COOP_TEST
static u32 vehicle_probe_identity(u32,u64*,u64*,u64*,u32*);
static u32 vehicle_probe_shadow(u32,u32);
static u64 vehicle_probe_calls(u32);
static u32 vehicle_probe_plane_shadow(u32,u32);
#else
/* The late-input helper is accepted only for the current explicit test body.
 * Normal P2 and every foreign private table remain excluded. (AI-assisted) */
__attribute__((section(".coop.extensions"),noinline))
static u32 plane_fixture_table_matches(u32 h,u64 entity){
 if(vehicle_probe.mode<5||vehicle_probe.mode>6||vehicle_probe.shadowed!=2||h!=vehicle_probe.vehicle||!entity||*(volatile u64*)entity!=PLANE_LATE_SHADOW)return 0;
 volatile PlaneLateInput *p=(volatile PlaneLateInput*)(PLANE_LATE_BASE+4096);
 return p->magic==PLANE_LATE_MAGIC&&p->owner==(u64)&session-0x8000ULL&&p->entity==entity&&p->vehicle==h&&p->pilot==vehicle_probe.pilot;
}
__attribute__((section(".coop.extensions"),noinline))
static u32 vehicle_probe_plane_shadow(u32 h,u32 install){
 u64 v=hover_lookup(h);if(!v||*(volatile u32*)(v+0xbf8)!=1)return 0;
 volatile PlaneLateInput *p=(volatile PlaneLateInput*)(PLANE_LATE_BASE+4096);
 if(p->magic!=PLANE_LATE_MAGIC||p->owner!=(u64)&session-0x8000ULL||p->shadow!=PLANE_LATE_SHADOW||p->original!=0x405ad50ULL||p->engine!=PLANE_LATE_BASE+0x4000)return 0;
 p->stop=1;
 if(install){
  if(p->busy||!vehicle_probe.pilot||vehicle_probe.pilot==session.p1||vehicle_probe.pilot==session.p2)return 0;
  for(u32 at=0;at<0x6a0;at+=8){
   u64 expected=*(volatile u64*)(0x405ad40ULL+at);
   if(at==16+0x498)expected=PLANE_LATE_BASE+0x800;
   else if(at==16+0x670)expected=PLANE_LATE_BASE+0x6000;
   if(*(volatile u64*)(PLANE_LATE_BASE+0x10000+at)!=expected)return 0;
  }
  p->entity=v;p->vehicle=h;p->pilot=vehicle_probe.pilot;p->mode=vehicle_probe.mode==6;p->remaining=10000;
  p->calls=p->applied=p->rejected=p->reason=0;for(u32 n=0;n<3;n++)p->phases[n]=0;
  *(volatile u32*)(PLANE_LATE_BASE+8192)=0;
  u64 expected=0x405ad50ULL;
  if(!__atomic_compare_exchange_n((u64*)v,&expected,PLANE_LATE_SHADOW,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;
  p->stop=0;return 1;
 }
 u64 expected=PLANE_LATE_SHADOW;
 if(*(volatile u64*)v==0x405ad50ULL){if(p->busy)return 0;plane_restore_pedal_lease(v);return 1;}
 if(!__atomic_compare_exchange_n((u64*)v,&expected,0x405ad50ULL,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;
 if(p->busy)return 0;plane_restore_pedal_lease(v);return 1;
}
/* The offline candidate has a fixed, checked near mapping. It stays mapped
 * until game exit, including after rollback, so in-flight physics callers
 * cannot execute freed code. This test never publishes to either player's car. */
static u64 vehicle_probe_calls(u32 hover){return *(volatile u64*)(hover?0x60001018ULL:0x60001040ULL);}
static u32 vehicle_probe_shadow(u32 handle,u32 install){
    if((i32)handle<0)return 0;
    u64 pool=*(volatile u64*)0x61a8250ULL;if(!pool)return 0;
    u32 index=handle>>8,count=*(volatile u32*)(pool+24),stride=*(volatile u32*)(pool+28);
    u64 entries=*(volatile u64*)(pool+8),generations=*(volatile u64*)(pool+16);
    if(!entries||!generations||index>=count||stride<16||stride>256||((volatile u8*)generations)[index]!=(u8)handle)return 0;
    u64 entity=*(volatile u64*)(entries+(u64)index*stride+8);if(!entity)return 0;
    u64 before=install?0x4056e28ULL:0x6000c010ULL,after=install?0x6000c010ULL:0x4056e28ULL;
    if(!install&&*(volatile u64*)entity==after)return 1;
    if(*(volatile u64*)entity!=before)return 0;
    if(install){
        if(*(volatile u64*)0x60001000ULL!=0x31544c4658554c44ULL||*(volatile u32*)0x60001010ULL||
           *(volatile u64*)0x6000c3d0ULL!=0x60000800ULL)return 0;
        for(u32 at=0;at<0x690;at+=8){
            if(at==0x3d0)continue;
            if(*(volatile u64*)(0x4056e18ULL+at)!=*(volatile u64*)(0x6000c000ULL+at))return 0;
        }
    }
    return __atomic_compare_exchange_n((u64*)entity,&before,after,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
}
static u32 vehicle_probe_identity(u32 handle,u64 *table,u64 *physics,u64 *shared,u32 *type){
    u64 pool=*(volatile u64*)0x61a8250ULL;if(!pool||(i32)handle<0)return 0;
    u32 index=handle>>8,count=*(volatile u32*)(pool+0x18),stride=*(volatile u32*)(pool+0x1c);
    u64 generations=*(volatile u64*)(pool+0x10),entries=*(volatile u64*)(pool+8);
    if(index>=count||!generations||!entries||stride<16||stride>256||((volatile u8*)generations)[index]!=(u8)handle)return 0;
    u64 entity=*(volatile u64*)(entries+(u64)index*stride+8);if(!entity)return 0;
    u64 t=*(volatile u64*)entity;
    if((t&7)||t<0x3f3c000ULL||t+0x3c8>0x40e0000ULL)return 0;
    u64 p=*(volatile u64*)(t+0x3c0),s=*(volatile u64*)(t+0x1f0);
    if(p<0x400000||p>=0x37c0000||s<0x400000||s>=0x37c0000)return 0;
    *table=t;*physics=p;*shared=s;*type=*(volatile u32*)(entity+0xbf8);return 1;
}
#endif
static u32 vehicle_probe_plane_model(u32 h){
    /* Every plane-menu model is eligible for the bounded class audit.
     * Plane input application separately checks exact physical type/table.
     * No unknown model can enter this job. (AI-assisted) */
    for(u32 n=0;n<sizeof(coop_plane_catalogue)/sizeof(coop_plane_catalogue[0]);n++)if(coop_plane_catalogue[n]==h)return 1;
    return 0;
}
static u32 vehicle_probe_model_allowed(u32 h){
    if(vehicle_probe_plane_model(h))return 1;
    static const char names[][12]={"deluxo","forklift","towtruck","bulldozer","bati","buzzard"};
    for(u32 n=0;n<sizeof(names)/sizeof(names[0]);n++)if(model_hash(names[n])==h)return 1;
    return 0;
}
static void vehicle_probe_finish(void){
    if(vehicle_probe.model_owned){N(SET_MODEL_AS_NO_LONGER_NEEDED,vehicle_probe.model);vehicle_probe.model_owned=0;}
    vehicle_probe.state=3;__atomic_store_n(&vehicle_probe.completed,vehicle_probe.request,__ATOMIC_RELEASE);
}
static u32 vehicle_probe_delete_pilot(void){
    u32 p=vehicle_probe.pilot;if(!p)return 1;
    if(p==session.p1||p==session.p2){vehicle_probe.error=11;return 0;}
    if(N(DOES_ENTITY_EXIST,p)){
        if((u32)N(GET_ENTITY_MODEL,p)!=vehicle_probe.pilot_model){vehicle_probe.error=11;return 0;}
        N(DELETE_PED,P(&p));
    }
    if(N(DOES_ENTITY_EXIST,vehicle_probe.pilot))return 0;
    vehicle_probe.pilot=0;vehicle_probe.pilots_deleted++;return 1;
}
static void vehicle_probe_plane_sample(u32 v,u32 end){
    u64 rot[4]={0},vel[4]={0};V(GET_ENTITY_ROTATION,rot,v,2);V(GET_ENTITY_VELOCITY,vel,v);
    for(u32 n=0;n<3;n++){
        if(end){vehicle_probe.rotation_end[n]=as_float(rot[n]);vehicle_probe.velocity_end[n]=as_float(vel[n]);}
        else{vehicle_probe.rotation_start[n]=as_float(rot[n]);vehicle_probe.velocity_start[n]=as_float(vel[n]);}
    }
    if(end)vehicle_probe.engine_end=N(GET_IS_VEHICLE_ENGINE_RUNNING,v);else vehicle_probe.engine_start=N(GET_IS_VEHICLE_ENGINE_RUNNING,v);
}
#ifndef GTA_COOP_TEST
__attribute__((section(".coop.extensions"),noinline))
#endif
static float vehicle_probe_extra_read(u32 v){
    return vehicle_probe.extra_kind==1?as_float(N(GET_VEHICLE_FLIGHT_NOZZLE_POSITION,v)):
        (float)(N(GET_ARE_BOMB_BAY_DOORS_OPEN,v)!=0);
}
#ifndef GTA_COOP_TEST
__attribute__((section(".coop.extensions"),noinline))
#endif
static void vehicle_probe_extra_set(u32 v,u32 target){
    if(vehicle_probe.extra_kind==1)N(SET_VEHICLE_FLIGHT_NOZZLE_POSITION,v,F((float)target));
    else if(target)N(OPEN_BOMB_BAY_DOORS,v);else N(CLOSE_BOMB_BAY_DOORS,v);
    vehicle_probe.extra_requests++;
}
#ifdef GTA_COOP_TEST
__attribute__((noinline))
#else
__attribute__((section(".coop.extensions"),noinline))
#endif
static void vehicle_probe_tick(void){
    /* Cleanup always runs for an owned handle, including a canceled/pause
     * request. Never let host disconnect or P2 despawn strand a test entity. */
    if(vehicle_probe.vehicle){
        u32 v=vehicle_probe.vehicle;
        if(N(DOES_ENTITY_EXIST,v)){
            u32 driver=N(GET_PED_IN_VEHICLE_SEAT,v,-1,0);
            if((u32)N(GET_ENTITY_MODEL,v)!=vehicle_probe.model||(driver&&driver!=vehicle_probe.pilot)){
                vehicle_probe.error=6;return; /* no deletion of an unexpected occupant/model */
            }
            if(vehicle_probe.mode>=2&&vehicle_probe.state==4){
                float values[4],original[4];for(u32 n=0;n<4;n++){values[n]=vehicle_probe.plane_values[n];original[n]=vehicle_probe.plane_original[n];}
                u32 running=vehicle_probe.request&&vehicle_probe.request!=vehicle_probe.completed&&!session.stop&&!session.paused&&
                    session.state==2&&session.mode==6&&session.phase==3&&!network()&&
                    (u32)N(GET_FRAME_COUNT)-vehicle_probe.started_frame<(vehicle_probe.mode==4?600u:60u)&&
                    (u32)N(GET_GAME_TIMER)-vehicle_probe.started<(vehicle_probe.mode==4?6000u:3000u);
                if(running&&vehicle_probe.mode==4&&!vehicle_probe.extra_stage){
                    float before=vehicle_probe_extra_read(v);
                    if((u32)N(GET_GAME_TIMER)-vehicle_probe.started>=1500&&coop_abs(before-(float)!vehicle_probe.extra_expected)<0.05f){
                        vehicle_probe.extra_before=before;vehicle_probe_extra_set(v,vehicle_probe.extra_expected);vehicle_probe.extra_stage=1;
                    }else vehicle_probe_extra_set(v,!vehicle_probe.extra_expected);
                }
                if(running&&plane_apply(v,vehicle_probe.pilot,vehicle_probe.plane_entity,values)){vehicle_probe.control_updates++;return;}
                if(running)vehicle_probe.error=10;
                if(vehicle_probe.mode==4){
                    vehicle_probe.extra_after=vehicle_probe_extra_read(v);
                    vehicle_probe.extra_verified=vehicle_probe.extra_stage==1&&vehicle_probe.extra_requests>=2&&
                        coop_abs(vehicle_probe.extra_before-(float)vehicle_probe.extra_expected)>0.5f&&
                        coop_abs(vehicle_probe.extra_after-(float)vehicle_probe.extra_expected)<0.05f;
                }
                vehicle_probe_plane_sample(v,1);plane_restore(v,vehicle_probe.plane_entity,original,values);
                vehicle_probe.restored++;vehicle_probe.state=2;
            }
            if(vehicle_probe.shadowed==2){
                if(!vehicle_probe_plane_shadow(v,0)){vehicle_probe.error=12;return;}
                vehicle_probe.shadowed=0;
            }
            if(vehicle_probe.shadowed==1){
                vehicle_probe.physics_calls=vehicle_probe_calls(0);vehicle_probe.hover_calls=vehicle_probe_calls(1);
                if(vehicle_probe.request&&vehicle_probe.request!=vehicle_probe.completed&&!session.stop&&!session.paused&&
                   session.state==2&&session.mode==6&&session.phase==3&&!network()&&
                   (u32)N(GET_FRAME_COUNT)-vehicle_probe.started_frame<60&&
                   (u32)N(GET_GAME_TIMER)-vehicle_probe.started<3000)return;
                if(!vehicle_probe_shadow(v,0)){vehicle_probe.error=8;return;}
                vehicle_probe.shadowed=0;vehicle_probe.restored++;
                if(!vehicle_probe.physics_calls&&!vehicle_probe.error)vehicle_probe.error=9;
            }
            if(!vehicle_probe_delete_pilot())return;
            N(DELETE_VEHICLE,P(&v));
        }
        if(!N(DOES_ENTITY_EXIST,vehicle_probe.vehicle)){
            if(!vehicle_probe_delete_pilot())return;
            vehicle_probe.shadowed=0;vehicle_probe.vehicle=0;vehicle_probe.deleted++;vehicle_probe_finish();
        }
        return;
    }
    if(!vehicle_probe_delete_pilot()||payload_probe_busy())return;
    u32 request=__atomic_load_n(&vehicle_probe.request,__ATOMIC_ACQUIRE);
    if(!request&&vehicle_probe.state==1){vehicle_probe.error=1;vehicle_probe_finish();return;}
    if(!request||request==vehicle_probe.completed)return;
    if(session.state!=2||session.mode!=6||session.phase!=3||session.stop||session.paused||network()){
        vehicle_probe.error=1;vehicle_probe_finish();return;
    }
    if(vehicle_probe.state!=1){
        vehicle_probe.error=0;vehicle_probe.table=vehicle_probe.physics=vehicle_probe.shared_method=0;
        vehicle_probe.created=vehicle_probe.deleted=0;vehicle_probe.physical_type=vehicle_probe.vehicle_class=0;
        vehicle_probe.shadowed=vehicle_probe.restored=0;vehicle_probe.physics_calls=vehicle_probe.hover_calls=0;
        vehicle_probe.plane_entity=0;vehicle_probe.control_updates=0;
        vehicle_probe.pilots_created=vehicle_probe.pilots_deleted=vehicle_probe.engine_start=vehicle_probe.engine_end=0;
        vehicle_probe.extra_requests=vehicle_probe.extra_verified=vehicle_probe.extra_stage=0;vehicle_probe.extra_before=vehicle_probe.extra_after=0;
        u32 h=vehicle_probe.model;
        if(vehicle_probe.mode>6||(vehicle_probe.mode==1&&h!=model_hash("deluxo"))||
           ((vehicle_probe.mode==2||vehicle_probe.mode==3||vehicle_probe.mode>=5)&&(!vehicle_probe_plane_model(h)||vehicle_probe.test_axis>4||
             vehicle_probe.test_value!=vehicle_probe.test_value||coop_abs(vehicle_probe.test_value)>1))||
           (vehicle_probe.mode==4&&((vehicle_probe.extra_kind!=1&&vehicle_probe.extra_kind!=2)||
             h!=model_hash(vehicle_probe.extra_kind==1?"hydra":"bombushka")||
             (vehicle_probe.test_value!=0&&vehicle_probe.test_value!=1)))||
           !vehicle_probe_model_allowed(h)||!N(IS_MODEL_VALID,h)||!N(IS_MODEL_IN_CDIMAGE,h)||!N(IS_MODEL_A_VEHICLE,h)){
            vehicle_probe.error=2;vehicle_probe_finish();return;
        }
        vehicle_probe.started=N(GET_GAME_TIMER);vehicle_probe.state=1;
        if(!N(HAS_MODEL_LOADED,h)){N(REQUEST_MODEL,h);vehicle_probe.model_owned=1;}
    }
    if(!N(HAS_MODEL_LOADED,vehicle_probe.model)){
        if((u32)N(GET_GAME_TIMER)-vehicle_probe.started>5000){vehicle_probe.error=3;vehicle_probe_finish();}
        return;
    }
    u32 v=N(CREATE_VEHICLE,vehicle_probe.model,F(-420),F(-580),F(vehicle_probe.mode==4?500:vehicle_probe.mode>=2?1500:-100),F(0),0,0,0);
    if(!v){vehicle_probe.error=4;vehicle_probe_finish();return;}
    vehicle_probe.vehicle=v;vehicle_probe.created++;vehicle_probe.state=2;
    N(SET_ENTITY_AS_MISSION_ENTITY,v,1,1);N(FREEZE_ENTITY_POSITION,v,1);N(SET_ENTITY_VISIBLE,v,0,0);
    if(N(DOES_ENTITY_EXIST,v)){
        u64 table=0,physics=0,shared=0;u32 type=0;
        if(vehicle_probe_identity(v,&table,&physics,&shared,&type)){
            vehicle_probe.table=table;vehicle_probe.physics=physics;vehicle_probe.shared_method=shared;
            vehicle_probe.physical_type=type;vehicle_probe.vehicle_class=N(GET_VEHICLE_CLASS,v);
        }else vehicle_probe.error=5;
    }else vehicle_probe.error=5;
    if(vehicle_probe.mode>=2&&!vehicle_probe.error&&!N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)){
        u64 entity=0;float original[4],values[4]={0,0,0,1};
        if(vehicle_probe.mode==4)values[3]=0;
        else if(vehicle_probe.test_axis<4)values[vehicle_probe.test_axis]=vehicle_probe.test_value;
        if(vehicle_probe.mode>=3){
            vehicle_probe.pilot_model=session.model;
            u32 pilot=N(CREATE_PED_INSIDE_VEHICLE,v,4,vehicle_probe.pilot_model,(u64)-1,0,0);
            if(pilot&&pilot!=session.p1&&pilot!=session.p2){
                vehicle_probe.pilot=pilot;vehicle_probe.pilots_created++;
                N(SET_ENTITY_AS_MISSION_ENTITY,pilot,1,1);N(SET_BLOCKING_OF_NON_TEMPORARY_EVENTS,pilot,1);N(SET_ENTITY_VISIBLE,pilot,0,0);
            }else vehicle_probe.error=11;
        }
        if(!vehicle_probe.error&&vehicle_probe.table==0x405ad50ULL&&vehicle_probe.physical_type==1&&
           plane_capture(v,vehicle_probe.pilot,&entity,original)&&plane_apply(v,vehicle_probe.pilot,entity,values)){
            vehicle_probe.plane_entity=entity;
            for(u32 n=0;n<4;n++){vehicle_probe.plane_original[n]=original[n];vehicle_probe.plane_values[n]=values[n];}
            if(vehicle_probe.mode>=5){
                if(!vehicle_probe_plane_shadow(v,1)){vehicle_probe.error=12;return;}
                vehicle_probe.shadowed=2;
            }
            vehicle_probe.state=4;vehicle_probe.control_updates=1;vehicle_probe.started=N(GET_GAME_TIMER);vehicle_probe.started_frame=N(GET_FRAME_COUNT);
            N(SET_VEHICLE_ENGINE_ON,v,1,1,0);N(FREEZE_ENTITY_POSITION,v,0);N(ACTIVATE_PHYSICS,v);N(SET_VEHICLE_FORWARD_SPEED,v,F(vehicle_probe.mode==4?0:40));
            if(vehicle_probe.mode==4){
                vehicle_probe.extra_before=vehicle_probe_extra_read(v);vehicle_probe.extra_expected=vehicle_probe.test_value>=0.5f;
                vehicle_probe_extra_set(v,!vehicle_probe.extra_expected);
            }
            vehicle_probe_plane_sample(v,0);return;
        }
        vehicle_probe.error=10;
    }
    if(vehicle_probe.mode==1&&!vehicle_probe.error&&!N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)){
        if(vehicle_probe.table==0x4056e28ULL&&vehicle_probe.physical_type==0&&vehicle_probe_shadow(v,1)){
            vehicle_probe.shadowed=1;vehicle_probe.started=N(GET_GAME_TIMER);vehicle_probe.started_frame=N(GET_FRAME_COUNT);
            N(SET_SPECIAL_FLIGHT_MODE_TARGET_RATIO,v,F(1));N(FREEZE_ENTITY_POSITION,v,0);N(ACTIVATE_PHYSICS,v);return;
        }
        vehicle_probe.error=7;
    }
    /* Same-tick cleanup; retry next tick only if the native retains it. */
    if(!N(GET_PED_IN_VEHICLE_SEAT,v,-1,0))N(DELETE_VEHICLE,P(&v));
    if(!N(DOES_ENTITY_EXIST,vehicle_probe.vehicle)){
        vehicle_probe.vehicle=0;vehicle_probe.deleted++;vehicle_probe_finish();
    }
}
