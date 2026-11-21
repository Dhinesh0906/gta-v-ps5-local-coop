/* Exact-build gamepad scalar conversion: mapper 0x2f6ac88, signed reader
 * 0x2f6e1e0 and positive reader 0x2f6e340. Keep flight independent of the
 * walking adapter's dead zone and diagonal normalization. */
typedef struct { float deadzone; unsigned flags; int multiplier[6]; } CoopPlaneInputSettings;
static float plane_clamp(float x){return x<-1?-1:x>1?1:x;}
static float plane_read_scalar(float raw,unsigned kind,int multiplier,const CoopPlaneInputSettings *s){
    float x=raw*(float)multiplier;
    if(s->deadzone>0&&!(s->flags&1)&&!((s->flags&2)&&((0x380007eu>>kind)&1))){
        if(x>s->deadzone)x=(x-s->deadzone)/(1-s->deadzone);
        else if(x<-s->deadzone)x=(x+s->deadzone)/(1-s->deadzone);
        else x=0;
    }
    return plane_clamp(x);
}
static float plane_read_axis(unsigned char byte,int multiplier,const CoopPlaneInputSettings *s){
    int value=(int)byte*2;if(value>509)value=509;
    return plane_read_scalar((float)(value-254)/255.0f,12,multiplier,s);
}
