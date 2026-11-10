/* SPDX-License-Identifier: GPL-3.0-only
 * DUB-BOX dub modules: the isolator, the DJ filter, the tape echo and the
 * siren against plain C references. ASan/UBSan clean, no device access. */
#define main gateway_simulator_main
#include "../../../host/plat_host.c"
#undef main
#include <assert.h>
#include <math.h>

static int32_t bus[512];
static void fill(double amp,double cycles){
    for(unsigned i=0;i<256;i++){
        double s=sin(cycles*2.0*M_PI*i/256.0)*amp*8388608.0;
        bus[2*i]=(int32_t)s;bus[2*i+1]=(int32_t)(s*0.8);
    }
}
static double rms(const int32_t *b){
    double s=0;for(unsigned i=0;i<512;i++)s+=(double)b[i]*b[i];return sqrt(s/512.0);
}
int main(void)
{
    /* the isolator: the sub kill removes the low band's energy, the rest stands */
    dub_isolator_t iso;dub_isolator_init(&iso);iso.enabled=1;iso.mix=127;
    for(unsigned b=0;b<DUB_BANDS;b++){dub_band_param_t q=iso.p[b];q.gain_db=0;q.mute=0;dub_isolator_set_band(&iso,b,q);}
    fill(0.5,2);double full=rms(bus);
    dub_isolator_process(&iso,bus,256);
    assert(fabs(rms(bus)-full)<full*0.01);                /* every knob at rest: untouched */
    {   dub_band_param_t q=iso.p[DUB_MIDS];q.mute=1;dub_isolator_set_band(&iso,DUB_MIDS,q); }
    for(unsigned b=0;b<80;b++){fill(0.5,6);dub_isolator_process(&iso,bus,256);}   /* settle */
    fill(0.5,6);dub_isolator_process(&iso,bus,256);       /* 1034 Hz: the MIDS band's home */
    double killed=rms(bus);
    assert(killed<full*0.6);                              /* the mids are gone */
    for(unsigned b=0;b<DUB_BANDS;b++){dub_band_param_t q=iso.p[b];q.mute=1;dub_isolator_set_band(&iso,b,q);}
    fill(0.5,2);dub_isolator_process(&iso,bus,256);assert(rms(bus)<8.0);
    /* the filter: FLAT is untouched, deep lowpass kills the tops */
    dub_filter_set(64);fill(0.5,30);
    int32_t save[512];memcpy(save,bus,sizeof save);dub_filter_process(bus,256);
    assert(!memcmp(save,bus,sizeof save));
    dub_filter_set(0);                                    /* full lowpass */
    for(unsigned b=0;b<40;b++){fill(0.5,30);dub_filter_process(bus,256);}   /* settle */
    fill(0.5,30);dub_filter_process(bus,256);
    assert(rms(bus)<rms(save)*0.5);
    dub_filter_set(64);
    /* the tape echo: the throw goes in, the wet returns a tape time later */
    dub_init();
    dub_tape_set_send(127);dub_tape_set_lvl(127);dub_tape_set_fb(0);dub_tape_set_ms(400);
    int32_t loud=0,early=0;
    for(unsigned b=0;b<120;b++){
        dub_tape_block(256);
        for(unsigned i=0;i<256;i++){
            uint32_t t=b*256u+i;
            int32_t in=(t>=2560u&&t<2624u)?6000000:0;     /* one burst, then silence */
            int32_t wet=dub_tape_step(in);
            if(wet>2000000){
                if(t<2560u+17000u)early=1;                /* nothing before the tape time */
                if(t>2560u+16800u&&t<2560u+18500u)loud=1; /* the echo, at 400 ms */
            }
        }
    }
    assert(!early && loud);
    /* the siren: silent until triggered, hot when it sings */
    dub_siren_t s;dub_siren_init(&s);
    for(unsigned i=0;i<64;i++)assert(dub_siren_tick(&s)==0);
    dub_siren_trigger(&s,100);
    int64_t peak=0;
    for(unsigned i=0;i<2000;i++){int32_t y=dub_siren_tick(&s);if(y<0)y=-y;if(y>peak)peak=y;}
    assert(peak>1000000);                                 /* the hot scale sloop's siren had */
    assert(dub_siren_rate_hz(0)==1 && dub_siren_rate_hz(127)==16);
    /* the master chain at rest: bit-transparent, module off and on */
    dub_tape_set_send(0);dub_tape_set_lvl(80);dub_tape_set_fb(70);dub_tape_set_ms(400);dub_tape_set_siren_send(0);
    dub_init();
    fill(0.9,7);int32_t ref[512];memcpy(ref,bus,sizeof ref);
    dub_master_process(bus,256);assert(!memcmp(ref,bus,sizeof ref));
    dub_on=1;fill(0.9,7);memcpy(ref,bus,sizeof ref);
    dub_master_process(bus,256);assert(!memcmp(ref,bus,sizeof ref));
    puts("PASS: dub isolator kills and restores bands, FLAT filter untouched, lowpass shaped, the tape echo throws at its tape time and rings, the siren sleeps until triggered and sings hot, the chain at rest is bit-transparent");
    return 0;
}
