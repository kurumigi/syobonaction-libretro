#include <unistd.h>
#include <SDL_libretro.h>
#include <file/file_path.h>

#define main osa_main
#include <OpenSyobonAction/main.cpp>


static void                        fallback_log(enum retro_log_level level, const char *fmt, ...);
static retro_log_printf_t          log_cb = fallback_log;
static retro_video_refresh_t       video_cb;
static retro_input_state_t         input_state_cb;
static retro_input_poll_t          input_poll_cb;
static retro_audio_sample_batch_t  audio_batch_cb;
static retro_environment_t         environ_cb;
static SDL_Thread                 *game_thread;

static void fallback_log(enum retro_log_level level, const char *fmt, ...)
{
    (void)level;
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt, va);
    va_end(va);
}

unsigned retro_api_version(void)
{
    return RETRO_API_VERSION;
}

void retro_set_environment(retro_environment_t cb)
{
    struct retro_log_callback log;
    bool t = true;
    environ_cb = cb;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log))
        log_cb = log.log;

    environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &t);
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
    video_cb = cb;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
    audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
    input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
    input_state_cb = cb;
}

void retro_get_system_info(struct retro_system_info *info)
{
    info->need_fullpath = false;
    info->valid_extensions = "";
    info->library_version = "0.3";
    info->library_name = "syobonaction";
    info->block_extract = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    int width = 480;
    int height = 420;
    info->geometry.base_width = width;
    info->geometry.base_height = height;
    info->geometry.max_width = width;
    info->geometry.max_height = height;
    info->timing.fps = 60.0;
    info->timing.sample_rate = 22050;
}


void retro_init(void)
{
    enum retro_pixel_format pixfmt = RETRO_PIXEL_FORMAT_XRGB8888;
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &pixfmt);
}

static int game_main(void *data)
{
    osa_main(0, NULL);
    return 0;
}

bool retro_load_game(const struct retro_game_info *game)
{
    char game_dir[512] = { 0 };
    const char *system_dir = NULL;
    environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir);

    fill_pathname_join_special(game_dir, system_dir, "SyobonAction", 512);
    if (!path_is_directory(game_dir) || chdir(game_dir) == -1) {
        log_cb(RETRO_LOG_ERROR, "Missing game assets directory: %s\n", game_dir);
        return false;
    }

    game_thread = SDL_CreateThread(game_main, NULL);

    struct retro_keyboard_callback keyboard = {
        .callback = SDL_libretro_KeyboardCallback,
    };
    environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &keyboard);

    return true;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
}

void retro_deinit(void)
{
}

void retro_reset(void)
{
}

static void pump_joypad_events(void)
{
    static SDL_keysym sym;
    static int16_t buttons[16] = {0};
    static const SDLKey bkeys[16] = {
        [RETRO_DEVICE_ID_JOYPAD_B]      = SDLK_SPACE,
        [RETRO_DEVICE_ID_JOYPAD_Y]      = SDLK_RETURN,
        [RETRO_DEVICE_ID_JOYPAD_SELECT] = SDLK_F1,
        [RETRO_DEVICE_ID_JOYPAD_START]  = SDLK_0,
        [RETRO_DEVICE_ID_JOYPAD_UP]     = SDLK_UP,
        [RETRO_DEVICE_ID_JOYPAD_DOWN]   = SDLK_DOWN,
        [RETRO_DEVICE_ID_JOYPAD_LEFT]   = SDLK_LEFT,
        [RETRO_DEVICE_ID_JOYPAD_RIGHT]  = SDLK_RIGHT,
        [RETRO_DEVICE_ID_JOYPAD_A]      = SDLK_z,
        [RETRO_DEVICE_ID_JOYPAD_X]      = SDLK_o,
        [RETRO_DEVICE_ID_JOYPAD_L]      = SDLK_s,
        [RETRO_DEVICE_ID_JOYPAD_R]      = SDLK_1,
        [RETRO_DEVICE_ID_JOYPAD_L2]     = SDLK_2,
        [RETRO_DEVICE_ID_JOYPAD_R2]     = SDLK_3,
        [RETRO_DEVICE_ID_JOYPAD_L3]     = SDLK_4,
        [RETRO_DEVICE_ID_JOYPAD_R3]     = SDLK_5,
    };

    for (int i = 0; i < 16; ++i) {
        int16_t state = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, i);
        SDLKey k = bkeys[i];

        if (k && buttons[i] != state) {
            buttons[i] = state;
            sym.scancode = k;
            sym.sym = k;
            SDL_PrivateKeyboard(state ? SDL_PRESSED : SDL_RELEASED, &sym);
        }
    }
}

void retro_run(void)
{
    if (SDL_GetVideoSurface() == NULL)
        return;

    input_poll_cb();
    pump_joypad_events();
    SDL_libretro_RefreshVideo(video_cb);
    SDL_libretro_ProduceAudio(audio_batch_cb);
}

size_t retro_serialize_size(void)
{
    return 0;
}

bool retro_serialize(void *data, size_t size)
{
    return false;
}

bool retro_unserialize(const void *data, size_t size)
{
    return false;
}

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) {}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info *info, size_t num_info)
{
    return false;
}

void retro_unload_game(void)
{
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
    SDL_WaitThread(game_thread, NULL);
}

unsigned retro_get_region(void)
{
    return RETRO_REGION_NTSC;
}

void *retro_get_memory_data(unsigned id)
{
    return 0;
}

size_t retro_get_memory_size(unsigned id)
{
    return 0;
}
