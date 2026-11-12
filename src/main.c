#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspctrl.h>
#include <psputils.h>
#include <pspaudio.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

PSP_MODULE_INFO("PSP Target Shooter 3D", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512
#define PI 3.14159265358979323846f
#define FOV_DEG 55.0f
#define CAMERA_Y 1.48f
#define MAX_TARGETS 7
#define GAME_SECONDS 90
#define AUDIO_SAMPLES 512
#define HUD_MAX_VERTICES 24000

typedef struct {
    unsigned int color;
    float x, y, z;
} Vertex;

typedef struct {
    const Vertex *vertices;
    unsigned int vertex_count;
    float cx, cy, cz;
    float radius, height;
} MeshDef;

#include "models_generated.h"

typedef enum { SCREEN_TITLE = 0, SCREEN_PLAY, SCREEN_PAUSE, SCREEN_OVER } GameScreen;

typedef struct {
    int state;               /* 0 waiting, 1 moving, 2 falling after a hit */
    int type;
    int life;
    int respawn;
    float x, y, z;
    float vx, vy, vz;
    float angle, spin;
    float scale;
} Target;

static unsigned int __attribute__((aligned(16))) gu_list[262144];
/* sceGuDrawBuffer/DispBuffer/DepthBuffer expect offsets from VRAM base,
   not CPU-visible absolute addresses such as 0x04000000. */
#define FRAME_BYTES ((unsigned int)(BUF_W * SCREEN_H * 4u))
#define PSP_FRAME_A ((void *)0x00000000u)
#define PSP_FRAME_B ((void *)FRAME_BYTES)
#define PSP_DEPTH_BUFFER ((void *)(FRAME_BYTES * 2u))
static Vertex __attribute__((aligned(64))) hud_vertices[HUD_MAX_VERTICES];
static short __attribute__((aligned(64))) audio_buffer[AUDIO_SAMPLES];

static Target targets[MAX_TARGETS];
static GameScreen screen_state = SCREEN_TITLE;
static SceCtrlData pad, oldpad;
static int score = 0, shots = 0, hits = 0, ammo = 12;
static int game_frames = GAME_SECONDS * 60;
static int flash_frames = 0, reload_frames = 0, hit_frames = 0;
static int audio_channel = -1;
static int hud_vertex_count = 0;
static float aim_x = 240.0f, aim_y = 126.0f;
static float weapon_recoil = 0.0f;
static float hit_x = 240.0f, hit_y = 126.0f;
static int last_points = 0;

static const Vertex ground_mesh[] __attribute__((aligned(16))) = {
    {0xff355c38u,-90.0f,0.0f,-90.0f},
    {0xff355c38u, 90.0f,0.0f,-90.0f},
    {0xff355c38u, 90.0f,0.0f, -1.0f},
    {0xff355c38u,-90.0f,0.0f,-90.0f},
    {0xff355c38u, 90.0f,0.0f, -1.0f},
    {0xff355c38u,-90.0f,0.0f, -1.0f}
};

static const unsigned char font5x7[36][5] = {
    {0x3e,0x51,0x49,0x45,0x3e}, {0x00,0x42,0x7f,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10}, {0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1e},
    {0x7e,0x11,0x11,0x11,0x7e}, {0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22}, {0x7f,0x41,0x41,0x22,0x1c},
    {0x7f,0x49,0x49,0x49,0x41}, {0x7f,0x09,0x09,0x09,0x01},
    {0x3e,0x41,0x49,0x49,0x7a}, {0x7f,0x08,0x08,0x08,0x7f},
    {0x00,0x41,0x7f,0x41,0x00}, {0x20,0x40,0x41,0x3f,0x01},
    {0x7f,0x08,0x14,0x22,0x41}, {0x7f,0x40,0x40,0x40,0x40},
    {0x7f,0x02,0x0c,0x02,0x7f}, {0x7f,0x04,0x08,0x10,0x7f},
    {0x3e,0x41,0x41,0x41,0x3e}, {0x7f,0x09,0x09,0x09,0x06},
    {0x3e,0x41,0x51,0x21,0x5e}, {0x7f,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31}, {0x01,0x01,0x7f,0x01,0x01},
    {0x3f,0x40,0x40,0x40,0x3f}, {0x1f,0x20,0x40,0x20,0x1f},
    {0x3f,0x40,0x38,0x40,0x3f}, {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}
};

static const MeshDef *target_mesh(int type) {
    switch (type) {
        case 0: return &MESH_CRATE;
        case 1: return &MESH_BARREL;
        case 2: return &MESH_SODA_CAN;
        default: return &MESH_SODA_BOTTLE;
    }
}

static float target_scale(int type) {
    static const float scales[4] = {0.43f, 0.70f, 2.10f, 1.30f};
    return scales[type & 3];
}

static int target_points(int type) {
    static const int points[4] = {25, 20, 10, 15};
    return points[type & 3];
}

static float rand_unit(void) {
    return (float)(rand() & 32767) / 32767.0f;
}

static void play_sfx(int kind) {
    int i;
    if (audio_channel < 0) return;
    for (i = 0; i < AUDIO_SAMPLES; ++i) {
        float t = (float)i / (float)AUDIO_SAMPLES;
        float envelope = (1.0f - t) * (1.0f - t);
        float noise = ((float)(rand() & 65535) / 32767.5f) - 1.0f;
        float frequency = kind == 0 ? 105.0f + t * 95.0f : 620.0f - t * 260.0f;
        float tone = sinf(2.0f * PI * frequency * (float)i / 44100.0f);
        float sample = kind == 0 ? (noise * 0.72f + tone * 0.28f) : (tone * 0.8f + noise * 0.2f);
        float value = sample * envelope * (kind == 0 ? 24500.0f : 13500.0f);
        if (value > 32767.0f) value = 32767.0f;
        if (value < -32768.0f) value = -32768.0f;
        audio_buffer[i] = (short)value;
    }
    sceAudioOutputBlocking(audio_channel, PSP_AUDIO_VOLUME_MAX, audio_buffer);
}

static void spawn_target(int i) {
    Target *t = &targets[i];
    int direction = (rand() & 1) ? 1 : -1;
    t->type = rand() % 4;
    t->scale = target_scale(t->type);
    t->state = 1;
    t->life = 0;
    t->respawn = 0;
    t->x = direction > 0 ? -4.8f : 4.8f;
    t->y = 0.65f + rand_unit() * 2.35f;
    t->z = -8.5f - rand_unit() * 7.5f;
    t->vx = (float)direction * (0.018f + rand_unit() * 0.035f);
    t->vy = (rand_unit() - 0.5f) * 0.008f;
    t->vz = 0.0f;
    t->angle = rand_unit() * PI * 2.0f;
    t->spin = (rand_unit() - 0.5f) * 0.025f;
}

static void reset_game(void) {
    int i;
    score = shots = hits = 0;
    ammo = 12;
    game_frames = GAME_SECONDS * 60;
    flash_frames = reload_frames = hit_frames = 0;
    aim_x = 240.0f;
    aim_y = 126.0f;
    weapon_recoil = 0.0f;
    for (i = 0; i < MAX_TARGETS; ++i) {
        targets[i].state = 0;
        spawn_target(i);
    }
    screen_state = SCREEN_PLAY;
}

static void update_aim(void) {
    float ax = (float)pad.Lx - 128.0f;
    float ay = (float)pad.Ly - 128.0f;
    if (fabsf(ax) > 10.0f) aim_x += ax * 0.035f;
    if (fabsf(ay) > 10.0f) aim_y += ay * 0.032f;
    if (pad.Buttons & PSP_CTRL_LEFT) aim_x -= 2.3f;
    if (pad.Buttons & PSP_CTRL_RIGHT) aim_x += 2.3f;
    if (pad.Buttons & PSP_CTRL_UP) aim_y -= 2.0f;
    if (pad.Buttons & PSP_CTRL_DOWN) aim_y += 2.0f;
    if (aim_x < 10.0f) aim_x = 10.0f;
    if (aim_x > 470.0f) aim_x = 470.0f;
    if (aim_y < 40.0f) aim_y = 40.0f;
    if (aim_y > 218.0f) aim_y = 218.0f;
}

static float camera_focal(void) {
    return 136.0f / tanf((FOV_DEG * PI / 180.0f) * 0.5f);
}

static int project_point(float x, float y, float z, float *sx, float *sy, float *depth) {
    float d = -z;
    float f = camera_focal();
    if (d < 0.2f) return 0;
    *sx = 240.0f + (x / d) * f;
    *sy = 136.0f - ((y - CAMERA_Y) / d) * f;
    *depth = d;
    return (*sx > -80.0f && *sx < 560.0f && *sy > -80.0f && *sy < 350.0f);
}

static void fire_weapon(void) {
    int i, best = -1;
    float best_depth = 1000.0f;
    if (ammo <= 0 || reload_frames > 0) {
        play_sfx(1);
        return;
    }
    --ammo;
    ++shots;
    flash_frames = 5;
    weapon_recoil = 0.12f;
    play_sfx(0);
    for (i = 0; i < MAX_TARGETS; ++i) {
        Target *t = &targets[i];
        const MeshDef *m;
        float sx, sy, depth, dx, dy, radius;
        if (t->state != 1) continue;
        if (!project_point(t->x, t->y, t->z, &sx, &sy, &depth)) continue;
        m = target_mesh(t->type);
        radius = (m->radius * t->scale * camera_focal() / depth) * 0.90f;
        if (radius < 7.0f) radius = 7.0f;
        dx = sx - aim_x;
        dy = sy - aim_y;
        if (dx*dx + dy*dy <= radius*radius && depth < best_depth) {
            best = i;
            best_depth = depth;
        }
    }
    if (best >= 0) {
        Target *t = &targets[best];
        float sx = 0.0f, sy = 0.0f, depth = 0.0f;
        project_point(t->x, t->y, t->z, &sx, &sy, &depth);
        hit_x = sx;
        hit_y = sy;
        last_points = target_points(t->type);
        score += last_points;
        ++hits;
        hit_frames = 32;
        t->state = 2;
        t->life = 70;
        t->vx *= 1.6f;
        t->vy = 0.095f + rand_unit() * 0.045f;
        t->vz = -0.018f;
        t->spin = (rand_unit() - 0.5f) * 0.16f;
    }
}

static void update_targets(void) {
    int i;
    for (i = 0; i < MAX_TARGETS; ++i) {
        Target *t = &targets[i];
        if (t->state == 0) {
            if (t->respawn > 0) --t->respawn;
            else if ((rand() % 100) < 3) spawn_target(i);
            continue;
        }
        if (t->state == 1) {
            t->x += t->vx;
            t->y += t->vy + sinf((float)(game_frames + i * 37) * 0.025f) * 0.002f;
            t->angle += t->spin;
            if (t->x < -5.4f || t->x > 5.4f) t->vx = -t->vx;
            if (t->y < 0.45f || t->y > 3.35f) t->vy = -t->vy;
        } else {
            t->x += t->vx;
            t->y += t->vy;
            t->z += t->vz;
            t->vy -= 0.0045f;
            t->vz -= 0.0004f;
            t->angle += t->spin;
            if (--t->life <= 0 || t->y < -2.0f || t->z < -30.0f) {
                t->state = 0;
                t->respawn = 18 + rand() % 65;
            }
        }
    }
}

static void update_game(void) {
    unsigned int pressed = pad.Buttons & ~oldpad.Buttons;
    update_aim();
    if (pressed & PSP_CTRL_START) {
        screen_state = SCREEN_PAUSE;
        return;
    }
    if (pressed & PSP_CTRL_SELECT) {
        reset_game();
        return;
    }
    if ((pressed & PSP_CTRL_SQUARE) && reload_frames == 0 && ammo < 12) {
        reload_frames = 36;
        play_sfx(1);
    }
    if ((pressed & PSP_CTRL_RTRIGGER) && reload_frames == 0) fire_weapon();
    if (reload_frames > 0) {
        --reload_frames;
        if (reload_frames == 0) ammo = 12;
    }
    update_targets();
    if (flash_frames > 0) --flash_frames;
    if (hit_frames > 0) --hit_frames;
    if (weapon_recoil > 0.0f) weapon_recoil *= 0.78f;
    if (weapon_recoil < 0.002f) weapon_recoil = 0.0f;
    if (game_frames > 0) --game_frames;
    else screen_state = SCREEN_OVER;
}

static void draw_mesh(const MeshDef *mesh, float x, float y, float z,
                      float scale, float rx, float ry, float rz) {
    ScePspFVector3 pos = {x, y, z};
    ScePspFVector3 scl = {scale, scale, scale};
    ScePspFVector3 ctr = {-mesh->cx, -mesh->cy, -mesh->cz};
    sceGumPushMatrix();
    sceGumTranslate(&pos);
    sceGumRotateY(ry);
    sceGumRotateX(rx);
    sceGumRotateZ(rz);
    sceGumScale(&scl);
    sceGumTranslate(&ctr);
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        mesh->vertex_count, 0, mesh->vertices);
    sceGumPopMatrix();
}

static void draw_environment(void) {
    int i;
    static const float tree_x[8] = {-10.0f,-7.5f,-4.7f,4.8f,7.4f,10.2f,-8.8f,8.9f};
    static const float tree_z[8] = {-26.0f,-32.0f,-38.0f,-36.0f,-30.0f,-40.0f,-46.0f,-48.0f};
    sceGumPushMatrix();
    sceGumLoadIdentity();
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        6, 0, ground_mesh);
    sceGumPopMatrix();
    for (i = 0; i < 8; ++i) {
        float s = (i % 3 == 0) ? 2.8f : 2.25f;
        float y = MESH_TREE.height * s * 0.5f;
        draw_mesh(&MESH_TREE, tree_x[i], y, tree_z[i], s,
                  0.0f, (float)(i * 31) * 0.017f, 0.0f);
    }
    draw_mesh(&MESH_ROCK, -9.0f, MESH_ROCK.height*1.35f, -17.0f, 2.7f, 0, 0.3f, 0);
    draw_mesh(&MESH_ROCK,  9.4f, MESH_ROCK.height*1.45f, -19.0f, 2.9f, 0, 1.1f, 0);
    draw_mesh(&MESH_ROCK, -13.0f, MESH_ROCK.height*1.5f, -31.0f, 3.0f, 0, 0.4f, 0);
}

static void draw_targets(void) {
    int i;
    for (i = 0; i < MAX_TARGETS; ++i) {
        Target *t = &targets[i];
        if (t->state == 0) continue;
        draw_mesh(target_mesh(t->type), t->x, t->y, t->z, t->scale,
                  t->angle * 0.12f, t->angle, t->angle * 0.08f);
    }
}

static void draw_weapon(void) {
    ScePspFVector3 pos = {0.43f, CAMERA_Y - 0.33f, -1.35f + weapon_recoil};
    ScePspFVector3 scl = {0.92f, 0.92f, 0.92f};
    ScePspFVector3 ctr = {-MESH_PISTOL.cx, -MESH_PISTOL.cy, -MESH_PISTOL.cz};
    sceGumPushMatrix();
    sceGumTranslate(&pos);
    sceGumRotateY(PI);
    sceGumRotateZ(-0.13f);
    sceGumScale(&scl);
    sceGumTranslate(&ctr);
    sceGuDrawArray(GU_TRIANGLES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
        MESH_PISTOL.vertex_count, 0, MESH_PISTOL.vertices);
    sceGumPopMatrix();
}

static void hud_add_rect(float x, float y, float w, float h, unsigned int color) {
    Vertex v[6] = {
        {color,x,y,0},{color,x+w,y,0},{color,x+w,y+h,0},
        {color,x,y,0},{color,x+w,y+h,0},{color,x,y+h,0}
    };
    int i;
    if (hud_vertex_count + 6 > HUD_MAX_VERTICES) return;
    for (i=0;i<6;i++) hud_vertices[hud_vertex_count++] = v[i];
}

static void hud_add_line(float x1, float y1, float x2, float y2, float width, unsigned int color) {
    float dx=x2-x1, dy=y2-y1, len=sqrtf(dx*dx+dy*dy);
    float ox, oy;
    Vertex v[6];
    int i;
    if (len < 0.01f || hud_vertex_count + 6 > HUD_MAX_VERTICES) return;
    ox = -dy / len * width * 0.5f;
    oy =  dx / len * width * 0.5f;
    v[0]=(Vertex){color,x1+ox,y1+oy,0}; v[1]=(Vertex){color,x2+ox,y2+oy,0}; v[2]=(Vertex){color,x2-ox,y2-oy,0};
    v[3]=(Vertex){color,x1+ox,y1+oy,0}; v[4]=(Vertex){color,x2-ox,y2-oy,0}; v[5]=(Vertex){color,x1-ox,y1-oy,0};
    for (i=0;i<6;i++) hud_vertices[hud_vertex_count++] = v[i];
}

static int glyph_index(char ch) {
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'Z') return 10 + ch - 'A';
    return -1;
}

static void hud_add_text(float x, float y, const char *text, float scale, unsigned int color) {
    int ci, col, row;
    while (*text) {
        char ch = *text++;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        ci = glyph_index(ch);
        if (ci >= 0) {
            for (col = 0; col < 5; ++col) {
                unsigned char bits = font5x7[ci][col];
                for (row = 0; row < 7; ++row)
                    if (bits & (1u << row))
                        hud_add_rect(x + col*scale, y + row*scale, scale, scale, color);
            }
        } else if (ch == ':') {
            hud_add_rect(x+2*scale,y+1*scale,scale,scale,color);
            hud_add_rect(x+2*scale,y+5*scale,scale,scale,color);
        } else if (ch == '-') {
            hud_add_rect(x+scale,y+3*scale,3*scale,scale,color);
        } else if (ch == '+') {
            hud_add_rect(x+2*scale,y+scale,scale,5*scale,color);
            hud_add_rect(x+scale,y+3*scale,3*scale,scale,color);
        } else if (ch == '%') {
            hud_add_line(x,y+6*scale,x+5*scale,y,scale,color);
            hud_add_rect(x,y,scale,scale,color);
            hud_add_rect(x+4*scale,y+5*scale,scale,scale,color);
        } else if (ch == '.') {
            hud_add_rect(x+2*scale,y+6*scale,scale,scale,color);
        }
        x += 6.0f * scale;
    }
}

static void hud_add_crosshair(void) {
    unsigned int c = flash_frames > 0 ? 0xffffd45au : 0xfff4f5f2u;
    hud_add_line(aim_x-15,aim_y,aim_x-5,aim_y,2.0f,c);
    hud_add_line(aim_x+5,aim_y,aim_x+15,aim_y,2.0f,c);
    hud_add_line(aim_x,aim_y-15,aim_x,aim_y-5,2.0f,c);
    hud_add_line(aim_x,aim_y+5,aim_x,aim_y+15,2.0f,c);
    hud_add_line(aim_x-3,aim_y-3,aim_x+3,aim_y+3,1.0f,c);
    hud_add_line(aim_x-3,aim_y+3,aim_x+3,aim_y-3,1.0f,c);
}

static void draw_hud(void) {
    char buf[48];
    int secs = game_frames / 60;
    int accuracy = shots > 0 ? (hits * 100) / shots : 0;
    hud_vertex_count = 0;
    hud_add_rect(0,0,480,36,0xb915202cu);
    hud_add_rect(10,7,150,23,0x99101820u);
    hud_add_rect(320,7,150,23,0x99101820u);
    hud_add_text(18,10,"PUNTOS",1.5f,0xffe7edf3u);
    snprintf(buf,sizeof(buf),"%06d",score);
    hud_add_text(80,10,buf,1.5f,0xffffdc78u);
    snprintf(buf,sizeof(buf),"BALAS %02d",ammo);
    hud_add_text(330,10,buf,1.5f,0xfff1f3f5u);
    snprintf(buf,sizeof(buf),"TIEMPO %02d",secs);
    hud_add_text(191,10,buf,1.5f,0xfff1f3f5u);
    if (reload_frames > 0) {
        hud_add_rect(175,244,130,17,0xb915202cu);
        hud_add_text(185,249,"RECARGANDO",1.0f,0xffffdf77u);
    } else {
        hud_add_text(12,250,"R DISPARA",1.0f,0xffedf0e8u);
        hud_add_text(118,250,"CUADRADO RECARGA",1.0f,0xffedf0e8u);
        snprintf(buf,sizeof(buf),"ACIERTOS %d %d%%",hits,accuracy);
        hud_add_text(336,250,buf,1.0f,0xffedf0e8u);
    }
    if (hit_frames > 0) {
        snprintf(buf,sizeof(buf),"+%d",last_points);
        hud_add_text(hit_x-12,hit_y-20,buf,1.6f,0xffffe36au);
    }
    if (flash_frames > 0) {
        hud_add_line(374,218,391,196,4.0f,0xffffd24au);
        hud_add_line(390,224,412,207,3.0f,0xfffff3a0u);
        hud_add_line(380,230,397,228,3.0f,0xffff9e33u);
    }
    if (screen_state == SCREEN_TITLE) {
        hud_add_rect(56,67,368,135,0xe80c1722u);
        hud_add_text(99,84,"PSP TARGET SHOOTER",2.0f,0xffffffffu);
        hud_add_text(127,118,"ENTRENAMIENTO 3D",1.5f,0xff80d9ffu);
        hud_add_text(143,150,"X INICIAR",1.5f,0xffffde75u);
        hud_add_text(89,179,"ANALOG O CRUCETA PARA APUNTAR",1.0f,0xffe9edf0u);
    } else if (screen_state == SCREEN_PAUSE) {
        hud_add_rect(88,91,304,85,0xe80c1722u);
        hud_add_text(181,108,"PAUSA",2.0f,0xffffffffu);
        hud_add_text(128,143,"START CONTINUAR",1.0f,0xffffde75u);
    } else if (screen_state == SCREEN_OVER) {
        hud_add_rect(75,75,330,116,0xe80c1722u);
        hud_add_text(163,91,"FIN DE PARTIDA",1.8f,0xffffffffu);
        snprintf(buf,sizeof(buf),"PUNTOS %d",score);
        hud_add_text(157,124,buf,1.4f,0xffffdc78u);
        hud_add_text(155,158,"X VOLVER A JUGAR",1.2f,0xff80d9ffu);
    }
    hud_add_crosshair();
    if (hud_vertex_count > 0) {
        unsigned int bytes = (unsigned int)hud_vertex_count * (unsigned int)sizeof(Vertex);
        bytes = (bytes + 63u) & ~63u;
        sceKernelDcacheWritebackRange(hud_vertices, bytes);
        sceGuDrawArray(GU_TRIANGLES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                       hud_vertex_count, 0, hud_vertices);
    }
}

static void render_frame(void) {
    ScePspFVector3 eye = {0.0f,CAMERA_Y,0.0f};
    ScePspFVector3 at  = {0.0f,CAMERA_Y,-1.0f};
    ScePspFVector3 up  = {0.0f,1.0f,0.0f};
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(0xff8ec5f2u);
    sceGuClearDepth(0);
    sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
    sceGumMatrixMode(GU_PROJECTION);
    sceGumLoadIdentity();
    sceGumPerspective(FOV_DEG, 16.0f/9.0f, 0.20f, 90.0f);
    sceGumMatrixMode(GU_VIEW);
    sceGumLoadIdentity();
    sceGumLookAt(&eye, &at, &up);
    sceGumMatrixMode(GU_MODEL);
    sceGumLoadIdentity();
    draw_environment();
    draw_targets();
    draw_weapon();
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CLIP_PLANES);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    draw_hud();
    sceGuDisable(GU_BLEND);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuFinish();
    sceGuSync(0,0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int main(void) {
    int i;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    /* Make generated mesh arrays visible to the GE before the first draw. */
    sceKernelDcacheWritebackAll();
    memset(&oldpad,0,sizeof(oldpad));
    sceGuInit();
    sceGuStart(GU_DIRECT,gu_list);
    sceGuDrawBuffer(GU_PSM_8888,PSP_FRAME_A,BUF_W);
    sceGuDispBuffer(SCREEN_W,SCREEN_H,PSP_FRAME_B,BUF_W);
    sceGuDepthBuffer(PSP_DEPTH_BUFFER,BUF_W);
    sceGuOffset(2048-(SCREEN_W/2),2048-(SCREEN_H/2));
    sceGuViewport(2048,2048,SCREEN_W,SCREEN_H);
    sceGuScissor(0,0,SCREEN_W,SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuEnable(GU_DEPTH_TEST);
    sceGuDepthRange(65535,0);
    sceGuDepthFunc(GU_GEQUAL);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_CLIP_PLANES);
    sceGuDisable(GU_FOG);
    sceGuFinish();
    sceGuSync(0,0);
    sceGuDisplay(GU_TRUE);
    audio_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,AUDIO_SAMPLES,PSP_AUDIO_FORMAT_MONO);
    srand((unsigned int)sceKernelGetSystemTimeLow());
    for (i = 0; i < MAX_TARGETS; ++i) {
        targets[i].state = 0;
        targets[i].respawn = i * 13;
    }
    while (1) {
        sceCtrlReadBufferPositive(&pad,1);
        if (pad.Buttons & PSP_CTRL_TRIANGLE) break;
        if (screen_state == SCREEN_TITLE) {
            if ((pad.Buttons & ~oldpad.Buttons) & PSP_CTRL_CROSS) reset_game();
        } else if (screen_state == SCREEN_PLAY) {
            update_game();
        } else if (screen_state == SCREEN_PAUSE) {
            if ((pad.Buttons & ~oldpad.Buttons) & PSP_CTRL_START) screen_state = SCREEN_PLAY;
            if ((pad.Buttons & ~oldpad.Buttons) & PSP_CTRL_SELECT) reset_game();
        } else if (screen_state == SCREEN_OVER) {
            if ((pad.Buttons & ~oldpad.Buttons) & PSP_CTRL_CROSS) reset_game();
        }
        if (screen_state != SCREEN_PLAY) {
            if (weapon_recoil > 0.0f) weapon_recoil *= 0.75f;
            if (weapon_recoil < 0.002f) weapon_recoil = 0.0f;
        }
        render_frame();
        oldpad = pad;
    }
    if (audio_channel >= 0) sceAudioChRelease(audio_channel);
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
    sceKernelExitGame();
    return 0;
}
