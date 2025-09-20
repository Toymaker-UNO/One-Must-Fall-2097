// main.c — libxmp로 PSM 디코드 + SDL2 오디오 출력 (SDL_mixer 불필요)
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "xmp.h"

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "Usage: %s <file.psm>\n", argv[0]); return 1; }

    const char *path = argv[1];
    const int sample_rate = 48000;   // 44,100~48,000 권장
    const int channels    = 2;       // libxmp 기본: S16LE stereo
    const int bytes_per_samp = 2;    // S16LE
    const int chunk_ms = 40;         // 40ms 단위로 큐잉
    const int chunk_bytes = sample_rate * channels * bytes_per_samp * chunk_ms / 1000;

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // libxmp 컨텍스트/모듈 로드
    xmp_context ctx = xmp_create_context();
    if (!ctx) { fprintf(stderr, "xmp_create_context failed\n"); SDL_Quit(); return 1; }

    if (xmp_load_module(ctx, path) != 0) {
        fprintf(stderr, "xmp_load_module failed (unsupported or corrupt?): %s\n", path);
        xmp_free_context(ctx); SDL_Quit(); return 1;
    }

    if (xmp_start_player(ctx, sample_rate, 0) != 0) {
        fprintf(stderr, "xmp_start_player failed\n");
        xmp_release_module(ctx); xmp_free_context(ctx); SDL_Quit(); return 1;
    }

    // SDL 오디오 장치 열기 (S16LE 스테레오)
    SDL_AudioSpec want = {0}, have = {0};
    want.freq = sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = (Uint8)channels;
    want.samples = (Uint16)(sample_rate * chunk_ms / 1000); // 내부 버퍼 크기
    want.callback = NULL; // 큐잉 방식 사용

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        xmp_end_player(ctx); xmp_release_module(ctx); xmp_free_context(ctx); SDL_Quit(); return 1;
    }

    // 재생 시작
    SDL_PauseAudioDevice(dev, 0);
    printf("Playing: %s\n(ESC or close window to stop)\n", path);

    uint8_t *buf = (uint8_t*)malloc(chunk_bytes);
    if (!buf) { fprintf(stderr, "malloc failed\n"); }

    int running = 1, ended = 0;
    while (running && !ended) {
        // 이벤트 처리 (ESC/창닫기)
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }

        // libxmp로 PCM 생성 → SDL 큐잉
        if (!buf) { SDL_Delay(10); continue; }

        // xmp_play_buffer: 0이면 계속, !=0이면 곡 종료/오류
        int r = xmp_play_buffer(ctx, buf, chunk_bytes, 0);
        if (r != 0) { ended = 1; break; }

        // 큐에 여유가 없으면 잠깐 대기
        while (SDL_GetQueuedAudioSize(dev) > (unsigned)(chunk_bytes * 4)) {
            SDL_Delay(5);
        }
        if (SDL_QueueAudio(dev, buf, chunk_bytes) != 0) {
            fprintf(stderr, "SDL_QueueAudio failed: %s\n", SDL_GetError());
            break;
        }
    }

    // drain
    while (SDL_GetQueuedAudioSize(dev) > 0) SDL_Delay(10);

    if (buf) free(buf);
    SDL_PauseAudioDevice(dev, 1);
    SDL_CloseAudioDevice(dev);

    xmp_end_player(ctx);
    xmp_release_module(ctx);
    xmp_free_context(ctx);
    SDL_Quit();
    return 0;
}
