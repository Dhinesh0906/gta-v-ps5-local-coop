/* Optional plane allocation runs once per verified owner, after two healthy
 * free-roam observations and 120 advancing game frames. A failure retains the
 * base controller/display; it never triggers allocation retries. (AI-assisted) */
struct plane_startup_gate {
 uint32_t pid,frame,samples,attempted;
 uint64_t owner;
};
static struct plane_startup_gate plane_startup;
static int plane_startup_observe(uint32_t pid,uint64_t owner,uint32_t frame,int healthy){
 if(plane_startup.pid!=pid||plane_startup.owner!=owner)
  plane_startup=(struct plane_startup_gate){.pid=pid,.owner=owner};
 if(plane_startup.attempted)return 0;
 if(!healthy){plane_startup.samples=0;return 0;}
 if(!plane_startup.samples){plane_startup.frame=frame;plane_startup.samples=1;return 0;}
 uint32_t elapsed=frame-plane_startup.frame;
 if(elapsed>3600){plane_startup.frame=frame;plane_startup.samples=1;return 0;}
 if(elapsed<120)return 0;
 plane_startup.samples++;return 1;
}
static int plane_startup_scene(int fd,uint32_t pid,uint64_t slot,uint64_t *owner,uint32_t *frame){
 unsigned char a[40],s[0x320],controls[0x100];uint64_t outer=0;
 if(!process_read(fd,pid,slot,&outer,8)||!existing_split(fd,pid,outer)||
    !process_read(fd,pid,outer+8192,a,sizeof(a)))return 0;
 *owner=split_q(a+16);
 if(!process_read(fd,pid,*owner+0x8000,s,sizeof(s))||
    !process_read(fd,pid,*owner+0x8400,controls,sizeof(controls)))return 0;
 *frame=split_w(s+0x30);
 /* Keep allocation away from loading, actor recovery and vehicle ownership.
  * The owner rechecks current pilot identity before any later publication. */
 return split_w(s+0x10)==2&&!split_w(s+0x14)&&!split_w(s+0x18)&&
  split_w(s+0x20)==6&&split_w(s+0x50)&&split_w(s+0x54)&&
  split_w(s+0x50)!=split_w(s+0x54)&&split_w(s+0x74)&&
  split_w(s+0x208)==3&&!split_w(s+0x218)&&!split_w(controls+0x50);
}
static void maybe_stage_plane_runtime(int fd,uint32_t pid,uint64_t slot){
 uint64_t owner=0;uint32_t frame=0;
 int healthy=plane_startup_scene(fd,pid,slot,&owner,&frame);
 if(!owner){plane_startup.samples=0;return;}
 unsigned char runtime[40];
 if(!process_read(fd,pid,owner+0x8b80,runtime,sizeof(runtime)))return;
 if(split_w(runtime)==1)return;
 unsigned nonzero=0;for(unsigned n=0;n<sizeof(runtime);n++)nonzero|=runtime[n];
 if(nonzero||!plane_startup_observe(pid,owner,frame,healthy))return;
 plane_startup.attempted=1;
 if(!publication_allowed()||!stage_plane_runtime(fd,pid,owner))return;
 uint64_t again=0;uint32_t next_frame=0;
 if(!publication_allowed()||!plane_startup_scene(fd,pid,slot,&again,&next_frame)||again!=owner||
    !process_read(fd,pid,owner+0x8b80,runtime,sizeof(runtime))){
  log_status("Optional plane helper staged stopped; scene changed, co-op retained");return;
 }
 for(unsigned n=0;n<sizeof(runtime);n++)if(runtime[n])return;
 uint32_t enabled=1;
 if(!process_write(fd,pid,owner+0x8b80,&enabled,4)||
    !process_read(fd,pid,owner+0x8b80,runtime,sizeof(runtime))||split_w(runtime)!=1){
  log_status("Optional plane activation readback failed; mapping retained; no retry");return;
 }
 log_status("P2 plane helper ready after stable free roam; base co-op/split retained");
}
