// Diagnostic only: read OS users and one secondary pad; never bind a game ped.
typedef unsigned long long u64;
typedef unsigned int u32;
typedef int i32;
typedef struct {
    u64 magic,forward;
    volatile u32 state,error,visits;
    i32 users_result,initial_result,initial_user,selected_user,handle;
    i32 open_result,read_result,close_result;
    u32 owned;
    i32 users[4];
    unsigned char data[256];
} Probe;
_Static_assert(__builtin_offsetof(Probe,data)==0x50,"Input probe layout");
_Static_assert(sizeof(Probe)==0x150,"Input probe size");
__attribute__((section(".probe.data"),used,visibility("hidden")))
volatile Probe probe;

__attribute__((section(".entry"),used,visibility("hidden")))
void _start(void *context){
    probe.visits++;
    if(probe.magic==0x3150414f435447ULL&&probe.state==1){
        probe.state=2;
        probe.initial_user=probe.selected_user=probe.handle=-1;
        for(u32 i=0;i<4;i++)probe.users[i]=-1;
        if(*(volatile unsigned char*)0x5abefb8ULL)probe.error=1;
        else{
            probe.initial_result=((i32 (*)(i32*))0x37ba080ULL)((i32*)&probe.initial_user);
            probe.users_result=((i32 (*)(i32*))0x37ba0a0ULL)((i32*)probe.users);
            if(probe.initial_result||probe.users_result)probe.error=2;
            else{
                for(u32 i=0;i<4;i++)if(probe.users[i]>0&&probe.users[i]!=probe.initial_user){
                    probe.selected_user=probe.users[i];break;
                }
                if(probe.selected_user<0)probe.error=3;
                else{
                    for(u32 i=0;i<4;i++)if(((i32*)0x4361650ULL)[i]==probe.selected_user&&
                        ((i32*)0x5ee4750ULL)[i]>=0)probe.handle=((i32*)0x5ee4750ULL)[i];
                    if(probe.handle<0){
                        probe.open_result=((i32 (*)(i32,i32,i32,void*))0x37ba400ULL)(probe.selected_user,0,0,(void*)0);
                        probe.handle=probe.open_result;
                        if(probe.handle>=0){
                            probe.owned=1;
                            for(u32 i=0;i<4;i++)if(((i32*)0x5ee4750ULL)[i]==probe.handle)probe.owned=0;
                        }
                    }
                    if(probe.handle<0)probe.error=4;
                    else{
                        probe.read_result=((i32 (*)(i32,void*))0x37ba410ULL)(probe.handle,(void*)probe.data);
                        if(probe.read_result)probe.error=5;
                        if(probe.owned){
                            probe.close_result=((i32 (*)(i32))0x37ba430ULL)(probe.handle);
                            if(probe.close_result)probe.error=6;
                        }
                    }
                }
            }
        }
        probe.state=3;
    }
    if(probe.forward)((void (*)(void*))probe.forward)(context);
}
