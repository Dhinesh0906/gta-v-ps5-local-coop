/* A host update may defer installation, but must not disable recovery forever
 * if the Mac disappears. Renew by changing the bounded lease token. */
struct coop_maintenance_lease{uint64_t started;size_t length;unsigned char token[128];int active;};
static int coop_maintenance_paused(struct coop_maintenance_lease *s,const unsigned char *token,size_t n,uint64_t now){
 if(!token){s->active=0;s->length=0;return 0;}
 if(n>sizeof(s->token))n=sizeof(s->token);
 if(!s->active||s->length!=n||memcmp(s->token,token,n)){
  s->active=1;s->length=n;s->started=now;memcpy(s->token,token,n);
 }
 return now<s->started||now-s->started<180000u;
}
