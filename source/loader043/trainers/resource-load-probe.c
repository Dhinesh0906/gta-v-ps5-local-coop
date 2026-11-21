// One original PS5 texture; all mounted objects remain alive until GTA exits.
#include "resource-probe.h"
#include "resource-body-layout.h"
__attribute__((section(".probe.data"),used,visibility("hidden")))
volatile Probe probe;
#ifdef RESOURCE_APP0_BODY
static const char alias[]="/app0/ghost_rider_probe/",file[]="/app0/ghost_rider_probe/gr_probe.ptd";
static const char physical[]="/app0/ghost_rider_probe/gr_probe.ptd";
static const char root[]="/app0/ghost_rider_probe";
#else
static const char alias[]="grprobe:/",file[]="grprobe:/gr_probe.ptd";
static const char physical[]="/data/gta5-ghost-resources/probe/gr_probe.ptd";
static const char root[]="/data/gta5-ghost-resources/probe";
#endif

static u64 open_body(void *self,const char *name,u64 *offset){
    u64 handle=((OpenBulk)((u64*)0x409e3d0ULL)[3])(self,name,offset);
    if(handle!=~0ULL)*offset+=16;
    return handle;
}

static u64 length_body(void *self,const char *name){
    u64 full=((GetLength)((u64*)0x409e3d0ULL)[23])(self,name);
    return gr_resource_body_length(full);
}

static u32 read_body(void *self,u64 handle,u64 offset,void *buffer,u32 length){
    u32 count=((ReadBulk)((u64*)0x409e3d0ULL)[8])(self,handle,offset,buffer,length);
    if(probe.request_called&&count>=16&&__sync_bool_compare_and_swap(&probe.worker_seen,0,1)){
        probe.worker_offset=offset;probe.worker_length=length;probe.worker_count=count;
        for(u32 i=0;i<16;i++)probe.worker_bytes[i]=((unsigned char*)buffer)[i];
        __atomic_store_n(&probe.worker_seen,2,__ATOMIC_RELEASE);
        for(u32 i=0;i<100&&!probe.worker_ack;i++)((void (*)(u32))0x37ba740ULL)(1000);
    }
    return count;
}

static void initialize(void){
    GetDevice get=(GetDevice)0x2f4ae80ULL;
    void *module=(void*)0x62755e0ULL,*raw=*(void**)0x5cbb030ULL;
    const char *paths[3]={"common:/","platform:/","/app0/"};
    for(u32 i=0;i<3;i++)probe.devices[i]=(u64)get(paths[i],1);
    if(*(u64*)module!=0x40c0ed0ULL||!raw||*(u64*)raw!=0x409da28ULL){probe.error=13;return;}
    FindSlot find=(FindSlot)((u64*)0x40c0ed0ULL)[9];
    probe.known_slot=find(module,"commonmenu");probe.missing_slot=find(module,"gr_probe");
    if(probe.known_slot==0xffffffffu||probe.missing_slot!=0xffffffffu){probe.error=14;return;}
    probe.alias_before=(u64)get(file,1);
    #ifndef RESOURCE_APP0_BODY
    if(probe.alias_before!=*(u64*)0x4333d30ULL){probe.error=2;return;}
#endif
    void *base=get(physical,1);
    #ifdef RESOURCE_APP0_BODY
    if(!base||*(u64*)base!=0x409df70ULL||probe.alias_before!=(u64)base){probe.error=3;return;}
#else
    if(!base||*(u64*)base!=0x409e7a0ULL){probe.error=3;return;}
#endif
    for(u32 i=0;i<59;i++)probe.adapter_vtable[i]=((u64*)0x409e3d0ULL)[i];
    probe.adapter_vtable[3]=(u64)&open_body;probe.adapter_vtable[8]=(u64)&read_body;
    probe.adapter_vtable[23]=(u64)&length_body;
    probe.relative.vtable=(u64)&probe.adapter_vtable;probe.relative.base=(u64)base;
    for(u32 i=0;i<sizeof(root);i++)probe.relative.root[i]=root[i];
    probe.relative.prefix_length=sizeof(alias)-2;probe.relative.allow_root=1;
    void *device=(void*)&probe.relative;
    probe.mounted=((MountGlobal)0x2f53840ULL)(alias,device,1);
    if(!probe.mounted){probe.error=5;return;}
    probe.alias_during=(u64)get(file,1);
    if(probe.alias_during!=(u64)device){probe.error=4;return;}
    u64 flags=0;
    probe.resource_version=((GetResourceVersion)((u64*)0x409e3d0ULL)[39])(device,file,&flags);
    probe.resource_flags=flags;probe.module_version=*(u32*)((char*)module+0x2c);
    probe.file_length=length_body(device,file);
    if(probe.resource_version!=5||probe.module_version!=5||flags!=0x5000034100020000ULL||
        probe.file_length!=166036){probe.error=21;return;}
#ifdef RESOURCE_BODY_CONTRACT_ONLY
    if(probe.command!=(
#ifdef RESOURCE_BODY_METADATA_ONLY
    11
#else
    10
#endif
    )||!probe.capture_buffer||probe.capture_capacity<166036){probe.error=22;return;}
    probe.raw_before=*(u32*)((char*)raw+0x7b0);
    probe.pool_before=*(u32*)((char*)module+0x60)&0x3fffffff;
    u64 offset=0,handle=open_body(device,file,&offset);
    probe.bulk_base=offset;
    if(handle==~0ULL){probe.error=11;return;}
    probe.capture_count=read_body(device,handle,offset,(void*)probe.capture_buffer,166036);
    ((CloseBulk)((u64*)0x409e3d0ULL)[14])(device,handle);
    if(offset!=16||probe.capture_count!=166036)probe.error=23;
#ifdef RESOURCE_BODY_METADATA_ONLY
    // Raw file catalogue only. No texture/module registration or load request.
    // Its path and device callbacks must remain valid until this GTA exits.
    probe.registered_index=((u32(*)(void*,const char*))((u64*)0x409da28ULL)[65])(raw,file);
    if(probe.registered_index==0xffffffffu){probe.error=25;return;}
    u64 *info=((u64*(*)(void*,u32))((u64*)0x409da28ULL)[61])(raw,probe.registered_index);
    probe.registered_entry=info[0];probe.loaded_entry=info[1];
    probe.alias_after=(u64)get(file,1);
    probe.raw_after=*(u32*)((char*)raw+0x7b0);probe.raw_registered=probe.raw_after;
    probe.pool_after=*(u32*)((char*)module+0x60)&0x3fffffff;
    if(probe.alias_after!=(u64)device||probe.raw_after!=probe.raw_before+1||probe.pool_after!=probe.pool_before||
       probe.registered_entry!=(0x8000000000000000ULL|((u64)166036<<16))||probe.loaded_entry!=flags)probe.error=26;
#else
    ((UnmountDevice)0x2f5bda0ULL)(device);
    ((UnmountPath)0x2f5ae90ULL)(alias);
    probe.alias_after=(u64)get(file,1);probe.mounted=0;
    probe.raw_after=*(u32*)((char*)raw+0x7b0);
    probe.pool_after=*(u32*)((char*)module+0x60)&0x3fffffff;
    if(probe.alias_after!=probe.alias_before||probe.raw_after!=probe.raw_before||probe.pool_after!=probe.pool_before)probe.error=24;
#endif
    probe.state=3;
#else
    probe.raw_before=*(u32*)((char*)raw+0x7b0);
    probe.pool_before=*(u32*)((char*)module+0x60)&0x3fffffff;
    probe.registered_index=((RegisterRaw)0x3584f20ULL)(file,"gr_probe.ptd",0,0);
    probe.registered_slot=find(module,"gr_probe");
    probe.raw_registered=*(u32*)((char*)raw+0x7b0);
    probe.pool_registered=*(u32*)((char*)module+0x60)&0x3fffffff;
    if(probe.registered_slot==0xffffffffu||probe.registered_index!=*(u32*)((char*)module+8)+probe.registered_slot){
        probe.error=15;return;
    }
    probe.registered_entry=(*(u64**)0x627d398ULL)[probe.registered_index];
    if((u32)probe.registered_entry!=probe.raw_before||probe.raw_registered!=probe.raw_before+1||
        probe.pool_registered!=probe.pool_before+1||((probe.registered_entry>>32)&3)||
        ((GetPointer)((u64*)0x40c0ed0ULL)[15])(module,probe.registered_slot)){probe.error=16;return;}
    probe.requested_index=probe.registered_index;probe.expected_handle=probe.raw_before;
    probe.before_entry=probe.registered_entry;probe.request_called=1;probe.phase=1;
    ((RequestObject)0x3583da0ULL)(probe.requested_index,0x14);
#endif
}

__attribute__((section(".entry"),used,visibility("hidden")))
void _start(void *context){
    probe.visits++;
    if(probe.magic==0x31534656505247ULL&&probe.state==1){
        probe.state=2;
        if(*(volatile unsigned char*)0x5abefb8ULL)probe.error=1;
        else initialize();
        if(probe.error){
            // A registration keeps its device alive even if a later step failed.
            if(probe.mounted&&!probe.raw_registered){
                ((UnmountDevice)0x2f5bda0ULL)((void*)&probe.relative);
                ((UnmountPath)0x2f5ae90ULL)(alias);
            }
            probe.state=3;
        }
    }
#ifndef RESOURCE_BODY_CONTRACT_ONLY
    if(probe.magic==0x31534656505247ULL&&probe.state==2&&probe.phase){
        void *module=(void*)0x62755e0ULL;
        u64 entry=(*(u64**)0x627d398ULL)[probe.requested_index];
        u64 pointer=((GetPointer)((u64*)0x40c0ed0ULL)[15])(module,probe.registered_slot);
        probe.ticks++;
        if(probe.phase==1){
            if(((entry>>32)&3)==1&&pointer){
                probe.loaded=1;probe.loaded_entry=entry;probe.loaded_pointer=pointer;
                for(u32 i=0;i<8;i++)probe.dictionary_words[i]=((u64*)pointer)[i];
            }else if(probe.ticks>=300)probe.error=19;
            if(probe.loaded||probe.error){
                probe.release_called=1;
                probe.release_result=((ReleaseObject)0x3579640ULL)(probe.requested_index)&1;
                probe.phase=2;
            }
        }else{
            probe.after_entry=entry;probe.after_pointer=pointer;
            if(!((entry>>32)&3)&&!pointer){probe.phase=3;probe.state=3;}
            else if(probe.ticks>=600){probe.error=20;probe.state=3;}
        }
    }
#endif
    if(probe.forward)((Forward)probe.forward)(context);
}
