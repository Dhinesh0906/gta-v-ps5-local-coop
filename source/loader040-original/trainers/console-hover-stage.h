/* Called only for a newly allocated, unpublished co-op owner. Transport
 * helpers and exact generated assets are supplied by the resident launcher. */
static int owned_hover_mapping(int fd,uint32_t pid){
    if(command(fd,0xbdaa0004u,&pid,4)||!success(fd))return -1;
    uint32_t count=0;if(recv_all(fd,&count,4)||count>65536)return -1;
    int overlaps=0,covered=0;
    for(uint32_t n=0;n<count;n++){
        unsigned char entry[58];uint64_t start=0,end=0;uint16_t prot=0;
        if(recv_all(fd,entry,sizeof(entry)))return -1;
        memcpy(&start,entry+32,8);memcpy(&end,entry+40,8);memcpy(&prot,entry+56,2);
        if(start<OWNED_HOVER_ADDRESS+65536&&end>OWNED_HOVER_ADDRESS){
            overlaps++;if(start<=OWNED_HOVER_ADDRESS&&end>=OWNED_HOVER_ADDRESS+65536&&(prot&7)==7)covered=1;
        }
    }
    return overlaps?(covered?1:-1):0;
}
static int stage_owned_hover(int fd,uint32_t pid,uint64_t coop){
    unsigned char check[4096];
    for(unsigned n=0;n<sizeof(owned_hover_fingerprints)/sizeof(owned_hover_fingerprints[0]);n++){
        const struct gta_fingerprint *fp=&owned_hover_fingerprints[n];
        for(uint32_t at=0;at<fp->length;at+=sizeof(check)){
            uint32_t length=fp->length-at;if(length>sizeof(check))length=sizeof(check);
            if(!process_read(fd,pid,fp->address+at,check,length)||memcmp(check,fp->bytes+at,length))return 0;
        }
    }
    int mapping=owned_hover_mapping(fd,pid);if(mapping<0)return 0;
    if(mapping){
        for(uint32_t at=0;at<65536;at+=4096){
            if(at==0x1000)continue;
            if(!process_read(fd,pid,OWNED_HOVER_ADDRESS+at,check,4096)||memcmp(check,owned_hover_image+at,4096))return 0;
        }
        unsigned char cfg[128];uint64_t owner=0,magic=0;uint32_t enabled=0;
        if(!process_read(fd,pid,OWNED_HOVER_ADDRESS+0x1000,cfg,sizeof(cfg)))return 0;
        memcpy(&magic,cfg,8);memcpy(&owner,cfg+8,8);memcpy(&enabled,cfg+16,4);
        if(enabled||(owner&&owner!=coop)||(magic&&magic!=0x31544c4658554c44ULL))return 0;
    }else{
        unsigned char request[16]={0};uint32_t length=65536,status=0;uint64_t hint=OWNED_HOVER_ADDRESS,address=0;
        memcpy(request,&pid,4);memcpy(request+4,&hint,8);memcpy(request+12,&length,4);
        /* Hinted allocation returns its address even on CMD_ERROR. Consume
         * both fields so road-only fallback retains protocol alignment. */
        if(command(fd,0xbdaa000eu,request,sizeof(request))||recv_all(fd,&status,4)||recv_all(fd,&address,8))return 0;
        if(status!=CMD_SUCCESS||address!=OWNED_HOVER_ADDRESS||owned_hover_mapping(fd,pid)!=1)return 0;
        for(uint32_t at=0;at<65536;at+=4096){
            if(!process_write(fd,pid,OWNED_HOVER_ADDRESS+at,owned_hover_image+at,4096)||
               !process_read(fd,pid,OWNED_HOVER_ADDRESS+at,check,4096)||memcmp(check,owned_hover_image+at,4096))return 0;
        }
    }
    unsigned char cfg[128]={0},actual[128];uint64_t magic=0x31544c4658554c44ULL;
    memcpy(cfg,&magic,8);memcpy(cfg+8,&coop,8); /* disabled until P2 owns a hovering Deluxo */
    return process_write(fd,pid,OWNED_HOVER_ADDRESS+0x1000,cfg,sizeof(cfg))&&
        process_read(fd,pid,OWNED_HOVER_ADDRESS+0x1000,actual,sizeof(actual))&&!memcmp(cfg,actual,sizeof(cfg));
}
