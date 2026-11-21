/* Request-driven, read-only snapshots for coop-anything's animcheck format.
 * Both actors are sampled on one script tick. No input or actor writes.
 * Zero request is off; each nonzero request is answered once, not a timer.
 */
typedef struct {
    u32 ped,model,flags,vehicle;
    float position[3],heading,bones[5][3];
} CoopPoseActor;
typedef struct {
    u32 request,completed,sequence,error,game_time,frame,native_ms,reserved;
    CoopPoseActor actors[2];
} CoopPoseProbe;
_Static_assert(sizeof(CoopPoseActor)==92,"Pose actor ABI");
_Static_assert(sizeof(CoopPoseProbe)==216,"Pose probe ABI");
COOP_SECTION(".coop.pose") volatile CoopPoseProbe pose_probe;

#ifdef GTA_COOP_TEST
__attribute__((noinline))
#else
__attribute__((section(".coop.extensions"),noinline))
#endif
static void pose_probe_tick(void){
    u32 request=__atomic_load_n(&pose_probe.request,__ATOMIC_ACQUIRE);
    if(!request||request==pose_probe.completed)return;
    pose_probe.sequence++;pose_probe.error=0;
    if(session.state!=2||session.mode!=6||session.phase!=3||session.stop||network()||session.paused){
        pose_probe.error=1;
    }else{
        u32 started=N(GET_GAME_TIMER);pose_probe.game_time=started;pose_probe.frame=N(GET_FRAME_COUNT);
        /* GTA bone tags, not runtime bone indices. Reference: SHVDN Bone.cs.
         * A missing/collapsed/out-of-range skeleton is reported invalid. */
        static const u32 tags[5]={0x2e28,0xb1c5,0x49d9,0x9d4d,0xdead};
        for(u32 actor=0;actor<2;actor++){
            volatile CoopPoseActor *a=&pose_probe.actors[actor];
            u32 ped=actor?session.p2:session.p1;a->ped=ped;a->flags=0;
            a->model=a->vehicle=0;
            if(!ped||!N(DOES_ENTITY_EXIST,ped))continue;
            float p[3];if(!coords(ped,p))continue;
            a->model=N(GET_ENTITY_MODEL,ped);a->flags=1;
            for(u32 axis=0;axis<3;axis++)a->position[axis]=p[axis];
            a->heading=as_float(N(GET_ENTITY_HEADING,ped));
            if(N(IS_ENTITY_DEAD,ped,0))a->flags|=4;
            if(N(IS_PED_IN_ANY_VEHICLE,ped,0))a->flags|=8;
            if(actor)a->vehicle=player_controls.vehicle;
            u32 valid=1;
            for(u32 bone=0;bone<5;bone++){
                u64 raw[4]={0};float point[3];V(GET_PED_BONE_COORDS,raw,ped,tags[bone],F(0),F(0),F(0));unpack(point,raw);
                if(!coop_position_valid(point)||coop_distance_squared(point,p)>64)valid=0;
                for(u32 axis=0;axis<3;axis++)a->bones[bone][axis]=point[axis];
            }
            float arm_delta=0;
            for(u32 axis=0;axis<3;axis++)arm_delta+=coop_abs(a->bones[1][axis]-a->bones[3][axis]);
            if(arm_delta<0.05f)valid=0; /* invalid tag fallback to ped origin */
            if(valid)a->flags|=2;
        }
        pose_probe.native_ms=(u32)N(GET_GAME_TIMER)-started;
    }
    pose_probe.sequence++;
    __atomic_store_n(&pose_probe.completed,request,__ATOMIC_RELEASE);
}
