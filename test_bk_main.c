#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

// OMF2097 프로젝트의 헤더 파일들
#include "src/video/video.h"
#include "src/video/vga_state.h"
#include "src/resources/bk_loader.h"
#include "src/resources/bk.h"
#include "src/utils/allocator.h"
#include "src/utils/log.h"
#include "src/resources/pathmanager.h"

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 400

static int running = 1;

void cleanup() {
    video_close();
    vga_state_close();
    SDL_Quit();
    log_close();
}

void handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = 0;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                }
                break;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("사용법: %s <bk_file>\n", argv[0]);
        printf("예시: %s game_resources/ARENA0.BK\n", argv[0]);
        return 1;
    }

    const char *bk_filename = argv[1];
    
    // 로그 초기화
    if (!log_init()) {
        fprintf(stderr, "로그 초기화 실패\n");
        return 1;
    }

    // SDL 초기화
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL 초기화 실패: %s\n", SDL_GetError());
        log_close();
        return 1;
    }

    // 비디오 초기화
    video_scan_renderers();
    if (!video_init(NULL, WINDOW_WIDTH, WINDOW_HEIGHT, 0, 0, 0, 0)) {
        fprintf(stderr, "비디오 초기화 실패\n");
        cleanup();
        return 1;
    }

    // VGA 상태 초기화
    vga_state_init();

    // BK 파일 로드
    bk bk_data;
    if (load_bk_file(&bk_data, bk_filename)) {
        fprintf(stderr, "BK 파일 로드 실패: %s\n", bk_filename);
        cleanup();
        return 1;
    }

    printf("BK 파일 로드 성공: %s\n", bk_filename);
    printf("배경 크기: %dx%d\n", bk_data.background.w, bk_data.background.h);
    printf("팔레트 개수: %d\n", (int)vector_size(&bk_data.palettes));
    printf("ESC 키를 누르면 종료됩니다.\n");

    // 메인 루프
    while (running) {
        handle_events();
        
        // 렌더링 준비
        video_render_prepare(0);
        
        // BK 배경 그리기 (320x200을 640x400으로 스케일)
        video_draw_size(&bk_data.background, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        
        // 렌더링 완료
        video_render_finish();
        
        // 프레임 레이트 제한 (약 60 FPS)
        SDL_Delay(16);
    }

    // 정리
    bk_free(&bk_data);
    cleanup();
    
    printf("프로그램을 종료합니다.\n");
    return 0;
}
