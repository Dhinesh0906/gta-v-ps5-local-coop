// PS5 resident launcher for the user's exact PPSA04264 01.011.001 Story Mode
// build. Connects to PS5Debug-NG over loopback and asks the local Payload
// Manager to relaunch it if boot-time injection did not take effect.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "console-assets.h"
#ifdef GTA_COOP_AUTOLOAD
#include "coop-assets.h"
#endif
#ifdef GTA_COOP_OWNED_HOVER
#include "owned-hover-assets.h"
#endif
#ifdef GTA_COOP_ROAD
#include "road-assets.h"
#endif
#ifdef GTA_CONSOLE_PS5
#include "sdk_shim.h"
#endif

#define DEBUG_PORT 744
#define MANAGER_PORT 8084
#define CMD_SUCCESS 0x80000000u
#define CMD_PROC_LIST 0xbdaa0001u
#define CMD_PROC_READ 0xbdaa0002u
#define CMD_PROC_WRITE 0xbdaa0003u
#define CMD_PROC_INFO 0xbdaa000au
#define CMD_PROC_ALLOC 0xbdaa000bu
#define MENU_MAGIC 0x325654474e5350ULL

static int net_socket(void){
#ifdef GTA_CONSOLE_PS5
    return sceNetSocket(0,AF_INET,SOCK_STREAM,0);
#else
    return socket(AF_INET,SOCK_STREAM,0);
#endif
}
static int net_connect(int fd,const struct sockaddr_in *addr){
#ifdef GTA_CONSOLE_PS5
    return sceNetConnect(fd,(void*)addr,sizeof(*addr));
#else
    return connect(fd,(const struct sockaddr*)addr,sizeof(*addr));
#endif
}
static int net_send(int fd,const void *buf,size_t len){
#ifdef GTA_CONSOLE_PS5
    return sceNetSend(fd,buf,len,0);
#else
    return (int)send(fd,buf,len,0);
#endif
}
static int net_recv(int fd,void *buf,size_t len){
#ifdef GTA_CONSOLE_PS5
    return sceNetRecv(fd,buf,len,0);
#else
    return (int)recv(fd,buf,len,0);
#endif
}
static void net_close(int fd){
#ifdef GTA_CONSOLE_PS5
    sceNetSocketClose(fd);
#else
    close(fd);
#endif
}
__attribute__((unused)) static void pause_seconds(unsigned seconds){
#ifdef GTA_CONSOLE_PS5
    sceKernelSleep(seconds);
#else
    sleep(seconds);
#endif
}
static void notify_install(void){
#ifdef GTA_CONSOLE_PS5
    struct {char pad[45];char message[3075];} req={0};
#ifdef GTA_GHOST_AUTOLOAD
    snprintf(req.message,sizeof(req.message),"GTA V Ghost Rider ready | L1 + D-pad Left");
#else
    snprintf(req.message,sizeof(req.message),"GTA V Story menu ready | L1 + D-pad Right");
#endif
    sceKernelSendNotificationRequest(0,&req,sizeof(req),0);
#endif
}
static void log_status(const char *message){
#ifdef GTA_CONSOLE_PS5
#ifdef GTA_GHOST_AUTOLOAD
    FILE *file=fopen("/data/gta5-ghost-autoload.log","a");
#elif defined(GTA_COOP_AUTOLOAD)
    FILE *file=fopen("/data/gta5-coop-autoload.log","a");
#else
    FILE *file=fopen("/data/gta5-story-autoload.log","a");
#endif
    if(file){fprintf(file,"%s\n",message);fclose(file);}
#else
    (void)message;
#endif
}
static int send_all(int fd,const void *data,size_t len){
    const unsigned char *p=data;
    while(len){int n=net_send(fd,p,len);if(n<=0)return -1;p+=n;len-=n;}
    return 0;
}
static int recv_all(int fd,void *data,size_t len){
    unsigned char *p=data;
    while(len){int n=net_recv(fd,p,len);if(n<=0)return -1;p+=n;len-=n;}
    return 0;
}
static int open_loopback(uint16_t port){
    int fd=net_socket();if(fd<0)return -1;
    /* Header and packet body are separate small writes. Avoid a delayed-ACK
     * stall on every debugger read, including console-local verification. */
    int no_delay=1;
#ifdef GTA_CONSOLE_PS5
    sceNetSetsockopt(fd,IPPROTO_TCP,1,&no_delay,sizeof(no_delay));
#else
    setsockopt(fd,IPPROTO_TCP,1,&no_delay,sizeof(no_delay));
#endif
    struct sockaddr_in addr={0};
    addr.sin_len=sizeof(addr);addr.sin_family=AF_INET;addr.sin_port=htons(port);
#ifdef GTA_CONSOLE_PS5
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
#else
    const char *host=getenv("GTA_HOST");
    if(!host)host="127.0.0.1";
    if(inet_pton(AF_INET,host,&addr.sin_addr)!=1){net_close(fd);return -1;}
#endif
    if(net_connect(fd,&addr)){net_close(fd);return -1;}
    return fd;
}
static int open_debugger(void){return open_loopback(DEBUG_PORT);}
#ifdef GTA_CONSOLE_PS5
static int request_debugger_relaunch(void){
    static const char request[]=
        "GET /loadpayload:ps5debug-NG_v1.3.2-local.elf HTTP/1.1\r\n"
        "Host: 127.0.0.1:8084\r\nConnection: close\r\n\r\n";
    int fd=open_loopback(MANAGER_PORT);
    if(fd<0)return 0;
    int sent=send_all(fd,request,sizeof(request)-1)==0;
    net_close(fd);
    return sent;
}
#endif
static int command(int fd,uint32_t code,const void *data,uint32_t length){
    uint32_t header[3]={0xffaabbccu,code,length};
    return send_all(fd,header,sizeof(header))||
        (length&&send_all(fd,data,length));
}
static int success(int fd){uint32_t value=0;return recv_all(fd,&value,4)==0&&value==CMD_SUCCESS;}
static int process_read(int fd,uint32_t pid,uint64_t at,void *out,uint32_t length){
    unsigned char arg[16]={0};memcpy(arg,&pid,4);memcpy(arg+4,&at,8);memcpy(arg+12,&length,4);
    return command(fd,CMD_PROC_READ,arg,sizeof(arg))==0&&success(fd)&&recv_all(fd,out,length)==0;
}
static int process_write(int fd,uint32_t pid,uint64_t at,const void *data,uint32_t length){
    unsigned char arg[16]={0};memcpy(arg,&pid,4);memcpy(arg+4,&at,8);memcpy(arg+12,&length,4);
    return command(fd,CMD_PROC_WRITE,arg,sizeof(arg))==0&&success(fd)&&send_all(fd,data,length)==0&&success(fd);
}
/* Recheck before publication too: maintenance may start during a long staging
 * cycle, after the worker passed its top-of-loop check. */
static int publication_allowed(void){
#if defined(GTA_CONSOLE_PS5) && defined(GTA_COOP_SPLIT)
    return access("/data/gta5-coop-maintenance",F_OK)!=0;
#else
    return 1;
#endif
}
static int process_info(int fd,uint32_t pid,char title[17],char content[65]){
    unsigned char data[188];
    if(command(fd,CMD_PROC_INFO,&pid,4)||!success(fd)||recv_all(fd,data,sizeof(data)))return 0;
    memcpy(title,data+108,16);title[16]=0;
    memcpy(content,data+124,64);content[64]=0;
    return 1;
}
static uint32_t find_game(int fd){
    if(command(fd,CMD_PROC_LIST,0,0)||!success(fd))return 0;
    uint32_t count=0;if(recv_all(fd,&count,4)||count>4096)return 0;
    unsigned char *entries=malloc((size_t)count*36);if(!entries)return 0;
    uint32_t found=0;
    if(recv_all(fd,entries,(size_t)count*36))goto done;
    for(uint32_t i=0;i<count;i++){
        unsigned char *entry=entries+i*36;
        if(memcmp(entry,"eboot.bin",9))continue;
        uint32_t pid=0;memcpy(&pid,entry+32,4);
        char title[17],content[65];
        if(process_info(fd,pid,title,content)&&!strcmp(title,"PPSA04264")&&
           !strcmp(content,"UP1004-PPSA04264_00-GTAVFULLGAMEDISC")){found=pid;break;}
    }
done:free(entries);return found;
}
static int game_ready(int fd,uint32_t pid){
    unsigned char bytes[256];
    for(size_t i=0;i<sizeof(gta_fingerprints)/sizeof(gta_fingerprints[0]);i++){
        const struct gta_fingerprint *fp=&gta_fingerprints[i];
        if(fp->length>sizeof(bytes)||!process_read(fd,pid,fp->address,bytes,fp->length)||
           memcmp(bytes,fp->bytes,fp->length))return 0;
    }
    unsigned char online=1;if(!process_read(fd,pid,GTA_NETWORK_FLAG,&online,1)||online)return 0;
    uint64_t root=0,player=0;
    if(!process_read(fd,pid,GTA_PLAYER_ROOT,&root,8)||!root||
       !process_read(fd,pid,root+8,&player,8)||!player)return 0;
    float health[2]={0};
    if(!process_read(fd,pid,player+0x250,health,8))return 0;
    if(!(health[1]>0&&health[1]<100000&&health[0]>=0&&health[0]<=health[1]*10))return 0;
    uint64_t player_info=0,weapons=0;
    return process_read(fd,pid,player+0x1088,&player_info,8)&&player_info&&
           process_read(fd,pid,player+0x1090,&weapons,8)&&weapons;
}
static int find_native_slot(int fd,uint32_t pid,uint64_t *out){
    unsigned char registry[0x58];
    if(!process_read(fd,pid,GTA_SCRIPT_REGISTRY,registry,sizeof(registry)))return 0;
    uint32_t buckets_count=0,stride=0;uint64_t program_slots=0,entries=0,buckets=0;
    memcpy(&program_slots,registry+8,8);memcpy(&stride,registry+0x1c,4);
    memcpy(&entries,registry+0x40,8);memcpy(&buckets,registry+0x48,8);
    memcpy(&buckets_count,registry+0x50,4);
    if(!buckets_count||buckets_count>65536||stride!=16||!program_slots||!entries||!buckets)return 0;
    const uint32_t hash=1459623836u;
    int32_t index=-1;
    if(!process_read(fd,pid,buckets+(uint64_t)(hash%buckets_count)*4,&index,4))return 0;
    for(unsigned n=0;index>=0&&index<16384&&n<16384;n++){
        unsigned char entry[12];
        if(!process_read(fd,pid,entries+(uint64_t)index*12,entry,12))return 0;
        uint32_t entry_hash=0;int32_t slot=0,next=-1;
        memcpy(&entry_hash,entry,4);memcpy(&slot,entry+4,4);memcpy(&next,entry+8,4);
        if(entry_hash==hash){
            if(slot<0||slot>=16384)return 0;
            uint64_t program=0;
            if(!process_read(fd,pid,program_slots+(uint64_t)slot*16,&program,8)||!program)return 0;
            unsigned char header[0x78];
            if(!process_read(fd,pid,program,header,sizeof(header)))return 0;
            uint32_t program_hash=0,count=0;uint64_t table=0;
            memcpy(&program_hash,header+0x58,4);memcpy(&count,header+0x2c,4);
            memcpy(&table,header+0x40,8);
            if(program_hash!=hash||!count||count>8192||!table)return 0;
            uint64_t candidate=0;unsigned found=0;
            uint64_t *handlers=malloc((size_t)count*8);
            if(!handlers)return 0;
            if(!process_read(fd,pid,table,handlers,count*8)){free(handlers);return 0;}
            for(uint32_t i=0;i<count;i++)if(handlers[i]==GTA_PLAYER_ID_HANDLER){candidate=table+(uint64_t)i*8;found++;}
            free(handlers);
            if(found==1){*out=candidate;return 1;}
            // The fixed main_persistent script has PLAYER_ID at index 858.
            // If that slot is already hooked, the caller validates the resident
            // image before treating it as ours.
            if(found==0&&count>858){*out=table+858*8;return 1;}
            return 0;
        }
        index=next;
    }
    return 0;
}
static int existing_menu(int fd,uint32_t pid,uint64_t base){
    uint64_t magic=0;if(!base||!process_read(fd,pid,base+0x10000,&magic,8)||magic!=MENU_MAGIC)return 0;
    unsigned char *code=malloc(0x10000);if(!code)return 0;
    int same=process_read(fd,pid,base,code,0x10000)&&!memcmp(code,gta_bridge_image,0x10000);
    free(code);
    uint32_t enabled=0;
    return same&&process_read(fd,pid,base+0x105fc,&enabled,4)&&enabled==1;
}
static uint64_t process_alloc(int fd,uint32_t pid,uint32_t length){
    uint32_t args[2]={pid,length};uint64_t address=0;
    if(command(fd,CMD_PROC_ALLOC,args,sizeof(args))||!success(fd)||recv_all(fd,&address,8))return 0;
    return address;
}
#ifdef GTA_COOP_AUTOLOAD
#ifdef GTA_COOP_OWNED_HOVER
#include "console-hover-stage.h"
#endif
#ifdef GTA_COOP_ROAD
static int existing_road(int fd,uint32_t pid,uint64_t owner);
#endif
#ifdef GTA_COOP_LATE_PLANE
static int existing_plane_runtime(int fd,uint32_t pid,uint64_t owner);
#endif
#ifdef GTA_COOP_OPPRESSOR2
static int existing_oppressor_runtime(int fd,uint32_t pid,uint64_t owner);
#endif
static int existing_coop(int fd,uint32_t pid,uint64_t base){
    uint64_t header[2]={0};uint32_t state=0;
    if(!base||!process_read(fd,pid,base+0x8000,header,16)||header[0]!=0x315353504f4f43ULL||
       !existing_menu(fd,pid,header[1])||!process_read(fd,pid,base+0x8010,&state,4)||
       (state!=1&&state!=2))return 0;
    unsigned char check[4096];
    for(uint32_t at=0;at<sizeof(coop_image);at+=4096){
        if(at>=0x8000&&at<0x9000)continue;
        if(!process_read(fd,pid,base+at,check,sizeof(check))||memcmp(check,coop_image+at,sizeof(check)))return 0;
    }
#ifdef GTA_COOP_PLAYER_ASSISTS
    uint32_t assists=0;if(!process_read(fd,pid,base+0x8f40,&assists,4)||assists!=3)return 0;
#endif
#ifdef GTA_COOP_ROAD
    if(!existing_road(fd,pid,base))return 0;
#endif
#ifdef GTA_COOP_LATE_PLANE
    if(!existing_plane_runtime(fd,pid,base))return 0;
#endif
#ifdef GTA_COOP_OPPRESSOR2
    if(!existing_oppressor_runtime(fd,pid,base))return 0;
#endif
    return 1;
}
#ifdef GTA_COOP_SPLIT
#include "console-split-stage.h"
#endif
#ifdef GTA_COOP_ROAD
#include "console-road-stage.h"
#endif
#ifdef GTA_COOP_LATE_PLANE
#include "console-plane-stage.h"
#include "console-plane-deferred.h"
#endif
#ifdef GTA_COOP_OPPRESSOR2
#include "console-oppressor2-stage.h"
#endif
static int install_coop(int fd,uint32_t pid,uint64_t slot){
    uint64_t menu=0;if(!process_read(fd,pid,slot,&menu,8))return 0;
#ifdef GTA_COOP_SPLIT
    if(existing_split(fd,pid,menu))return 1;
#endif
    if(existing_coop(fd,pid,menu))return 1;
    if(!existing_menu(fd,pid,menu))return 0;
#ifdef GTA_COOP_ROAD
    static uint32_t attempted_pid=0;
    if(attempted_pid==pid)return 0;
#endif
    unsigned char check[4096];
    for(unsigned i=0;i<sizeof(coop_fingerprints)/sizeof(coop_fingerprints[0]);i++){
        const struct gta_fingerprint *fp=&coop_fingerprints[i];
        if(fp->length>sizeof(check)||!process_read(fd,pid,fp->address,check,fp->length)||
           memcmp(check,fp->bytes,fp->length))return 0;
    }
    unsigned char request[16]={0};uint32_t size=sizeof(coop_image);uint64_t base=0;
#ifdef GTA_COOP_ROAD
    attempted_pid=pid;
#endif
    memcpy(request,&pid,4);memcpy(request+12,&size,4);
    if(command(fd,0xbdaa000eu,request,sizeof(request))||!success(fd)||recv_all(fd,&base,8)||!base||(base&4095))return 0;
    unsigned char *image=malloc(size);if(!image)return 0;
    memcpy(image,coop_image,size);
    uint64_t magic=0x315353504f4f43ULL;uint32_t state=1,mode=6;
    memcpy(image+0x8000,&magic,8);memcpy(image+0x8008,&menu,8);
    memcpy(image+0x8010,&state,4);memcpy(image+0x8020,&mode,4);
#ifdef GTA_COOP_PLANE_CONTROLS
    /* All image/native fingerprints were checked above. The plane owner
     * still checks live driver, GUID generation and exact vehicle class. */
    uint32_t plane_enabled=1;memcpy(image+0x8b00,&plane_enabled,4);
#endif
#ifdef GTA_COOP_LATE_PLANE
    /* The optional late-input helper is staged after healthy free roam, never
     * as a prerequisite for publishing P2 or the display. (AI-assisted) */
    memset(image+0x8b80,0,40);
#endif
#ifdef GTA_COOP_AIRCRAFT_EXTRAS
    uint32_t aircraft_enabled=1;memcpy(image+0x8c00,&aircraft_enabled,4);
#endif
#ifdef GTA_COOP_PLANE_PAYLOADS
    uint32_t payloads_enabled=1;memcpy(image+0x8d00,&payloads_enabled,4);
#ifdef GTA_COOP_AUTO_EQUIP_PAYLOADS
    uint32_t auto_equip_payloads=1;memcpy(image+0x8d70,&auto_equip_payloads,4);
#endif
#endif
#ifdef GTA_COOP_PRIMARY_PAYLOAD_EQUIPMENT
    uint32_t primary_payload_equipment=1;memcpy(image+0x8dc0,&primary_payload_equipment,4);
#endif
#ifdef GTA_COOP_OWNED_HOVER
    if(stage_owned_hover(fd,pid,base)){
        uint32_t hover_enabled=1;memcpy(image+OWNED_HOVER_MAILBOX,&hover_enabled,4);
        log_status("P2 Deluxo flight staged for automatic activation");
    }else log_status("P2 Deluxo flight could not be staged; installing other co-op controls");
#endif
#ifdef GTA_COOP_PLAYER_ASSISTS
    uint32_t assist_enabled=3;memcpy(image+0x8f40,&assist_enabled,4);
#endif
#ifdef GTA_COOP_ROAD
    if(!stage_road_runtime(fd,pid,base,image)){
        free(image);log_status("Road staging failed; no co-op hook published; retained mapping until game exit");return 0;
    }
    log_status("P2 continuous road input helper staged locally");
#endif
    int okay=1;
    for(uint32_t at=0;at<size;at+=4096){
        if(!process_write(fd,pid,base+at,image+at,4096)||
           !process_read(fd,pid,base+at,check,4096)||memcmp(check,image+at,4096)){okay=0;break;}
    }
    free(image);uint64_t current=0;
    if(okay&&(!process_read(fd,pid,slot,&current,8)||current!=menu))okay=0;
    if(okay&&!publication_allowed())okay=0;
    if(okay){
        okay=process_write(fd,pid,slot,&base,8);
        if(!process_read(fd,pid,slot,&current,8)||current!=base)okay=0;
    }
    if(okay){
        log_status("Installed GTA V co-op controls | P1 double R3 toggles P2");
#ifdef GTA_CONSOLE_PS5
        struct {char pad[45];char message[3075];} req={0};
        snprintf(req.message,sizeof(req.message),"GTA V co-op loaded | P1 double R3 toggles P2");
        sceKernelSendNotificationRequest(0,&req,sizeof(req),0);
#endif
    }
    return okay;
}
#endif
static int install_menu(int fd,uint32_t pid,uint64_t slot){
    uint64_t current=0;
    if(!process_read(fd,pid,slot,&current,8))return 0;
    if(current!=GTA_PLAYER_ID_HANDLER){
#ifdef GTA_COOP_AUTOLOAD
#ifdef GTA_COOP_SPLIT
        if(existing_split(fd,pid,current))return 1;
#endif
        if(existing_coop(fd,pid,current))return 1;
#endif
        return existing_menu(fd,pid,current);
    }
    const uint32_t image_size=(uint32_t)sizeof(gta_bridge_image);
    uint64_t base=process_alloc(fd,pid,image_size);if(!base)return 0;
    unsigned char *image=malloc(image_size);if(!image)return 0;
    memcpy(image,gta_bridge_image,image_size);
    uint64_t magic=MENU_MAGIC;
    memcpy(image+0x10000,&magic,8);
    memcpy(image+0x10008,&current,8);
    memcpy(image+0x10550,gta_native_handlers,sizeof(gta_native_handlers));
    uint32_t enabled=1;memcpy(image+0x105fc,&enabled,4);
    int okay=1;
    for(uint32_t offset=0;offset<image_size;offset+=4096){
        if(!process_write(fd,pid,base+offset,image+offset,4096)){okay=0;break;}
    }
    unsigned char check[128];
    if(okay&&(!process_read(fd,pid,base,check,64)||memcmp(check,image,64)||
              !process_read(fd,pid,base+0x10550,check,128)||memcmp(check,image+0x10550,128)))okay=0;
    if(okay&&(!process_read(fd,pid,slot,&current,8)||current!=GTA_PLAYER_ID_HANDLER))okay=0;
    if(okay&&!publication_allowed())okay=0;
    if(okay){
        okay=process_write(fd,pid,slot,&base,8);
        uint64_t actual=0;
        if(!process_read(fd,pid,slot,&actual,8)||actual!=base)okay=0;
    }
    free(image);
    if(okay){
#ifdef GTA_GHOST_AUTOLOAD
        log_status("Installed GTA V Ghost Rider candidate");
#else
        log_status("Installed GTA V Story menu");
#endif
        notify_install();
    }
    return okay;
}
#ifdef GTA_COOP_AUTOLOAD
#include "console-coop-recovery.h"
#endif
static int cycle(void){
    int fd=open_debugger();if(fd<0)return -1;
    int result=0;uint32_t pid=find_game(fd);uint64_t slot=0;
    if(pid&&game_ready(fd,pid)){
        if(find_native_slot(fd,pid,&slot)&&slot){
#ifdef GTA_COOP_AUTOLOAD
            int recovery=recover_retained_story(fd,pid,slot);
            if(recovery<0)goto cycle_done;
            if(recovery>0){result=(int)pid;goto remember_chain;}
#endif
        if(install_menu(fd,pid,slot)){
#ifdef GTA_COOP_AUTOLOAD
            if(install_coop(fd,pid,slot)){
#ifdef GTA_COOP_SPLIT
                if(install_split(fd,pid,slot))result=(int)pid;
#else
                result=(int)pid;
#endif
            }
#else
            result=(int)pid;
#endif
        }
        }
    }
#ifdef GTA_COOP_AUTOLOAD
remember_chain:
    if(result>0){
        remember_coop_chain(fd,pid,slot);
#ifdef GTA_COOP_LATE_PLANE
        maybe_stage_plane_runtime(fd,pid,slot);
#endif
#ifdef GTA_COOP_OPPRESSOR2
        maybe_stage_oppressor_runtime(fd,pid,slot);
#endif
    }
cycle_done:
#endif
    net_close(fd);return result;
}
#if defined(GTA_CONSOLE_PS5) && defined(GTA_COOP_SPLIT)
#include "coop-maintenance-lease.h"
static int coop_maintenance_active(void){
 static struct coop_maintenance_lease lease;
 FILE *file=fopen("/data/gta5-coop-maintenance","r");
 if(!file)return coop_maintenance_paused(&lease,0,0,0);
 unsigned char token[128];size_t n=fread(token,1,sizeof(token),file);fclose(file);
 struct timespec ts;if(clock_gettime(CLOCK_MONOTONIC,&ts))return 1;
 uint64_t now=(uint64_t)ts.tv_sec*1000u+(uint64_t)ts.tv_nsec/1000000u;
 if(coop_maintenance_paused(&lease,token,n,now))return 1;
 static const char prefix[]="agent controlled live update\n";
 if(n<sizeof(prefix)-1||memcmp(token,prefix,sizeof(prefix)-1))return 1;
 if(unlink("/data/gta5-coop-maintenance"))return 1;
 coop_maintenance_paused(&lease,0,0,now);
 log_status("Expired abandoned host-update lease; automatic GTA recovery resumed");return 0;
}
#endif
#ifdef GTA_CONSOLE_PS5
int main(int argc,char **argv){
    (void)argc;(void)argv;
#ifdef GTA_COOP_SPLIT
    /* Agent maintenance can pause reinstallation during a live update. A
     * fresh payload start clears the agent-owned lock, including after boot. */
    unlink("/data/gta5-coop-maintenance");
#endif
    log_status("GTA V console autoload worker started");
    int previously_ready=0;
    unsigned debugger_missing=0;
    for(;;){
#ifdef GTA_COOP_SPLIT
        if(coop_maintenance_active()){pause_seconds(1);continue;}
#endif
        int result=cycle();
        if(result>0&&!previously_ready)log_status("GTA V Story menu present");
        previously_ready=result>0;
        if(result<0){
            if(debugger_missing<1000000)debugger_missing++;
            // The manager's boot sequence only confirms ELF bytes were sent.
            // Retry once ShellCore and its local loader have settled.
            if(debugger_missing>=3&&(debugger_missing-3)%12==0){
                log_status(request_debugger_relaunch()?
                    "Requested PS5Debug relaunch from local Payload Manager":
                    "Could not reach local Payload Manager for PS5Debug relaunch");
            }
        }else debugger_missing=0;
        pause_seconds(5);
    }
    return 0;
}
#else
int main(void){int result=cycle();printf("GTA console autoload probe: %d\n",result);return result<0?1:0;}
#endif
