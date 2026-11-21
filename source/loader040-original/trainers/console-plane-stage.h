/* Console-local fixed-address plane copy staging. Exact bytes, typed owner,
 * stopped old configuration and near allocation are required. (AI-assisted) */
#include "console-plane-assets.h"
static int plane_helper_immutable(int fd,uint32_t pid){
 return split_immutable(fd,pid,PLANE_INPUT_ADDRESS,plane_input_image,4096)&&
  split_immutable(fd,pid,PLANE_INPUT_ADDRESS+0x4000,plane_input_image+0x4000,sizeof(plane_input_image)-0x4000);
}
static int existing_plane_runtime(int fd,uint32_t pid,uint64_t owner){
 unsigned char runtime[40],cfg[160];
 if(!process_read(fd,pid,owner+0x8b80,runtime,sizeof(runtime)))return 0;
 /* An optional helper may be pending or unavailable. Only an entirely zero,
  * unowned mailbox is accepted without its helper. (AI-assisted) */
 unsigned nonzero=0;for(unsigned n=0;n<sizeof(runtime);n++)nonzero|=runtime[n];
 if(!nonzero)return 1;
 return split_w(runtime)==1&&
  plane_helper_immutable(fd,pid)&&process_read(fd,pid,PLANE_INPUT_ADDRESS+4096,cfg,sizeof(cfg))&&
  split_q(cfg)==0x3154504e494c5050ull&&split_q(cfg+8)==PLANE_INPUT_ADDRESS+0x4000&&split_q(cfg+16)==owner&&
  split_q(cfg+32)==PLANE_INPUT_ADDRESS+0x10010&&split_q(cfg+40)==0x405ad50ull;
}
static int stage_plane_runtime(int fd,uint32_t pid,uint64_t owner){
 const char *step="stock fingerprint";uint32_t allocation_status=0;uint64_t allocation_address=0;
 struct split_maps maps={0};int okay=0;
 for(unsigned n=0;n<sizeof(plane_input_fingerprints)/sizeof(plane_input_fingerprints[0]);n++){
  const struct gta_fingerprint *f=&plane_input_fingerprints[n];if(!split_immutable(fd,pid,f->address,f->bytes,f->length))goto done;
 }
 step="read mappings";if(!split_getmaps(fd,pid,&maps))goto done;
 int overlap=0;for(unsigned n=0;n<maps.n;n++)if(maps.v[n].start<PLANE_INPUT_ADDRESS+sizeof(plane_input_image)&&maps.v[n].end>PLANE_INPUT_ADDRESS)overlap++;
 if(overlap){
  step="validate inactive existing mapping";
  unsigned char cfg[160];
  if(!split_range(&maps,PLANE_INPUT_ADDRESS,sizeof(plane_input_image),7)||!plane_helper_immutable(fd,pid)||!process_read(fd,pid,PLANE_INPUT_ADDRESS+4096,cfg,sizeof(cfg))||
    split_q(cfg)!=0x3154504e494c5050ull||!split_w(cfg+52)||split_w(cfg+56)||(split_q(cfg+16)&&split_q(cfg+16)!=owner))goto done;
 }else{
  unsigned char request[16]={0};uint32_t length=sizeof(plane_input_image);
  step="fixed near allocation";
  split_putw(request,pid);split_putq(request+4,PLANE_INPUT_ADDRESS);split_putw(request+12,length);
  if(command(fd,0xbdaa000eu,request,16)||recv_all(fd,&allocation_status,4)||recv_all(fd,&allocation_address,8)||allocation_status!=CMD_SUCCESS||allocation_address!=PLANE_INPUT_ADDRESS)goto done;
  step="allocated permissions";free(maps.v);maps.v=0;if(!split_getmaps(fd,pid,&maps)||!split_range(&maps,allocation_address,length,7))goto done;
  step="image write and readback";if(!split_write_image(fd,pid,allocation_address,plane_input_image,length))goto done;
 }
 unsigned char cfg[160]={0},check[160];
 split_putq(cfg,0x3154504e494c5050ull);split_putq(cfg+8,PLANE_INPUT_ADDRESS+0x4000);split_putq(cfg+16,owner);
 split_putq(cfg+32,PLANE_INPUT_ADDRESS+0x10010);split_putq(cfg+40,0x405ad50ull);split_putw(cfg+52,1);
 step="stopped configuration write and readback";
 okay=process_write(fd,pid,PLANE_INPUT_ADDRESS+4096,cfg,sizeof(cfg))&&process_read(fd,pid,PLANE_INPUT_ADDRESS+4096,check,sizeof(check))&&!memcmp(cfg,check,sizeof(cfg));
 if(okay){unsigned char zero[24]={0},check_lease[24];step="pedal lease reset";okay=process_write(fd,pid,PLANE_INPUT_ADDRESS+0x1100,zero,sizeof(zero))&&process_read(fd,pid,PLANE_INPUT_ADDRESS+0x1100,check_lease,sizeof(check_lease))&&!memcmp(zero,check_lease,sizeof(zero));}
 /* Staging never enables the live owner. Publication is a separate step. */
done:
 if(!okay){char message[256];snprintf(message,sizeof(message),"Optional plane stage failed | step=%s status=0x%08x address=0x%llx pid=%u owner=0x%llx; co-op retained",step,allocation_status,(unsigned long long)allocation_address,pid,(unsigned long long)owner);log_status(message);}
 free(maps.v);return okay;
}
