/* Request/release only the already-verified original texture registration.
 * No file writes, mounts, entity creation, model changes or player movement.
 */
#include "resource-probe.h"
__attribute__((section(".probe.data"),used,visibility("hidden"))) volatile Probe probe;
static const char name[]="gr_probe_body",path[]="/app0/ghost_rider_probe/gr_probe_body.ptd";
static void start(void){
 void *module=(void*)0x62755e0ULL,*raw=*(void**)0x5cbb030ULL;
 if(!raw||*(u64*)raw!=0x409da28ULL||*(u64*)module!=0x40c0ed0ULL||probe.command!=21){probe.error=1;return;}
 u32 slot=((FindSlot)((u64*)0x40c0ed0ULL)[9])(module,name);probe.registered_slot=slot;
 if(slot==0xffffffffu||*(u32*)((char*)module+8)+slot!=probe.requested_index||probe.expected_handle>=*(u32*)((char*)raw+0x7b0)){probe.error=2;return;}
 u64 *info=((u64*(*)(void*,u32))((u64*)0x409da28ULL)[61])(raw,probe.expected_handle);
 if(!info||info[0]!=0x8000000288940000ULL||info[1]!=0x5000034100020000ULL||!info[2]||!info[3]){probe.error=3;return;}
 const char *actual=(const char*)info[3];for(u32 n=0;n<sizeof(path);n++)if(actual[n]!=path[n]){probe.error=4;return;}
 u64 entry=(*(u64**)0x627d398ULL)[probe.requested_index];probe.before_entry=entry;
 if(entry!=(0x0800000000000000ULL|probe.expected_handle)||((GetPointer)((u64*)0x40c0ed0ULL)[15])(module,slot)){probe.error=5;return;}
 probe.phase=1;probe.request_called=1;
 ((RequestObject)0x3583da0ULL)(probe.requested_index,0x14);
}
__attribute__((section(".entry"),used,visibility("hidden"))) void _start(void *context){
 probe.visits++;
 if(probe.magic==0x31534656505247ULL&&probe.state==1){probe.state=2;if(*(volatile unsigned char*)0x5abefb8ULL)probe.error=6;else start();if(probe.error)probe.state=3;}
 if(probe.state==2&&probe.phase){
  void *module=(void*)0x62755e0ULL;u64 entry=(*(u64**)0x627d398ULL)[probe.requested_index],p=((GetPointer)((u64*)0x40c0ed0ULL)[15])(module,probe.registered_slot);probe.ticks++;
  if(probe.phase==1){
   if(((entry>>32)&3)==1&&p){probe.loaded=1;probe.loaded_entry=entry;probe.loaded_pointer=p;for(u32 n=0;n<8;n++)probe.dictionary_words[n]=((u64*)p)[n];}
   else if(probe.ticks>=300)probe.error=7;
   if(probe.loaded||probe.error){probe.release_called=1;probe.release_result=((ReleaseObject)0x3579640ULL)(probe.requested_index)&1;probe.phase=2;}
  }else{
   probe.after_entry=entry;probe.after_pointer=p;
   if(!((entry>>32)&3)&&!p){probe.phase=3;probe.state=3;}
   else if(probe.ticks>=600){probe.error=8;probe.state=3;}
  }
 }
 if(probe.forward)((Forward)probe.forward)(context);
}
