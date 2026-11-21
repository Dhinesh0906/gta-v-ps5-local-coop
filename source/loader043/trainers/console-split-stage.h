/* Console-local staging for the confirmed split/audio package. Uses only
 * debugger loopback reads/writes; no Ethernet, Mac service or kernel writes.
 * Output allocation, GPU retirement and listener cleanup remain game-owned. */
#include <time.h>
#include "console-split-assets.h"
struct split_map{char name[32];uint64_t start,end,offset;uint16_t prot;}__attribute__((packed));
struct split_maps{struct split_map *v;uint32_t n;};
struct split_bindings{uint64_t reg,unreg,status,start,end;};
static uint64_t split_q(const void *p){uint64_t v;memcpy(&v,p,8);return v;}
static uint32_t split_w(const void *p){uint32_t v;memcpy(&v,p,4);return v;}
static void split_putq(void *p,uint64_t v){memcpy(p,&v,8);}
static void split_putw(void *p,uint32_t v){memcpy(p,&v,4);}
static int split_range(const struct split_maps *maps,uint64_t a,uint64_t bytes,unsigned prot){
 if(!a||a>UINT64_MAX-bytes)return 0;
 for(uint32_t n=0;n<maps->n;n++)if((maps->v[n].prot&prot)==prot&&a>=maps->v[n].start&&a+bytes<=maps->v[n].end)return 1;
 return 0;
}
static int split_getmaps(int fd,uint32_t pid,struct split_maps *maps){
 if(command(fd,0xbdaa0004u,&pid,4)||!success(fd)||recv_all(fd,&maps->n,4)||!maps->n||maps->n>65536)return 0;
 maps->v=malloc((size_t)maps->n*sizeof(*maps->v));if(!maps->v)return 0;
 if(recv_all(fd,maps->v,(size_t)maps->n*sizeof(*maps->v))){free(maps->v);maps->v=0;return 0;}return 1;
}
static int split_kread(int fd,uint64_t a,void *p,uint32_t size){
 if(a<0xffff800000000000ull||a>UINT64_MAX-size||!size||size>0x100000)return 0;
 unsigned char arg[12];split_putq(arg,a);split_putw(arg+8,size);
 return !command(fd,0xbdcc0002u,arg,12)&&success(fd)&&!recv_all(fd,p,size);
}
static int split_kq(int fd,uint64_t a,uint64_t *p){return split_kread(fd,a,p,8);}
static int split_platform(int fd){
 uint16_t platform=0,firmware=0;uint32_t count=0;char name[513]={0};
 return !command(fd,0xbd000502u,0,0)&&!recv_all(fd,&platform,2)&&platform==5&&
  /* 13.42 and 13.60 share the ALLPROC/module layout in our SDK. Keep
   * other firmware closed; resolve and verify exports before publication. */
  !command(fd,0xbd000500u,0,0)&&!recv_all(fd,&firmware,2)&&(firmware==1342||firmware==1360)&&
  !command(fd,0xbd000501u,0,0)&&!recv_all(fd,&count,4)&&count&&count<=512&&
  !recv_all(fd,name,count)&&strstr(name,"v1.3.2");
}
static int split_resolve(int fd,uint32_t pid,const struct split_maps *maps,struct split_bindings *b){
 uint64_t imported=0,kbase=0,proc=0,found=0,node=0;
 if(!split_platform(fd)||!process_read(fd,pid,0x40dcf68ull,&imported,8))return 0;
 for(uint32_t n=0;n<maps->n;n++)if(!strncmp(maps->v[n].name,"libSceVideoOut.sprx",32)&&(maps->v[n].prot&5)==5&&imported>=maps->v[n].start&&imported<maps->v[n].end){b->start=maps->v[n].start;b->end=maps->v[n].end;break;}
 if(!b->start||command(fd,0xbdcc0001u,0,0)||!success(fd)||recv_all(fd,&kbase,8)||kbase>UINT64_MAX-0x28c9e88ull||!split_kq(fd,kbase+0x28c9e80ull,&proc))return 0;
 for(unsigned n=0;n<2048&&proc;n++){
  unsigned char record[0xc0];if(!split_kread(fd,proc,record,sizeof(record)))return 0;
  if(split_w(record+0xbc)==pid){found=proc;break;}uint64_t next=split_q(record);if(next==proc)return 0;proc=next;
 }
 if(!found||!split_kq(fd,found+0x3e8,&node)||!split_kq(fd,node,&node))return 0;
 for(unsigned n=0;n<512&&node;n++){
  unsigned char module[0x180];if(!split_kread(fd,node,module,sizeof(module)))return 0;
  if(split_q(module+0x30)==b->start){
   unsigned char dispatch[0x120];if(!split_kread(fd,split_q(module+0x148),dispatch,sizeof(dispatch)))return 0;
   uint64_t table=split_q(dispatch+0x28),bytes=split_q(dispatch+0x30),strings=split_q(dispatch+0x38);
   if(!bytes||bytes%24||bytes>0x100000)return 0;
   unsigned char *symbols=malloc((size_t)bytes);if(!symbols)return 0;
   if(!split_kread(fd,table,symbols,(uint32_t)bytes)){free(symbols);return 0;}
   uint32_t max=0;for(uint32_t at=0;at<bytes;at+=24)if(split_w(symbols+at)>max)max=split_w(symbols+at);
   if(max>0x80000){free(symbols);return 0;}unsigned char *text=malloc(max+128u);if(!text){free(symbols);return 0;}
   if(!split_kread(fd,strings,text,max+128u)){free(text);free(symbols);return 0;}
   const char *names[3]={"rKBUtgRrtbk","N5KDtkIjjJ4","SbU3dwp80lQ"};uint64_t values[3]={0};unsigned hits[3]={0};
   for(uint32_t at=0;at<bytes;at+=24)if((symbols[at+4]&15)==2&&(symbols[at+6]||symbols[at+7])){
    for(unsigned k=0;k<3;k++)if(!memcmp(text+split_w(symbols+at),names[k],11)){hits[k]++;values[k]=b->start+split_q(symbols+at+8);}
   }
   free(text);free(symbols);
   for(unsigned k=0;k<3;k++)if(hits[k]!=1||values[k]<b->start||values[k]>=b->end)return 0;
   b->reg=values[0];b->unreg=values[1];b->status=values[2];return b->reg==imported;
  }
  uint64_t next=split_q(module);if(next==node)return 0;node=next;
 }
 return 0;
}
static int split_immutable(int fd,uint32_t pid,uint64_t base,const unsigned char *image,uint32_t bytes){
 unsigned char block[4096];for(uint32_t at=0;at<bytes;at+=sizeof(block)){
  uint32_t count=bytes-at;if(count>sizeof(block))count=sizeof(block);
  if(!process_read(fd,pid,base+at,block,count)||memcmp(block,image+at,count))return 0;
 }return 1;
}
#include "console-camera-image.h"
static int split_replace(int fd,uint32_t pid,uint64_t at,uint64_t before,uint64_t after){
 uint64_t value=0;return process_read(fd,pid,at,&value,8)&&value==before&&process_write(fd,pid,at,&after,8)&&process_read(fd,pid,at,&value,8)&&value==after;
}
static void split_log_fps(int fd,uint32_t pid,uint64_t base,uint64_t camera){
 static uint32_t previous_pid=0,previous_count[2];static uint64_t previous_base=0;static double previous_time=0;
 unsigned char b[720],c[72];if(!process_read(fd,pid,base+SPLIT_MAILBOX,b,sizeof(b))||!process_read(fd,pid,camera+8192,c,sizeof(c)))return;
 if(split_w(b+SPLIT_OFF_state)!=1||split_w(b+SPLIT_OFF_error)||split_w(c+24)!=3||split_w(c+28)){previous_time=0;return;}
 struct timespec time;if(clock_gettime(CLOCK_MONOTONIC,&time))return;double now=(double)time.tv_sec+time.tv_nsec/1e9;
 if(previous_pid==pid&&previous_base==base&&previous_time&&now-previous_time<4)return;
 uint32_t count[2]={split_w(b+SPLIT_OFF_displayedUpdates),split_w(b+SPLIT_OFF_displayedUpdates+4)};
 if(previous_pid==pid&&previous_base==base&&previous_time&&now>previous_time&&now-previous_time<15&&count[0]>=previous_count[0]&&count[1]>=previous_count[1]){
  char message[256];snprintf(message,sizeof(message),"split_fps pid=%u seconds=%.3f p1=%.3f p2=%.3f pending=%u held=%u waits=%u",pid,now-previous_time,(count[0]-previous_count[0])/(now-previous_time),(count[1]-previous_count[1])/(now-previous_time),split_w(b+SPLIT_OFF_pendingCount),split_w(b+SPLIT_OFF_heldFrames),split_w(b+SPLIT_OFF_waits));log_status(message);
 }
 previous_pid=pid;previous_base=base;previous_time=now;previous_count[0]=count[0];previous_count[1]=count[1];
}
static int existing_split(int fd,uint32_t pid,uint64_t current){
 unsigned char a[40],s[80];if(!current||!process_read(fd,pid,current+8192,a,sizeof(a))||split_q(a)!=0x3144554131504f43ull)return 0;
 uint64_t entry=split_q(a+8),owner=split_q(a+16);if(entry<SPLIT_SCRIPT_ENTRY)return 0;uint64_t base=entry-SPLIT_SCRIPT_ENTRY;
 if(!existing_coop(fd,pid,owner)||!split_immutable(fd,pid,current,split_audio_image,8192)||!split_immutable(fd,pid,base,split_image,SPLIT_CODE_END)||!process_read(fd,pid,base+SPLIT_MAILBOX,s,sizeof(s))||split_q(s)!=0x3154504d4f435053ull||split_q(s+16)!=owner)return 0;
 uint64_t camera=split_q(s+8),value=0,device=0;
 if(split_q(a+24)!=base+SPLIT_MAILBOX||split_q(a+32)!=camera+8192||split_q(s+24)!=camera+8192||!split_camera_immutable(fd,pid,camera)||
  !process_read(fd,pid,0x40df088ull,&value,8)||value!=base+SPLIT_FLIP_ENTRY||!process_read(fd,pid,0x61fc7c0ull,&device,8)||!device||!process_read(fd,pid,device,&value,8)||value!=base+SPLIT_SHADOW+16)return 0;
 split_log_fps(fd,pid,base,camera);return 1;
}
static uint64_t split_allocate(int fd,uint32_t pid,uint32_t size,struct split_maps *maps){
 unsigned char request[16]={0};uint64_t base=0;split_putw(request,pid);split_putw(request+12,size);
 if(command(fd,0xbdaa000eu,request,16)||!success(fd)||recv_all(fd,&base,8)||!base||(base&63))return 0;
 free(maps->v);maps->v=0;if(!split_getmaps(fd,pid,maps)||!split_range(maps,base,size,3))return 0;return base;
}
static int split_write_image(int fd,uint32_t pid,uint64_t base,const unsigned char *image,uint32_t bytes){
 for(uint32_t n=0;n<bytes;n+=4096)if(!process_write(fd,pid,base+n,image+n,4096))return 0;
 return split_immutable(fd,pid,base,image,bytes);
}
/* GET_RENDERING_CAM's current pointer can remain populated after a mission.
 * Its native handler also requires membership in the script-camera manager's
 * active list (d949c0, manager+2f8/+300). Match that check instead of treating
 * every retained camera-pool object as a live mission camera. */
static int split_primary_camera_busy(int fd,uint32_t pid){
 uint64_t camera=0,manager=0,list=0;uint16_t count=0;
 if(!process_read(fd,pid,0x4402570ull,&camera,8))return 1;
 if(!camera)return 0;
 if(!process_read(fd,pid,0x43fc6b8ull,&manager,8)||!manager||
    !process_read(fd,pid,manager+0x2f8,&list,8)||
    !process_read(fd,pid,manager+0x300,&count,2)||count>256)return 1;
 if(!count)return 0;if(!list)return 1;
 uint64_t active[256];if(!process_read(fd,pid,list,active,(uint32_t)count*8))return 1;
 for(uint16_t n=0;n<count;n++)if(active[n]==camera)return 1;
 return 0;
}
static int install_split(int fd,uint32_t pid,uint64_t slot){
 static uint32_t allocated_pid=0;uint64_t owner=0;
 if(!process_read(fd,pid,slot,&owner,8))return 0;if(existing_split(fd,pid,owner))return 1;
 if(!existing_coop(fd,pid,owner)||allocated_pid==pid)return 0;
 for(unsigned n=0;n<sizeof(split_fingerprints)/sizeof(split_fingerprints[0]);n++){
  const struct gta_fingerprint *f=&split_fingerprints[n];if(!split_immutable(fd,pid,f->address,f->bytes,f->length))return 0;
 }
 /* GET_RENDERING_CAM (1a6e360) reads this exact-build pointer before
  * converting it to a script-camera handle. The Prologue owns a mission
  * camera here. Wait for gameplay instead of allocating output/camera
  * resources that the camera helper would immediately reject. Existing
  * verified split chains were handled above, so their owned camera is safe. */
 if(split_primary_camera_busy(fd,pid)){
  static uint32_t waiting_pid=0;
  if(waiting_pid!=pid){log_status("Waiting for the primary mission camera before starting split-screen");waiting_pid=pid;}
  return 0;
 }
 struct split_maps maps={0};struct split_bindings b={0};uint64_t device=0,vtable=0,labels=0,flip=0;uint32_t handle=0,dimensions[2]={0};unsigned char table[208],buffers[96],format=0;
 uint64_t base=0,camera=0,audio_base=0,data=0;int okay=0,script_maybe=0;uint32_t count=0,rotation=0;
 const char *stage="video/context validation"; /* Persistent failure detail. (AI-assisted) */
 if(!split_getmaps(fd,pid,&maps)||!split_resolve(fd,pid,&maps,&b)||!process_read(fd,pid,0x61fc7c0ull,&device,8)||!split_range(&maps,device,0x1a0,3)||!process_read(fd,pid,device,&vtable,8)||vtable!=0x40a3c60ull||!process_read(fd,pid,vtable-16,table,208)||split_q(table+128)!=0x3074200ull||!process_read(fd,pid,0x5f46758ull,&labels,8)||!split_range(&maps,labels,256,1)||!process_read(fd,pid,0x5f46780ull,buffers,96)||!process_read(fd,pid,0x62ae2b0ull,dimensions,8)||dimensions[0]!=3840||dimensions[1]!=2160||!process_read(fd,pid,0x62ae2c0ull,&format,1)||format!=24||!process_read(fd,pid,0x5f466f0ull,&handle,4)||!handle||!process_read(fd,pid,0x40df088ull,&flip,8))goto done;
 if(!process_read(fd,pid,0x62ae318ull,&count,4)||count!=3||!process_read(fd,pid,0x62ae218ull,&rotation,4)||rotation!=3)goto done;
 for(unsigned n=0;n<3;n++){
  uint64_t p=split_q(buffers+n*32);if((p&63)||!split_range(&maps,p,33423360u,1)||split_q(buffers+n*32+8)||split_q(buffers+n*32+16)||split_q(buffers+n*32+24))goto done;
  for(unsigned k=0;k<n;k++)if(p==split_q(buffers+k*32))goto done;
 }
 {int found=0;for(unsigned n=0;n<maps.n;n++)if(!strncmp(maps.v[n].name,"libSceAgc.sprx",32)&&(maps.v[n].prot&5)==5&&flip==maps.v[n].start+0x7b60)found=1;if(!found)goto done;}
 allocated_pid=pid;
 stage="renderer allocation";base=split_allocate(fd,pid,sizeof(split_image),&maps);if(!base)goto done;
 stage="camera allocation";camera=split_allocate(fd,pid,sizeof(split_camera_image),&maps);if(!camera)goto done;
 stage="audio allocation";audio_base=split_allocate(fd,pid,sizeof(split_audio_image),&maps);if(!audio_base)goto done;
 stage="frame storage allocation";data=split_allocate(fd,pid,SPLIT_DATA_BYTES,&maps);
 stage="allocated mapping permissions";
 if(!base||!camera||!audio_base||!data||!split_range(&maps,base,sizeof(split_image),7)||!split_range(&maps,camera,sizeof(split_camera_image),7)||!split_range(&maps,audio_base,sizeof(split_audio_image),7))goto done;
 unsigned char *image=malloc(sizeof(split_image));if(!image)goto done;memcpy(image,split_image,sizeof(split_image));
 unsigned char cam[sizeof(split_camera_image)],aud[sizeof(split_audio_image)];memcpy(cam,split_camera_image,sizeof(cam));memcpy(aud,split_audio_image,sizeof(aud));
 unsigned char *c=image+SPLIT_CAPTURE,*s=image+SPLIT_MAILBOX;
 split_putq(c,0x315041434d524643ull);split_putq(c+8,0x3074200ull);split_putw(c+24,1);split_putq(c+240,data);split_putq(c+248,SPLIT_DATA_BYTES);split_putq(c+256,device);split_putq(c+264,labels);for(unsigned n=0;n<3;n++)split_putq(c+272+n*8,split_q(buffers+n*32));
 split_putq(s,0x3154504d4f435053ull);split_putq(s+8,camera);split_putq(s+16,owner);split_putq(s+24,camera+8192);split_putq(s+32,flip);split_putq(s+40,b.reg);split_putq(s+48,b.unreg);split_putq(s+56,b.status);split_putq(s+64,b.start);split_putq(s+72,b.end);split_putw(s+SPLIT_OFF_handle,handle);split_putw(s+SPLIT_OFF_cookie,0x53000000u|(((uint32_t)(base>>14)^pid)&0xffffffu));split_putw(s+SPLIT_OFF_latest,5);split_putw(s+SPLIT_OFF_verifyCopy,0);split_putw(s+SPLIT_OFF_periodUs,17094);
#ifdef SPLIT_FRESH_HISTORY
 split_putw(s+SPLIT_OFF_historyRemaining,UINT32_MAX);
#endif
 memcpy(image+SPLIT_SHADOW,table,208);split_putq(image+SPLIT_SHADOW+128,base);
 split_putq(cam+8192,0x31444c4f484d4143ull);split_putq(cam+8200,owner);split_putq(cam+8208,owner);split_putw(cam+8216,5);
 split_putq(aud+8192,0x3144554131504f43ull);split_putq(aud+8200,base+SPLIT_SCRIPT_ENTRY);split_putq(aud+8208,owner);split_putq(aud+8216,base+SPLIT_MAILBOX);split_putq(aud+8224,camera+8192);split_putw(aud+8232,1);
 stage="renderer image readback";int staged=split_write_image(fd,pid,base,image,sizeof(split_image));
 if(staged){stage="camera image readback";staged=split_write_image(fd,pid,camera,cam,sizeof(cam));}
 if(staged){stage="audio image readback";staged=split_write_image(fd,pid,audio_base,aud,sizeof(aud));}
 free(image);if(!staged)goto done;
 stage="publication lease";
 if(!publication_allowed())goto done;
 stage="graphics hook publication";if(!split_replace(fd,pid,device,vtable,base+SPLIT_SHADOW+16))goto done;
 stage="flip hook publication";if(!split_replace(fd,pid,0x40df088ull,flip,base+SPLIT_FLIP_ENTRY))goto done;
 if(!publication_allowed())goto done;
 stage="Story hook publication";script_maybe=1;if(!split_replace(fd,pid,slot,owner,audio_base))goto done;
 stage="post-publication chain verification";
 okay=existing_split(fd,pid,audio_base);
 if(okay){
  char message[256];snprintf(message,sizeof(message),"Installed local split/audio | pid=%u owner=0x%llx split=0x%llx camera=0x%llx audio=0x%llx data=0x%llx",pid,(unsigned long long)owner,(unsigned long long)base,(unsigned long long)camera,(unsigned long long)audio_base,(unsigned long long)data);log_status(message);
#ifdef GTA_CONSOLE_PS5
  struct{char pad[45];char message[3075];}req={0};snprintf(req.message,sizeof(req.message),"GTA co-op + split-screen + P1 audio ready | double R3");sceKernelSendNotificationRequest(0,&req,sizeof(req),0);
#endif
 }
done:
 if(!okay&&base&&!script_maybe){
  uint64_t value=0;if(process_read(fd,pid,0x40df088ull,&value,8)&&value==base+SPLIT_FLIP_ENTRY)split_replace(fd,pid,0x40df088ull,value,flip);
  if(device&&process_read(fd,pid,device,&value,8)&&value==base+SPLIT_SHADOW+16)split_replace(fd,pid,device,value,vtable);
 }
 if(!okay&&allocated_pid==pid){char message[192];snprintf(message,sizeof(message),"Split staging incomplete at %s; retained mappings and refused repeated allocation until game exit",stage);log_status(message);}
 free(maps.v);return okay;
}
