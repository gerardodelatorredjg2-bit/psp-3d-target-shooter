#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspctrl.h>
#include <pspdebug.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("PSP 3D Target Shooter", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512
#define PI 3.14159265f
#define MAX_TARGETS 8

typedef struct { unsigned int color; float x, y, z; } Vertex;
typedef struct {
    float x, y, vx, vy, size;
    int active, type, points;
} Target;

static unsigned int __attribute__((aligned(16))) list[262144];
static unsigned int __attribute__((aligned(16))) frame_a[BUF_W * SCREEN_H];
static unsigned int __attribute__((aligned(16))) frame_b[BUF_W * SCREEN_H];
static Target targets[MAX_TARGETS];
static int score = 0, ammo = 12, hits = 0, shots = 0;
static int paused = 0, flash = 0, frame = 0;
static float aim_x = 240.0f, aim_y = 125.0f;
static SceCtrlData pad, oldpad;

static void draw_tri(const Vertex *v, int n) {
    sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   n, 0, v);
}
static void rect(float x, float y, float w, float h, unsigned int c) {
    Vertex v[6] = {
        {c,x,y,0},{c,x+w,y,0},{c,x+w,y+h,0},
        {c,x,y,0},{c,x+w,y+h,0},{c,x,y+h,0}
    };
    draw_tri(v,6);
}
static void line(float x1,float y1,float x2,float y2,float t,unsigned int c) {
    float dx=x2-x1, dy=y2-y1, len=sqrtf(dx*dx+dy*dy);
    if(len<0.01f) return;
    float ox=-dy/len*t*0.5f, oy=dx/len*t*0.5f;
    Vertex v[6]={{c,x1+ox,y1+oy,0},{c,x2+ox,y2+oy,0},{c,x2-ox,y2-oy,0},
                 {c,x1+ox,y1+oy,0},{c,x2-ox,y2-oy,0},{c,x1-ox,y1-oy,0}};
    draw_tri(v,6);
}
static void circle(float cx,float cy,float r,unsigned int c) {
    Vertex v[48]; int i;
    for(i=0;i<16;i++) {
        float a=(float)i*2.0f*PI/16.0f, b=(float)(i+1)*2.0f*PI/16.0f;
        v[i*3]=(Vertex){c,cx,cy,0};
        v[i*3+1]=(Vertex){c,cx+cosf(a)*r,cy+sinf(a)*r,0};
        v[i*3+2]=(Vertex){c,cx+cosf(b)*r,cy+sinf(b)*r,0};
    }
    draw_tri(v,48);
}
static void spawn_target(int i) {
    targets[i].size=10.0f+(float)(rand()%9);
    targets[i].x=(float)(rand()%400)+40.0f;
    targets[i].y=55.0f+(float)(rand()%90);
    targets[i].vx=((float)(rand()%100)-50.0f)/32.0f;
    targets[i].vy=((float)(rand()%30)-15.0f)/20.0f;
    targets[i].type=rand()%3;
    targets[i].points=targets[i].type==2?30:targets[i].type==1?20:10;
    targets[i].active=1;
}
static void draw_target(Target *t) {
    float s=t->size; unsigned int body=t->type==0?0xff4080d0:t->type==1?0xff40b080:0xff3090d0;
    rect(t->x-s,t->y-s,s*2,s*2,body);
    rect(t->x-s+3,t->y-s+3,s*2-6,4,0xffb0d8e8);
    rect(t->x-s+3,t->y+s-5,s*2-6,3,0xff183850);
    rect(t->x-s+5,t->y-s+5,3,s*2-10,0xff183850);
    rect(t->x+s-8,t->y-s+5,3,s*2-10,0xff183850);
}
static void draw_scene(void) {
    int i;
    rect(0,0,480,178,0xffb97843);
    rect(0,178,480,94,0xff3c6840);
    circle(395,47,23,0xff70c8ef);
    /* distant hills */
    for(i=0;i<6;i++) rect(i*90-10,155+(i%2)*8,100,24,0xff456b42);
    /* range floor perspective */
    line(0,205,480,205,2,0xff75945a);
    line(240,178,0,272,1,0xff75945a);
    line(240,178,480,272,1,0xff75945a);
    for(i=0;i<MAX_TARGETS;i++) if(targets[i].active) draw_target(&targets[i]);
    /* crosshair */
    line(aim_x-12,aim_y,aim_x-4,aim_y,2,0xffffffff);
    line(aim_x+4,aim_y,aim_x+12,aim_y,2,0xffffffff);
    line(aim_x,aim_y-12,aim_x,aim_y-4,2,0xffffffff);
    line(aim_x,aim_y+4,aim_x,aim_y+12,2,0xffffffff);
    circle(aim_x,aim_y,2,0xffffffff);
    /* low-poly pistol silhouette, no arms or hands */
    rect(360,216,80,22,0xff20242b);
    rect(374,202,54,18,0xff3b4149);
    rect(395,194,31,10,0xff68717b);
    rect(386,236,28,31,0xff242a31);
    rect(391,240,4,22,0xff59616a);
    rect(432,220,14,8,0xffa5adb5);
    if(flash>0) {
        Vertex muzzle[3]={{0xff40dffff,449,214,0},{0xff40dffff,472,204,0},{0xff40dffff,466,229,0}};
        draw_tri(muzzle,3);
    }
    /* HUD */
    rect(8,8,148,30,0xaa101820);
    rect(324,8,148,30,0xaa101820);
    pspDebugScreenSetXY(1,1); pspDebugScreenSetTextColor(0xffffffff);
    pspDebugScreenPrintf("SCORE %06d   HITS %d",score,hits);
    pspDebugScreenSetXY(42,1);
    pspDebugScreenPrintf("AMMO %02d   SHOTS %d",ammo,shots);
    if(paused) {
        rect(165,112,150,48,0xee101820);
        pspDebugScreenSetXY(20,16); pspDebugScreenPrintf("PAUSED - START TO RESUME");
    }
}
static void update_game(void) {
    int i;
    if(paused) return;
    if(pad.Buttons & PSP_CTRL_LEFT) aim_x-=2.2f;
    if(pad.Buttons & PSP_CTRL_RIGHT) aim_x+=2.2f;
    if(pad.Buttons & PSP_CTRL_UP) aim_y-=2.2f;
    if(pad.Buttons & PSP_CTRL_DOWN) aim_y+=2.2f;
    aim_x+=(float)pad.Lx-128.0f; aim_y+=((float)pad.Ly-128.0f)*0.7f;
    if(aim_x<12)aim_x=12;if(aim_x>468)aim_x=468;
    if(aim_y<38)aim_y=38;if(aim_y>190)aim_y=190;
    if((pad.Buttons & PSP_CTRL_RTRIGGER) && !(oldpad.Buttons & PSP_CTRL_RTRIGGER) && ammo>0) {
        int found=0; ammo--;shots++;flash=3;
        for(i=0;i<MAX_TARGETS;i++) if(targets[i].active) {
            float dx=targets[i].x-aim_x,dy=targets[i].y-aim_y;
            if(dx*dx+dy*dy<targets[i].size*targets[i].size*1.5f) {
                score+=targets[i].points;hits++;targets[i].active=0;found=1;break;
            }
        }
        (void)found;
    }
    if(ammo==0) { ammo=12; }
    for(i=0;i<MAX_TARGETS;i++) {
        if(!targets[i].active) { if((rand()%100)<3) spawn_target(i); continue; }
        targets[i].x+=targets[i].vx;targets[i].y+=targets[i].vy;
        if(targets[i].x<20||targets[i].x>460)targets[i].vx=-targets[i].vx;
        if(targets[i].y<45||targets[i].y>170)targets[i].vy=-targets[i].vy;
    }
    if(flash>0)flash--;
}
int main(void) {
    int i;
    sceCtrlSetSamplingCycle(0);sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    pspDebugScreenInit();
    sceGuInit();
    sceGuStart(GU_DIRECT,list);
    sceGuDrawBuffer(GU_PSM_8888,(void*)frame_a,BUF_W);
    sceGuDispBuffer(SCREEN_W,SCREEN_H,(void*)frame_b,BUF_W);
    sceGuDepthBuffer((void*)(0x1100000),BUF_W);
    sceGuOffset(2048-(SCREEN_W/2),2048-(SCREEN_H/2));
    sceGuViewport(2048,2048,SCREEN_W,SCREEN_H);
    sceGuScissor(0,0,SCREEN_W,SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuFinish();sceGuSync(0,0);sceGuDisplay(GU_TRUE);
    srand(0x505350);
    for(i=0;i<MAX_TARGETS;i++) { targets[i].active=0; if(i<4)spawn_target(i); }
    memset(&oldpad,0,sizeof(oldpad));
    while(1) {
        sceCtrlReadBufferPositive(&pad,1);
        if((pad.Buttons & PSP_CTRL_START) && !(oldpad.Buttons & PSP_CTRL_START)) paused=!paused;
        if((pad.Buttons & PSP_CTRL_SELECT) && !(oldpad.Buttons & PSP_CTRL_SELECT)) {
            score=hits=shots=0;ammo=12;for(i=0;i<MAX_TARGETS;i++)spawn_target(i);
        }
        if(pad.Buttons & PSP_CTRL_TRIANGLE) break;
        sceGuStart(GU_DIRECT,list);
        sceGuClearColor(0xff000000);sceGuClear(GU_COLOR_BUFFER_BIT);
        pspDebugScreenSetOffset((int)frame_a);
        pspDebugScreenSetXY(0,0);
        update_game();
        draw_scene();
        sceGuFinish();sceGuSync(0,0);
        sceDisplayWaitVblankStart();
        sceGuSwapBuffers();
        oldpad=pad;frame++;
    }
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
