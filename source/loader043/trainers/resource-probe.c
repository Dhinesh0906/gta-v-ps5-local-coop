// One-shot file-device lookup on the existing Story script thread.
// Command 2 briefly mounts a private alias of /app0 and removes it again.
// Command 3 reads the header of an unchanged PS5 texture outside the game folder.
// Command 5 registers that original texture under a new name, without loading it.
// Command 6 requests and releases the registered texture over successive ticks.
// Command 7 reads a stock loose texture through its existing file device.
// Command 8 tests a private file device that exposes the compressed body at +16.
// No command writes game files or creates game entities.
#include "resource-probe.h"
__attribute__((section(".probe.data"),used,visibility("hidden")))
volatile Probe probe;
static const char paths[3][16]={"common:/","platform:/","/app0/"};
static u64 body_open_bulk(void *self,const char *file,u64 *offset){
    u64 handle=((OpenBulk)((u64*)0x409e3d0ULL)[3])(self,file,offset);
    if(handle!=~0ULL)*offset+=16;
    return handle;
}
__attribute__((section(".entry"),used,visibility("hidden")))
void _start(void *context){
    probe.visits++;
    if(probe.magic==0x31534656505247ULL&&probe.state==1){
        probe.state=2;
        if(*(volatile unsigned char*)0x5abefb8ULL){probe.error=1;}
        else{
            GetDevice get_device=(GetDevice)0x2f4ae80ULL;
            for(u32 i=0;i<3;i++)probe.devices[i]=(u64)get_device(paths[i],1);
            if(probe.command==6){
                void *module=(void*)0x62755e0ULL;
                if(*(u64*)module!=0x40c0ed0ULL)probe.error=13;
                else{
                    u32 slot=((FindSlot)((u64*)0x40c0ed0ULL)[9])(module,"gr_probe");
                    if(slot==0xffffffffu||probe.requested_index!=*(u32*)((char*)module+8)+slot)probe.error=17;
                    else{
                        probe.before_entry=(*(u64**)0x627d398ULL)[probe.requested_index];
                        u64 pointer=((GetPointer)((u64*)0x40c0ed0ULL)[15])(module,slot);
                        if((u32)probe.before_entry!=probe.expected_handle||
                            ((probe.before_entry>>32)&3)||pointer)probe.error=18;
                        else{
                            probe.request_called=1;
                            ((RequestObject)0x3583da0ULL)(probe.requested_index,0x14);
                            probe.phase=1;
                        }
                    }
                }
            }else if(probe.command==2||probe.command==8){
                const int body=probe.command==8;
                const char *alias="grprobe:/",*file=body?"grprobe:/gr_probe.ptd":"grprobe:/eboot.bin";
                const char *root=body?"/data/gta5-ghost-resources/probe":"/app0";
                const char *physical=body?"/data/gta5-ghost-resources/probe/gr_probe.ptd":"/app0/eboot.bin";
                u64 base=body?(u64)get_device(physical,1):probe.devices[2];
                probe.alias_before=(u64)get_device(file,1);
                if(probe.alias_before&&probe.alias_before!=*(u64*)0x4333d30ULL){probe.error=2;}
                else if(!base||*(u64*)base!=(body?0x409e7a0ULL:0x409df70ULL)){probe.error=3;}
                else{
                    probe.relative.vtable=0x409e3d0ULL;
                    if(body){
                        for(u32 i=0;i<59;i++)probe.adapter_vtable[i]=((u64*)0x409e3d0ULL)[i];
                        probe.adapter_vtable[3]=(u64)&body_open_bulk;
                        probe.relative.vtable=(u64)&probe.adapter_vtable;
                    }
                    probe.relative.base=base;
                    for(u32 i=0;i<256;i++){probe.relative.root[i]=root[i];if(!root[i])break;}
                    probe.relative.prefix_length=8;probe.relative.allow_root=1;
                    GetAttributes direct=(GetAttributes)(*(u64**)base)[32];
                    probe.attributes_direct=direct((void*)base,physical);
                    probe.mounted=((MountGlobal)0x2f53840ULL)(alias,(void*)&probe.relative,1);
                    if(probe.mounted){
                        GetAttributes attributes=(GetAttributes)((u64*)0x409e3d0ULL)[32];
                        probe.attributes_alias=attributes((void*)&probe.relative,file);
                        probe.alias_during=(u64)get_device(file,1);
                        if(probe.alias_during!=(u64)&probe.relative)probe.error=4;
                        if(!probe.error&&body){
                            u64 *vtable=(u64*)probe.relative.vtable,flags=0,offset=0;
                            void *device=(void*)&probe.relative;
                            probe.file_device=(u64)device;probe.file_vtable=(u64)vtable;
                            probe.file_attributes=probe.attributes_alias;
                            probe.resource_version=((GetResourceVersion)vtable[39])(device,file,&flags);
                            probe.resource_flags=flags;
                            probe.file_length=((GetLength)vtable[23])(device,file);
                            u64 handle=((OpenBulk)vtable[3])(device,file,&offset);
                            probe.bulk_base=offset;
                            if(handle==~0ULL)probe.error=11;
                            else{
                                probe.read_count=((ReadBulk)vtable[8])(device,handle,offset,(void*)probe.header,16);
                                if(probe.capture_buffer&&probe.capture_capacity>=166036&&
                                    probe.capture_capacity<=0x2c000&&probe.file_length==166052&&offset==16)
                                    probe.capture_count=((ReadBulk)vtable[8])(device,handle,offset,
                                        (void*)probe.capture_buffer,166036);
                                ((CloseBulk)vtable[14])(device,handle);
                                if(probe.read_count!=16||probe.resource_version!=5||offset!=16)probe.error=21;
                            }
                        }
                    }else probe.error=5;
                    ((UnmountDevice)0x2f5bda0ULL)((void*)&probe.relative);
                    ((UnmountPath)0x2f5ae90ULL)(alias);
                    probe.alias_after=(u64)get_device(file,1);
                    if(probe.alias_after!=probe.alias_before)probe.error=6;
                    else if(!probe.error&&(probe.attributes_direct==0xffffffffu||
                            probe.attributes_direct!=probe.attributes_alias))probe.error=7;
                }
            }else if((probe.command>=3&&probe.command<=5)||probe.command==7){
                const char *file=probe.command==7?"platform:/textures/credits.ptd":
                    "/data/gta5-ghost-resources/probe/gr_probe.ptd";
                void *device=get_device(file,1);
                probe.file_device=(u64)device;
                if(!device)probe.error=8;
                else{
                    u64 *vtable=*(u64**)device;
                    probe.file_vtable=(u64)vtable;
                    if((u64)vtable!=(probe.command==7?0x409e3d0ULL:0x409e7a0ULL))probe.error=9;
                    else{
                        probe.file_attributes=((GetAttributes)vtable[32])(device,file);
                        if(probe.file_attributes==0xffffffffu)probe.error=10;
                        else{
                            probe.file_length=((GetLength)vtable[23])(device,file);
                            u64 offset=0;
                            u64 handle=((OpenBulk)vtable[3])(device,file,&offset);
                            probe.bulk_base=offset;
                            if(handle==~0ULL)probe.error=11;
                            else{
                                probe.read_count=((ReadBulk)vtable[8])(device,handle,offset,(void*)probe.header,16);
                                if(probe.capture_buffer&&probe.file_length<=probe.capture_capacity&&
                                    probe.file_length<=166052&&probe.capture_capacity<=0x2c000){
                                    probe.capture_count=((ReadBulk)vtable[8])(device,handle,offset,
                                        (void*)probe.capture_buffer,(u32)probe.file_length);
                                    if(probe.command==7&&offset>=16)
                                        ((ReadBulk)vtable[8])(device,handle,offset-16,(void*)probe.preceding_bytes,16);
                                }
                                ((CloseBulk)vtable[14])(device,handle);
                                if(probe.read_count!=16)probe.error=12;
                            }
                            if(!probe.error&&probe.command==7){
                                u64 flags=0;
                                probe.resource_version=((GetResourceVersion)vtable[39])(device,file,&flags);
                                probe.resource_flags=flags;
                            }
                        }
                    }
                }
                if(!probe.error&&(probe.command==4||probe.command==5)){
                    void *module=(void*)0x62755e0ULL;
                    void *raw=*(void**)0x5cbb030ULL;
                    if(*(u64*)module!=0x40c0ed0ULL||!raw||*(u64*)raw!=0x409da28ULL)probe.error=13;
                    else{
                        u64 *vtable=*(u64**)device,flags=0;
                        probe.resource_version=((GetResourceVersion)vtable[39])(device,file,&flags);
                        probe.resource_flags=flags;
                        probe.module_version=*(u32*)((char*)module+0x2c);
                        probe.raw_before=*(u32*)((char*)raw+0x7b0);
                        probe.pool_before=*(u32*)((char*)module+0x60)&0x3fffffff;
                        FindSlot find=(FindSlot)((u64*)0x40c0ed0ULL)[9];
                        probe.known_slot=find(module,"commonmenu");
                        probe.missing_slot=find(module,"gr_probe");
                        probe.raw_after=*(u32*)((char*)raw+0x7b0);
                        probe.pool_after=*(u32*)((char*)module+0x60)&0x3fffffff;
                        if(probe.resource_version!=probe.module_version||probe.known_slot==0xffffffffu||
                            probe.missing_slot!=0xffffffffu||probe.raw_before!=probe.raw_after||
                            probe.pool_before!=probe.pool_after)probe.error=14;
                        if(!probe.error&&probe.command==5){
                            probe.registered_index=((RegisterRaw)0x3584f20ULL)(file,"gr_probe.ptd",0,0);
                            probe.registered_slot=find(module,"gr_probe");
                            probe.raw_registered=*(u32*)((char*)raw+0x7b0);
                            probe.pool_registered=*(u32*)((char*)module+0x60)&0x3fffffff;
                            if(probe.registered_index==0xffffffffu||probe.registered_slot==0xffffffffu||
                                probe.registered_index!=*(u32*)((char*)module+8)+probe.registered_slot)probe.error=15;
                            else{
                                probe.registered_entry=(*(u64**)0x627d398ULL)[probe.registered_index];
                                if((u32)probe.registered_entry!=probe.raw_before||
                                    probe.raw_registered!=probe.raw_before+1||
                                    probe.pool_registered!=probe.pool_before+1)probe.error=16;
                            }
                        }
                    }
                }
            }
        }
        if(probe.command!=6||probe.error)probe.state=3;
    }
    if(probe.magic==0x31534656505247ULL&&probe.command==6&&probe.state==2&&probe.phase){
        void *module=(void*)0x62755e0ULL;
        u32 slot=probe.requested_index-*(u32*)((char*)module+8);
        u64 entry=(*(u64**)0x627d398ULL)[probe.requested_index];
        u64 pointer=((GetPointer)((u64*)0x40c0ed0ULL)[15])(module,slot);
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
        }else if(probe.phase==2){
            probe.after_entry=entry;probe.after_pointer=pointer;
            if(!((entry>>32)&3)&&!pointer){probe.phase=3;probe.state=3;}
            else if(probe.ticks>=600){probe.error=20;probe.state=3;}
        }
    }
    if(probe.forward)((Forward)probe.forward)(context);
}
