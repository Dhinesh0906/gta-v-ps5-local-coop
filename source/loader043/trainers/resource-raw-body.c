/* Native /app0 compressed-body route. No virtual device or callback adapter.
 * Only this newly catalogued test file's resource metadata is overridden.
 * Command20 registers/checks only;21 additionally requests and releases it.
 */
#include "resource-probe.h"
__attribute__((section(".probe.data"),used,visibility("hidden"))) volatile Probe probe;
static const char file[]="/app0/ghost_rider_probe/gr_probe_body.ptd";
static const char name[]="gr_probe_body.ptd",slot_name[]="gr_probe_body";
static const unsigned char expected[16]={0xec,0xbd,0x0b,0x5c,0x53,0x47,0xf6,0x38,0x3e,0x09,0x50,0x43,0x90,0x92,0xb6,0xd2};
static void initialize(void){
#ifdef RESOURCE_RAW_REGISTER_ONLY
 if(probe.command!=20)
#else
 if(probe.command!=20&&probe.command!=21)
#endif
 {probe.error=1;return;}
 void *device=((GetDevice)0x2f4ae80ULL)(file,1),*raw=*(void**)0x5cbb030ULL,*module=(void*)0x62755e0ULL;
 if(!device||*(u64*)device!=0x409df70ULL||!raw||*(u64*)raw!=0x409da28ULL||*(u64*)module!=0x40c0ed0ULL){probe.error=2;return;}
 FindSlot find=(FindSlot)((u64*)0x40c0ed0ULL)[9];
 if(find(module,slot_name)!=0xffffffffu){probe.error=3;return;}
 u64 *vt=*(u64**)device;probe.file_length=((GetLength)vt[23])(device,file);
 if(probe.file_length!=166036){probe.error=4;return;}
 u64 offset=0,handle=((OpenBulk)vt[3])(device,file,&offset);if(handle==~0ULL){probe.error=5;return;}
 probe.bulk_base=offset;probe.read_count=((ReadBulk)vt[8])(device,handle,offset,(void*)probe.header,16);((CloseBulk)vt[14])(device,handle);
 if(offset||probe.read_count!=16){probe.error=6;return;}for(u32 n=0;n<16;n++)if(probe.header[n]!=expected[n]){probe.error=7;return;}
 probe.raw_before=*(u32*)((char*)raw+0x7b0);probe.pool_before=*(u32*)((char*)module+0x60)&0x3fffffff;
 u32 raw_index=((u32(*)(void*,const char*))((u64*)0x409da28ULL)[65])(raw,file);
 probe.expected_handle=raw_index;probe.raw_registered=*(u32*)((char*)raw+0x7b0);
 if(raw_index!=probe.raw_before||probe.raw_registered!=probe.raw_before+1){probe.error=8;return;}
 u64 *info=((u64*(*)(void*,u32))((u64*)0x409da28ULL)[61])(raw,raw_index);
 if(!info||info[0]||info[1]!=166036||!info[2]){probe.error=9;return;}
 // The metadata consumer at2f46bd0 caches once mtime(+16) is nonzero.
 // Preserve its native mtime and path. Change only our new entry's two words.
 probe.before_entry=info[0];probe.after_entry=info[1];probe.worker_offset=(u64)info;
 info[0]=0x8000000288940000ULL;info[1]=0x5000034100020000ULL;
 probe.registered_entry=info[0];probe.resource_flags=info[1];
 probe.registered_index=((RegisterRaw)0x3584f20ULL)(file,name,0,0);
 probe.registered_slot=find(module,slot_name);probe.pool_registered=*(u32*)((char*)module+0x60)&0x3fffffff;
 if(probe.registered_slot==0xffffffffu||probe.registered_index!=*(u32*)((char*)module+8)+probe.registered_slot||probe.pool_registered!=probe.pool_before+1){probe.error=10;return;}
 u64 entry=(*(u64**)0x627d398ULL)[probe.registered_index];probe.loaded_entry=entry;
 if((u32)entry!=raw_index||((entry>>32)&3)||((GetPointer)((u64*)0x40c0ed0ULL)[15])(module,probe.registered_slot)){probe.error=11;return;}
 if(probe.command==20){probe.state=3;return;}
#ifndef RESOURCE_RAW_REGISTER_ONLY
 probe.requested_index=probe.registered_index;probe.request_called=1;probe.phase=1;
 ((RequestObject)0x3583da0ULL)(probe.requested_index,0x14);
#endif
}
__attribute__((section(".entry"),used,visibility("hidden"))) void _start(void *context){
 probe.visits++;
 if(probe.magic==0x31534656505247ULL&&probe.state==1){probe.state=2;if(*(volatile unsigned char*)0x5abefb8ULL)probe.error=12;else initialize();if(probe.error)probe.state=3;}
#ifndef RESOURCE_RAW_REGISTER_ONLY
 if(probe.state==2&&probe.phase){
  void *module=(void*)0x62755e0ULL;u64 entry=(*(u64**)0x627d398ULL)[probe.requested_index],p=((GetPointer)((u64*)0x40c0ed0ULL)[15])(module,probe.registered_slot);probe.ticks++;
  if(probe.phase==1){
   if(((entry>>32)&3)==1&&p){probe.loaded=1;probe.loaded_pointer=p;for(u32 n=0;n<8;n++)probe.dictionary_words[n]=((u64*)p)[n];}
   else if(probe.ticks>=300)probe.error=13;
   if(probe.loaded||probe.error){probe.release_called=1;probe.release_result=((ReleaseObject)0x3579640ULL)(probe.requested_index)&1;probe.phase=2;}
  }else if(!((entry>>32)&3)&&!p){probe.phase=3;probe.state=3;}else if(probe.ticks>=600){probe.error=14;probe.state=3;}
 }
#endif
 if(probe.forward)((Forward)probe.forward)(context);
}
