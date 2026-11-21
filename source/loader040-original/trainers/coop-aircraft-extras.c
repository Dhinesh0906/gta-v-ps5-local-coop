/* Model-aware plane buttons. Stock animated natives only, owned P2 pilot;
 * no new player slot, shared code edits or forced aircraft pose. */
#include "coop-flight-press.h"
typedef struct {
    u32 enabled,vehicle,error,vtol_mask,bay_mask,last_bind_ms,bindings_valid,last_frame;
    CoopFlightPress vtol,bay;
    u32 vtol_requests,bay_requests,gear_taps,boost_taps,blocked;
} AircraftExtras;
_Static_assert(sizeof(AircraftExtras)==84,"Aircraft extras ABI");
COOP_SECTION(".coop.aircraft") volatile AircraftExtras aircraft_extras;
#ifndef GTA_COOP_TEST
#define AIR_EXT __attribute__((section(".coop.extensions"),noinline))
#else
#define AIR_EXT
#endif
static AIR_EXT void aircraft_buttons_release(void){
    aircraft_extras.vehicle=0;
    aircraft_extras.vtol=(CoopFlightPress){0};aircraft_extras.bay=(CoopFlightPress){0};
}
static AIR_EXT u32 aircraft_bindings(u32 now){
    if(aircraft_extras.bindings_valid&&now-aircraft_extras.last_bind_ms<1000)return 1;
    static const u32 actions[2]={COOP_INPUT_VEH_FLY_VERTICAL_FLIGHT_MODE,COOP_INPUT_VEH_FLY_BOMB_BAY};
    u32 masks[2];
    for(u32 n=0;n<2;n++){
        u64 label=N(GET_CONTROL_INSTRUCTIONAL_BUTTONS_STRING,0,actions[n],1);
#ifndef GTA_COOP_TEST
        if(label!=0x548eb50ULL+(u64)actions[n]*64)return 0;
#endif
        masks[n]=button_from_label((const char*)label);if(!masks[n])return 0;
    }
    if(aircraft_extras.bindings_valid&&(masks[0]!=aircraft_extras.vtol_mask||masks[1]!=aircraft_extras.bay_mask))aircraft_buttons_release();
    aircraft_extras.vtol_mask=masks[0];aircraft_extras.bay_mask=masks[1];
    aircraft_extras.last_bind_ms=now;aircraft_extras.bindings_valid=1;return 1;
}
static AIR_EXT u32 aircraft_defer_action(u32 vehicle,u32 mask){
    if(!aircraft_extras.enabled||!plane_controls.enabled)return 0;
#ifndef GTA_COOP_TEST
    if(!plane_entity(vehicle,session.p2))return 0;
#else
    if((u32)N(GET_VEHICLE_CLASS,vehicle)!=16)return 0;
#endif
    return aircraft_bindings(player_controls.last_ms)&&mask==aircraft_extras.vtol_mask;
}
static AIR_EXT u32 aircraft_vtol_model(u32 h){
    /* Current catalogue candidates. Native setter separately checks exact
     * model/handling capability; an ordinary plane never gets a VTOL call. */
    static const char names[][12]={"hydra","tula","raiju","avenger","avenger2","avenger3","avenger4"};
    for(u32 n=0;n<sizeof(names)/sizeof(names[0]);n++)if(h==model_hash(names[n]))return 1;
    return 0;
}
static AIR_EXT void aircraft_short_press(u32 v){
    if(player_controls.aircraft_bindings[AB_BOOST]==aircraft_extras.vtol_mask&&N(GET_HAS_ROCKET_BOOST,v)){
        player_controls.boost_requests++;aircraft_extras.boost_taps++;
        if(!N(IS_ROCKET_BOOST_ACTIVE,v))N(SET_ROCKET_BOOST_ACTIVE,v,1);
        if(N(IS_ROCKET_BOOST_ACTIVE,v))player_controls.boost_readback++;else player_controls.boost_rejected++;
    }else if(player_controls.aircraft_bindings[AB_GEAR]==aircraft_extras.vtol_mask&&N(GET_VEHICLE_HAS_LANDING_GEAR,v)){
        u32 state=N(GET_LANDING_GEAR_STATE,v);
        if(state==0||state==4){N(CONTROL_LANDING_GEAR,v,state==0?1:0);player_controls.gear_requests++;aircraft_extras.gear_taps++;}
        else{player_controls.gear_blocked++;aircraft_extras.blocked++;}
    }
}
static AIR_EXT void aircraft_buttons_tick(void){
    if(!aircraft_extras.enabled||!plane_controls.enabled||!plane_controls.active||session.state!=2||session.stop||session.error||
       session.phase!=3||session.mode!=6||session.paused||!session.connected||!session.input.armed||network()||
       !session.p2||session.p2==session.p1||!player_controls.p2_collision||player_controls.action==5){aircraft_buttons_release();return;}
    /* Avoid repeated context natives from same-frame Story callbacks.
     * Cheap cancellation above remains immediate. (AI-assisted) */
    if(aircraft_extras.last_frame==session.last_frame)return;
    aircraft_extras.last_frame=session.last_frame;
    u32 v=plane_controls.vehicle;
    if(!v||v!=player_controls.vehicle||!N(IS_PED_IN_ANY_VEHICLE,session.p2,0)||
       (u32)N(GET_VEHICLE_PED_IS_IN,session.p2,0)!=v||(u32)N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)!=session.p2||
       (u32)N(GET_VEHICLE_CLASS,v)!=16){aircraft_buttons_release();return;}
    if(!aircraft_bindings(player_controls.last_ms)){aircraft_extras.error=1;aircraft_buttons_release();return;}
    if(aircraft_extras.vehicle!=v){aircraft_buttons_release();aircraft_extras.vehicle=v;}
    CoopFlightPress press=aircraft_extras.vtol;
    u32 event=flight_press_tick(&press,player_controls.last_ms,(session.input.held&aircraft_extras.vtol_mask)!=0,250);
    aircraft_extras.vtol=press;
    if(event==FLIGHT_PRESS_TAP)aircraft_short_press(v);
    else if(event==FLIGHT_PRESS_HOLD&&aircraft_vtol_model(N(GET_ENTITY_MODEL,v))){
        float ratio=as_float(N(GET_VEHICLE_FLIGHT_NOZZLE_POSITION,v));
        if(ratio==ratio&&(ratio<=0.05f||ratio>=0.95f)&&ratio>=0&&ratio<=1){
            N(SET_VEHICLE_FLIGHT_NOZZLE_POSITION,v,F(ratio>=0.95f?0:1));aircraft_extras.vtol_requests++;
        }else aircraft_extras.blocked++;
    }
    press=aircraft_extras.bay;
    event=flight_press_tick(&press,player_controls.last_ms,(session.input.held&aircraft_extras.bay_mask)!=0,500);
    aircraft_extras.bay=press;
    if(event==FLIGHT_PRESS_HOLD){
        u32 open=N(GET_ARE_BOMB_BAY_DOORS_OPEN,v)!=0;
        if(open)N(CLOSE_BOMB_BAY_DOORS,v);else N(OPEN_BOMB_BAY_DOORS,v);
        if((N(GET_ARE_BOMB_BAY_DOORS_OPEN,v)!=0)!=open)aircraft_extras.bay_requests++;
        else aircraft_extras.blocked++;
    }
    aircraft_extras.error=0;
}
#undef AIR_EXT
