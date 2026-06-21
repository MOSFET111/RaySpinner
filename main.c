#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>

#define WIN_WIDTH 900
#define WIN_HEIGHT 600
#define COLOR_WHITE 0xFFFFFFFF
#define MAP_WIDTH 9
#define MAP_HEIGHT 9
#define CELL_SIZE 64
#define FOV 0.66
#define MOV_SPEED 3
#define ROT_SPEED 2 

bool running = true;

// This acts as our live-running buffer modified by ring transformations
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
    int map_layout[MAP_HEIGHT][MAP_WIDTH];
} Level;

// Global state to store what ring layer the crosshair is currently targeted at
int target_layer = 0;

// Minimap Peek state tracking
int peek_charges = 0;
bool is_peeking = false;
bool m_key_released = true; 

Level level_database[] = {
    {
        .id = 1,
        .max_peeks = 3,
        .spawn_x = 3.5,
        .spawn_y = 2.5,
        .map_layout = {
            {1, 1, 1, 1, 1, 1, 1, 1, 1},
            {1, 2, 0, 0, 1, 0, 0, 3, 1},
            {1, 0, 0, 0, 1, 0, 1, 0, 1},
            {1, 0, 1, 0, 0, 0, 1, 0, 1},
            {1, 0, 1, 1, 1, 1, 1, 0, 1},
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

void draw_vertical_line(int x, int drawStart, int drawEnd, Uint32 wallColor, SDL_Surface *psurface) {
    int height = drawEnd - drawStart + 1; 
    SDL_Rect rect = { x, drawStart, 1, height };
    SDL_FillRect(psurface, &rect, wallColor);
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
            if (map[y][x] == 1) color = 0xFF0000FF; 
            if (map[y][x] == 2) color = 0xFF00FF00; 
            if (map[y][x] == 3) color = 0xFFFF0000; 
            if (map[y][x] == 4) color = 0xFFFFFF00; 

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
            
            if(event.type == SDL_KEYDOWN) {
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

                if (target_layer != player_layer) {
                    if(event.key.keysym.scancode == SDL_SCANCODE_E) {
                        rotate_map_ring(target_layer, true); 
                    }
                    if(event.key.keysym.scancode == SDL_SCANCODE_Q) {
                        rotate_map_ring(target_layer, false); 
                    }
                }

                if(event.key.keysym.scancode == SDL_SCANCODE_M) {
                    if(peek_charges > 0 && m_key_released) {
                        is_peeking = true;
                        peek_charges--;
                        m_key_released = false; 
                    }
                }

                // Temporary Level Switch test binding for verification: Press 'N' key
                if(event.key.keysym.scancode == SDL_SCANCODE_N) {
                    current_level_idx = (current_level_idx + 1) % 2; 
                    load_game_level(current_level_idx, &player);
                }
            }

            if(event.type == SDL_KEYUP) {
                if(event.key.keysym.scancode == SDL_SCANCODE_M) {
                    is_peeking = false;
                    m_key_released = true; 
                }
            }
        }

        double current_mov_speed = MOV_SPEED * delta_time;
        double current_rot_speed = ROT_SPEED * delta_time;
        const Uint8 *state = SDL_GetKeyboardState(NULL);

        if (state[SDL_SCANCODE_W] || state[SDL_SCANCODE_UP]) {
            if(map[(int)player.pos.y][(int)(player.pos.x + player.dir.x * current_mov_speed)] == 0) 
                player.pos.x += player.dir.x * current_mov_speed;
            if(map[(int)(player.pos.y + player.dir.y * current_mov_speed)][(int)player.pos.x] == 0) 
                player.pos.y += player.dir.y * current_mov_speed;
        }
        if (state[SDL_SCANCODE_S] || state[SDL_SCANCODE_DOWN]) {
            if(map[(int)player.pos.y][(int)(player.pos.x - player.dir.x * current_mov_speed)] == 0) 
                player.pos.x -= player.dir.x * current_mov_speed;
            if(map[(int)(player.pos.y - player.dir.y * current_mov_speed)][(int)player.pos.x] == 0) 
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

        SDL_Rect ceiling = {0, 0, WIN_WIDTH, WIN_HEIGHT / 2};
        SDL_Rect floor = {0, WIN_HEIGHT / 2, WIN_WIDTH, WIN_HEIGHT / 2};
        SDL_FillRect(psurface, &ceiling, 0xFF333333); 
        SDL_FillRect(psurface, &floor, 0xFF1A1A1A);   

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

            uint32_t wallColor = COLOR_WHITE;
            if(map[mapY][mapX] == 1) wallColor = 0xFF0000FF; 
            if(map[mapY][mapX] == 2) wallColor = 0xFF00FF00; 
            if(map[mapY][mapX] == 3) wallColor = 0xFFFF0000; 
            if(map[mapY][mapX] == 4) wallColor = 0xFFFFFF00; 
            
            if(side == 1) wallColor = (wallColor >> 1) & 0x7F7F7F7F; 

            draw_vertical_line(x, drawStart, drawEnd, wallColor, psurface);
        }

        if (is_peeking) {
            draw_minimap(psurface, &player);
        }
            
        SDL_UpdateWindowSurface(pwindow);
        SDL_Delay(1); 
    }

    SDL_DestroyWindow(pwindow);
    SDL_Quit();
    return 0;
}