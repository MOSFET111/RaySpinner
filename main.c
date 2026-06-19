#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>

#define WIN_WIDTH 900
#define WIN_HEIGHT 600
#define COLOR_WHITE 0xFFFFFFFF
#define MAP_WIDTH 24
#define MAP_HEIGHT 24
#define CELL_SIZE 64
#define FOV 0.66
#define MOV_SPEED 0.05
#define ROT_SPEED 0.03
// FIX: Spawn player at a safe location in an empty corridor (value 0)
#define PLAYER_XPOS 7.5
#define PLAYER_YPOS 2.5

bool running = true;

// FIX: Set to height/width dimensions safely
int map[MAP_HEIGHT][MAP_WIDTH] =
{
  {4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,7,7,7,7,7,7,7,7},
  {4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,7},
  {4,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
  {4,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
  {4,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,7},
  {4,0,4,0,0,0,0,5,5,5,5,5,5,5,5,5,7,7,0,7,7,7,7,7},
  {4,0,5,0,0,0,0,5,0,5,0,5,0,5,0,5,7,0,0,0,7,7,7,1},
  {4,0,6,0,0,0,0,5,0,0,0,0,0,0,0,5,7,0,0,0,0,0,0,8},
  {4,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,7,7,1},
  {4,0,8,0,0,0,0,5,0,0,0,0,0,0,0,5,7,0,0,0,0,0,0,8},
  {4,0,0,0,0,0,0,5,0,0,0,0,0,0,0,5,7,0,0,0,7,7,7,1},
  {4,0,0,0,0,0,0,5,5,5,5,0,5,5,5,5,7,7,7,7,7,7,7,1},
  {6,6,6,6,6,6,6,6,6,6,6,0,6,6,6,6,6,6,6,6,6,6,6,6},
  {8,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
  {6,6,6,6,6,6,0,6,6,6,6,0,6,6,6,6,6,6,6,6,6,6,6,6},
  {4,4,4,4,4,4,0,4,4,4,6,0,6,2,2,2,2,2,2,2,3,3,3,3},
  {4,0,0,0,0,0,0,0,0,4,6,0,6,2,0,0,0,0,0,2,0,0,0,2},
  {4,0,0,0,0,0,0,0,0,0,0,0,6,2,0,0,5,0,0,2,0,0,0,2},
  {4,0,0,0,0,0,0,0,0,4,6,0,6,2,0,0,0,0,0,2,2,0,2,2},
  {4,0,6,0,6,0,0,0,0,4,6,0,0,0,0,0,5,0,0,0,0,0,0,2},
  {4,0,0,5,0,0,0,0,0,4,6,0,6,2,0,0,0,0,0,2,2,0,2,2},
  {4,0,6,0,6,0,0,0,0,4,6,0,6,2,0,0,5,0,0,2,0,0,0,2},
  {4,0,0,0,0,0,0,0,0,4,6,0,6,2,0,0,0,0,0,2,0,0,0,2},
  {4,4,4,4,4,4,4,4,4,4,1,1,1,2,2,2,2,2,2,3,3,3,3,3}
};

typedef struct {
    double x;
    double y;
} Vector2D;

typedef struct {
    double x;
    double y;
    uint32_t color;
} Cell;

typedef struct {
    Vector2D pos;
    Vector2D dir;
    Vector2D camera_plane;
} Player;

void get_all_map_coords(int map[MAP_HEIGHT][MAP_WIDTH], Cell points_out[], int *count_out) {
    *count_out = 0;

    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (map[y][x] > 0) {
                if (*count_out < MAP_HEIGHT * MAP_WIDTH) {
                    switch (map[y][x]){
                        case 1: points_out[*count_out].color = 0xFF0000FF; break; // Red
                        case 2: points_out[*count_out].color = 0xFF00FF00; break; // Green
                        case 3: points_out[*count_out].color = 0xFFFF0000; break; // Blue
                        case 4: points_out[*count_out].color = 0xFFFFFF00; break; // Yellow
                        case 5: points_out[*count_out].color = 0xFFFF00FF; break; // Magenta
                        case 6: points_out[*count_out].color = 0xFF00FFFF; break; // Cyan
                        case 7: points_out[*count_out].color = 0xFF888888; break; // Gray
                        default: points_out[*count_out].color = 0xFFFFFFFF; break; // White
                    }
                    // FIX: Tracking raw map coords (0 to 24), not pixel multipliers
                    points_out[*count_out].x = (double)x;
                    points_out[*count_out].y = (double)y;
                    (*count_out)++; 
                }
            }
        }
    }
}

void draw_vertical_line(int x, int drawStart, int drawEnd, Uint32 wallColor, SDL_Surface *psurface) {
    int height = drawEnd - drawStart + 1; 
    SDL_Rect rect = { x, drawStart, 1, height };
    SDL_FillRect(psurface, &rect, wallColor);
}

void draw_map(SDL_Surface *surface, Cell points[], int count) {
    for (int i = 0; i < count; i++) {
        // FIX: Scale inside the 2D draw utility ONLY
        int x = (int)(points[i].x * CELL_SIZE);
        int y = (int)(points[i].y * CELL_SIZE);
        uint32_t color = points[i].color;

        SDL_Rect rect = {x, y, CELL_SIZE, CELL_SIZE};
        SDL_FillRect(surface, &rect, color);
    }
}

int main(int argc, char *argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    
    SDL_Window *pwindow = SDL_CreateWindow("RaySpinner", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Surface *psurface = SDL_GetWindowSurface(pwindow);

    Player player = {
        .pos = {PLAYER_XPOS, PLAYER_YPOS},
        .dir = {-1.0, 0.0},
        .camera_plane = {0.0, FOV}
    };

    Cell wall_points[MAP_WIDTH * MAP_HEIGHT];
    int wall_point_count = 0;
    get_all_map_coords(map, wall_points, &wall_point_count);

    while(running){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_QUIT){
                running = false;
            }
        }

        // Get the state of all keys on the keyboard
        const Uint8 *state = SDL_GetKeyboardState(NULL);

        // 1. MOVE FORWARD (W or Up Arrow)
        if (state[SDL_SCANCODE_W] || state[SDL_SCANCODE_UP]) {
            // Check if the next X position is an empty space (0), if so, move!
            if(map[(int)player.pos.y][(int)(player.pos.x + player.dir.x * MOV_SPEED)] == 0) 
                player.pos.x += player.dir.x * MOV_SPEED;
            // Check if the next Y position is an empty space (0)
            if(map[(int)(player.pos.y + player.dir.y * MOV_SPEED)][(int)player.pos.x] == 0) 
                player.pos.y += player.dir.y * MOV_SPEED;
        }

        // 2. MOVE BACKWARD (S or Down Arrow)
        if (state[SDL_SCANCODE_S] || state[SDL_SCANCODE_DOWN]) {
            if(map[(int)player.pos.y][(int)(player.pos.x - player.dir.x * MOV_SPEED)] == 0) 
                player.pos.x -= player.dir.x * MOV_SPEED;
            if(map[(int)(player.pos.y - player.dir.y * MOV_SPEED)][(int)player.pos.x] == 0) 
                player.pos.y -= player.dir.y * MOV_SPEED;
        }

        // 3. ROTATE RIGHT (D or Right Arrow)
        if (state[SDL_SCANCODE_D] || state[SDL_SCANCODE_RIGHT]) {
            // Rotate direction vector
            double oldDirX = player.dir.x;
            player.dir.x = player.dir.x * cos(-ROT_SPEED) - player.dir.y * sin(-ROT_SPEED);
            player.dir.y = oldDirX * sin(-ROT_SPEED) + player.dir.y * cos(-ROT_SPEED);
            
            // Rotate camera plane vector perfectly in sync
            double oldPlaneX = player.camera_plane.x;
            player.camera_plane.x = player.camera_plane.x * cos(-ROT_SPEED) - player.camera_plane.y * sin(-ROT_SPEED);
            player.camera_plane.y = oldPlaneX * sin(-ROT_SPEED) + player.camera_plane.y * cos(-ROT_SPEED);
        }

        // 4. ROTATE LEFT (A or Left Arrow)
        if (state[SDL_SCANCODE_A] || state[SDL_SCANCODE_LEFT]) {
            // Rotate direction vector (positive rotSpeed turns left)
            double oldDirX = player.dir.x;
            player.dir.x = player.dir.x * cos(ROT_SPEED) - player.dir.y * sin(ROT_SPEED);
            player.dir.y = oldDirX * sin(ROT_SPEED) + player.dir.y * cos(ROT_SPEED);
            
            // Rotate camera plane vector perfectly in sync
            double oldPlaneX = player.camera_plane.x;
            player.camera_plane.x = player.camera_plane.x * cos(ROT_SPEED) - player.camera_plane.y * sin(ROT_SPEED);
            player.camera_plane.y = oldPlaneX * sin(ROT_SPEED) + player.camera_plane.y * cos(ROT_SPEED);
        }

        // Draw sky/ceiling and floor
        SDL_Rect ceiling = {0, 0, WIN_WIDTH, WIN_HEIGHT / 2};
        SDL_Rect floor = {0, WIN_HEIGHT / 2, WIN_WIDTH, WIN_HEIGHT / 2};
        SDL_FillRect(psurface, &ceiling, 0xFF333333); 
        SDL_FillRect(psurface, &floor, 0xFF1A1A1A);   

        // Raycasting loop
        for(int x = 0; x < WIN_WIDTH; x++){
            double camera_x = 2 * x / (double)WIN_WIDTH - 1; 
            
            Vector2D ray_dir = {
                player.dir.x + player.camera_plane.x * camera_x,
                player.dir.y + player.camera_plane.y * camera_x
            };

            // FIX: Changed map_pos from floating vectors to strict int counters
            int mapX = (int)player.pos.x;
            int mapY = (int)player.pos.y;

            Vector2D delta_dist = {
                ray_dir.x == 0 ? 1e30 : fabs(1 / ray_dir.x),
                ray_dir.y == 0 ? 1e30 : fabs(1 / ray_dir.y)
            };

            Vector2D side_dist;
            Vector2D step;

            // FIX: Using mapX and mapY structures directly
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
                    mapX += step.x; // Strict int addition
                    side = 0;
                } else {
                    side_dist.y += delta_dist.y;
                    mapY += step.y; // Strict int addition
                    side = 1;
                }

                // Map bounds security boundary check
                if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) {
                    break;
                }

                // FIX: Checking standard row/column references correctly
                if(map[mapY][mapX] > 0) hit = 1;
            }

            double perpWallDist;
            if (side == 0) perpWallDist = (side_dist.x - delta_dist.x);
            else           perpWallDist = (side_dist.y - delta_dist.y);

            // Avoid division by zero bugs if hugging a wall closely
            if(perpWallDist <= 0) perpWallDist = 0.01;

            int lineHeight = (int)(WIN_HEIGHT / perpWallDist);

            int drawStart = -lineHeight / 2 + WIN_HEIGHT / 2;
            if (drawStart < 0) drawStart = 0;
            
            int drawEnd = lineHeight / 2 + WIN_HEIGHT/ 2;
            if (drawEnd >= WIN_HEIGHT) drawEnd = WIN_HEIGHT - 1;

            // Optional Color Switch Setup
            uint32_t wallColor = COLOR_WHITE;
            if(map[mapY][mapX] == 4) wallColor = 0xFFFFFF00; // Yellow for wall 4
            if(side == 1) wallColor = (wallColor >> 1) & 0x7F7F7F7F; // Shadow depth

            draw_vertical_line(x, drawStart, drawEnd, wallColor, psurface);
        }
            
        SDL_UpdateWindowSurface(pwindow);
        SDL_Delay(10);
    }

    SDL_DestroyWindow(pwindow);
    SDL_Quit();
    return 0;
}