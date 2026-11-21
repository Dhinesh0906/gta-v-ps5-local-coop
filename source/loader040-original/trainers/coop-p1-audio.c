/* P1 listening-position lease. Script-thread native calls only. The original
 * split renderer and its camera remain untouched. Existing foreign listener
 * overrides are preserved; stop/despawn releases only our own flag. */
#include <stdint.h>
#include <stddef.h>
#include "coop-native-handlers.h"
typedef uint64_t U;typedef uint32_t W;
typedef struct{float x,y,z,w;}Vec;
typedef struct{U *result;W count,pad;U *args;W vectors,pad1;float *dest[4];Vec temp[4];}Context;
typedef struct{
 U magic,forward,owner,renderer,camera;
 W state,stop,busy,on,context,last_frame,updates,releases,error,wait_reason,p1,flags_before,count_before;
 W saved[9],last[9],position[3];
}Audio;
_Static_assert(sizeof(Audio)==176,"Audio mailbox ABI");
_Static_assert(offsetof(Audio,state)==40&&offsetof(Audio,saved)==92,"Audio fields");
#define SEC(s) __attribute__((section(s),used,visibility("hidden")))
SEC(".audio.data") volatile Audio audio;
static W word(U a){return *(volatile W*)(uintptr_t)a;}
static U quad(U a){return *(volatile U*)(uintptr_t)a;}
static W *flag(W id){return (W*)(uintptr_t)(0x58098e0ull+0xf534u+id*0xa8u);}
static const U positions[3]={0x5484d60ull,0x5484d40ull,0x5484d50ull};
static U invoke(U h,W count,U *args,U *out){
 U result[4]={0},padded[32]={0};Context c={0};for(W n=0;n<count&&n<32;n++)padded[n]=args[n];
 c.result=out?out:result;c.count=count;c.args=padded;((void(*)(Context*))(uintptr_t)h)(&c);
 if(c.vectors<=4)for(W n=0;n<c.vectors;n++)if(c.dest[n]){c.dest[n][0]=c.temp[n].x;c.dest[n][2]=c.temp[n].y;c.dest[n][4]=c.temp[n].z;}
 return c.result[0];
}
static int ours(void){
 for(W n=0;n<9;n++)if(word(positions[n/3]+(n%3)*4u)!=audio.last[n])return 0;
 return 1;
}
static void microphone(W enabled,const volatile W *values){
 U args[10]={enabled};for(W n=0;n<9;n++)args[n+1]=values[n];
 invoke(0x1a59ba0ull,10,args,0);
}
static int release(void){
 if(!audio.on)return 1;
 W id=((W(*)(void))0xa19f50ull)();if(id!=audio.context){audio.wait_reason=4;return 0;}
 if((*flag(id)&1u)&&ours())microphone(0,audio.saved);
 /* A game script changing the positions has taken over the override. Its
  * state must remain intact, even if that means relinquishing our lease. */
 audio.on=0;audio.releases++;return 1;
}
static void tick(void){
 if(audio.magic!=0x3144554131504f43ull||audio.state==3)return;
 U owner=audio.owner;W p1=word(owner+0x8050);
 int wanted=!audio.stop&&!audio.error&&quad(owner+0x8000)==0x315353504f4f43ull&&word(owner+0x8010)==2&&
  !word(owner+0x8018)&&word(owner+0x8020)==6&&word(owner+0x8208)==3&&!word(owner+0x8218)&&
  word(audio.renderer+80)==1&&!word(audio.renderer+84)&&!word(audio.renderer+88)&&word(audio.camera+24)==3&&p1;
 if(!wanted){if(release())audio.state=audio.stop?3:audio.error?4:1;return;}
 U args[2]={p1,1},raw[4]={0};
 if(!invoke(COOP_H_DOES_ENTITY_EXIST,1,args,0)){audio.wait_reason=1;release();return;}
 W frame=(W)invoke(COOP_H_GET_FRAME_COUNT,0,args,0);if(frame==audio.last_frame)return;audio.last_frame=frame;
 W id=((W(*)(void))0xa19f50ull)();if(id>=24){audio.wait_reason=2;return;}
 if(audio.on&&(id!=audio.context)){audio.wait_reason=4;return;}
 if(audio.on&&(!(*flag(id)&1u)||!ours())){audio.on=0;audio.error=3;audio.state=4;return;}
 if(!audio.on){
  W count=word(0x5819ee0ull),flags=*flag(id);
  if(count||(flags&1u)){audio.wait_reason=3;return;}
  audio.context=id;audio.flags_before=flags;audio.count_before=count;
  for(W n=0;n<9;n++)audio.saved[n]=word(positions[n/3]+(n%3)*4u);
 }
 invoke(COOP_H_GET_ENTITY_COORDS,2,args,raw);
 for(W n=0;n<3;n++){
  W bits=(W)raw[n];union{W i;float f;}v={.i=bits};
  if(v.f!=v.f||v.f>40000.f||v.f< -40000.f){audio.error=2;release();return;}
  audio.position[n]=bits;
 }
 for(W n=0;n<9;n++)audio.last[n]=audio.position[n%3];
 microphone(1,audio.last);audio.on=1;
 if(!(*flag(id)&1u)||!ours()){audio.error=1;release();return;}
 audio.p1=p1;audio.updates++;audio.state=2;audio.wait_reason=0;
}
SEC(".entry") void _start(Context *c){
 if(audio.forward)((void(*)(Context*))(uintptr_t)audio.forward)(c);
 if(!__atomic_exchange_n(&audio.busy,1,__ATOMIC_ACQUIRE)){tick();__atomic_store_n(&audio.busy,0,__ATOMIC_RELEASE);}
}
