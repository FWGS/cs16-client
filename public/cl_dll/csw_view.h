#pragma once 
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CS_WEB_ABI_VERSION 1

typedef struct ServoView ServoView;

typedef struct ServoViewConfig {
    uint32_t    abi_version;
    uint32_t    width, height;
    const char *url;
    const char *resources_dir;   /* unused with baked-in-resources */
    void       *native_display;  /* reserved for GPU sharing */
    void       *share_context;   /* reserved for GPU sharing */
    void      (*log)(int level, const char *msg, ...);
} ServoViewConfig;

typedef struct ServoViewFrame {
    uint32_t gl_texture;         /* 0 = software frame, use `pixels` */
    uint32_t width;
    uint32_t height;
    uint64_t frame_id;
    int32_t  flipped_y;          /* 0 = row 0 is the top of the page */
    const uint8_t *pixels;       /* RGBA8, valid until the next acquire/destroy */
    uint32_t stride;             /* bytes per row */
} ServoViewFrame;

/* All calls must come from the same thread (the engine's main/GL thread).
   Each call may make Servo's own GL context current: the caller must
   save and restore the engine's context around every call. */
uint32_t   cs_web_abi_version(void);
ServoView *cs_web_create(const ServoViewConfig *cfg);
void       cs_web_destroy(ServoView *v);
void       cs_web_resize(ServoView *v, uint32_t w, uint32_t h);
void       cs_web_load_url(ServoView *v, const char *url);
/* Returns 1 and fills `out` only when a NEW frame was produced; 0 = nothing new. */
int        cs_web_acquire(ServoView *v, ServoViewFrame *out);

void cs_web_mouse_move(ServoView *v, float x, float y);
void cs_web_mouse_button(ServoView *v, int button /*0 L,1 R,2 M*/, int down, float x, float y);
void cs_web_wheel(ServoView *v, float dx, float dy, float x, float y);
/* Keyboard. Exactly one of codepoint / special is non-zero.
   codepoint: a printable character (UTF-32), already shifted by the caller.
   special:   1 Enter, 2 Backspace, 3 Tab, 4 Escape, 5 Up, 6 Down, 7 Left, 8 Right,
              9 Home, 10 End, 11 PageUp, 12 PageDown, 13 Insert, 14 Delete.
   mods:      bit 0 Shift, bit 1 Ctrl, bit 2 Alt, bit 3 Meta. */
void cs_web_key(ServoView *v, int down, uint32_t codepoint, int special, uint32_t mods);
/* Inserts text as key presses (paste, simple IME substitute). */
void cs_web_text(ServoView *v, const char *utf8);

#ifdef __cplusplus
}
#endif