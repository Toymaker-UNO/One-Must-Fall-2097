// main.c — libxmp로 PSM 디코드 + SDL2 오디오 출력 (+ 진행률 표시)
// - 창을 만들어 키 입력(ESC) 정상 처리
// - ESC/닫기 시 즉시 정지 (Pause + ClearQueue)
// - 진행률: 창 타이틀 & 콘솔에 주기적으로 갱신
#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "xmp.h"

static void fmt_time(int ms, char *out, size_t cap) {
    if (ms < 0) { snprintf(out, cap, "--:--"); return; }
    int total_sec = ms / 1000;
    int mm = total_sec / 60;
    int ss = total_sec % 60;
    snprintf(out, cap, "%02d:%02d", mm, ss);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.psm>\n", argv[0]);
        return 1;
    }

    const char *path = argv[1];
    const int sample_rate     = 48000;  // 44100~48000 권장
    const int channels        = 2;      // S16LE stereo
    const int bytes_per_samp  = 2;      // S16LE
    const int chunk_ms        = 10;     // 낮을수록 반응성↑
    const int chunk_bytes     = sample_rate * channels * bytes_per_samp * chunk_ms / 1000;

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // 아주 작은 제어창(표시해야 포커스/키 이벤트가 옴)
    SDL_Window *win = SDL_CreateWindow(
        "PSM Player",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        380, 130, SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_RaiseWindow(win);

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
    printf("Playing: %s\n", path);
    printf("Press ESC or close the window to stop. Progress updates below...\n");

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

    // 진행률 표시 주기(밀리초)
    const Uint32 progress_interval_ms = 200;
    Uint32 last_progress_tick = 0;

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

        // ---- 진행률 갱신 ----
        Uint32 now = SDL_GetTicks();
        if (now - last_progress_tick >= progress_interval_ms) {
            struct xmp_frame_info fi;
            xmp_get_frame_info(ctx, &fi);  // fi.time / fi.total_time (ms)

            char t_now[16], t_total[16];
            fmt_time(fi.time, t_now, sizeof(t_now));
            fmt_time(fi.total_time, t_total, sizeof(t_total));

            int percent = 0;
            if (fi.total_time > 0 && fi.time >= 0) {
                // 반올림된 % (0~100)
                percent = (int)((fi.time * 100LL + fi.total_time / 2) / fi.total_time);
                if (percent > 100) percent = 100;
            }

            // 창 타이틀 업데이트
            char title[256];
            snprintf(title, sizeof(title), "PSM Player  [%s / %s]  %d%%  -  %s",
                     t_now, t_total, percent, path);
            SDL_SetWindowTitle(win, title);

            // 콘솔 한 줄 진행 바(간단)
            // 30칸 바 + \r 로 덮어쓰기
            char bar[64];
            int bars = (percent * 30) / 100;
            if (bars < 0) bars = 0; if (bars > 30) bars = 30;
            for (int i = 0; i < 30; ++i) bar[i] = (i < bars) ? '#' : '-';
            bar[30] = '\0';
            printf("\r[%s] %3d%%  (%s / %s)   ", bar, percent, t_now, t_total);
            fflush(stdout);

            last_progress_tick = now;
        }
    }

    // 즉시 정지: 일시정지 + 큐 비우기
    SDL_PauseAudioDevice(dev, 1);
    SDL_ClearQueuedAudio(dev);

    // 줄바꿈 정리
    printf("\n");

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
