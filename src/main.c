#include <raylib.h>

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

#include "res.h"

#define RAYLIB_LOG_LEVEL LOG_ERROR

#ifndef DRAW_HITBOX
#define DRAW_HITBOX 0
#endif

#ifndef DRAW_TEXTURE
#define DRAW_TEXTURE 1
#endif

#ifndef PLAY_SOUND
#define PLAY_SOUND 1
#endif

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 854

#define HITBOX_LINE_THICKNESS 2

#define BOUNDARY_TOP    0
#define BOUNDARY_BOTTOM 100

#define OBSTACLE_WIDTH   80
#define OBSTACLE_HEIGHT  510
#define OBSTACLE_PADDING 80
#define OBSTACLE_MARGIN  180
#define OBSTACLE_SPEED   170
#define OBSTACLE_FRAC    5

#define BIRD_HIT_RADIUS     20
#define BIRD_JUMP_FORCE     510
#define BIRD_ROTATION_SPEED 100
#define BIRD_ROTATION_MIN   60 /* will be cast to negative */
#define BIRD_ROTATION_MAX   60
#define BIRD_INITIAL_SCORE  0

#define FLASH_INITIAL_ALPHA 0.8
#define FLASH_DECAY_SPEED   17.0f

#define SIMULATION_GRAVITY 1500

typedef enum {
    INTRO,
    PLAY,
    OVER,
} GameMode;

typedef struct {
    Rectangle hitBoxTop;
    Rectangle hitBoxBottom;
    float offset;
    float distance;
    int passed;
} Obstacle;

typedef struct {
    Vector2 hitCircleCenter;
    float velocity;
    float rotation;
} Bird;

typedef struct {
    Texture2D background;
    Texture2D base;
    Texture2D pipe;

    Texture2D gameOver;
    Texture2D intro;
    Texture2D digit[10];

    Texture2D birdFlapUp;
    Texture2D birdFlapMid;
    Texture2D birdFlapDown;
} Textures;

typedef struct {
    Sound flap;
    Sound point;
    Sound hit;
} Sounds;

typedef struct {
    Camera2D camera;

    GameMode mode;
    unsigned int score;

    Rectangle backgrounds[2];
    Rectangle bases[2];
    float flashIntensity;

    Obstacle obstacles[2];
    Bird bird;

    Textures textures;
    Sounds sounds;
} Game;

void GameLoad(Game* game);
void GameUnload(Game* game);

void GameReset(Game* game);
void GameIntroDraw(Game* game);
void GameIntroUpdate(Game* game, float frameTime);
void GamePlayDraw(Game* game);
void GamePlayUpdate(Game* game, float frameTime);
void GameOverDraw(Game* game);
void GameOverUpdate(Game* game, float frameTime);

void FrameUpdateDraw(void);

Game game;

int main(void) {
    SetTraceLogLevel(RAYLIB_LOG_LEVEL);

    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Flappy Bird");
    InitAudioDevice();

    GameLoad(&game);
    GameReset(&game);
    game.mode = INTRO;

#ifdef PLATFORM_WEB
    emscripten_set_main_loop(FrameUpdateDraw, 0, 1);
#else
    for (; !WindowShouldClose();) {
        FrameUpdateDraw();
    }
#endif

    GameUnload(&game);

    CloseAudioDevice();
    CloseWindow();

    return 0;
}

void FrameUpdateDraw(void) {
    float frameTime = GetFrameTime();
    switch (game.mode) {
        case INTRO:
            GameIntroUpdate(&game, frameTime);
            break;
        case PLAY:
            GamePlayUpdate(&game, frameTime);
            break;
        case OVER:
            GameOverUpdate(&game, frameTime);
            break;
        default:
            break;
    }

    BeginDrawing();
    {
        ClearBackground(SKYBLUE);
        BeginMode2D(game.camera);
        switch (game.mode) {
            case INTRO:
                GameIntroDraw(&game);
                break;
            case PLAY:
                GamePlayDraw(&game);
                break;
            case OVER:
                GameOverDraw(&game);
                break;
            default:
                break;
        }
        EndMode2D();
    }
    EndDrawing();
}

int IsInputReceived(Sounds* sounds) {
    int val = IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (val) {
#if PLAY_SOUND
        PlaySound(sounds->flap);
#else
        (void)sounds;
#endif
    }

    return val;
}

void BirdDraw(Bird* bird, Textures* textures) {
#if DRAW_TEXTURE
    Texture2D* texture = &textures->birdFlapMid;
    if (bird->rotation <= -10.0f) {
        texture = &textures->birdFlapDown;
    } else if (bird->rotation >= 5.0f) {
        texture = &textures->birdFlapUp;
    }

    Rectangle adjust = (Rectangle){-5.0f, 0.0f, 15.0f, 0.0f};
    DrawTexturePro(
        *texture,
        (Rectangle){0.0f, 0.0f, texture->width, texture->height},
        (Rectangle){
            bird->hitCircleCenter.x + adjust.x,
            bird->hitCircleCenter.y + adjust.y,
            (float)BIRD_HIT_RADIUS * 2.0f + adjust.width,
            (float)BIRD_HIT_RADIUS * 2.0f + adjust.height,
        },
        (Vector2){(float)BIRD_HIT_RADIUS - (adjust.x / 2.0f), BIRD_HIT_RADIUS + adjust.y},
        bird->rotation,
        WHITE
    );
#else
    (void)textures;
#endif

#if DRAW_HITBOX
    DrawRing(
        bird->hitCircleCenter,
        (float)(BIRD_HIT_RADIUS - HITBOX_LINE_THICKNESS),
        (float)BIRD_HIT_RADIUS,
        0.0f,
        360.0f,
        36,
        RED
    );
#endif
}

void BirdUpdate(Bird* bird, int jump, float frameTime) {
    if (jump) {
        bird->velocity = -(float)BIRD_JUMP_FORCE;
        bird->rotation = -(float)BIRD_ROTATION_MIN;
    } else {
        bird->velocity += (float)SIMULATION_GRAVITY * frameTime;
        if (bird->rotation < (float)BIRD_ROTATION_MAX) {
            bird->rotation += (float)BIRD_ROTATION_SPEED * frameTime;
        }
    }

    bird->hitCircleCenter.y += bird->velocity * frameTime;
}

int BirdIsCollide(Bird* bird, Obstacle* obstacles, int obstacleCount, Sounds* sounds) {
    int isCollide = 0;
    if (bird->hitCircleCenter.y >= (float)(SCREEN_HEIGHT - (BIRD_HIT_RADIUS + BOUNDARY_BOTTOM))) {
        isCollide = 1;
        bird->hitCircleCenter.y = (float)(SCREEN_HEIGHT - (BIRD_HIT_RADIUS + BOUNDARY_BOTTOM));
    }
    if (bird->hitCircleCenter.y <= (float)(BIRD_HIT_RADIUS + BOUNDARY_TOP)) {
        isCollide = 1;
        bird->hitCircleCenter.y = (float)(BIRD_HIT_RADIUS + BOUNDARY_TOP);
    }
    for (int i = 0; isCollide == 0 && i < obstacleCount; i++) {
        if (CheckCollisionCircleRec(bird->hitCircleCenter, (float)BIRD_HIT_RADIUS, obstacles[i].hitBoxTop)) {
            isCollide = 1;
            break;
        }
        if (CheckCollisionCircleRec(bird->hitCircleCenter, (float)BIRD_HIT_RADIUS, obstacles[i].hitBoxBottom)) {
            isCollide = 1;
            break;
        }
    }

    if (isCollide) {
#if PLAY_SOUND
        PlaySound(sounds->hit);
#else
        (void)sounds;
#endif
    }

    return isCollide;
}

unsigned int BirdIsPassed(Bird* bird, Obstacle* obstacles, int obstacleCount, Sounds* sounds) {
    unsigned int passCount = 0;

    for (int i = 0; i < obstacleCount; i++) {
        if (obstacles[i].passed) {
            continue;
        }
        if (bird->hitCircleCenter.x - (float)BIRD_HIT_RADIUS <= obstacles[i].distance + (float)OBSTACLE_WIDTH) {
            continue;
        }

        obstacles[i].passed = 1;
        passCount++;
    }
    if (passCount) {
#if PLAY_SOUND
        PlaySound(sounds->point);
#else
        (void)sounds;
#endif
    }

    return passCount;
}

void ObstacleDraw(Obstacle* obstacle, Textures* textures) {
    obstacle->hitBoxTop = (Rectangle){
        obstacle->distance,
        (float)BOUNDARY_TOP,
        (float)OBSTACLE_WIDTH,
        obstacle->offset,
    };
    obstacle->hitBoxBottom = (Rectangle){
        obstacle->distance,
        (float)(BOUNDARY_TOP + obstacle->offset + OBSTACLE_MARGIN),
        (float)OBSTACLE_WIDTH,
        (float)((SCREEN_HEIGHT - BOUNDARY_BOTTOM) - (obstacle->offset + OBSTACLE_MARGIN)),
    };

#if DRAW_TEXTURE
    DrawTexturePro(
        textures->pipe,
        (Rectangle){0.0f, 0.0f, -(float)textures->pipe.width, (float)textures->pipe.height},
        (Rectangle){
            obstacle->hitBoxTop.x + (float)OBSTACLE_WIDTH,
            obstacle->hitBoxTop.y + obstacle->offset,
            obstacle->hitBoxTop.width + 1,
            (float)OBSTACLE_HEIGHT,
        },
        (Vector2){0.0f, 0.0f},
        180.0f,
        WHITE
    );
    DrawTexturePro(
        textures->pipe,
        (Rectangle){
            0.0f,
            0.0f,
            (float)textures->pipe.width,
            (float)textures->pipe.height,
        },
        (Rectangle){
            obstacle->hitBoxBottom.x,
            obstacle->hitBoxBottom.y,
            obstacle->hitBoxBottom.width + 1,
            (float)OBSTACLE_HEIGHT,
        },
        (Vector2){0.0f, 0.0f},
        0.0f,
        WHITE
    );
#else
    (void)textures;
#endif

#if DRAW_HITBOX
    DrawRectangleLinesEx(obstacle->hitBoxTop, (float)HITBOX_LINE_THICKNESS, RED);
    DrawRectangleLinesEx(obstacle->hitBoxBottom, (float)HITBOX_LINE_THICKNESS, RED);
#endif
}

float GetNextOffset(int step) {
    float area = (SCREEN_HEIGHT - (BOUNDARY_TOP + BOUNDARY_BOTTOM + OBSTACLE_PADDING + OBSTACLE_MARGIN));
    return ((area / (float)(OBSTACLE_FRAC + 1)) * (float)step) + (BOUNDARY_TOP + OBSTACLE_PADDING);
}

void ObstacleUpdate(Obstacle* obstacle, float frameTime) {
    float nextDistance = obstacle->distance - (frameTime * OBSTACLE_SPEED);
    float nextOffset = obstacle->offset;

    if (nextDistance <= -(float)OBSTACLE_WIDTH) {
        nextDistance = (float)SCREEN_WIDTH;
        nextOffset = GetNextOffset(GetRandomValue(0, OBSTACLE_FRAC));
        obstacle->passed = 0;
    }

    obstacle->distance = nextDistance;
    obstacle->offset = nextOffset;
}

void BaseDraw(Rectangle* bases, int baseCount, Textures* textures) {
    for (int i = 0; i < baseCount; i++) {
#if DRAW_TEXTURE
        DrawTexturePro(
            textures->base,
            (Rectangle){0.0f, 0.0f, (float)textures->base.width, (float)textures->base.height},
            (Rectangle){bases[i].x, bases[i].y, bases[i].width + 1, bases[i].height},
            (Vector2){0.0f, 0.0f},
            0.0f,
            WHITE
        );
#else
        (void)bases;
        (void)textures;
#endif
    }

#if DRAW_HITBOX
    DrawLineEx(
        (Vector2){0.0f, SCREEN_HEIGHT - BOUNDARY_BOTTOM},
        (Vector2){SCREEN_WIDTH, SCREEN_HEIGHT - BOUNDARY_BOTTOM},
        (float)HITBOX_LINE_THICKNESS,
        RED
    );
#endif
}

void BaseUpdate(Rectangle* bases, int baseCount, float frameTime) {
    for (int i = 0; i < baseCount; i++) {
        float nextX = bases[i].x - (frameTime * (float)OBSTACLE_SPEED);
        if (nextX <= -(float)SCREEN_WIDTH) {
            nextX = (float)SCREEN_WIDTH;
        }

        bases[i].x = nextX;
    }
}

void BackgroundDraw(Rectangle* backgrounds, int backgroundCount, Textures* textures) {
    for (int i = 0; i < backgroundCount; i++) {
#if DRAW_TEXTURE
        DrawTexturePro(
            textures->background,
            (Rectangle){0.0f, 0.0f, textures->background.width, textures->background.height},
            (Rectangle){backgrounds[i].x, backgrounds[i].y, backgrounds[i].width + 1, backgrounds[i].height},
            (Vector2){0.0f, 0.0f},
            0.0f,
            WHITE
        );
#else
        (void)backgrounds;
        (void)textures;
#endif
    }

#if DRAW_HITBOX
    DrawLineEx(
        (Vector2){0.0f, BOUNDARY_TOP + HITBOX_LINE_THICKNESS},
        (Vector2){SCREEN_WIDTH, BOUNDARY_TOP + HITBOX_LINE_THICKNESS},
        (float)HITBOX_LINE_THICKNESS,
        RED
    );
#endif
}

void BackgroundUpdate(Rectangle* backgrounds, int backgroundCount, float frameTime) {
    for (int i = 0; i < backgroundCount; i++) {
        float nextX = backgrounds[i].x - (frameTime * ((float)OBSTACLE_SPEED / 4.0f));
        if (nextX <= -(float)SCREEN_WIDTH) {
            nextX = (float)SCREEN_WIDTH;
        }

        backgrounds[i].x = nextX;
    }
}

void GameReset(Game* game) {
    game->camera = (Camera2D){0};
    game->camera.zoom = 1.0f;

    game->backgrounds[0] = (Rectangle){
        0.0f,
        0.0f,
        (float)SCREEN_WIDTH,
        (float)(SCREEN_HEIGHT - BOUNDARY_BOTTOM),
    };
    game->backgrounds[1] = (Rectangle){
        (float)SCREEN_WIDTH,
        0.0f,
        (float)SCREEN_WIDTH,
        (float)(SCREEN_HEIGHT - BOUNDARY_BOTTOM),
    };

    game->bases[0] = (Rectangle){
        0.0f,
        (float)(SCREEN_HEIGHT - BOUNDARY_BOTTOM),
        (float)SCREEN_WIDTH,
        (float)BOUNDARY_BOTTOM,
    };
    game->bases[1] = (Rectangle){
        (float)SCREEN_WIDTH,
        (float)(SCREEN_HEIGHT - BOUNDARY_BOTTOM),
        (float)SCREEN_WIDTH,
        (float)BOUNDARY_BOTTOM,
    };

    game->obstacles[0] = (Obstacle){0};
    game->obstacles[0].offset = GetNextOffset(OBSTACLE_FRAC / 2 + 1);
    game->obstacles[0].distance = (float)SCREEN_WIDTH;
    game->obstacles[1] = (Obstacle){0};
    game->obstacles[1].offset = GetNextOffset(OBSTACLE_FRAC / 2 + 1);
    game->obstacles[1].distance = (float)SCREEN_WIDTH + ((float)(SCREEN_WIDTH + OBSTACLE_WIDTH) / 2.0f);

    game->bird = (Bird){0};
    game->bird.hitCircleCenter = (Vector2){(float)SCREEN_WIDTH / 2.0f, (float)SCREEN_HEIGHT / 2.0f};

    game->score = BIRD_INITIAL_SCORE;
}

void ScoreDraw(unsigned int score, Vector2 pos, Textures* textures) {
    unsigned int base = score % 10;
#if DRAW_TEXTURE
    Vector2 size = (Vector2){20.0f, 35.0f};
    DrawTexturePro(
        textures->digit[base],
        (Rectangle){
            0.0f,
            0.0f,
            textures->digit[base].width,
            textures->digit[base].height,
        },
        (Rectangle){(SCREEN_WIDTH - (pos.x + size.x)), pos.y, size.x, size.y},
        (Vector2){0.0f, 0.0f},
        0.0f,
        WHITE
    );
#else
    (void)pos;
    (void)textures;
#endif

    int digit = 2;
    for (score /= 10; score > 0; digit++, score /= 10) {
        base = score % 10;
#if DRAW_TEXTURE
        DrawTexturePro(
            textures->digit[base],
            (Rectangle){
                0.0f,
                0.0f,
                textures->digit[base].width,
                textures->digit[base].height,
            },
            (Rectangle){(SCREEN_WIDTH - (pos.x + (digit * (size.x + 0.5f)))), pos.y, size.x, size.y},
            (Vector2){0.0f, 0.0f},
            0.0f,
            WHITE
        );
#else
        (void)digit;
#endif
    }
}

void GameIntroDraw(Game* game) {
    BackgroundDraw(game->backgrounds, 2, &game->textures);
    BaseDraw(game->bases, 2, &game->textures);

#if DRAW_TEXTURE
    float marginTop = 30.0f;
    Vector2 padding = (Vector2){100.0f, 160.0f};
    DrawTexturePro(
        game->textures.intro,
        (Rectangle){0.0f, 0.0f, game->textures.intro.width, game->textures.intro.height},
        (Rectangle){
            padding.x,
            BOUNDARY_TOP + padding.y + marginTop,
            SCREEN_WIDTH - (2 * padding.x),
            SCREEN_HEIGHT - (BOUNDARY_TOP + BOUNDARY_BOTTOM + (2 * padding.y)),
        },
        (Vector2){0.0f, 0.0f},
        0.0f,
        WHITE
    );
#endif

    ScoreDraw(game->score, (Vector2){10.0f, 10.0f}, &game->textures);
}

void GameIntroUpdate(Game* game, float frameTime) {
    if (!IsInputReceived(&game->sounds)) {
        return;
    }

    BirdUpdate(&game->bird, 1, frameTime);
    game->mode = PLAY;
}

void GamePlayDraw(Game* game) {
    BackgroundDraw(game->backgrounds, 2, &game->textures);
    ObstacleDraw(&game->obstacles[0], &game->textures);
    ObstacleDraw(&game->obstacles[1], &game->textures);
    BaseDraw(game->bases, 2, &game->textures);

    BirdDraw(&game->bird, &game->textures);

    ScoreDraw(game->score, (Vector2){10.0f, 10.0f}, &game->textures);
}

void GamePlayUpdate(Game* game, float frameTime) {
    BackgroundUpdate(game->backgrounds, 2, frameTime);
    BaseUpdate(game->bases, 2, frameTime);

    ObstacleUpdate(&game->obstacles[0], frameTime);
    ObstacleUpdate(&game->obstacles[1], frameTime);

    BirdUpdate(&game->bird, IsInputReceived(&game->sounds), frameTime);
    game->score += BirdIsPassed(&game->bird, game->obstacles, 2, &game->sounds);
    if (BirdIsCollide(&game->bird, game->obstacles, 2, &game->sounds)) {
        game->flashIntensity = FLASH_INITIAL_ALPHA;
        game->mode = OVER;
    }
}

void GameOverDraw(Game* game) {
    GamePlayDraw(game);

#if DRAW_TEXTURE
    DrawRectangle(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(WHITE, game->flashIntensity));

    float marginTop = 120.0f;
    Vector2 size = (Vector2){300.0f, 70.0f};
    DrawTexturePro(
        game->textures.gameOver,
        (Rectangle){0.0f, 0.0f, game->textures.gameOver.width, game->textures.gameOver.height},
        (Rectangle){(SCREEN_WIDTH / 2.0f) - (size.x / 2.0f), marginTop, size.x, size.y},
        (Vector2){0.0f, 0.0f},
        0.0f,
        WHITE
    );
#endif
}

void GameOverUpdate(Game* game, float frameTime) {
    if (game->flashIntensity >= 0.0f) {
        game->flashIntensity -= FLASH_DECAY_SPEED * frameTime;
    } else {
        game->flashIntensity = 0.0f;
    }

    if (!IsInputReceived(&game->sounds)) {
        return;
    }

    GameReset(game);
    BirdUpdate(&game->bird, 1, frameTime);
    game->mode = PLAY;
}

void TextureFromPngMemory(Texture2D* texture, const unsigned char* data, int length) {
    Image image = LoadImageFromMemory(".png", data, length);
    *texture = LoadTextureFromImage(image);
    UnloadImage(image);
}

void SoundFromWavMemory(Sound* sound, const unsigned char* data, int length) {
    Wave wave = LoadWaveFromMemory(".wav", data, length);
    *sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
}

void GameLoad(Game* game) {
    *game = (Game){0};

    TextureFromPngMemory(&game->textures.background, res_textures_bg_png, res_textures_bg_png_len);
    TextureFromPngMemory(&game->textures.base, res_textures_base_png, res_textures_base_png_len);
    TextureFromPngMemory(&game->textures.pipe, res_textures_pipe_png, res_textures_pipe_png_len);

    TextureFromPngMemory(&game->textures.gameOver, res_textures_over_png, res_textures_over_png_len);
    TextureFromPngMemory(&game->textures.intro, res_textures_intro_png, res_textures_intro_png_len);
    TextureFromPngMemory(&game->textures.digit[0], res_textures_0_png, res_textures_0_png_len);
    TextureFromPngMemory(&game->textures.digit[1], res_textures_1_png, res_textures_1_png_len);
    TextureFromPngMemory(&game->textures.digit[2], res_textures_2_png, res_textures_2_png_len);
    TextureFromPngMemory(&game->textures.digit[3], res_textures_3_png, res_textures_3_png_len);
    TextureFromPngMemory(&game->textures.digit[4], res_textures_4_png, res_textures_4_png_len);
    TextureFromPngMemory(&game->textures.digit[5], res_textures_5_png, res_textures_5_png_len);
    TextureFromPngMemory(&game->textures.digit[6], res_textures_6_png, res_textures_6_png_len);
    TextureFromPngMemory(&game->textures.digit[7], res_textures_7_png, res_textures_7_png_len);
    TextureFromPngMemory(&game->textures.digit[8], res_textures_8_png, res_textures_8_png_len);
    TextureFromPngMemory(&game->textures.digit[9], res_textures_9_png, res_textures_9_png_len);

    TextureFromPngMemory(&game->textures.birdFlapDown, res_textures_bird_d_png, res_textures_bird_d_png_len);
    TextureFromPngMemory(&game->textures.birdFlapMid, res_textures_bird_m_png, res_textures_bird_m_png_len);
    TextureFromPngMemory(&game->textures.birdFlapUp, res_textures_bird_u_png, res_textures_bird_u_png_len);

    SoundFromWavMemory(&game->sounds.flap, res_sounds_flap_wav, res_sounds_flap_wav_len);
    SoundFromWavMemory(&game->sounds.point, res_sounds_point_wav, res_sounds_point_wav_len);
    SoundFromWavMemory(&game->sounds.hit, res_sounds_hit_wav, res_sounds_hit_wav_len);
}

void GameUnload(Game* game) {
    UnloadTexture(game->textures.background);
    UnloadTexture(game->textures.base);
    UnloadTexture(game->textures.pipe);

    UnloadTexture(game->textures.gameOver);
    UnloadTexture(game->textures.intro);
    UnloadTexture(game->textures.digit[0]);
    UnloadTexture(game->textures.digit[1]);
    UnloadTexture(game->textures.digit[2]);
    UnloadTexture(game->textures.digit[3]);
    UnloadTexture(game->textures.digit[4]);
    UnloadTexture(game->textures.digit[5]);
    UnloadTexture(game->textures.digit[6]);
    UnloadTexture(game->textures.digit[7]);
    UnloadTexture(game->textures.digit[8]);
    UnloadTexture(game->textures.digit[9]);

    UnloadTexture(game->textures.birdFlapUp);
    UnloadTexture(game->textures.birdFlapMid);
    UnloadTexture(game->textures.birdFlapDown);

    UnloadSound(game->sounds.flap);
    UnloadSound(game->sounds.point);
    UnloadSound(game->sounds.hit);
}
