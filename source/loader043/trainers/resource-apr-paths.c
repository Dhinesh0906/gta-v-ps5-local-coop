/* Resolve file IDs/sizes only. No APR command buffers or resource requests. */
#include "resource-probe.h"
typedef struct { int result;u32 id;u64 size;u32 error_index,padding;} AprPath;
typedef struct {u64 magic,forward;volatile u32 state,visits;u32 error,count;AprPath paths[5];} AprProbe;
__attribute__((section(".probe.data"),used,visibility("hidden"))) volatile AprProbe query;
static const char *const paths[]={"/app0/eboot.bin","/data/gta5-ghost-resources/probe/gr_probe.ptd","grprobe:/gr_probe.ptd","/app0/ghost_rider_probe/gr_probe.ptd","/app0/ghost_rider_probe/gr_probe_body.ptd"};
__attribute__((section(".entry"),used,visibility("hidden"))) void _start(void *context){
 if(query.magic==0x31525041505247ULL&&query.state==1){
  query.state=2;query.visits++;
  for(u32 i=0;i<5;i++){
   const char *path=paths[i];u32 id=0xffffffffu,index=0xffffffffu;u64 size=0;
   int result=((int(*)(const char *const*,u32,u32*,u64*,u32*))0x37bd3d0ULL)(&path,1,&id,&size,&index);
   query.paths[i].result=result;query.paths[i].id=id;query.paths[i].size=size;query.paths[i].error_index=index;query.count++;
  }
  query.state=3;
 }
 if(query.forward)((Forward)query.forward)(context);
}
_Static_assert(sizeof(AprPath)==24,"APR result layout");
_Static_assert(__builtin_offsetof(AprProbe,paths)==32,"APR mailbox layout");
