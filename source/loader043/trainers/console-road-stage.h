/* Private immutable helper per physics class. Each original method pointer
 * stays fixed even if an old car callback returns after a vehicle change. */
static int road_existing_helper(int fd,uint32_t pid,uint64_t owner,uint64_t helper,const unsigned char *image,const unsigned char *original,uint32_t bytes,uint64_t method,uint64_t table_ptr,int mid){
 unsigned char mailbox[128],table[0x690],expected[0x690];if(bytes!=sizeof(table))return 0;
 if(!helper||!split_immutable(fd,pid,helper,image,8192)||!process_read(fd,pid,helper+8192,mailbox,sizeof(mailbox))||
  split_q(mailbox)!=0x3154504e49444152ull||split_q(mailbox+8)!=method||split_q(mailbox+16)!=owner||
  split_q(mailbox+32)!=helper+0x4010||split_q(mailbox+40)!=table_ptr)return 0;
 memcpy(expected,original,bytes);split_putq(expected+16+0x3c0,helper);if(mid)split_putq(expected+16+0x670,helper+ROAD_MID_ENTRY);
 return process_read(fd,pid,helper+0x4000,table,bytes)&&!memcmp(table,expected,bytes);
}
static int existing_road(int fd,uint32_t pid,uint64_t owner){
 unsigned char runtime[64];if(!process_read(fd,pid,owner+ROAD_RUNTIME_OFFSET,runtime,sizeof(runtime))||split_w(runtime+16)!=1)return 0;
 if(!road_existing_helper(fd,pid,owner,split_q(runtime),road_image,road_original_table,sizeof(road_original_table),0x263c900ull,0x4056e28ull,1))return 0;
#ifdef ROAD_BIKE_INPUT
 if(split_w(runtime+52)>1||!road_existing_helper(fd,pid,owner,split_q(runtime+56),bike_image,bike_original_table,sizeof(bike_original_table),0x26c8520ull,0x4057b48ull,0))return 0;
#endif
 return 1;
}
static uint64_t stage_road_helper(int fd,uint32_t pid,uint64_t owner,const unsigned char *source,const unsigned char *original,uint32_t bytes,uint64_t method,uint64_t table_ptr,int mid,struct split_maps *maps){
 uint64_t helper=split_allocate(fd,pid,32768,maps);if(!helper||!split_range(maps,helper,32768,7))return 0;
 unsigned char *image=malloc(32768);if(!image)return 0;memcpy(image,source,32768);memcpy(image+0x4000,original,bytes);
 split_putq(image+0x4010+0x3c0,helper);if(mid)split_putq(image+0x4010+0x670,helper+ROAD_MID_ENTRY);
 unsigned char *m=image+8192;split_putq(m,0x3154504e49444152ull);split_putq(m+8,method);split_putq(m+16,owner);split_putq(m+32,helper+0x4010);split_putq(m+40,table_ptr);
 int okay=split_write_image(fd,pid,helper,image,32768);free(image);return okay?helper:0;
}
static int stage_road_runtime(int fd,uint32_t pid,uint64_t owner,unsigned char *owner_image){
 for(unsigned n=0;n<sizeof(road_fingerprints)/sizeof(road_fingerprints[0]);n++){
  const struct gta_fingerprint *f=&road_fingerprints[n];if(!split_immutable(fd,pid,f->address,f->bytes,f->length))return 0;
 }
 if(!split_immutable(fd,pid,0x4056e18ull,road_original_table,sizeof(road_original_table)))return 0;
#ifdef ROAD_BIKE_INPUT
 for(unsigned n=0;n<sizeof(bike_fingerprints)/sizeof(bike_fingerprints[0]);n++){
  const struct gta_fingerprint *f=&bike_fingerprints[n];if(!split_immutable(fd,pid,f->address,f->bytes,f->length))return 0;
 }
 if(!split_immutable(fd,pid,0x4057b38ull,bike_original_table,sizeof(bike_original_table)))return 0;
#endif
 struct split_maps maps={0};if(!split_getmaps(fd,pid,&maps))return 0;
 uint64_t helper=stage_road_helper(fd,pid,owner,road_image,road_original_table,sizeof(road_original_table),0x263c900ull,0x4056e28ull,1,&maps),bike=0;
#ifdef ROAD_BIKE_INPUT
 if(helper)bike=stage_road_helper(fd,pid,owner,bike_image,bike_original_table,sizeof(bike_original_table),0x26c8520ull,0x4057b48ull,0,&maps);
 int okay=helper&&bike;
#else
 int okay=helper!=0;
#endif
 if(okay){split_putq(owner_image+ROAD_RUNTIME_OFFSET,helper);split_putq(owner_image+ROAD_RUNTIME_OFFSET+56,bike);split_putw(owner_image+ROAD_RUNTIME_OFFSET+16,1);}
 free(maps.v);return okay;
}
