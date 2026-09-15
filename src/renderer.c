#include "renderer.h"

#include <pspdisplay.h>
#include <pspgu.h>
#include <psprtc.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272
#define ABGR(a, b, g, r) ((unsigned int)(((a) << 24) | ((b) << 16) | ((g) << 8) | (r)))
#define WHITE ABGR(255, 255, 255, 255)
#define MUTED ABGR(255, 205, 205, 215)

static unsigned int __attribute__((aligned(16))) display_list[262144];

typedef struct {
    unsigned int color;
    short x;
    short y;
    short z;
    short padding;
} Vertex;

/*
 * The PSP GE rounds this vertex format to a 4-byte stride.
 * GU_COLOR_8888 + GU_VERTEX_16BIT therefore consumes 12 bytes per vertex,
 * not the 10 bytes occupied by the visible fields. Keep the explicit padding.
 */
typedef char vertex_stride_must_be_12[(sizeof(Vertex) == 12) ? 1 : -1];

static const unsigned int accents[] = {
    ABGR(255, 210, 92, 116), ABGR(255, 232, 111, 79),
    ABGR(255, 174, 92, 196), ABGR(255, 110, 156, 74)
};

static const unsigned char font[][5] = {
    ['0']={62,81,73,69,62},['1']={0,66,127,64,0},['2']={98,81,73,73,70},['3']={34,65,73,73,54},['4']={24,20,18,127,16},['5']={39,69,69,69,57},['6']={60,74,73,73,48},['7']={1,113,9,5,3},['8']={54,73,73,73,54},['9']={6,73,73,41,30},
    ['A']={126,17,17,17,126},['B']={127,73,73,73,54},['C']={62,65,65,65,34},['D']={127,65,65,34,28},['E']={127,73,73,73,65},['F']={127,9,9,9,1},['G']={62,65,73,73,122},['H']={127,8,8,8,127},['I']={0,65,127,65,0},['J']={32,64,65,63,1},['K']={127,8,20,34,65},['L']={127,64,64,64,64},['M']={127,2,12,2,127},['N']={127,4,8,16,127},['O']={62,65,65,65,62},['P']={127,9,9,9,6},['Q']={62,65,81,33,94},['R']={127,9,25,41,70},['S']={70,73,73,73,49},['T']={1,1,127,1,1},['U']={63,64,64,64,63},['V']={31,32,64,32,31},['W']={127,32,24,32,127},['X']={99,20,8,20,99},['Y']={3,4,120,4,3},['Z']={97,81,73,69,67},['-']={8,8,8,8,8},['.']={0,96,96,0,0},[':']={0,54,54,0,0},['/']={32,16,8,4,2},['+']={8,8,62,8,8},['!']={0,0,95,0,0},['?']={2,1,81,9,6}
};

static void rect(float x, float y, float width, float height, unsigned int color) {
    int x0, y0, x1, y1;
    Vertex *vertices;

    if (width <= 0 || height <= 0) return;
    x0 = (int)(x + 0.5f); y0 = (int)(y + 0.5f);
    x1 = (int)(x + width + 0.5f); y1 = (int)(y + height + 0.5f);
    if (x1 <= 0 || y1 <= 0 || x0 >= SCREEN_WIDTH || y0 >= SCREEN_HEIGHT) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > SCREEN_WIDTH) x1 = SCREEN_WIDTH;
    if (y1 > SCREEN_HEIGHT) y1 = SCREEN_HEIGHT;

    vertices = (Vertex *)sceGuGetMemory(2 * sizeof(Vertex));
    vertices[0] = (Vertex){color, (short)x0, (short)y0, 0, 0};
    vertices[1] = (Vertex){color, (short)x1, (short)y1, 0, 0};
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D,
                   2, NULL, vertices);
}

static void rounded_rect(float x, float y, float w, float h, float r, unsigned int color) {
    if (r < 1) { rect(x, y, w, h, color); return; }
    rect(x + r, y, w - 2 * r, h, color); rect(x, y + r, w, h - 2 * r, color);
    rect(x + 2, y + 2, r, r, color); rect(x + w - r - 2, y + 2, r, r, color);
    rect(x + 2, y + h - r - 2, r, r, color); rect(x + w - r - 2, y + h - r - 2, r, r, color);
}

static void text(float x, float y, float scale, unsigned int color, const char *value) {
    for (; *value; ++value) {
        char ch = *value;
        int column, row;
        const unsigned char *glyph;
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        if (ch == ' ') { x += 6 * scale; continue; }
        glyph = ((unsigned char)ch < 128) ? font[(unsigned char)ch] : font[(int)'?'];
        for (column = 0; column < 5; ++column)
            for (row = 0; row < 7; ++row)
                if (glyph[column] & (1 << row)) rect(x + column * scale, y + row * scale, scale, scale, color);
        x += 6 * scale;
    }
}

static float text_width(const char *value, float scale) { return strlen(value) * 6 * scale - scale; }
static void centered_text(float cx, float y, float scale, unsigned int color, const char *value) { text(cx - text_width(value, scale) / 2, y, scale, color, value); }
static unsigned int mix_color(unsigned int a, unsigned int b, int amount) {
    const int inv = 255 - amount;
    const int ar = a & 255, ag = (a >> 8) & 255, ab = (a >> 16) & 255;
    const int br = b & 255, bg = (b >> 8) & 255, bb = (b >> 16) & 255;
    return ABGR(255, (ab * inv + bb * amount) / 255,
                (ag * inv + bg * amount) / 255,
                (ar * inv + br * amount) / 255);
}

static unsigned int shade(int accent, int lightness) {
    const unsigned int base = accents[accent];
    if (lightness >= 0) return mix_color(base, WHITE, lightness);
    return mix_color(base, ABGR(255, 0, 0, 0), -lightness);
}

static unsigned int alpha_color(unsigned int color, int alpha) {
    return (color & 0x00ffffffu) | ((unsigned int)alpha << 24);
}

static void background(int accent) {
    /* Full-screen accent field. Panels provide depth; the wallpaper stays clean. */
    rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, shade(accent, -185));
}

static void chrome(const char *title, int accent) {
    ScePspDateTime now;
    char clock_text[32];

    rect(0, 0, 480, 35, alpha_color(shade(accent, -190), 238));
    rect(0, 34, 480, 1, alpha_color(shade(accent, 80), 115));
    rounded_rect(13, 8, 19, 19, 5, accents[accent]);
    rect(18,13,4,4,WHITE); rect(24,13,4,4,WHITE);
    rect(18,19,4,4,WHITE); rect(24,19,4,4,WHITE);
    text(42,11,2,WHITE,title);

    if (sceRtcGetCurrentClockLocalTime(&now) >= 0) {
        snprintf(clock_text, sizeof(clock_text), "%02d:%02d %02d/%02d/%04d",
                 now.hour, now.minute, now.day, now.month, now.year);
        text(480 - text_width(clock_text, 1) - 14, 13, 1, MUTED, clock_text);
    }
}

static void button(float x, float y, float w, float h, const char *label,
                   int primary, int accent) {
    rounded_rect(x + 1, y + 2, w, h, 6, ABGR(70,0,0,0));
    rounded_rect(x, y, w, h, 6,
                 primary ? accents[accent] : alpha_color(shade(accent, -145), 235));
    rect(x + 7, y + 1, w - 14, 1, alpha_color(WHITE, 65));
    centered_text(x + w / 2, y + h / 2 - 4, 1, WHITE, label);
}

static unsigned int tile_color(int value, int accent) {
    int level = 0;
    int v = value;
    if (value == 0) return alpha_color(shade(accent, -155), 155);
    while (v > 2 && level < 10) { v >>= 1; ++level; }
    /* Tile hierarchy stays monochromatic but gets brighter/richer with value. */
    return shade(accent, -118 + level * 20);
}

static float ease_out_cubic(float t) {
    const float q = 1.0f - t;
    return 1.0f - q * q * q;
}

static void draw_tile(float x, float y, float w, float h, int value, int accent,
                      float scale) {
    char buffer[16];
    float tw = w * scale, th = h * scale;
    float tx = x + (w - tw) * 0.5f, ty = y + (h - th) * 0.5f;
    rounded_rect(tx + 1, ty + 2, tw, th, 6, ABGR(80,0,0,0));
    rounded_rect(tx, ty, tw, th, 6, tile_color(value, accent));
    if (value && scale > 0.55f) {
        const float fs = value >= 128 ? 1.0f : 2.0f;
        snprintf(buffer, sizeof(buffer), "%d", value);
        centered_text(x + w / 2, y + (fs == 2.0f ? 8 : 12), fs, WHITE, buffer);
    }
}

static void draw_home(const GameState *game, const UiState *ui) {
    const char *items[] = {"PLAY", "SETTINGS", "CONTROLS", "ABOUT", "QUIT"};
    char buffer[24];
    int i;
    background(ui->accent);
    chrome("2048", ui->accent);
    text(30,62,3,WHITE,"2048");
    rounded_rect(28,105,276,137,10,alpha_color(shade(ui->accent,-176),235));
    for (i = 0; i < 5; ++i) {
        button(44,116 + i * 23,244,19,items[i],ui->home_selection == i,ui->accent);
    }
    rounded_rect(327,74,124,105,10,alpha_color(shade(ui->accent,-170),225));
    text(342,91,1,MUTED,"BEST");
    snprintf(buffer,sizeof(buffer),"%d",game->best_score);
    text(342,111,3,WHITE,buffer);
}

static void draw_game(const GameState *game, const UiState *ui) {
    const float tile_w = 65.0f, tile_h = 31.0f;
    int row, column;
    char buffer[32];

    background(ui->accent);
    chrome("2048", ui->accent);
    rounded_rect(14,47,321,210,10,alpha_color(shade(ui->accent,-176),235));
    rounded_rect(25,66,296,177,9,alpha_color(shade(ui->accent,-215),220));
    for (row = 0; row < 4; ++row) for (column = 0; column < 4; ++column)
        rounded_rect(31 + column * 72, 77 + row * 40, tile_w, tile_h, 6,alpha_color(shade(ui->accent,-150),125));

    if (game->animation_frame > 0 && game->animation_duration > 0) {
        const float raw = 1.0f - (float)game->animation_frame / game->animation_duration;
        const float t = ease_out_cubic(raw);
        int i;
        for (i = 0; i < game->motion_count; ++i) {
            const TileMotion *m = &game->motions[i];
            const float sx = 31 + m->from_column * 72, sy = 77 + m->from_row * 40;
            const float ex = 31 + m->to_column * 72, ey = 77 + m->to_row * 40;
            draw_tile(sx+(ex-sx)*t,sy+(ey-sy)*t,tile_w,tile_h,m->value,ui->accent,1.0f);
        }
    } else {
        for (row = 0; row < 4; ++row) for (column = 0; column < 4; ++column) {
            const int value = game->cells[row][column]; float scale = 1.0f;
            if (!value) continue;
            if (game->spawn_frame > 0) {
                int i; const float settle = (float)game->spawn_frame / 7.0f;
                for (i=0;i<game->motion_count;++i) if(game->motions[i].merged && game->motions[i].to_row==row && game->motions[i].to_column==column){scale=1.0f+0.13f*settle;break;}
                if(row==game->spawn_row && column==game->spawn_column){const float t=1.0f-(float)game->spawn_frame/7.0f;scale=t<0.65f?0.65f+t*0.70f:1.10f-(t-0.65f)*0.2857f;}
            }
            draw_tile(31+column*72,77+row*40,tile_w,tile_h,value,ui->accent,scale);
        }
    }

    rounded_rect(346,47,120,210,10,alpha_color(shade(ui->accent,-170),235));
    text(360,66,1,MUTED,"SCORE"); snprintf(buffer,sizeof(buffer),"%d",game->score); text(360,82,2,WHITE,buffer);
    text(360,116,1,MUTED,"BEST"); snprintf(buffer,sizeof(buffer),"%d",game->best_score); text(360,132,2,shade(ui->accent,115),buffer);

    if (!game_is_animating(game) && (game->paused || game->won || game_is_over(game))) {
        rect(14,47,321,210,ABGR(165,0,0,0));
        rounded_rect(68,78,212,157,10,shade(ui->accent,-145));
        if (game->paused) {
            centered_text(174,91,2,WHITE,"PAUSED");
            if (ui->pause_confirm_quit) {
                centered_text(174,132,1,WHITE,"QUIT TO MAIN MENU?");
                button(91,163,76,24,"NO",ui->quit_selection==0,ui->accent);
                button(181,163,76,24,"YES",ui->quit_selection==1,ui->accent);
            } else {
                button(94,126,160,20,"CONTINUE",ui->pause_selection==0,ui->accent);
                button(94,150,160,20,"NEW GAME",ui->pause_selection==1,ui->accent);
                button(94,174,160,20,"UNDO",ui->pause_selection==2,ui->accent);
                button(94,198,160,20,"QUIT",ui->pause_selection==3,ui->accent);
            }
        } else if(game->won){centered_text(174,119,2,WHITE,"2048"); button(94,158,160,24,"CONTINUE",1,ui->accent);}
        else {centered_text(174,119,2,WHITE,"GAME OVER"); button(94,158,160,24,"NEW GAME",1,ui->accent);}
    }
}

static void draw_settings(const GameState *game, const UiState *ui) {
    const char *items[]={"ACCENT COLOR","SOUND","RESET BEST SCORE"}; int i; (void)game;
    background(ui->accent); chrome("SETTINGS",ui->accent); rounded_rect(38,58,404,172,10,alpha_color(shade(ui->accent,-176),235));
    for(i=0;i<3;++i){ if(i==ui->settings_selection) rounded_rect(54,83+i*40,370,31,6,alpha_color(shade(ui->accent,-85),210)); text(68,94+i*40,1,i==ui->settings_selection?WHITE:MUTED,items[i]); if(i==0){rounded_rect(365,90,16,16,5,accents[ui->accent]);} if(i==1) text(365,94+i*40,1,WHITE,ui->sound_enabled?"ON":"OFF"); }
}

static void draw_controls(const UiState *ui) {
    background(ui->accent); chrome("CONTROLS",ui->accent);
    rounded_rect(34,57,412,177,18,alpha_color(shade(ui->accent,-176),235));
    /* Vector PSP silhouette. */
    rounded_rect(66,82,348,116,30,shade(ui->accent,-205));
    rounded_rect(147,94,186,91,4,shade(ui->accent,-235));
    rounded_rect(155,101,170,77,2,shade(ui->accent,-120));
    /* D-pad. */
    rect(91,118,42,14,shade(ui->accent,70)); rect(105,104,14,42,shade(ui->accent,70));
    /* Face buttons. */
    rounded_rect(361,109,16,16,8,shade(ui->accent,70)); rounded_rect(379,127,16,16,8,shade(ui->accent,70));
    rounded_rect(343,127,16,16,8,shade(ui->accent,70)); rounded_rect(361,145,16,16,8,shade(ui->accent,70));
    /* Analog and Start. */
    rounded_rect(88,157,30,30,15,shade(ui->accent,-80)); rounded_rect(268,187,35,5,2,shade(ui->accent,60));
    text(48,207,1,WHITE,"D-PAD / ANALOG   MOVE"); text(204,207,1,WHITE,"X   SELECT");
    text(48,221,1,WHITE,"O   BACK"); text(204,221,1,WHITE,"START   PAUSE");
}

static void draw_about(const UiState *ui) {
    background(ui->accent); chrome("ABOUT",ui->accent);
    rounded_rect(55,62,370,164,10,alpha_color(shade(ui->accent,-176),235));
    text(75,79,3,WHITE,"2048");
    text(75,115,1,WHITE,"VERSION 1.0.0");
    text(75,131,1,MUTED,"BUILD: " __DATE__);
    text(75,147,1,WHITE,"AUTHOR: MISS VIOLIN MELODY");
    text(75,163,1,WHITE,"HTTPS://VIOLINMELODY.NET");
    text(75,187,1,MUTED,"BUILT WITH PSPDEV / PSPSDK");
    text(75,203,1,MUTED,"NOT AFFILIATED WITH SONY.");
}

static void draw_quit_confirm(const UiState *ui) {
    background(ui->accent); chrome("2048",ui->accent);
    rounded_rect(108,86,264,104,10,alpha_color(shade(ui->accent,-176),245));
    centered_text(240,105,2,WHITE,"QUIT GAME?");
    button(139,145,90,26,"NO",ui->quit_selection==0,ui->accent);
    button(251,145,90,26,"YES",ui->quit_selection==1,ui->accent);
}

void renderer_init(void) {
    sceGuInit(); sceGuStart(GU_DIRECT,display_list); sceGuDrawBuffer(GU_PSM_8888,(void *)0,512); sceGuDispBuffer(480,272,(void *)0x88000,512);
    sceGuOffset(2048-240,2048-136); sceGuViewport(2048,2048,480,272); sceGuScissor(0,0,480,272); sceGuEnable(GU_SCISSOR_TEST); sceGuDisable(GU_DEPTH_TEST); sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND); sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0); sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); sceGuDisplay(GU_TRUE);
}

void renderer_shutdown(void) { sceGuTerm(); }

void renderer_draw(const GameState *game, const UiState *ui) {
    sceGuStart(GU_DIRECT,display_list); sceGuClearColor(shade(ui->accent,-230)); sceGuClear(GU_COLOR_BUFFER_BIT);
    switch(ui->screen){ case SCREEN_HOME: draw_home(game,ui); break; case SCREEN_GAME: draw_game(game,ui); break; case SCREEN_SETTINGS: draw_settings(game,ui); break; case SCREEN_CONTROLS: draw_controls(ui); break; case SCREEN_ABOUT: draw_about(ui); break; case SCREEN_QUIT_CONFIRM: draw_quit_confirm(ui); break; }
    sceGuFinish(); sceGuSync(0,0); sceDisplayWaitVblankStart(); sceGuSwapBuffers();
}
