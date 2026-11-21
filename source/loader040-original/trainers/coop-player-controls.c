/* Included by coop-session.c: tasks affect only its owned ped. This mirrors
 * P1's configured action buttons, not GTA's native player/weapon-wheel state.
 */
typedef struct {
    u32 initialized,error,last_ms,last_bind_ms,bindings[12];
    u32 last_sprint_tap,sprint_taps,last_action_ms,action;
    u32 vehicle,pending_vehicle,entry_started,vehicle_action;
    u32 weapons[32],weapon_count,weapon_index,previous_weapon,wheel_changed;
    u32 aim_ticks,shoot_ticks,reloads,entries,exits,drive_ticks,melee_ticks,cover_ticks;
    u32 stealth,cover,sprint_ticks,binding_updates;
    float aim[3],move_speed;
    u32 weapon_groups[32];
    u32 vehicle_bindings[8],ability_vehicle,vehicle_model;
    u32 boost_requests,boost_readback,boost_rejected,parachute_requests,transform_requests;
    u32 wings_active,hover_active,vehicle_shots,last_vehicle_shot;
    float primary_distance_squared;
    u32 current_vehicle_weapon,weapon_cycle_requests;
    u32 stream_vehicle,stream_vehicle_model,stream_original_flag,stream_original_valid;
    u32 collision_requests,physics_wakes,last_stream_request,p2_static,p2_collision;
    u32 body_rotation_ticks;
    u32 effective_boost_binding,steer_bias_ticks,passenger_shooting,special_car_binding;
    u32 primary_recoveries,primary_wait_ticks,primary_recovery_reason,primary_resume_enabled;
    u32 vehicle_jump_requests,vehicle_jump_rejected;
    u32 private_movement;float movement_yaw;
    u32 extra_vehicle,extra_flags,light_binding,light_started;
    u32 light_held,light_consumed,horn_started,horn_held;
    u32 horn_consumed,last_horn_ms,light_cycles,roof_requests;
    u32 siren_requests,horn_ticks,burnout_ticks,roof_blocked;
    u32 trailer_detach_requests,shuffle_requests;
    u32 enter_pending,enter_pressed_at,bail_requested,bail_requests,passenger_entry_requests;
    u32 aircraft_bindings[4],gear_requests,gear_blocked;
    u32 tow_detach_requests;
} PlayerControls;
_Static_assert(sizeof(PlayerControls)==0x2d8,"Player-controls mailbox size");
COOP_SECTION(".coop.controls") volatile PlayerControls player_controls;
#include "coop-vehicle-weapons.h"
#include "coop-road-capabilities.h"
#include "coop-input-ids.h"
#ifdef GTA_COOP_TEST
static i32 entity_load_collision_flag(u32);
static u32 vehicle_request_stock_jump(u32);
#else
/* Exact-build input at 0x2761779 sets this byte; the stock jumping-car
 * physics at 0x2645683 consumes it. No custom force/velocity/animation. The
 * outer call already requires owned P2 to be the driver and GET_CAR_HAS_JUMP.
 * Re-resolve the generation-checked script GUID immediately before writing. */
static u32 vehicle_request_stock_jump(u32 handle) {
    if((i32)handle<0)return 0;
    u64 pool=*(volatile u64*)0x61a8250ULL;if(!pool)return 0;
    u32 index=handle>>8,count=*(volatile u32*)(pool+0x18),stride=*(volatile u32*)(pool+0x1c);
    u64 generations=*(volatile u64*)(pool+0x10),entries=*(volatile u64*)(pool+8);
    if(index>=count||!generations||!entries||stride<16||stride>256||
       ((volatile u8*)generations)[index]!=(u8)handle)return 0;
    u64 vehicle=*(volatile u64*)(entries+(u64)index*stride+8);
    if(!vehicle||*(volatile u32*)(vehicle+0x354)||*(volatile u8*)(vehicle+0x358))return 0;
    *(volatile u8*)(vehicle+0x358)=1;
    return *(volatile u8*)(vehicle+0x358)==1;
}
static i32 entity_load_collision_flag(u32 handle) {
    /* Read only: the same generation-checked script-GUID lookup as the exact
     * SET_ENTITY_LOAD_COLLISION_FLAG wrapper. Its helper stores the inverse
     * of the requested setting in entity +0x158 bit 2. */
    if((i32)handle<0)return -1;
    u64 pool=*(volatile u64*)0x61a8250ULL;
    if(!pool)return -1;
    u32 index=handle>>8,count=*(volatile u32*)(pool+0x18),stride=*(volatile u32*)(pool+0x1c);
    u64 generations=*(volatile u64*)(pool+0x10),entries=*(volatile u64*)(pool+8);
    if(index>=count||!generations||!entries||stride<16||stride>256||
       ((volatile u8*)generations)[index]!=(u8)handle)return -1;
    u64 entity=*(volatile u64*)(entries+(u64)index*stride+8);
    if(!entity)return -1;
    return (*(volatile u32*)(entity+0x158)&4u)==0;
}
#endif
static void controls_release_vehicle_streaming(void) {
    if(player_controls.stream_original_valid&&player_controls.stream_vehicle&&
       N(DOES_ENTITY_EXIST,player_controls.stream_vehicle)&&
       (u32)N(GET_ENTITY_MODEL,player_controls.stream_vehicle)==player_controls.stream_vehicle_model)
        N(SET_ENTITY_LOAD_COLLISION_FLAG,player_controls.stream_vehicle,player_controls.stream_original_flag,0);
    player_controls.stream_original_valid=0;player_controls.stream_vehicle=0;
}
static void controls_extras_reset(void){
    player_controls.extra_vehicle=0;player_controls.extra_flags=0;
    player_controls.light_held=player_controls.light_consumed=0;
    player_controls.horn_held=player_controls.horn_consumed=0;
}
static void controls_reset_for_ped(void) {
    controls_release_vehicle_streaming();
    controls_extras_reset();
    player_controls.enter_pending=player_controls.bail_requested=0;
    player_controls.initialized=0;player_controls.action=0;player_controls.pending_vehicle=0;
    player_controls.cover=0;player_controls.weapon_count=0;player_controls.weapon_index=0;
    player_controls.previous_weapon=0;player_controls.wheel_changed=0;
    player_controls.ability_vehicle=0;player_controls.current_vehicle_weapon=0;
    player_controls.wings_active=player_controls.hover_active=0;
    player_controls.passenger_shooting=0;
}

enum { B_SPRINT,B_JUMP,B_ENTER,B_ATTACK,B_AIM,B_STEALTH,B_WEAPON,B_COVER,B_RELOAD,B_ACCEL,B_BRAKE,B_HANDBRAKE };
enum { VB_HORN,VB_BOOST,VB_PARACHUTE,VB_WINGS,VB_TRANSFORM,VB_FIRE,VB_AIM,VB_WEAPON };
enum { AB_GEAR,AB_FIRE,AB_WEAPON,AB_BOOST };
static u32 button_from_label(const char *s) {
    if(!s||s[0]!='b'||s[1]!='_'||s[2]<'0'||s[2]>'9')return 0;
    u32 id=(u32)(s[2]-'0');
    if(s[3]){if(s[3]<'0'||s[3]>'9'||s[4])return 0;id=id*10u+(u32)(s[3]-'0');}
    switch(id){
        case 4:return 0x10;case 5:return 0x40;case 6:return 0x80;case 7:return 0x20;
        case 16:return 2;case 25:return 4;
        case 30:return 0x4000;case 31:return 0x2000;case 32:return 0x8000;case 33:return 0x1000;
        case 34:return 0x400;case 35:return 0x100;case 36:return 0x800;case 37:return 0x200;
        default:return 0;
    }
}
static void controls_bindings(u32 now) {
    static const u32 actions[12]={21,22,23,24,25,36,37,44,45,71,72,76};
    if(player_controls.binding_updates&&now-player_controls.last_bind_ms<1000)return;
    player_controls.last_bind_ms=now;player_controls.error=0;
    for(u32 i=0;i<12;i++){
        u64 label=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,actions[i],1);
#ifndef GTA_COOP_TEST
        /* The audited handler always returns its own 64-byte action buffer. */
        if(label!=0x548eb50ULL+(u64)actions[i]*64u){player_controls.error=1;return;}
#endif
        u32 mask=button_from_label((const char*)label);
        if(!mask){player_controls.error=2;return;}
        player_controls.bindings[i]=mask;
    }
    static const u32 vehicle_actions[8]={COOP_INPUT_VEH_HORN,COOP_INPUT_VEH_ROCKET_BOOST,
        COOP_INPUT_VEH_PARACHUTE,COOP_INPUT_VEH_BIKE_WINGS,COOP_INPUT_VEH_TRANSFORM,
        COOP_INPUT_VEH_ATTACK,COOP_INPUT_VEH_AIM,COOP_INPUT_VEH_SELECT_NEXT_WEAPON};
    static const u32 defaults[8]={2,2,0x4000,0x4000,0x4000,0x800,0x400,0x8000};
    for(u32 i=0;i<8;i++){
        u64 label=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,vehicle_actions[i],1);
#ifndef GTA_COOP_TEST
        if(label!=0x548eb50ULL+(u64)vehicle_actions[i]*64u){player_controls.error=1;return;}
#endif
        u32 mask=button_from_label((const char*)label);
        /* Inactive vehicle contexts can have no instructional label. Keep the
         * standard DualSense action as a fallback, without disabling walking. */
        player_controls.vehicle_bindings[i]=mask?mask:defaults[i];
    }
    /* Exact PS5 enum: boost is 388, not legacy PC 351 (replay FOV).
     * Some model contexts still override the generic boost action below. */
    u64 car_label=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,73,1);
#ifndef GTA_COOP_TEST
    if(car_label!=0x548eb50ULL+73u*64u){player_controls.error=1;return;}
#endif
    player_controls.special_car_binding=button_from_label((const char*)car_label);
    if(!player_controls.special_car_binding)player_controls.special_car_binding=0x4000;
    u64 light_label=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,74,1);
#ifndef GTA_COOP_TEST
    if(light_label!=0x548eb50ULL+74u*64u){player_controls.error=1;return;}
#endif
    player_controls.light_binding=button_from_label((const char*)light_label);
    if(!player_controls.light_binding)player_controls.light_binding=0x20;
    static const u32 air_actions[4]={COOP_INPUT_VEH_FLY_UNDERCARRIAGE,COOP_INPUT_VEH_FLY_ATTACK,
        COOP_INPUT_VEH_FLY_SELECT_NEXT_WEAPON,COOP_INPUT_VEH_FLY_BOOST};
    static const u32 air_defaults[4]={2,0x4000,0x80,2};
    for(u32 i=0;i<4;i++){
        u64 label=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,air_actions[i],1);
#ifndef GTA_COOP_TEST
        if(label!=0x548eb50ULL+(u64)air_actions[i]*64u){player_controls.error=1;return;}
#endif
        u32 mask=button_from_label((const char*)label);
        player_controls.aircraft_bindings[i]=mask?mask:air_defaults[i];
    }
    player_controls.binding_updates++;
}
static u32 action_down(u32 action) {
    u32 mask=player_controls.bindings[action];
    return (session.input.held&mask)!=0 ||
        (mask==0x100&&session.data[8]>32) || (mask==0x200&&session.data[9]>32);
}
static u32 action_pressed(u32 action) { return (session.input.pressed&player_controls.bindings[action])!=0; }
static void controls_stop_task(void) {
    player_controls.enter_pending=0;
    controls_extras_reset();
    if(player_controls.action||session.moving)N(CLEAR_PED_TASKS,session.p2);
    player_controls.action=session.moving=0;player_controls.cover=0;
}
static u32 vehicle_pressed(u32 action);
static u32 vehicle_down(u32 action);
#ifdef GTA_COOP_TEST
__attribute__((noinline))
#else
__attribute__((section(".coop.extensions"),noinline))
#endif
static void controls_vehicle_extras(u32 now,u32 vehicle){
    if(player_controls.extra_vehicle!=vehicle){
        controls_extras_reset();player_controls.extra_vehicle=vehicle;
        for(u32 i=0;i<sizeof(coop_road_siren_models)/sizeof(coop_road_siren_models[0]);i++)
            if(coop_road_siren_models[i]==player_controls.vehicle_model){player_controls.extra_flags=1;break;}
    }
    u32 light_down=(session.input.held&player_controls.light_binding)!=0;
    if(session.input.pressed&player_controls.light_binding){
        player_controls.light_held=1;player_controls.light_started=now;player_controls.light_consumed=0;
    }
    if(light_down&&player_controls.light_held&&!player_controls.light_consumed&&now-player_controls.light_started>=500){
        u32 trailer=0;player_controls.light_consumed=2; /* Long press checked; normal lights remain available. */
        if(N(GET_VEHICLE_TRAILER_VEHICLE,vehicle,P(&trailer))&&trailer&&trailer!=vehicle&&N(DOES_ENTITY_EXIST,trailer)){
            N(DETACH_VEHICLE_FROM_TRAILER,vehicle);player_controls.trailer_detach_requests++;player_controls.light_consumed=1;
        }else{
            /* Tow-hook release is distinct from a semi-trailer hitch. Both
             * natives resolve this exact truck's type-7 component. A short
             * tap still changes lights; never send either call as passenger. */
            u32 towed=N(GET_ENTITY_ATTACHED_TO_TOW_TRUCK,vehicle);
            u32 towed_model=towed&&towed!=vehicle&&N(DOES_ENTITY_EXIST,towed)?N(GET_ENTITY_MODEL,towed):0;
            if(towed_model&&N(IS_MODEL_A_VEHICLE,towed_model)){
                N(DETACH_VEHICLE_FROM_TOW_TRUCK,vehicle,towed);
                player_controls.tow_detach_requests++;player_controls.light_consumed=1;
            }
        }
        if(!(player_controls.light_consumed&1)&&N(IS_VEHICLE_A_CONVERTIBLE,vehicle,0)){
            player_controls.light_consumed=1;
            u32 state=N(GET_CONVERTIBLE_ROOF_STATE,vehicle);
            if(!N(IS_ENTITY_IN_AIR,vehicle)&&(state==0||state==2)){
                if(state==0)N(LOWER_CONVERTIBLE_ROOF,vehicle,0);else N(RAISE_CONVERTIBLE_ROOF,vehicle,0);
                player_controls.roof_requests++;
            }else player_controls.roof_blocked++;
        }
    }
    if(!light_down&&player_controls.light_held){
        if(!(player_controls.light_consumed&1)){
            u32 lights=0,full=0;
            if(N(GET_VEHICLE_LIGHTS_STATE,vehicle,P(&lights),P(&full))){
                if(!lights){N(SET_VEHICLE_LIGHTS,vehicle,2);N(SET_VEHICLE_FULLBEAM,vehicle,0);}
                else if(!full)N(SET_VEHICLE_FULLBEAM,vehicle,1);
                else{N(SET_VEHICLE_FULLBEAM,vehicle,0);N(SET_VEHICLE_LIGHTS,vehicle,1);}
                player_controls.light_cycles++;
            }
        }
        player_controls.light_held=player_controls.light_consumed=0;
    }
    u32 horn_down=vehicle_down(VB_HORN);
    if(vehicle_pressed(VB_HORN)){
        player_controls.horn_held=1;player_controls.horn_started=now;player_controls.horn_consumed=0;
    }
    if(horn_down&&player_controls.horn_held&&!N(GET_HAS_ROCKET_BOOST,vehicle)&&!N(GET_CAR_HAS_JUMP,vehicle)){
        u32 siren=player_controls.extra_flags&1;
        if(!siren||now-player_controls.horn_started>=500){
            if(vehicle_pressed(VB_HORN)||!player_controls.horn_consumed||now-player_controls.last_horn_ms>=100){
                N(START_VEHICLE_HORN,vehicle,200,model_hash("HELDDOWN"),0);
                player_controls.horn_ticks++;player_controls.last_horn_ms=now;
            }
            player_controls.horn_consumed=1;
        }
    }
    if(!horn_down&&player_controls.horn_held){
        if((player_controls.extra_flags&1)&&!player_controls.horn_consumed&&
           !N(GET_HAS_ROCKET_BOOST,vehicle)&&!N(GET_CAR_HAS_JUMP,vehicle)){
            u32 on=N(IS_VEHICLE_SIREN_ON,vehicle);
            N(SET_VEHICLE_SIREN,vehicle,!on);player_controls.siren_requests++;
        }
        player_controls.horn_held=player_controls.horn_consumed=0;
    }
}
static void controls_loadout(void) {
    static const char names[][32]={"WEAPON_UNARMED","WEAPON_PISTOL","WEAPON_COMBATPISTOL","WEAPON_APPISTOL",
        "WEAPON_PISTOL50","WEAPON_SNSPISTOL","WEAPON_HEAVYPISTOL","WEAPON_MICROSMG","WEAPON_SMG",
        "WEAPON_ASSAULTSMG","WEAPON_COMBATPDW","WEAPON_PUMPSHOTGUN","WEAPON_SAWNOFFSHOTGUN",
        "WEAPON_ASSAULTSHOTGUN","WEAPON_BULLPUPSHOTGUN","WEAPON_ASSAULTRIFLE","WEAPON_CARBINERIFLE",
        "WEAPON_ADVANCEDRIFLE","WEAPON_SPECIALCARBINE","WEAPON_BULLPUPRIFLE","WEAPON_MG","WEAPON_COMBATMG",
        "WEAPON_SNIPERRIFLE","WEAPON_HEAVYSNIPER","WEAPON_MARKSMANRIFLE","WEAPON_GRENADELAUNCHER",
        "WEAPON_RPG","WEAPON_MINIGUN","WEAPON_GRENADE","WEAPON_STICKYBOMB","WEAPON_KNIFE"};
    u32 selected=N(GET_SELECTED_PED_WEAPON,session.p1);
    for(u32 i=0;i<sizeof(names)/sizeof(names[0]);i++){
        u32 weapon=model_hash(names[i]);
        if(i&&!N(IS_WEAPON_VALID,weapon))continue;
        u32 at=player_controls.weapon_count++;
        player_controls.weapons[at]=weapon;
        player_controls.weapon_groups[at]=i==0?7:i<=6?0:i<=10?1:i<=14?3:i<=21?2:i<=24?4:i<=27?5:i<=29?6:7;
        if(i){u32 ammo=N(HAS_PED_GOT_WEAPON,session.p1,weapon,0)?N(GET_AMMO_IN_PED_WEAPON,session.p1,weapon):120;if(ammo>9999)ammo=9999;
            N(GIVE_WEAPON_TO_PED,session.p2,weapon,ammo,0,0);}
        if(weapon==selected)player_controls.weapon_index=at;
    }
    if(selected&&N(IS_WEAPON_VALID,selected)){
        u32 found=0;for(u32 i=0;i<player_controls.weapon_count;i++)if(player_controls.weapons[i]==selected)found=1;
        if(!found&&player_controls.weapon_count<32){
            u32 ammo=N(GET_AMMO_IN_PED_WEAPON,session.p1,selected);if(ammo>9999)ammo=9999;
            u32 at=player_controls.weapon_count++;player_controls.weapons[at]=selected;
            player_controls.weapon_groups[at]=0;
            N(GIVE_WEAPON_TO_PED,session.p2,selected,ammo,0,0);player_controls.weapon_index=at;
        }
    }
    player_controls.previous_weapon=player_controls.weapon_index?0:(player_controls.weapon_count>1?1:0);
    N(SET_CURRENT_PED_WEAPON,session.p2,player_controls.weapons[player_controls.weapon_index],1);
}
static void controls_init(u32 now) {
    controls_bindings(now);if(player_controls.error)return;
    u64 rot[4]={0};V(GET_GAMEPLAY_CAM_ROT,rot,2);
    session.orbit.yaw=coop_wrap_angle(as_float(rot[2])*0.01745329252f);session.orbit.pitch=0;
    player_controls.private_movement=1;
    N(SET_ENTITY_LOAD_COLLISION_FLAG,session.p2,1,0);
    N(SET_PED_KEEP_TASK,session.p2,1);
    N(SET_PED_CONFIG_FLAG,session.p2,333,1); /* ScriptForceNoTimesliceIntelligenceUpdate */
    N(SET_PED_CONFIG_FLAG,session.p2,32,0); /* WillFlyThroughWindscreen */
    player_controls.last_ms=now;controls_loadout();player_controls.initialized=1;
}
static u32 vehicle_pressed(u32 action){return (session.input.pressed&player_controls.vehicle_bindings[action])!=0;}
static u32 vehicle_down(u32 action){return (session.input.held&player_controls.vehicle_bindings[action])!=0;}
static u32 vehicle_boost_binding(void){
    /* Scramjet separates its jump/horn button from rocket boost. The latter
     * uses X, the same configured face action used for special car actions.
     * Vigilante/Voltic2 retain the horn binding. */
    return (player_controls.vehicle_model==model_hash("scramjet")||player_controls.vehicle_model==model_hash("oppressor2"))?
        player_controls.special_car_binding:player_controls.vehicle_bindings[VB_BOOST];
}
static void vehicle_weapon_select(u32 cycle) {
    u32 current=0;
    N(GET_CURRENT_PED_VEHICLE_WEAPON,session.p2,P(&current));
    player_controls.current_vehicle_weapon=current;
    for(u32 m=0;m<sizeof(coop_vehicle_weapon_models)/sizeof(coop_vehicle_weapon_models[0]);m++){
        if(coop_vehicle_weapon_models[m].model!=player_controls.vehicle_model)continue;
        u32 first=coop_vehicle_weapon_models[m].first,count=coop_vehicle_weapon_models[m].count,start=0;
        for(u32 i=0;i<count;i++)if(coop_vehicle_weapon_hashes[first+i]==current){
            if(!cycle)return;
            start=(i+1)%count;break;
        }
        for(u32 i=0;i<count;i++){
            u32 weapon=coop_vehicle_weapon_hashes[first+(start+i)%count],readback=0;
            if(N(SET_CURRENT_PED_VEHICLE_WEAPON,session.p2,weapon)&&
               N(GET_CURRENT_PED_VEHICLE_WEAPON,session.p2,P(&readback))&&readback==weapon){
                player_controls.current_vehicle_weapon=readback;return;
            }
        }
        return;
    }
}
static void controls_vehicle_abilities(u32 now,u32 vehicle,u32 aircraft) {
    if(player_controls.ability_vehicle!=vehicle){
        player_controls.ability_vehicle=vehicle;player_controls.vehicle_model=N(GET_ENTITY_MODEL,vehicle);
        player_controls.wings_active=player_controls.hover_active=0;vehicle_weapon_select(0);
    }
    player_controls.effective_boost_binding=aircraft?player_controls.aircraft_bindings[AB_BOOST]:vehicle_boost_binding();
    if(!aircraft&&vehicle_pressed(VB_HORN)&&N(GET_CAR_HAS_JUMP,vehicle)){
        if(player_controls.p2_collision&&!N(IS_ENTITY_IN_AIR,vehicle)&&vehicle_request_stock_jump(vehicle))
            player_controls.vehicle_jump_requests++;
        else player_controls.vehicle_jump_rejected++;
    }
    if((session.input.pressed&player_controls.effective_boost_binding)&&
       !(aircraft&&(u32)N(GET_VEHICLE_CLASS,vehicle)==16&&aircraft_defer_action(vehicle,player_controls.effective_boost_binding))&&N(GET_HAS_ROCKET_BOOST,vehicle)){
        player_controls.boost_requests++;
        if(!N(IS_ROCKET_BOOST_ACTIVE,vehicle))N(SET_ROCKET_BOOST_ACTIVE,vehicle,1);
        if(N(IS_ROCKET_BOOST_ACTIVE,vehicle))player_controls.boost_readback++;
        else player_controls.boost_rejected++;
    }
    if(vehicle_pressed(VB_PARACHUTE)&&N(GET_VEHICLE_HAS_PARACHUTE,vehicle)&&N(GET_VEHICLE_CAN_DEPLOY_PARACHUTE,vehicle)){
        N(VEHICLE_START_PARACHUTING,vehicle,1);player_controls.parachute_requests++;
    }
    u32 model=player_controls.vehicle_model;
    if(vehicle_pressed(VB_WINGS)&&model==model_hash("oppressor")){
        player_controls.wings_active=!player_controls.wings_active;
        N(SET_GLIDER_ACTIVE,vehicle,player_controls.wings_active);player_controls.transform_requests++;
    }else if(vehicle_pressed(VB_TRANSFORM)){
        if(model==model_hash("stromberg")||model==model_hash("toreador")){
            if(N(IS_VEHICLE_IN_SUBMARINE_MODE,vehicle))N(TRANSFORM_TO_CAR,vehicle,0);
            else N(TRANSFORM_TO_SUBMARINE,vehicle,0);
            player_controls.transform_requests++;
        }else if(model==model_hash("deluxo")){
            player_controls.hover_active=!player_controls.hover_active;
            N(SET_SPECIAL_FLIGHT_MODE_TARGET_RATIO,vehicle,F(player_controls.hover_active));player_controls.transform_requests++;
        }else if(model==model_hash("blazer5")&&N(GET_HAS_RETRACTABLE_WHEELS,vehicle)){
            if(N(GET_IS_WHEELS_RETRACTED,vehicle))N(SET_WHEELS_EXTENDED_INSTANTLY,vehicle);
            else N(SET_WHEELS_RETRACTED_INSTANTLY,vehicle);
            player_controls.transform_requests++;
        }
    }
    u32 weapon_binding=aircraft?player_controls.aircraft_bindings[AB_WEAPON]:player_controls.vehicle_bindings[VB_WEAPON];
    if(session.input.pressed&weapon_binding){vehicle_weapon_select(1);player_controls.weapon_cycle_requests++;}
    /* Weapon-equipped cars use their native driver's weapon; both configured
     * vehicle attack and the vehicle aim/fire button request it. Unarmed cars
     * never receive mounted-weapon shooting calls. */
    u32 firing=aircraft?(session.input.held&player_controls.aircraft_bindings[AB_FIRE]):(vehicle_down(VB_FIRE)||vehicle_down(VB_AIM));
    /* Mk II's handbrake/R1 controls descent; its mounted attack uses L1. */
    if(model==model_hash("oppressor2"))firing=vehicle_down(VB_AIM);
    if(aircraft&&aircraft_bomb_context(vehicle))firing=0;
    if(firing&&now-player_controls.last_vehicle_shot>=100){
        u32 weapon=0;
        if(N(GET_CURRENT_PED_VEHICLE_WEAPON,session.p2,P(&weapon))&&weapon&&weapon!=model_hash("WEAPON_UNARMED")){
            N(SET_VEHICLE_SHOOT_AT_TARGET,session.p2,0,F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]));
            player_controls.current_vehicle_weapon=weapon;player_controls.vehicle_shots++;player_controls.last_vehicle_shot=now;
        }
    }
}
#ifdef GTA_COOP_TEST
__attribute__((noinline))
#else
__attribute__((section(".coop.extensions"),noinline))
#endif
static void controls_vehicle(u32 now,u32 vehicle) {
    player_controls.vehicle=vehicle;player_controls.pending_vehicle=0;player_controls.enter_pending=0;
    if(action_pressed(B_ENTER)){
        controls_extras_reset();
        if(player_controls.action==5){
            N(CLEAR_PED_TASKS,session.p2);player_controls.action=0;return;
        }
        /* A normal NPC exit task can brake/wait indefinitely. Moving exits
         * request the existing bail animation immediately, including aircraft
         * and passengers; only P2's task is changed. (AI-assisted) */
        u32 bail=as_float(N(GET_ENTITY_SPEED,vehicle))>2.0f;
        N(TASK_LEAVE_VEHICLE,session.p2,vehicle,bail?4160:0);player_controls.exits++;player_controls.bail_requested=bail;
        if(bail)player_controls.bail_requests++;
        player_controls.action=5;player_controls.entry_started=now;player_controls.last_action_ms=now;return;
    }
    if(player_controls.action==5){
        if(action_down(B_ENTER)&&!player_controls.bail_requested&&now-player_controls.entry_started>=500){
            if(as_float(N(GET_ENTITY_SPEED,vehicle))>2.0f){
                N(TASK_LEAVE_VEHICLE,session.p2,vehicle,4160);player_controls.bail_requested=1;
                player_controls.bail_requests++;player_controls.last_action_ms=now;
            }
        }
        /* Leave tasks can fail while a shared car is moving. Retry the owned
         * passenger's exit; never replace the driver's task or freeze the car. */
        if(action_pressed(B_ACCEL)||action_pressed(B_BRAKE)||now-player_controls.entry_started>=7000){
            N(CLEAR_PED_TASKS,session.p2);player_controls.action=0;
        }else{
            if(now-player_controls.last_action_ms>=2000){
                N(TASK_LEAVE_VEHICLE,session.p2,vehicle,player_controls.bail_requested?4160:0);player_controls.last_action_ms=now;
            }
            return;
        }
    }
    if(player_controls.action==12){
        u32 status=N(GET_SCRIPT_TASK_STATUS,session.p2,0x153011fc);
        if(now-player_controls.last_action_ms<4000&&status<7)return;
        player_controls.action=0;
    }
    /* Never steer P1's vehicle while P2 is a passenger. */
    if((u32)N(GET_PED_IN_VEHICLE_SEAT,vehicle,-1,0)!=session.p2){
        controls_extras_reset();
        controls_release_vehicle_streaming();
        /* Front passenger -> empty driver uses the stock seat animation.
         * Occupied drivers, particularly P1, are never displaced. */
        if((session.input.pressed&player_controls.light_binding)&&!N(GET_PED_IN_VEHICLE_SEAT,vehicle,-1,0)){
            i32 passengers=(i32)N(GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS,vehicle);
            if(passengers>0&&passengers<=32){
                for(i32 seat=0;seat<passengers;seat++)if((u32)N(GET_PED_IN_VEHICLE_SEAT,vehicle,seat,0)==session.p2){
                    if(N(CAN_SHUFFLE_SEAT,vehicle,seat)){
                        N(TASK_SHUFFLE_TO_NEXT_VEHICLE_SEAT,session.p2,vehicle,0);
                        player_controls.action=12;player_controls.last_action_ms=now;player_controls.shuffle_requests++;return;
                    }
                    break;
                }
            }
        }
        u32 passenger_action=action_down(B_ATTACK)?1u:action_down(B_AIM)?2u:0u;
        if(player_controls.passenger_shooting!=passenger_action&&player_controls.passenger_shooting){
            N(CLEAR_PED_TASKS,session.p2);player_controls.passenger_shooting=0;
        }
        if(passenger_action&&(!player_controls.passenger_shooting||now-player_controls.last_vehicle_shot>=150)){
          if(passenger_action==1){
            N(TASK_DRIVE_BY,session.p2,0,0,F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]),F(100),70,0,0xc6ee6b4c);
            player_controls.vehicle_shots++;
          }else{
            N(TASK_VEHICLE_AIM_AT_COORD,session.p2,F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]));
          }
          player_controls.last_vehicle_shot=now;player_controls.passenger_shooting=passenger_action;
        }
        return;
    }
    if(player_controls.stream_vehicle!=vehicle){
        controls_release_vehicle_streaming();
        i32 original=entity_load_collision_flag(vehicle);
        if(original>=0){
            player_controls.stream_vehicle=vehicle;player_controls.stream_vehicle_model=N(GET_ENTITY_MODEL,vehicle);
            player_controls.stream_original_flag=(u32)original;player_controls.stream_original_valid=1;
            N(SET_ENTITY_LOAD_COLLISION_FLAG,vehicle,1,0);
        }
    }
    if(player_controls.p2_collision){N(ACTIVATE_PHYSICS,vehicle);player_controls.physics_wakes++;}
    u32 cls=N(GET_VEHICLE_CLASS,vehicle);
    controls_vehicle_abilities(now,vehicle,cls==15||cls==16);
    if(cls==15||cls==16){
        /* Stock animated gear transitions; never force an instant gear pose.
         * JATO shares L3 on some models: defer that ambiguity rather than
         * requesting both gear and boost from the same press. */
        if((session.input.pressed&player_controls.aircraft_bindings[AB_GEAR])&&
           !(cls==16&&aircraft_defer_action(vehicle,player_controls.aircraft_bindings[AB_GEAR]))&&N(GET_VEHICLE_HAS_LANDING_GEAR,vehicle)){
            if(player_controls.aircraft_bindings[AB_GEAR]==player_controls.effective_boost_binding&&N(GET_HAS_ROCKET_BOOST,vehicle))
                player_controls.gear_blocked++;
            else{
                u32 state=N(GET_LANDING_GEAR_STATE,vehicle);
                if(state==0||state==4){
                    N(CONTROL_LANDING_GEAR,vehicle,state==0?1:0);player_controls.gear_requests++;
                }else player_controls.gear_blocked++;
            }
        }
        return;
    }
    if(cls==8)player_motorcycle_tick(vehicle);
    /* The owned Mk II copy consumes analog flight input directly. Ground
     * steering tasks and generic airborne-car torque would fight it. */
    if(oppressor_controls_owns(vehicle))return;
    if(cls>22||cls==14||cls==15||cls==16||cls==21)return;
    controls_vehicle_extras(now,vehicle);
    u32 direct_road=road_runtime_owns(vehicle);
    /* Per-frame analog steering prevents the AI's short drive task from
     * deciding/holding the wheel angle between discrete turn requests. This
     * native's bias resets automatically when calls cease. Positive is left. */
    if(!direct_road){N(SET_VEHICLE_STEER_BIAS,vehicle,F(-session.input.x));player_controls.steer_bias_ticks++;}
    if(player_controls.p2_collision&&(session.input.x||session.input.y)&&
       (N(IS_ENTITY_UPSIDEDOWN,vehicle)||N(IS_ENTITY_IN_AIR,vehicle))){
        /* Local continuous torque supplies the standard left-stick body
         * rotation while airborne/inverted. Never rotate a passenger's car,
         * replace its position, or apply an angular-impulse kick. */
        N(APPLY_FORCE_TO_ENTITY,vehicle,4,F(-session.input.y*6),F(session.input.x*6),F(0),
            F(0),F(0),F(0),0,1,1,1,0,1);
        player_controls.body_rotation_ticks++;
    }
    if(direct_road)return;
    float steer=session.input.x;u32 action=1;
    if(action_down(B_HANDBRAKE)&&!(player_controls.current_vehicle_weapon&&vehicle_down(VB_FIRE)))action=steer<-0.2f?4:steer>0.2f?5:6;
    else if(action_down(B_ACCEL)&&action_down(B_BRAKE)){action=30;player_controls.burnout_ticks++;}
    else if(action_down(B_BRAKE))action=steer<-0.2f?13:steer>0.2f?14:3;
    else if(action_down(B_ACCEL))action=steer<-0.2f?7:steer>0.2f?8:9;
    else if(steer<-0.2f)action=10;else if(steer>0.2f)action=11;
    /* Apply a changed direction immediately, even within the refresh window.
     * Extend identical tasks before expiry without restarting them each tick. */
    if(action==player_controls.vehicle_action&&player_controls.action==6&&now-player_controls.last_action_ms<100)return;
    N(TASK_VEHICLE_TEMP_ACTION,session.p2,vehicle,action,250);
    player_controls.action=6;player_controls.vehicle_action=action;player_controls.drive_ticks++;
    player_controls.last_action_ms=now;
}
static float controls_aircraft_entry_distance(u32 vehicle,const float pos[3]){
    float origin[3];if(!coords(vehicle,origin))return 1000000.0f;
    float best=1000000.0f;
    /* Native supports entry point IDs 0..15. Do not infer a plane/heli door
     * from a car's seat number; ask the installed layout for its positions. */
    for(u32 entry=0;entry<16;entry++){
        u64 raw[4]={0};float point[3];V(GET_ENTRY_POINT_POSITION,raw,vehicle,entry);unpack(point,raw);
        if(!coop_position_valid(point)||(!point[0]&&!point[1]&&!point[2])||coop_distance_squared(point,origin)>10000.0f)continue;
        float d=coop_distance_squared(point,pos);if(d<best)best=d;
    }
    /* Some layouts have no usable entry-point result. Retain the previous
     * bounded centre fallback, without selecting aircraft 80 m away. */
    if(best==1000000.0f){float d=coop_distance_squared(origin,pos);if(d<=900.0f)best=d;}
    return best;
}
__attribute__((noinline)) static u32 controls_enter(u32 now,const float pos[3],u32 passenger_requested) {
    u32 vehicle=N(GET_CLOSEST_VEHICLE,F(pos[0]),F(pos[1]),F(pos[2]),F(7.0),0,127);
    float where[3];float distance=1000000.0f;
    if(vehicle&&N(DOES_ENTITY_EXIST,vehicle)&&coords(vehicle,where))distance=coop_distance_squared(where,pos);
    if(distance>64){vehicle=0;distance=1000000.0f;}
    /* Exact-build filter 0x1c470e0: 0x4000 enables type-1 planes;
     * 0x1000 enables type-8 helicopters. 127 excludes both. */
    static const u32 aircraft_filters[2]={16386,4098},aircraft_classes[2]={16,15};
    for(u32 type=0;type<2;type++){
        u32 aircraft=N(GET_CLOSEST_VEHICLE,F(pos[0]),F(pos[1]),F(pos[2]),F(80.0),0,aircraft_filters[type]);
        if(!aircraft||!N(DOES_ENTITY_EXIST,aircraft)||(u32)N(GET_VEHICLE_CLASS,aircraft)!=aircraft_classes[type])continue;
        float candidate=controls_aircraft_entry_distance(aircraft,pos);
        if(candidate<=900&&candidate<distance){vehicle=aircraft;distance=candidate;}
    }
    if(!vehicle)return 0;
    i32 seat=-1;u32 occupant=N(GET_PED_IN_VEHICLE_SEAT,vehicle,-1,0);
    if(occupant==session.p1||passenger_requested){
        i32 passengers=(i32)N(GET_VEHICLE_MAX_NUMBER_OF_PASSENGERS,vehicle);
        if(passengers<1||passengers>32)return 0;
        for(seat=0;seat<passengers;seat++)if(N(IS_VEHICLE_SEAT_FREE,vehicle,seat,0))break;
        if(seat==passengers)return 0;
    }
    N(TASK_ENTER_VEHICLE,session.p2,vehicle,7000,seat,F(2),seat==-1&&occupant&&occupant!=session.p1?8:1,0);
    if(passenger_requested)player_controls.passenger_entry_requests++;
    player_controls.pending_vehicle=vehicle;player_controls.entry_started=now;player_controls.entries++;
    player_controls.action=4;session.moving=0;return 1;
}
static void controls_melee(u32 now) {
    if(now-player_controls.last_action_ms<300)return;
    u32 target=0;
    N(GET_CLOSEST_PED,F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]),F(2.5),1,0,P(&target),0,0,-1);
    float at[3],pos[3];
    if(target&&target!=session.p1&&target!=session.p2&&N(DOES_ENTITY_EXIST,target)&&!N(IS_ENTITY_DEAD,target,0)&&
       coords(target,at)&&coords(session.p2,pos)&&coop_distance_squared(at,pos)<=9){
        N(TASK_PUT_PED_DIRECTLY_INTO_MELEE,session.p2,target,F(0.1),F(0.6),F(0),16);
        player_controls.melee_ticks++;player_controls.action=7;player_controls.last_action_ms=now;
    }
}
static void player_controls_tick(u32 now,const float pos[3]) {
    if(!player_controls.initialized)controls_init(now);
    controls_bindings(now);
    if(player_controls.error||!session.input.armed||session.paused){controls_stop_task();return;}
    player_controls.p2_collision=N(HAS_COLLISION_LOADED_AROUND_ENTITY,session.p2)!=0;
    player_controls.p2_static=N(IS_ENTITY_STATIC,session.p2)!=0;
    if(player_controls.p2_collision){
        N(SET_PED_RESET_FLAG,session.p2,214,1); /* ForceProcessPhysicsUpdateEachSimStep */
        N(SET_PED_RESET_FLAG,session.p2,325,1); /* ResetMovementStaticCounter */
    }
    if(now-player_controls.last_stream_request>=250){
        /* The entity flag maintains the extra collision loader. Request the
         * local cell as well; do not replace P1's global streaming focus. */
        N(REQUEST_COLLISION_AT_COORD,F(pos[0]),F(pos[1]),F(pos[2]));
        player_controls.collision_requests++;player_controls.last_stream_request=now;
    }
    float dt=(now-player_controls.last_ms)*0.001f;player_controls.last_ms=now;
    if(!action_down(B_WEAPON)){
        if(dt>0.1f)dt=0.1f;
        if(dt>0){session.orbit.yaw=coop_wrap_angle(session.orbit.yaw-coop_axis(session.data[6])*dt*2.2f);
            session.orbit.pitch+=coop_axis(session.data[7])*dt*1.5f;
            if(session.orbit.pitch<-1.1f)session.orbit.pitch=-1.1f;if(session.orbit.pitch>1.1f)session.orbit.pitch=1.1f;}
    }
    float facing[2];coop_orbit_movement(session.orbit.yaw,0,1,facing);
    player_controls.aim[0]=pos[0]+facing[0]*100;player_controls.aim[1]=pos[1]+facing[1]*100;
    player_controls.aim[2]=pos[2]+1.3f-coop_sin(session.orbit.pitch)*100;
    u32 vehicle=N(GET_VEHICLE_PED_IS_IN,session.p2,0);
    if(vehicle&&!N(IS_PED_IN_ANY_VEHICLE,session.p2,0)){
        /* The previous vehicle can survive an exit/ejection in this getter.
         * Actual occupancy must govern routing, not that stale association. */
        if(player_controls.vehicle&&!player_controls.pending_vehicle)controls_stop_task();
        vehicle=0;
    }
    if(vehicle){
        if(player_controls.stream_vehicle&&player_controls.stream_vehicle!=vehicle)controls_release_vehicle_streaming();
        controls_vehicle(now,vehicle);return;
    }
    controls_release_vehicle_streaming();
    controls_extras_reset();
    /* Let the engine finish ledge/ vault tasks; movement tasks would cancel them. */
    if(N(IS_PED_CLIMBING,session.p2)||N(IS_PED_VAULTING,session.p2)){
        player_controls.action=11;return;
    }
    if(player_controls.action==11)player_controls.action=0;
    if(player_controls.action==10){
        if(now-player_controls.last_action_ms<250)return;
        if(!N(IS_PED_FALLING,session.p2)&&!N(IS_PED_JUMPING,session.p2)){
            N(TASK_JUMP,session.p2,1,0,0);session.jump_count++;
        }
        player_controls.action=2;player_controls.last_action_ms=now;return;
    }
    if(player_controls.p2_collision&&player_controls.p2_static){N(ACTIVATE_PHYSICS,session.p2);player_controls.physics_wakes++;}
    player_controls.vehicle=0;
    player_controls.ability_vehicle=0;player_controls.current_vehicle_weapon=0;
    if(player_controls.action==5){
        if(now-player_controls.last_action_ms<500)return;
        player_controls.action=0;player_controls.pending_vehicle=0;
    }
    if(player_controls.pending_vehicle&&action_pressed(B_ENTER)){
        controls_stop_task();player_controls.pending_vehicle=0;return;
    }
    if(action_pressed(B_ENTER)){player_controls.enter_pending=1;player_controls.enter_pressed_at=now;}
    if(player_controls.enter_pending){
        u32 held=now-player_controls.enter_pressed_at>=500;
        if(action_down(B_ENTER)&&!held)return;
        player_controls.enter_pending=0;
        if(controls_enter(now,pos,held))return;
    }
    if(player_controls.pending_vehicle){
        /* Preserve the stock boarding task while the player is still holding
         * movement/sprint from the approach. A fresh Triangle press above
         * explicitly cancels; merely moving the stick must not cancel entry. */
        if(now-player_controls.entry_started<7500)return;
        controls_stop_task();player_controls.pending_vehicle=0;
    }
    if(action_pressed(B_WEAPON))player_controls.wheel_changed=0;
    if(action_down(B_WEAPON)){
        controls_stop_task();
        if((session.input.pressed&0x00a0u)&&player_controls.weapon_count){
            u32 index=player_controls.weapon_index;
            index=(index+((session.input.pressed&0x20)?1u:player_controls.weapon_count-1u))%player_controls.weapon_count;
            player_controls.previous_weapon=player_controls.weapon_index;player_controls.weapon_index=index;
            N(SET_CURRENT_PED_WEAPON,session.p2,player_controls.weapons[index],1);
            player_controls.wheel_changed=1;return;
        }
        float x=coop_axis(session.data[6]),y=-coop_axis(session.data[7]);
        if(x*x+y*y>0.4f&&player_controls.weapon_count){
            /* Silent radial selection: no shared P1 weapon-wheel/HUD calls. */
            u32 sector=coop_abs(x)>coop_abs(y)?(x>0?2:6):(y>0?0:4);
            if(coop_abs(x)>0.35f&&coop_abs(y)>0.35f)sector=y>0?(x>0?1:7):(x>0?3:5);
            for(u32 index=0;index<player_controls.weapon_count;index++)if(player_controls.weapon_groups[index]==sector){
                if(index!=player_controls.weapon_index){player_controls.previous_weapon=player_controls.weapon_index;
                    player_controls.weapon_index=index;N(SET_CURRENT_PED_WEAPON,session.p2,player_controls.weapons[index],1);}
                break;
            }
            player_controls.wheel_changed=1;
        }
        return;
    }
    if((session.input.released&player_controls.bindings[B_WEAPON])&&!player_controls.wheel_changed&&player_controls.weapon_count){
        u32 previous=player_controls.weapon_index;
        player_controls.weapon_index=(previous+1u)%player_controls.weapon_count;
        player_controls.previous_weapon=previous;
        N(SET_CURRENT_PED_WEAPON,session.p2,player_controls.weapons[player_controls.weapon_index],1);
    }
    if(action_pressed(B_STEALTH)&&!(session.input.held&4)){
        player_controls.stealth=!N(GET_PED_STEALTH_MOVEMENT,session.p2);
        N(SET_PED_STEALTH_MOVEMENT,session.p2,player_controls.stealth,P("DEFAULT_ACTION"));
    }
    float direction[2];coop_orbit_movement(session.orbit.yaw,0,1,direction);
    player_controls.aim[0]=pos[0]+direction[0]*40;player_controls.aim[1]=pos[1]+direction[1]*40;
    player_controls.aim[2]=pos[2]+1.3f-coop_sin(session.orbit.pitch)*80;
    if(!coop_position_valid((const float*)player_controls.aim)){player_controls.error=3;controls_stop_task();return;}
    player_assist_aim(now,pos);
    if(action_pressed(B_COVER)){
        if(player_controls.cover)controls_stop_task();
        else{N(TASK_SEEK_COVER_FROM_POS,session.p2,F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]),2000,0);
            player_controls.cover=1;player_controls.cover_ticks++;player_controls.action=8;}
        return;
    }
    u32 weapon=player_controls.weapons[player_controls.weapon_index],unarmed=weapon==model_hash("WEAPON_UNARMED");
    u32 melee=unarmed||weapon==model_hash("WEAPON_KNIFE");
    if(action_pressed(B_RELOAD)&&!melee&&player_controls.weapon_groups[player_controls.weapon_index]!=6){N(TASK_RELOAD_WEAPON,session.p2,1);player_controls.reloads++;
        player_controls.action=3;player_controls.last_action_ms=now;return;}
    if(player_controls.action==3&&now-player_controls.last_action_ms<1300)return;
    if(action_pressed(B_JUMP)&&!N(IS_PED_FALLING,session.p2)&&!N(IS_PED_JUMPING,session.p2)){
        N(TASK_CLIMB,session.p2,1);session.moving=0;
        player_controls.action=10;player_controls.last_action_ms=now;return;
    }
    if(N(IS_PED_FALLING,session.p2)||N(IS_PED_JUMPING,session.p2))return;
    if(player_controls.action==2&&now-player_controls.last_action_ms<350)return;
    if(action_pressed(B_SPRINT)){
        player_controls.sprint_taps=now-player_controls.last_sprint_tap<450?player_controls.sprint_taps+1u:1u;
        player_controls.last_sprint_tap=now;
    }
    float x=session.input.x,y=session.input.y;u32 aim=action_down(B_AIM),fire=action_down(B_ATTACK);
    if(melee&&(fire||action_pressed(B_RELOAD))){
        player_controls.aim[0]=pos[0]+direction[0]*0.9f;player_controls.aim[1]=pos[1]+direction[1]*0.9f;player_controls.aim[2]=pos[2];
        controls_melee(now);return;
    }
    if(player_controls.weapon_groups[player_controls.weapon_index]==6){
        /* Grenades and sticky bombs require a projectile task; the gun task
         * does not throw them. One press starts one throw, leaving its
         * animation uninterrupted instead of resubmitting every frame. */
        if(action_pressed(B_ATTACK)&&now-player_controls.last_action_ms>=800){
            N(TASK_THROW_PROJECTILE,session.p2,F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]),0,0);
            player_controls.shoot_ticks++;player_controls.last_action_ms=now;player_controls.action=9;
        }
        if(player_controls.action==9&&now-player_controls.last_action_ms<800)return;
        fire=0;
    }
    if(player_controls.cover&&!x&&!y&&!aim&&!fire)return;
    if(now-player_controls.last_action_ms<100)return;
    if(!x&&!y&&!aim&&!fire){controls_stop_task();return;}
    player_controls.cover=0;
    /* P2 owns this hidden orientation; moving P1's visible camera must not
     * rotate P2's walking basis. Keep the shared-camera basis as a fallback. */
    float movement_yaw=session.orbit.yaw;
    if(!player_controls.private_movement){
        u64 visible_camera[4]={0};V(GET_GAMEPLAY_CAM_ROT,visible_camera,2);
        float visible_yaw=as_float(visible_camera[2]);
        if(visible_yaw!=visible_yaw||coop_abs(visible_yaw)>36000){controls_stop_task();return;}
        movement_yaw=coop_wrap_angle(visible_yaw*0.01745329252f);
    }
    player_controls.movement_yaw=movement_yaw;
    float movement[2];coop_orbit_movement(movement_yaw,x,y,movement);
    float speed=action_down(B_SPRINT)?2.0f:1.0f;
    if(player_controls.sprint_taps>=2&&now-player_controls.last_sprint_tap<650)speed=3.0f;
    if(aim||fire)speed=1.0f;
    player_controls.move_speed=speed;if(speed>1)player_controls.sprint_ticks++;
    float target[3]={pos[0]+movement[0],pos[1]+movement[1],pos[2]};
    if(aim||fire){
        if(x||y)N(TASK_GO_TO_COORD_WHILE_AIMING_AT_COORD,session.p2,F(target[0]),F(target[1]),F(target[2]),
            F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]),F(speed),fire,F(0.2),F(0.2),0,0,1,0xc6ee6b4c);
        else if(fire)N(TASK_SHOOT_AT_COORD,session.p2,F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]),180,0xc6ee6b4c);
        else N(TASK_AIM_GUN_AT_COORD,session.p2,F(player_controls.aim[0]),F(player_controls.aim[1]),F(player_controls.aim[2]),180,0,0);
        if(aim)player_controls.aim_ticks++;if(fire)player_controls.shoot_ticks++;
    }else{
        float heading=as_float(N(GET_HEADING_FROM_VECTOR_2D,F(movement[0]),F(movement[1])));
        /* Keep the destination beyond the immediate braking/arrival zone.
         * Inputs still refresh at 100 ms and neutral clears the task at once. */
        target[0]=pos[0]+movement[0]*3;target[1]=pos[1]+movement[1]*3;
        N(TASK_GO_STRAIGHT_TO_COORD,session.p2,F(target[0]),F(target[1]),F(target[2]),F(speed),1000,F(heading),F(0.1));
    }
    player_controls.action=1;player_controls.last_action_ms=now;session.last_task=now;
    session.moving=(x||y)!=0;if(session.moving)session.movement_ticks++;
}
