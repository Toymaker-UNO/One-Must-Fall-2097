// omf_sounds_player.c
// OMF 2097 SOUNDS.DAT 분석 & 재생/덤프 (8-bit unsigned PCM, mono)
// 빌드(Windows MinGW): gcc omf_sounds_player.c -o omf_sounds_player -lSDL2
// 빌드(Linux): gcc omf_sounds_player.c -o omf_sounds_player `sdl2-config --cflags --libs`

#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>

#ifdef _WIN32
#include <direct.h>   // _mkdir
#else
#include <sys/stat.h> // mkdir(mode)
#endif


#define OFFSET_TABLE_BYTES 1200      // 300 * 4
#define OFFSET_COUNT       (OFFSET_TABLE_BYTES/4)
#define MAX_SOUNDS         (OFFSET_COUNT - 1) // 첫 항목은 0, 두번째가 1200(데이터 시작), 실사운드는 0..(MAX_SOUNDS-1)

typedef struct {
    uint32_t *offsets;  // size: OFFSET_COUNT
    uint8_t  *blob;     // entire file
    size_t    size;
    int       sample_rate; // e.g., 11025
} SoundPack;

typedef struct {
    const uint8_t *ptr;
    uint32_t       len;
    uint32_t       pos;
    int            finished;
} PlaybackState;

static PlaybackState g_state;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    if (g_state.finished || g_state.ptr == NULL) {
        memset(stream, 0x80, len); // silence for U8
        return;
    }
    uint32_t remain = g_state.len - g_state.pos;
    uint32_t to_copy = (remain < (uint32_t)len) ? remain : (uint32_t)len;
    memcpy(stream, g_state.ptr + g_state.pos, to_copy);
    if (to_copy < (uint32_t)len) {
        memset(stream + to_copy, 0x80, len - to_copy);
        g_state.finished = 1;
    }
    g_state.pos += to_copy;
}

static int read_file(const char *path, uint8_t **out_buf, size_t *out_size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror("fopen"); return -1; }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return -1; }

    uint8_t *buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(fp); return -1; }

    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        perror("fread");
        free(buf); fclose(fp); return -1;
    }
    fclose(fp);
    *out_buf = buf; *out_size = (size_t)sz;
    return 0;
}

static int load_soundpack(const char *dat_path, SoundPack *sp, int sample_rate) {
    memset(sp, 0, sizeof(*sp));
    sp->sample_rate = sample_rate;

    if (read_file(dat_path, &sp->blob, &sp->size) != 0) {
        fprintf(stderr, "Failed to read %s\n", dat_path);
        return -1;
    }
    if (sp->size < OFFSET_TABLE_BYTES) {
        fprintf(stderr, "File too small\n");
        return -1;
    }
    sp->offsets = (uint32_t*)malloc(OFFSET_TABLE_BYTES);
    if (!sp->offsets) return -1;

    // 오프셋 테이블 파싱 (리틀엔디언)
    for (int i = 0; i < OFFSET_COUNT; ++i) {
        uint32_t v = 0;
        memcpy(&v, sp->blob + i*4, 4);
        // little-endian 해석
        sp->offsets[i] = (uint32_t)(
            ((v & 0x000000FFU)      ) |
            ((v & 0x0000FF00U)      ) |
            ((v & 0x00FF0000U)      ) |
            ((v & 0xFF000000U)      )
        );
    }
    // sanity check
    if (sp->offsets[1] != OFFSET_TABLE_BYTES) {
        fprintf(stderr, "Unexpected header size: offsets[1]=%u (expected %d)\n",
                sp->offsets[1], OFFSET_TABLE_BYTES);
        // 그래도 계속 진행 (일부 변형 대비)
    }
    return 0;
}

static int get_sound_bounds(const SoundPack *sp, int index, uint32_t *start, uint32_t *len) {
    // 사용자에게 보이는 index는 0..MAX_SOUNDS-1
    if (index < 0 || index >= (int)MAX_SOUNDS) return -1;
    uint32_t s = sp->offsets[index+1];
    uint32_t e = (index+2 < OFFSET_COUNT) ? sp->offsets[index+2] : (uint32_t)sp->size;
    if (s >= e || e > sp->size) return -1;
    *start = s; *len = e - s;
    return 0;
}

static int dump_wav(const char *path, const uint8_t *pcm_u8, uint32_t len, int sample_rate) {
    // 간단한 8-bit PCM mono WAV 헤더 작성
    FILE *fp = fopen(path, "wb");
    if (!fp) { perror("fopen"); return -1; }

    uint32_t riff_size = 36 + len;
    uint16_t audio_format = 1;      // PCM
    uint16_t num_channels = 1;      // mono
    uint32_t byte_rate = (uint32_t)sample_rate * num_channels * 1; // 8-bit=1 byte
    uint16_t block_align = num_channels * 1;
    uint16_t bits_per_sample = 8;
    uint32_t data_chunk_size = len;

    // RIFF chunk
    fwrite("RIFF", 1, 4, fp);
    fwrite(&riff_size, 4, 1, fp);
    fwrite("WAVE", 1, 4, fp);

    // fmt chunk
    fwrite("fmt ", 1, 4, fp);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, fp);
    fwrite(&audio_format, 2, 1, fp);
    fwrite(&num_channels, 2, 1, fp);
    fwrite(&sample_rate, 4, 1, fp);
    fwrite(&byte_rate, 4, 1, fp);
    fwrite(&block_align, 2, 1, fp);
    fwrite(&bits_per_sample, 2, 1, fp);

    // data chunk
    fwrite("data", 1, 4, fp);
    fwrite(&data_chunk_size, 4, 1, fp);
    fwrite(pcm_u8, 1, len, fp);

    fclose(fp);
    return 0;
}

static void print_list(const SoundPack *sp) {
    for (int i = 0; i < (int)MAX_SOUNDS; ++i) {
        uint32_t st, ln;
        if (get_sound_bounds(sp, i, &st, &ln) == 0) {
            printf("%3d: start=%u length=%u bytes\n", i, st, ln);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s <SOUNDS.DAT> [--play N] [--dump OUTDIR] [--rate HZ] [--list]\n",
                argv[0]);
        return 1;
    }
    const char *dat_path = argv[1];
    int want_list = 0;
    int want_play = 0;
    int play_index = 0;
    const char *dump_dir = NULL;
    int sample_rate = 11025;

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--list") == 0) {
            want_list = 1;
        } else if (strcmp(argv[i], "--play") == 0 && i+1 < argc) {
            want_play = 1; play_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--dump") == 0 && i+1 < argc) {
            dump_dir = argv[++i];
        } else if (strcmp(argv[i], "--rate") == 0 && i+1 < argc) {
            sample_rate = atoi(argv[++i]);
        } else {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
        }
    }

    SoundPack sp;
    if (load_soundpack(dat_path, &sp, sample_rate) != 0) return 1;

    if (want_list) {
        print_list(&sp);
    }

    if (dump_dir) {
        #ifdef _WIN32
            _mkdir(dump_dir);
        #else
            char cmd[512];
            snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dump_dir);
            system(cmd);
        #endif
        for (int i = 0; i < (int)MAX_SOUNDS; ++i) {
            uint32_t st, ln;
            if (get_sound_bounds(&sp, i, &st, &ln) == 0 && ln > 0) {
                char outpath[512];
                snprintf(outpath, sizeof(outpath), "%s/sfx_%03d.wav", dump_dir, i);
                dump_wav(outpath, sp.blob + st, ln, sample_rate);
            }
        }
        printf("Dumped WAVs to %s\n", dump_dir);
    }

    if (want_play) {
        uint32_t st, ln;
        if (get_sound_bounds(&sp, play_index, &st, &ln) != 0 || ln == 0) {
            fprintf(stderr, "Invalid sound index %d\n", play_index);
            return 1;
        }
        if (SDL_Init(SDL_INIT_AUDIO) != 0) {
            fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
            return 1;
        }
        SDL_AudioSpec want, have;
        SDL_zero(want);
        want.freq = sp.sample_rate;
        want.format = AUDIO_U8; // 8-bit unsigned
        want.channels = 1;
        want.samples = 2048;
        want.callback = audio_callback;

        SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
        if (!dev) {
            fprintf(stderr, "SDL_OpenAudioDevice error: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        printf("Playing index %d (len=%u bytes) at %d Hz\n", play_index, ln, have.freq);
        g_state.ptr = sp.blob + st;
        g_state.len = ln;
        g_state.pos = 0;
        g_state.finished = 0;

        SDL_PauseAudioDevice(dev, 0);
        // 간단히 완료될 때까지 대기
        while (!g_state.finished) {
            SDL_Delay(30);
        }
        SDL_CloseAudioDevice(dev);
        SDL_Quit();
    }

    free(sp.offsets);
    free(sp.blob);
    return 0;
}
