/* Optional local Mk II staging fails closed while retaining base co-op.
 * Activation waits for stable free roam, like the plane helper. (AI-assisted) */
#include "console-oppressor2-assets.h"
static int oppressor_helper_immutable(int fd,uint32_t pid){
 return split_immutable(fd,pid,OPPRESSOR_INPUT_ADDRESS,oppressor_input_image,4096)&&
  split_immutable(fd,pid,OPPRESSOR_INPUT_ADDRESS+8192,oppressor_input_image+8192,sizeof(oppressor_input_image)-8192);
}
static int existing_oppressor_runtime(int fd,uint32_t pid,uint64_t owner){
 unsigned char runtime[64],cfg[88];
 if(!process_read(fd,pid,owner+0x8a40,runtime,sizeof(runtime)))return 0;
 unsigned nonzero=0;for(unsigned n=0;n<sizeof(runtime);n++)nonzero|=runtime[n];
 if(!nonzero)return 1;
 return split_w(runtime+24)==1&&oppressor_helper_immutable(fd,pid)&&process_read(fd,pid,OPPRESSOR_INPUT_ADDRESS+4096,cfg,sizeof(cfg))&&
  split_q(cfg)==0x315450525032504fULL&&split_q(cfg+8)==owner+0x8a40&&split_q(cfg+24)==OPPRESSOR_INPUT_ADDRESS+0xf010;
}
static int stage_oppressor_runtime(int fd,uint32_t pid,uint64_t owner){
 struct split_maps maps={0};int okay=0;unsigned char runtime[64];
 if(!process_read(fd,pid,owner+0x8a40,runtime,sizeof(runtime)))return 0;
 for(unsigned n=0;n<sizeof(runtime);n++)if(runtime[n])return 0;
 for(unsigned n=0;n<sizeof(oppressor_input_fingerprints)/sizeof(oppressor_input_fingerprints[0]);n++){
  const struct gta_fingerprint *f=&oppressor_input_fingerprints[n];if(!split_immutable(fd,pid,f->address,f->bytes,f->length))goto done;
 }
 if(!split_getmaps(fd,pid,&maps))goto done;
 int overlap=0;for(unsigned n=0;n<maps.n;n++)if(maps.v[n].start<OPPRESSOR_INPUT_ADDRESS+sizeof(oppressor_input_image)&&maps.v[n].end>OPPRESSOR_INPUT_ADDRESS)overlap++;
 if(overlap){
  unsigned char cfg[88];
  if(!split_range(&maps,OPPRESSOR_INPUT_ADDRESS,sizeof(oppressor_input_image),7)||!oppressor_helper_immutable(fd,pid)||!process_read(fd,pid,OPPRESSOR_INPUT_ADDRESS+4096,cfg,sizeof(cfg))||
    split_q(cfg)!=0x315450525032504fULL||!split_w(cfg+32)||split_w(cfg+36)||split_q(cfg+16)||(split_q(cfg+8)&&split_q(cfg+8)!=owner+0x8a40))goto done;
 }else{
  unsigned char request[16]={0};uint32_t length=sizeof(oppressor_input_image),status=0;uint64_t address=0;
  split_putw(request,pid);split_putq(request+4,OPPRESSOR_INPUT_ADDRESS);split_putw(request+12,length);
  if(command(fd,0xbdaa000eu,request,16)||recv_all(fd,&status,4)||recv_all(fd,&address,8)||status!=CMD_SUCCESS||address!=OPPRESSOR_INPUT_ADDRESS)goto done;
  free(maps.v);maps.v=0;if(!split_getmaps(fd,pid,&maps)||!split_range(&maps,address,length,7)||!split_write_image(fd,pid,address,oppressor_input_image,length))goto done;
 }
 unsigned char cfg[88]={0},check[88];
 split_putq(cfg,0x315450525032504fULL);split_putq(cfg+8,owner+0x8a40);split_putq(cfg+24,OPPRESSOR_INPUT_ADDRESS+0xf010);split_putw(cfg+32,1);
 okay=process_write(fd,pid,OPPRESSOR_INPUT_ADDRESS+4096,cfg,sizeof(cfg))&&process_read(fd,pid,OPPRESSOR_INPUT_ADDRESS+4096,check,sizeof(check))&&!memcmp(cfg,check,sizeof(cfg));
done:
 if(!okay)log_status("Optional Mk II stage failed; base co-op retained");
 free(maps.v);return okay;
}
static struct plane_startup_gate oppressor_startup;
static void maybe_stage_oppressor_runtime(int fd,uint32_t pid,uint64_t slot){
 uint64_t owner=0;uint32_t frame=0;int healthy=plane_startup_scene(fd,pid,slot,&owner,&frame);
 if(!owner){oppressor_startup.samples=0;return;}
 if(oppressor_startup.pid!=pid||oppressor_startup.owner!=owner)oppressor_startup=(struct plane_startup_gate){.pid=pid,.owner=owner};
 if(oppressor_startup.attempted)return;
 unsigned char runtime[64];if(!process_read(fd,pid,owner+0x8a40,runtime,sizeof(runtime)))return;
 if(split_w(runtime+24)==1)return;
 for(unsigned n=0;n<sizeof(runtime);n++)if(runtime[n])return;
 if(!healthy){oppressor_startup.samples=0;return;}
 if(!oppressor_startup.samples){oppressor_startup.frame=frame;oppressor_startup.samples=1;return;}
 uint32_t elapsed=frame-oppressor_startup.frame;if(elapsed>3600){oppressor_startup.frame=frame;return;}if(elapsed<120)return;
 oppressor_startup.attempted=1;
 if(!publication_allowed()||!stage_oppressor_runtime(fd,pid,owner))return;
 uint64_t again=0;uint32_t next_frame=0;
 if(!publication_allowed()||!plane_startup_scene(fd,pid,slot,&again,&next_frame)||again!=owner||!process_read(fd,pid,owner+0x8a40,runtime,sizeof(runtime)))return;
 for(unsigned n=0;n<sizeof(runtime);n++)if(runtime[n])return;
 uint32_t enabled=1;
 if(!process_write(fd,pid,owner+0x8a58,&enabled,4)||!process_read(fd,pid,owner+0x8a40,runtime,sizeof(runtime))||split_w(runtime+24)!=1){log_status("Optional Mk II activation failed; base co-op retained");return;}
 log_status("P2 Oppressor Mk II helper ready after stable free roam");
}
