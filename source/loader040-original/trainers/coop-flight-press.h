/* A held flight action must not also fire its short-press action. Arming
 * after neutral prevents a held input surviving pause/driver/model changes. */
typedef struct { unsigned started,down,used,armed; } CoopFlightPress;
enum { FLIGHT_PRESS_NONE,FLIGHT_PRESS_TAP,FLIGHT_PRESS_HOLD };
static __attribute__((always_inline)) inline unsigned flight_press_tick(CoopFlightPress *p,unsigned now,unsigned down,unsigned threshold){
    if(!p->armed){if(!down)p->armed=1;return FLIGHT_PRESS_NONE;}
    if(down&&!p->down){p->started=now;p->used=0;}
    unsigned event=FLIGHT_PRESS_NONE;
    if(down&&!p->used&&now-p->started>=threshold){event=FLIGHT_PRESS_HOLD;p->used=1;}
    else if(!down&&p->down&&!p->used)event=now-p->started>=threshold?FLIGHT_PRESS_HOLD:FLIGHT_PRESS_TAP;
    p->down=down;return event;
}
