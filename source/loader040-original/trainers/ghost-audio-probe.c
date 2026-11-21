// Standalone, one-shot PS5 AudioOut check for the original Ghost Rider sounds.
// It reads one staged PCM file; it is not part of the active GTA autoloader.
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int sceUserServiceGetInitialUser(int *user_id);
extern int sceAudioOutInit(void);
extern int sceAudioOutOpen(int user_id,int type,int index,uint32_t grain,uint32_t hz,uint32_t format);
extern int sceAudioOutOutput(int handle,const void *samples);
extern int sceAudioOutClose(int handle);

int main(void){
    FILE *log=fopen("/data/gta5-ghost-audio-probe.log","w");
    FILE *sound=fopen("/data/gta5-ghost-audio/whistle.pcm","rb");
    if(!sound){if(log){fprintf(log,"pcm missing\n");fclose(log);}return 1;}
    int user=0,rc=sceUserServiceGetInitialUser(&user);
    if(rc<0){if(log){fprintf(log,"user error %x\n",(unsigned)rc);fclose(log);}fclose(sound);return 2;}
    rc=sceAudioOutInit();
    if(rc<0){if(log){fprintf(log,"init error %x\n",(unsigned)rc);fclose(log);}fclose(sound);return 3;}
    int port=sceAudioOutOpen(user,0,0,256,48000,1);
    if(port<0){if(log){fprintf(log,"open error %x user %d\n",(unsigned)port,user);fclose(log);}fclose(sound);return 4;}
    int16_t grain[256*2];unsigned blocks=0;
    for(;;){
        size_t got=fread(grain,1,sizeof(grain),sound);
        if(!got)break;
        if(got<sizeof(grain))memset((unsigned char*)grain+got,0,sizeof(grain)-got);
        rc=sceAudioOutOutput(port,grain);
        if(rc<0)break;
        blocks++;
    }
    sceAudioOutClose(port);fclose(sound);
    if(log){fprintf(log,"output %s blocks %u rc %x\n",rc<0?"failed":"complete",blocks,(unsigned)rc);fclose(log);}
    return rc<0?5:0;
}
