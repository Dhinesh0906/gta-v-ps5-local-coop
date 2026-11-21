#pragma once

enum { GR_ATTACK=1, GR_AIM=2 };
typedef struct { unsigned held,pressed,released,blocked,suppressed; } GhostInput;

static void ghost_input_update(GhostInput *input,unsigned raw,unsigned blocked){
    unsigned previous=input->held;
    if(blocked||input->blocked)input->suppressed|=raw;
    input->suppressed&=raw;
    input->held=blocked?0:raw&~input->suppressed;
    input->pressed=input->released=0;
    if(!blocked&&!input->blocked){
        input->pressed=input->held&~previous;
        input->released=previous&~input->held;
    }
    input->blocked=blocked;
}
