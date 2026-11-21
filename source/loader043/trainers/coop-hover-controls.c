/* Per-instance Deluxo adapter. Default off until the host has staged and
 * fingerprinted the owned near image. Never changes shared game code, ped
 * type, global player identity, or another driver's table. */
typedef struct {
    u32 enabled,status,error,vehicle;u64 entity;
    u32 installs,restores,rejected,expiry;u64 physics_calls,overrides;
    float roll,pitch;
} HoverControls;
_Static_assert(sizeof(HoverControls)==64,"Owned hover control ABI");
COOP_SECTION(".coop.hover") volatile HoverControls hover_controls;
#ifdef GTA_COOP_TEST
static u32 hover_publish(u32,u32,u64*);
static u32 hover_restore(u32,u64);
static void hover_config(u32,u32);
static void hover_snapshot(void);
#else
static u64 hover_lookup(u32 handle){
    if((i32)handle<0)return 0;u64 pool=*(volatile u64*)0x61a8250ULL;if(!pool)return 0;
    u32 index=handle>>8,count=*(volatile u32*)(pool+24),stride=*(volatile u32*)(pool+28);
    u64 entries=*(volatile u64*)(pool+8),generations=*(volatile u64*)(pool+16);
    if(!entries||!generations||index>=count||stride<16||stride>256||((volatile u8*)generations)[index]!=(u8)handle)return 0;
    return *(volatile u64*)(entries+(u64)index*stride+8);
}
static void hover_config(u32 enabled,u32 expiry){
    *(volatile u32*)0x60001010ULL=0;
    *(volatile u64*)0x60001008ULL=(u64)&session-0x8000ULL;
    *(volatile u32*)0x60001014ULL=expiry;
    __atomic_store_n((u32*)0x60001010ULL,enabled,__ATOMIC_RELEASE);
}
static u32 hover_publish(u32 h,u32 ped,u64 *entity){
    u64 v=hover_lookup(h),p=hover_lookup(ped);
    if(!v||!p||p==hover_lookup(session.p1)||*(volatile u64*)v!=0x4056e28ULL||*(volatile u32*)(v+0xbf8))return 0;
    i32 index=(i32)*(volatile signed char*)(v+0xc71);u32 seats=*(volatile u8*)(v+0xc70);
    if(index<0||(u32)index>=seats||*(volatile u64*)(v+0xc78+(u32)index*8)!=p)return 0;
    if(*(volatile u64*)0x60001000ULL!=0x31544c4658554c44ULL||*(volatile u32*)0x60001010ULL||
       *(volatile u64*)0x6000c3d0ULL!=0x60000800ULL)return 0;
    for(u32 at=0;at<0x690;at+=8){
        if(at==0x3d0)continue;
        if(*(volatile u64*)(0x4056e18ULL+at)!=*(volatile u64*)(0x6000c000ULL+at))return 0;
    }
    u64 before=0x4056e28ULL;
    if(!__atomic_compare_exchange_n((u64*)v,&before,0x6000c010ULL,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST))return 0;
    *entity=v;return 1;
}
static u32 hover_restore(u32 h,u64 expected){
    u64 v=hover_lookup(h);if(!v||v!=expected)return 1;
    u64 before=0x6000c010ULL;
    if(*(volatile u64*)v==0x4056e28ULL)return 1;
    return __atomic_compare_exchange_n((u64*)v,&before,0x4056e28ULL,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);
}
static void hover_snapshot(void){
    hover_controls.physics_calls=*(volatile u64*)0x60001040ULL;
    hover_controls.overrides=*(volatile u64*)0x60001020ULL;
    hover_controls.roll=*(volatile float*)0x60001030ULL;hover_controls.pitch=*(volatile float*)0x60001034ULL;
}
#endif
static u32 hover_controls_release(void){
    if(!hover_controls.vehicle){hover_controls.status=0;return 1;}
    hover_config(0,0);
    if(!hover_restore(hover_controls.vehicle,hover_controls.entity)){
        hover_controls.error=2;hover_controls.status=3;return 0;
    }
    hover_controls.vehicle=0;hover_controls.entity=0;hover_controls.restores++;hover_controls.status=0;return 1;
}
#ifdef GTA_COOP_TEST
__attribute__((noinline))
#else
__attribute__((section(".coop.extensions"),noinline))
#endif
static void hover_controls_tick(void){
    if(!hover_controls.enabled||session.state!=2||session.mode!=6||session.phase!=3||session.stop||session.error||
       session.paused||!session.connected||!session.input.armed||network()||!session.p2||session.p2==session.p1||
       player_controls.vehicle_model!=model_hash("deluxo")||!player_controls.hover_active||!player_controls.p2_collision||
       !N(IS_PED_IN_ANY_VEHICLE,session.p2,0)){
        hover_controls_release();return;
    }
    u32 v=N(GET_VEHICLE_PED_IS_IN,session.p2,0);
    if(!v||v!=player_controls.vehicle||(u32)N(GET_PED_IN_VEHICLE_SEAT,v,-1,0)!=session.p2||
       (u32)N(GET_ENTITY_MODEL,v)!=model_hash("deluxo")){
        hover_controls_release();return;
    }
    if(hover_controls.vehicle!=v){
        if(!hover_controls_release())return;
        hover_config(0,0);u64 entity=0;
        if(!hover_publish(v,session.p2,&entity)){
            hover_controls.error=1;hover_controls.rejected++;hover_controls.status=1;return;
        }
        hover_controls.entity=entity;hover_controls.vehicle=v;hover_controls.installs++;hover_controls.error=0;
    }
    hover_controls.expiry=session.elapsed+2000;
    hover_config(1,hover_controls.expiry);hover_snapshot();hover_controls.status=2;
}
