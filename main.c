#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL_mixer.h>
#define WIN_WIDTH 900
#define WIN_HEIGHT 600
#define COLOR_WHITE 0xFFFFFFFF
#define MAP_WIDTH 9
#define MAP_HEIGHT 9
#define CELL_SIZE 64
#define FOV 0.66
#define MOV_SPEED 3
#define ROT_SPEED 2 

// Texture Definitions
#define TEX_WIDTH 64
#define TEX_HEIGHT 64
#define NUM_TEXTURES 5 // 0 = unused, 1-4 match our block layouts

uint32_t textures[NUM_TEXTURES][TEX_HEIGHT][TEX_WIDTH];

typedef enum {
    STATE_PLAYING,
    STATE_GAMEOVER,
    STATE_VICTORY
} GameState;

bool running = true;
GameState current_state = STATE_PLAYING;

int map[MAP_HEIGHT][MAP_WIDTH]; 

typedef struct {
    double x;
    double y;
} Vector2D;

typedef struct {
    Vector2D pos;
    Vector2D dir;
    Vector2D camera_plane;
} Player;

typedef struct {
    int id;
    int max_peeks;
    double spawn_x;
    double spawn_y;
    int win_x;            
    int win_y;            
    bool completed;       
    int map_layout[MAP_HEIGHT][MAP_WIDTH];
} Level;

int target_layer = 0;
int peek_charges = 0;
bool is_peeking = false;
bool m_key_released = true; 

Level level_database[] = {
    {
        .id = 1,
        .max_peeks = 3,
        .spawn_x = 3.5,
        .spawn_y = 2.5,
        .win_x = 4,
        .win_y = 5,
        .completed = false,
        .map_layout = {
            {1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 2, 0, 0, 1, 0, 0, 3, 1},
            {1, 0, 0, 0, 1, 0, 1, 0, 1},
            {1, 0, 3, 0, 0, 0, 1, 0, 1},
            {1, 0, 3, 3, 1, 1, 1, 0, 1},
            {1, 0, 0, 0, 4, 0, 0, 0, 1},
            {1, 1, 1, 0, 1, 0, 1, 1, 1},
            {1, 0, 0, 0, 1, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1}
        }
    },
    {
        .id = 2,
        .max_peeks = 1, 
        .spawn_x = 4.5,
        .spawn_y = 7.5,
        .win_x = 4,
        .win_y = 4,
        .completed = false,
        .map_layout = {
            {1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 2, 2, 2, 2, 2, 0, 1},
            {1, 0, 2, 0, 0, 0, 2, 0, 1},
            {1, 0, 2, 0, 4, 0, 2, 0, 1}, 
            {1, 0, 2, 0, 0, 0, 2, 0, 1},
            {1, 0, 2, 2, 2, 2, 2, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 0, 1},
            {1, 1, 1, 1, 1, 1, 1, 1, 1}
        }
    }
};

int current_level_idx = 0;

int get_tile_at_position(double x, double y) {
    int mapX = (int)x;
    int mapY = (int)y;
    if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
        return -1; 
    }
    return map[mapY][mapX];
}

void generate_textures() {
    for (int y = 0; y < TEX_HEIGHT; y++) {
        for (int x = 0; x < TEX_WIDTH; x++) {
            // Texture 1: BLUE Grid (Regular Walls)
            textures[1][y][x] = (x % 8 == 0 || y % 8 == 0) ? 0xFF00FFFF : 0xFF0000AA;

            // Texture 2: YELLOW/GOLD Matrix pattern (Map Jammers)
            textures[2][y][x] = ((x + y) % 16 < 2 || x % 16 == 0) ? 0xFFFFFF00 : 0xFF888800;

            // Texture 3: RED Hazard Warning Stripes (Death Tiles)
            textures[3][y][x] = ((x + y) / 8 % 2 == 0) ? 0xFFFF0000 : 0xFF330000;

            // Texture 4: GREEN Circuit Glowing Board (Victory Pad)
            textures[4][y][x] = ((x / 16) % 2 == (y / 16) % 2) ? 0xFF00FF00 : 0xFF004400;
        }
    }
}

void load_game_level(int level_idx, Player *player) {
    Level target = level_database[level_idx];
    player->pos.x = target.spawn_x;
    player->pos.y = target.spawn_y;
    player->dir.x = -1.0; 
    player->dir.y = 0.0;
    player->camera_plane.x = 0.0;
    player->camera_plane.y = FOV;

    peek_charges = target.max_peeks;
    is_peeking = false;
    current_state = STATE_PLAYING; 

    for(int y = 0; y < MAP_HEIGHT; y++) {
        for(int x = 0; x < MAP_WIDTH; x++) {
            map[y][x] = target.map_layout[y][x];
        }
    }
}

void rotate_map_ring(int layer, bool clockwise) {
    int startX = layer;
    int startY = layer;
    int endX = MAP_WIDTH - 1 - layer;
    int endY = MAP_HEIGHT - 1 - layer;

    if (startX >= endX || startY >= endY) return;

    int temp[256]; 
    int count = 0;

    for (int x = startX; x <= endX; x++) temp[count++] = map[startY][x];
    for (int y = startY + 1; y <= endY; y++) temp[count++] = map[y][endX];
    for (int x = endX - 1; x >= startX; x--) temp[count++] = map[endY][x];
    for (int y = endY - 1; y > startY; y--) temp[count++] = map[y][startX];

    int write_index = 0;
    int offset = clockwise ? (count - 1) : 1; 

    for (int x = startX; x <= endX; x++) {
        map[startY][x] = temp[(write_index + offset) % count];
        write_index++;
    }
    for (int y = startY + 1; y <= endY; y++) {
        map[y][endX] = temp[(write_index + offset) % count];
        write_index++;
    }
    for (int x = endX - 1; x >= startX; x--) {
        map[endY][x] = temp[(write_index + offset) % count];
        write_index++;
    }
    for (int y = endY - 1; y > startY; y--) {
        map[y][startX] = temp[(write_index + offset) % count];
        write_index++;
    }
}

void draw_minimap(SDL_Surface *surface, Player *player) {
    int cell_draw_size = 24; 
    int start_x = (WIN_WIDTH - (MAP_WIDTH * cell_draw_size)) / 2;
    int start_y = (WIN_HEIGHT - (MAP_HEIGHT * cell_draw_size)) / 2;

    SDL_Rect panel = { start_x - 12, start_y - 12, (MAP_WIDTH * cell_draw_size) + 24, (MAP_HEIGHT * cell_draw_size) + 24 };
    SDL_FillRect(surface, &panel, 0xFF151515); 

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            SDL_Rect cell_rect = { start_x + (x * cell_draw_size), start_y + (y * cell_draw_size), cell_draw_size - 1, cell_draw_size - 1 };
            
            uint32_t color = 0xFF2A2A2A; 
            if (map[y][x] == 1) color = 0xFF0000FF; // Blue
            if (map[y][x] == 2) color = 0xFFFFFF00; // Yellow
            if (map[y][x] == 3) color = 0xFFFF0000; // Red
            if (map[y][x] == 4) color = 0xFF00FF00; // Green

            SDL_FillRect(surface, &cell_rect, color);
        }
    }

    SDL_Rect p_rect = { 
        start_x + (int)(player->pos.x * cell_draw_size) - 4, 
        start_y + (int)(player->pos.y * cell_draw_size) - 4, 
        8, 8 
    };
    SDL_FillRect(surface, &p_rect, 0xFFFF00FF);
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    SDL_Window *pwindow = SDL_CreateWindow("RaySpinner", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Surface *psurface = SDL_GetWindowSurface(pwindow);
    
    //init audio
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    printf("SDL_mixer Error: %s\n", Mix_GetError());
    }

    // Load your assets (Ogg or MP3 for music, WAV for SFX)
    Mix_Music *bgm = Mix_LoadMUS("assets/Dark-Lands.mp3");
    Mix_Chunk *death_sfx = Mix_LoadWAV("assets/scrunch.wav");

    // Play BGM on loop (-1 means infinite looping)
    Mix_PlayMusic(bgm, -1);
    generate_textures(); 

    Player player;
    load_game_level(current_level_idx, &player);

    Uint64 last_time = SDL_GetTicks64();
    double delta_time = 0.0;

    while(running){
        Uint64 current_time = SDL_GetTicks64();
        delta_time = (current_time - last_time) / 1000.0; 
        last_time = current_time;
        if (delta_time > 0.1) delta_time = 0.1;

        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_QUIT){
                running = false;
            }
            
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.scancode == SDL_SCANCODE_R) {
                    load_game_level(current_level_idx, &player);
                    printf("Level Reloaded.\n");
                    continue;
                }

                if (current_state == STATE_PLAYING) {
                    int playerMapX = (int)player.pos.x;
                    int playerMapY = (int)player.pos.y;

                    int pLeft = playerMapX;
                    int pRight = (MAP_WIDTH - 1) - playerMapX;
                    int pTop = playerMapY;
                    int pBottom = (MAP_HEIGHT - 1) - playerMapY;

                    int player_layer = pLeft;
                    if (pRight < player_layer)  player_layer = pRight;
                    if (pTop < player_layer)    player_layer = pTop;
                    if (pBottom < player_layer) player_layer = pBottom;

                    bool q_pressed = (event.key.keysym.scancode == SDL_SCANCODE_Q);
                    bool e_pressed = (event.key.keysym.scancode == SDL_SCANCODE_E);

                    if (q_pressed || e_pressed) {
                        bool allowed_to_spin = true;

                        if (target_layer == player_layer) {
                            bool clockwise = e_pressed;
                            int startX = target_layer;
                            int startY = target_layer;
                            int endX = MAP_WIDTH - 1 - target_layer;
                            int endY = MAP_HEIGHT - 1 - target_layer;

                            if (startX < endX && startY < endY) {
                                int temp[256];
                                int count = 0;
                                int player_temp_idx = -1;

                                for (int x = startX; x <= endX; x++) {
                                    if (x == playerMapX && startY == playerMapY) player_temp_idx = count;
                                    temp[count++] = map[startY][x];
                                }
                                for (int y = startY + 1; y <= endY; y++) {
                                    if (endX == playerMapX && y == playerMapY) player_temp_idx = count;
                                    temp[count++] = map[y][endX];
                                }
                                for (int x = endX - 1; x >= startX; x--) {
                                    if (x == playerMapX && endY == playerMapY) player_temp_idx = count;
                                    temp[count++] = map[endY][x];
                                }
                                for (int y = endY - 1; y > startY; y--) {
                                    if (startX == playerMapX && y == playerMapY) player_temp_idx = count;
                                    temp[count++] = map[y][startX];
                                }

                                if (player_temp_idx != -1) {
                                    int offset = clockwise ? (count - 1) : 1;
                                    int incoming_block_value = temp[(player_temp_idx + offset) % count];

                                    if (incoming_block_value == 1) {
                                        allowed_to_spin = false;
                                    }
                                }
                            }
                        }

                        if (allowed_to_spin) {
                            if (e_pressed) rotate_map_ring(target_layer, true);
                            if (q_pressed) rotate_map_ring(target_layer, false);
                        }
                    }

                    if (event.key.keysym.scancode == SDL_SCANCODE_M) {
                        if (peek_charges > 0 && m_key_released && get_tile_at_position(player.pos.x, player.pos.y) != 2) {
                            is_peeking = true;
                            peek_charges--;
                            m_key_released = false;
                        }
                    }
                }

                if (event.key.keysym.scancode == SDL_SCANCODE_N) {
                    if (level_database[current_level_idx].completed) {
                        current_level_idx = (current_level_idx + 1) % 2;
                        load_game_level(current_level_idx, &player);
                        printf("Loading Level %d...\n", current_level_idx + 1);
                    } else {
                        printf("Clear the level first! Stand on the green tile (4).\n");
                    }
                }
            }
            if(event.type == SDL_KEYUP) {
                if(event.key.keysym.scancode == SDL_SCANCODE_M) {
                    is_peeking = false;
                    m_key_released = true; 
                }
            }   
        }

        int current_tile = get_tile_at_position(player.pos.x, player.pos.y);
        if (current_state == STATE_PLAYING) {
            double current_mov_speed = MOV_SPEED * delta_time;
            double current_rot_speed = ROT_SPEED * delta_time;
            const Uint8 *state = SDL_GetKeyboardState(NULL);

            int currentX = (int)player.pos.x;
            int currentY = (int)player.pos.y;

            if (state[SDL_SCANCODE_W] || state[SDL_SCANCODE_UP]) {
                int nextX = (int)(player.pos.x + player.dir.x * current_mov_speed);
                int nextY = (int)(player.pos.y + player.dir.y * current_mov_speed);

                if (nextX == currentX || map[currentY][nextX] == 0 || map[currentY][nextX] == 2 || map[currentY][nextX] == 3 || map[currentY][nextX] == 4) 
                    player.pos.x += player.dir.x * current_mov_speed;
                
                if (nextY == currentY || map[nextY][currentX] == 0 || map[nextY][currentX] == 2 || map[nextY][currentX] == 3 || map[nextY][currentX] == 4) 
                    player.pos.y += player.dir.y * current_mov_speed;
            }
            
            if (state[SDL_SCANCODE_S] || state[SDL_SCANCODE_DOWN]) {
                int nextX = (int)(player.pos.x - player.dir.x * current_mov_speed);
                int nextY = (int)(player.pos.y - player.dir.y * current_mov_speed);

                if (nextX == currentX || map[currentY][nextX] == 0 || map[currentY][nextX] == 2 || map[currentY][nextX] == 3 || map[currentY][nextX] == 4) 
                    player.pos.x -= player.dir.x * current_mov_speed;
                
                if (nextY == currentY || map[nextY][currentX] == 0 || map[nextY][currentX] == 2 || map[nextY][currentX] == 3 || map[nextY][currentX] == 4) 
                    player.pos.y -= player.dir.y * current_mov_speed;
            }
            if (state[SDL_SCANCODE_D] || state[SDL_SCANCODE_RIGHT]) {
                double oldDirX = player.dir.x;
                player.dir.x = player.dir.x * cos(-current_rot_speed) - player.dir.y * sin(-current_rot_speed);
                player.dir.y = oldDirX * sin(-current_rot_speed) + player.dir.y * cos(-current_rot_speed);
                double oldPlaneX = player.camera_plane.x;
                player.camera_plane.x = player.camera_plane.x * cos(-current_rot_speed) - player.camera_plane.y * sin(-current_rot_speed);
                player.camera_plane.y = oldPlaneX * sin(-current_rot_speed) + player.camera_plane.y * cos(-current_rot_speed);
            }
            if (state[SDL_SCANCODE_A] || state[SDL_SCANCODE_LEFT]) {
                double oldDirX = player.dir.x;
                player.dir.x = player.dir.x * cos(current_rot_speed) - player.dir.y * sin(current_rot_speed);
                player.dir.y = oldDirX * sin(current_rot_speed) + player.dir.y * cos(current_rot_speed);
                double oldPlaneX = player.camera_plane.x;
                player.camera_plane.x = player.camera_plane.x * cos(current_rot_speed) - player.camera_plane.y * sin(current_rot_speed);
                player.camera_plane.y = oldPlaneX * sin(current_rot_speed) + player.camera_plane.y * cos(current_rot_speed);
            }

            // Check tile assignment at finalized runtime coordinates
            current_tile = get_tile_at_position(player.pos.x, player.pos.y);
            if (current_tile == 3) {
                Mix_PlayChannel(-1, death_sfx, 0);
                current_state = STATE_GAMEOVER;
                printf("Game Over! You stepped on a RED death tile. Press 'R' to Reload.\n");
            }

            int roundedX = (int)player.pos.x;
            int roundedY = (int)player.pos.y;
            if (roundedX == level_database[current_level_idx].win_x && roundedY == level_database[current_level_idx].win_y) {
                level_database[current_level_idx].completed = true;
                current_state = STATE_VICTORY;
                printf("Level Complete! Press 'N' for next level or 'R' to replay.\n");
            }
        }

        SDL_Rect ceiling_rect = {0, 0, WIN_WIDTH, WIN_HEIGHT / 2};
        SDL_Rect floor_rect = {0, WIN_HEIGHT / 2, WIN_WIDTH, WIN_HEIGHT / 2};
        
        if (current_state == STATE_GAMEOVER) {
            SDL_FillRect(psurface, &ceiling_rect, 0xFF551111); 
            SDL_FillRect(psurface, &floor_rect, 0xFF220505);   
        } else if (current_state == STATE_VICTORY) {
            SDL_FillRect(psurface, &ceiling_rect, 0xFF114411); 
            SDL_FillRect(psurface, &floor_rect, 0xFF052205);   
        } else {
            SDL_FillRect(psurface, &ceiling_rect, 0xFF333333); 
            SDL_FillRect(psurface, &floor_rect, 0xFF1A1A1A);   
        }

        uint32_t *pixels = (uint32_t *)psurface->pixels;
        int pitch = psurface->pitch / 4; 

        for(int x = 0; x < WIN_WIDTH; x++){
            double camera_x = 2 * x / (double)WIN_WIDTH - 1; 
            Vector2D ray_dir = {
                player.dir.x + player.camera_plane.x * camera_x,
                player.dir.y + player.camera_plane.y * camera_x
            };

            int mapX = (int)player.pos.x;
            int mapY = (int)player.pos.y;

            Vector2D delta_dist = {
                ray_dir.x == 0 ? 1e30 : fabs(1 / ray_dir.x),
                ray_dir.y == 0 ? 1e30 : fabs(1 / ray_dir.y)
            };

            Vector2D side_dist;
            Vector2D step;

            if(ray_dir.x < 0) {
                step.x = -1;
                side_dist.x = (player.pos.x - mapX) * delta_dist.x;
            } else {
                step.x = 1;
                side_dist.x = (mapX + 1.0 - player.pos.x) * delta_dist.x;
            }
            if(ray_dir.y < 0) {
                step.y = -1;
                side_dist.y = (player.pos.y - mapY) * delta_dist.y;
            } else {
                step.y = 1;
                side_dist.y = (mapY + 1.0 - player.pos.y) * delta_dist.y;
            }

            int hit = 0; 
            int side = 0; 
            while(hit == 0) {
                if(side_dist.x < side_dist.y) {
                    side_dist.x += delta_dist.x;
                    mapX += step.x; 
                    side = 0;
                } else {
                    side_dist.y += delta_dist.y;
                    mapY += step.y; 
                    side = 1;
                }

                if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
                    break;
                }

                if(map[mapY][mapX] > 0) hit = 1;
            }

            if (x == WIN_WIDTH / 2) {
                int distFromLeft = mapX;
                int distFromRight = (MAP_WIDTH - 1) - mapX;
                int distFromTop = mapY;
                int distFromBottom = (MAP_HEIGHT - 1) - mapY;

                target_layer = distFromLeft;
                if (distFromRight < target_layer)  target_layer = distFromRight;
                if (distFromTop < target_layer)    target_layer = distFromTop;
                if (distFromBottom < target_layer) target_layer = distFromBottom;
            }

            double perpWallDist;
            if (side == 0) perpWallDist = (side_dist.x - delta_dist.x);
            else           perpWallDist = (side_dist.y - delta_dist.y);

            if(perpWallDist <= 0) perpWallDist = 0.01;

            int lineHeight = (int)(WIN_HEIGHT / perpWallDist);

            int drawStart = -lineHeight / 2 + WIN_HEIGHT / 2;
            if (drawStart < 0) drawStart = 0;
            
            int drawEnd = lineHeight / 2 + WIN_HEIGHT/ 2;
            if (drawEnd >= WIN_HEIGHT) drawEnd = WIN_HEIGHT - 1;

            int texNum = map[mapY][mapX];
            if (texNum < 1 || texNum >= NUM_TEXTURES) texNum = 1; 

            double wallX; 
            if (side == 0) wallX = player.pos.y + perpWallDist * ray_dir.y;
            else           wallX = player.pos.x + perpWallDist * ray_dir.x;
            wallX -= floor(wallX);

            int texX = (int)(wallX * (double)TEX_WIDTH);
            if(side == 0 && ray_dir.x > 0) texX = TEX_WIDTH - texX - 1;
            if(side == 1 && ray_dir.y < 0) texX = TEX_WIDTH - texX - 1;

            double stepY = 1.0 * TEX_HEIGHT / lineHeight;
            double texPos = (drawStart - WIN_HEIGHT / 2 + lineHeight / 2) * stepY;

            for(int y = drawStart; y <= drawEnd; y++) {
                int texY = (int)texPos & (TEX_HEIGHT - 1);
                texPos += stepY;
                
                uint32_t color = textures[texNum][texY][texX];
                
                if(side == 1) {
                    color = (color >> 1) & 0x7F7F7F7F;
                }
                
                pixels[y * pitch + x] = color;
            }
        }

        if (is_peeking && current_tile != 2 && current_state == STATE_PLAYING) {
            draw_minimap(psurface, &player);
        }
            
        SDL_UpdateWindowSurface(pwindow);
        SDL_Delay(1); 
    }
    // Free audio resources and close mixer
    Mix_FreeMusic(bgm);
    Mix_FreeChunk(death_sfx);
    Mix_CloseAudio();
    
    SDL_DestroyWindow(pwindow);
    SDL_Quit();
    return 0;
}