// Runtime adapters for the recovered PC script. Included after GR/GRV helpers.
#include "ghost-input.h"

typedef struct {
    u32 entity,model,state,started,flags;
    const char *dictionary,*clip;
    float blend_in,blend_out;
} GhostAnimation;
enum { GR_ANIM_IDLE,GR_ANIM_LOADING,GR_ANIM_STARTING,GR_ANIM_PLAYING,GR_ANIM_FAILED,GR_ANIM_FINISHED };
static GhostInput gr_input;

static u32 GR_CODE gr_control(u32 control){
    return GR(IS_CONTROL_PRESSED,0,control)||GR(IS_DISABLED_CONTROL_PRESSED,0,control);
}
static void GR_CODE gr_input_tick(u32 vehicle,u32 blocked){
    u32 raw=0;
    if(gr_control(24)||gr_control(257)||(vehicle&&gr_control(69)))raw|=GR_ATTACK;
    if(gr_control(25)||(vehicle&&gr_control(68)))raw|=GR_AIM;
    ghost_input_update(&gr_input,raw,blocked);
    if(!blocked){
        const u32 controls[]={24,25,257,vehicle?69:24,vehicle?68:25};
        for(u32 i=0;i<sizeof(controls)/sizeof(controls[0]);i++)N(DISABLE_CONTROL_ACTION,0,controls[i],1);
    }
}
static u32 GR_CODE gr_anim_valid(const GhostAnimation *animation){
    return animation->entity&&GR(DOES_ENTITY_EXIST,animation->entity)&&
           GR(GET_ENTITY_MODEL,animation->entity)==animation->model;
}
static void GR_CODE gr_anim_stop(GhostAnimation *animation){
    if(animation->state&&gr_anim_valid(animation))
        GR(STOP_ANIM_TASK,animation->entity,P(animation->dictionary),P(animation->clip),F(1));
    animation->state=GR_ANIM_IDLE;
}
static void GR_CODE gr_anim_start(GhostAnimation *animation,u32 entity,const char *dictionary,
                                 const char *clip,u32 flags,float blend_in,float blend_out){
    gr_anim_stop(animation);
    if(!entity||!GR(DOES_ENTITY_EXIST,entity)){animation->state=GR_ANIM_FAILED;return;}
    animation->entity=entity;animation->model=GR(GET_ENTITY_MODEL,entity);
    animation->dictionary=dictionary;animation->clip=clip;animation->flags=flags;
    animation->blend_in=blend_in;animation->blend_out=blend_out;
    animation->started=now();animation->state=GR_ANIM_LOADING;
    GR(REQUEST_ANIM_DICT,P(dictionary));
}
static void GR_CODE gr_anim_tick(GhostAnimation *animation){
    if(animation->state==GR_ANIM_IDLE||animation->state==GR_ANIM_FAILED||animation->state==GR_ANIM_FINISHED)return;
    if(!gr_anim_valid(animation)){animation->state=GR_ANIM_FAILED;return;}
    if(animation->state==GR_ANIM_LOADING){
        GR(REQUEST_ANIM_DICT,P(animation->dictionary));
        if(GR(HAS_ANIM_DICT_LOADED,P(animation->dictionary))){
            GR(TASK_PLAY_ANIM,animation->entity,P(animation->dictionary),P(animation->clip),
               F(animation->blend_in),F(animation->blend_out),(u64)-1,animation->flags,F(0),0,0,0);
            animation->state=GR_ANIM_STARTING;animation->started=now();
        }else if(now()-animation->started>4000)animation->state=GR_ANIM_FAILED;
    }else if(animation->state==GR_ANIM_STARTING){
        if(GR(IS_ENTITY_PLAYING_ANIM,animation->entity,P(animation->dictionary),P(animation->clip),3))
            animation->state=GR_ANIM_PLAYING;
        else if(now()-animation->started>1500)animation->state=GR_ANIM_FAILED;
    }else if(animation->state==GR_ANIM_PLAYING&&
             !GR(IS_ENTITY_PLAYING_ANIM,animation->entity,P(animation->dictionary),P(animation->clip),3))
        animation->state=GR_ANIM_FINISHED;
}
static float GR_CODE gr_anim_progress(const GhostAnimation *animation){
    if(!gr_anim_valid(animation))return -1;
    if(animation->state==GR_ANIM_FINISHED)return 1;
    if(animation->state!=GR_ANIM_PLAYING)return -1;
    return as_float(GR(GET_ENTITY_ANIM_CURRENT_TIME,animation->entity,P(animation->dictionary),P(animation->clip)));
}
static void GR_CODE gr_anim_speed(const GhostAnimation *animation,float speed){
    if(animation->state==GR_ANIM_PLAYING&&gr_anim_valid(animation))
        GR(SET_ENTITY_ANIM_SPEED,animation->entity,P(animation->dictionary),P(animation->clip),F(speed));
}
static void GR_CODE gr_anim_seek(const GhostAnimation *animation,float progress){
    if(animation->state==GR_ANIM_PLAYING&&gr_anim_valid(animation)&&progress>=0&&progress<=1)
        GR(SET_ENTITY_ANIM_CURRENT_TIME,animation->entity,P(animation->dictionary),P(animation->clip),F(progress));
}
