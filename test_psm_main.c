// main.c — libxmp로 PSM 디코드 + SDL2 오디오 출력 (SDL_mixer 불필요)
// 창을 만들어 키 입력(ESC) 정상 처리 + ESC/닫기 시 즉시 정지
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "xmp.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.psm>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    const int sample_rate     = 48000;  // 44100~48000 권장
    const int channels        = 2;      // S16LE stereo
    const int bytes_per_samp  = 2;      // S16LE
    const int chunk_ms        = 10;     // 낮을수록 반응성↑, CPU 사용 약간↑
    const int chunk_bytes     = sample_rate * channels * bytes_per_samp * chunk_ms / 1000;

    // 오디오 + 비디오(창) 초기화: 키 이벤트 받으려면 VIDEO 필요
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // 아주 작은 제어창(표시해야 포커스/키 이벤트가 옴)
    SDL_Window *win = SDL_CreateWindow(
        "PSM Player - Press ESC to stop",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        360, 120, SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_RaiseWindow(win); // 포커스 끌어오기

    // libxmp 컨텍스트/모듈 로드
    xmp_context ctx = xmp_create_context();
    if (!ctx) {
        fprintf(stderr, "xmp_create_context failed\n");
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    if (xmp_load_module(ctx, path) != 0) {
        fprintf(stderr, "xmp_load_module failed (unsupported or corrupt?): %s\n", path);
        xmp_free_context(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    if (xmp_start_player(ctx, sample_rate, 0) != 0) {
        fprintf(stderr, "xmp_start_player failed\n");
        xmp_release_module(ctx);
        xmp_free_context(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    // SDL 오디오 장치 (큐 방식)
    SDL_AudioSpec want = {0}, have = {0};
    want.freq = sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = (Uint8)channels;
    want.samples = (Uint16)(sample_rate * chunk_ms / 1000);
    want.callback = NULL;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        xmp_end_player(ctx);
        xmp_release_module(ctx);
        xmp_free_context(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_PauseAudioDevice(dev, 0);
    printf("Playing: %s\n(Press ESC or close the window to stop immediately)\n", path);

    uint8_t *buf = (uint8_t*)malloc((size_t)chunk_bytes);
    if (!buf) {
        fprintf(stderr, "malloc failed\n");
        SDL_PauseAudioDevice(dev, 1);
        SDL_CloseAudioDevice(dev);
        xmp_end_player(ctx);
        xmp_release_module(ctx);
        xmp_free_context(ctx);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    int running = 1, ended = 0;
    while (running && !ended) {
        // 이벤트 처리 (창 닫기/ESC)
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = 0;
        }
        if (!running) break; // 즉시 탈출

        // libxmp로 PCM 생성 (약 10ms 분량)
        int r = xmp_play_buffer(ctx, buf, chunk_bytes, 0);
        if (r != 0) { ended = 1; break; }

        // 큐가 너무 길면 잠깐 대기(반응성 확보)
        while (SDL_GetQueuedAudioSize(dev) > (unsigned)(chunk_bytes * 3)) {
            SDL_Delay(2);
        }

        if (SDL_QueueAudio(dev, buf, (Uint32)chunk_bytes) != 0) {
            fprintf(stderr, "SDL_QueueAudio failed: %s\n", SDL_GetError());
            break;
        }
    }

    // 즉시 정지: 일시정지 + 큐 비우기
    SDL_PauseAudioDevice(dev, 1);
    SDL_ClearQueuedAudio(dev);

    // 정리
    free(buf);
    SDL_CloseAudioDevice(dev);

    xmp_end_player(ctx);
    xmp_release_module(ctx);
    xmp_free_context(ctx);

    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
