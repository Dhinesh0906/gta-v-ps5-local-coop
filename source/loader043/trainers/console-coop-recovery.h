/* Retain the last fully verified chain across an in-process Story reload.
 * Rebinding preserves the menu, P2 state, camera/audio leases and secondary
 * pad handle. Starting a second owner here would leak/lose pad ownership. */
struct coop_recovery_anchor{uint32_t pid;uint64_t slot,entry;};
static struct coop_recovery_anchor coop_anchor;
static int recovery_owned_entry(int fd,uint32_t pid,uint64_t entry){
#ifdef GTA_COOP_SPLIT
 return existing_split(fd,pid,entry);
#else
 return existing_coop(fd,pid,entry);
#endif
}
static void remember_coop_chain(int fd,uint32_t pid,uint64_t slot){
 uint64_t entry=0;
 if(process_read(fd,pid,slot,&entry,8)&&recovery_owned_entry(fd,pid,entry))
  coop_anchor=(struct coop_recovery_anchor){pid,slot,entry};
}
/* 1: existing/reconnected, 0: ordinary startup, -1: known retained owner
 * cannot be reattached safely; do not create competing pad/graphics owners. */
static int recover_retained_story(int fd,uint32_t pid,uint64_t slot){
 if(!coop_anchor.entry||coop_anchor.pid!=pid)return 0;
 uint64_t current=0,retired=0;
 if(!process_read(fd,pid,slot,&current,8))return -1;
 if(slot==coop_anchor.slot&&current==coop_anchor.entry)return 0;
 if(current!=GTA_PLAYER_ID_HANDLER)return -1; /* never replace foreign hooks */
 if(slot!=coop_anchor.slot&&(!process_read(fd,pid,coop_anchor.slot,&retired,8)||retired))return -1;
 if(!recovery_owned_entry(fd,pid,coop_anchor.entry)||!publication_allowed())return -1;
 if(!process_read(fd,pid,slot,&current,8)||current!=GTA_PLAYER_ID_HANDLER)return -1;
 if(!process_write(fd,pid,slot,&coop_anchor.entry,8)||!process_read(fd,pid,slot,&current,8)||current!=coop_anchor.entry)return -1;
 coop_anchor.slot=slot;
 log_status("Reconnected retained GTA Story co-op/menu/split chain; secondary controller handle preserved");
 return 1;
}
