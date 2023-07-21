#include <stdio.h>
#include "raylib.h"

/*
todo
maybe make struct to define windows within the screen ie grid-window, top-bar-window, popup-menu-window. store size and pos.
^ fix that dumb idea, camera is all messed up and makes even less sense now. camera needs to be its own thing no struct needed

detect when grid is changed so it can be reset and algo can be ran again
add more buttons
add check boxes for options, might be algo specific
add drop down menu for swapping algorithms
add more algorithms
make pretty
*/

enum ALGO_STATUS {
    ALGO_NONE,
    ALGO_INPROGRESS,
    ALGO_FOUND,
    ALGO_COMPLETE,
};

enum NODELOC {
    LOC_GRID,
    LOC_OPEN,
    LOC_CLOSED,
    LOC_WALL,
    LOC_PATH
};

enum DIRECTIONS {
    DIR_UPLEFT,
    DIR_UP,
    DIR_UPRIGHT,
    DIR_RIGHT,
    DIR_DOWNRIGHT,
    DIR_DOWN,
    DIR_DOWNLEFT,
    DIR_LEFT,
};

typedef struct node {
    int parent;
    int f, g, h, loc;
} Node;

typedef struct GridData {
    Node *grid;
    int width, height, size;
    int start_pos, end_pos;
} GridData;

typedef struct node_list {
    //node *base;
    int *nodes;
    int length, max_length;
} NList;

enum ui_element_type {
    UI_NONE = 0,
    UI_WINDOW = 1,
    UI_BUTTON = 2,
    UI_VISIBLE = 1,
    UI_INVISIBLE = 0,
    UI_INTERACTABLE = 1,
    UI_NONINTERACTABLE = 0
};

typedef struct ui_element {
    int x, y, width, height;
    char *text;
    char type, visible, capture;
} UI_Element;

int list_index_of(NList *l, int ele) {
    for (int i = 0; i < l->length; i++) if (l->nodes[i] == ele) return i;
    return -1;
}

void list_add(NList *l, int i) {
    if (list_index_of(l, i) != -1) printf("Duplicate Element Added At Index %i\n", l->length);
    if (l->length == l->max_length) printf("Add Index Out Of Bounds\n");
    else l->nodes[l->length++] = i;
}

void list_remove_index(NList *l, int i) {
    if (l->length == 0) printf("Failed to remove index %i from empty list\n", i);
    else if (i < 0 || i > l->length) printf("Remove Index Out Of Bounds\n");
    else l->nodes[i] = l->nodes[--l->length];
}

void list_print(NList *l) {
    printf("[");
    for (int i = 0; i < l->length - 1; i++) printf("%i, ", l->nodes[i]);
    if (l->length > 0) printf("%i]\n", l->nodes[l->length - 1]);
    else printf("]\n");
}

int wrap_range(int i, int rng) {
    if (i >= 0 && i < rng) return i;
    if (i >= rng) return i % rng;
    return rng - (-i) % 5;
}

int dist(int i1, int i2, int width) {
    return (i1 % width - i2 % width) * (i1 % width - i2 % width) + (i1 / width - i2 / width) * (i1 / width - i2 / width);
}

int maxi_mag(int lhs, int rhs) {
    if (lhs < 0) lhs = -lhs;
    if (rhs < 0) rhs = -rhs;
    if (lhs > rhs) return lhs;
    else return rhs;
}

// ALGORITHMS
int AStarStep(GridData gd, NList *openList, char status) {
    int dir[8] = {-1 - gd.width, -gd.width, 1 - gd.width, 1, 1 + gd.width, gd.width, -1 + gd.width, -1};

    if (status == ALGO_NONE) list_add(openList, gd.start_pos);
    else if (openList->length == 0 && status == ALGO_INPROGRESS) return ALGO_COMPLETE;
    else if (status == ALGO_COMPLETE || status == ALGO_FOUND) return status;

    int smallest = 0;
    for (int i = 0; i < openList->length; i++) {
        if (gd.grid[openList->nodes[i]].f < gd.grid[openList->nodes[smallest]].f) smallest = i;
        else if (gd.grid[openList->nodes[i]].f == gd.grid[openList->nodes[smallest]].f && gd.grid[openList->nodes[i]].h < gd.grid[openList->nodes[smallest]].h) smallest = i;
    }
    //list_add(closedList, openList->nodes[smallest]);
    int current = openList->nodes[smallest];
    list_remove_index(openList, smallest);
    gd.grid[current].loc = LOC_CLOSED;

    if (current == gd.end_pos) {
        //list_add(closedList, current);
        int n = current;
        while (gd.grid[n].parent >= 0) {
            gd.grid[n].loc = LOC_PATH;
            n = gd.grid[n].parent;
        }
        return ALGO_FOUND;
    }
    for (int i = 0; i < 8; i++) {
        int next = current + dir[i];
        if (next >= 0 && next < gd.size && gd.grid[next].loc != LOC_WALL) {
            // account for traveling through corners
            if (i % 2 == 0 && gd.grid[current + dir[wrap_range(i + 1, 8)]].loc == LOC_WALL && gd.grid[current + dir[wrap_range(i - 1, 8)]].loc == LOC_WALL) continue;
            // account for edge wraping
            if (next % gd.width - current % gd.width < -1 || next % gd.width - current % gd.width > 1) continue;

            int g_temp = gd.grid[current].g + dist(current, next, gd.width);
            int h_temp = dist(next, gd.end_pos, gd.width);
            int f_temp = g_temp + h_temp;
            if (gd.grid[next].loc == LOC_GRID) {
                gd.grid[next] = (Node){current, f_temp, g_temp, h_temp, LOC_OPEN};
                list_add(openList, next);
            } else if (gd.grid[next].loc != LOC_GRID && f_temp < gd.grid[next].f) {
                gd.grid[next] = (Node){current, f_temp, g_temp, h_temp, gd.grid[next].loc};
            }
        }
    }
    return ALGO_INPROGRESS;
}

int AStarFull(GridData gd, NList *openList) {
    int dir[8] = {-1 - gd.width, -gd.width, 1 - gd.width, 1, 1 + gd.width, gd.width, -1 + gd.width, -1};

    list_add(openList, gd.start_pos);

    while (openList->length > 0) {

        int smallest = 0;
        for (int i = 0; i < openList->length; i++) {
            if (gd.grid[openList->nodes[i]].f < gd.grid[openList->nodes[smallest]].f) smallest = i;
            else if (gd.grid[openList->nodes[i]].f == gd.grid[openList->nodes[smallest]].f && gd.grid[openList->nodes[i]].h < gd.grid[openList->nodes[smallest]].h) smallest = i; // settle f ties by looking at h value
        }
        //list_add(closedList, openList->nodes[smallest]);
        int current = openList->nodes[smallest];
        list_remove_index(openList, smallest);
        gd.grid[current].loc = LOC_CLOSED;

        if (current == gd.end_pos) {
            //list_add(closedList, current);
            int n = current;
            while (gd.grid[n].parent >= 0) {
                gd.grid[n].loc = LOC_PATH;
                n = gd.grid[n].parent;
            }
            return ALGO_FOUND;
        }
        for (int i = 0; i < 8; i++) {
            int next = current + dir[i];
            if (next >= 0 && next < gd.size && gd.grid[next].loc != LOC_WALL) {
                // account for traveling through corners
                if (i % 2 == 0 && gd.grid[current + dir[wrap_range(i + 1, 8)]].loc == LOC_WALL && gd.grid[current + dir[wrap_range(i - 1, 8)]].loc == LOC_WALL) continue;
                // account for edge wraping
                if (next % gd.width - current % gd.width < -1 || next % gd.width - current % gd.width > 1) continue;

                int g_temp = gd.grid[current].g + dist(current, next, gd.width);
                int h_temp = dist(next, gd.end_pos, gd.width);
                int f_temp = g_temp + h_temp;
                if (gd.grid[next].loc == LOC_GRID) {
                    gd.grid[next] = (Node){current, f_temp, g_temp, h_temp, LOC_OPEN};
                    list_add(openList, next);
                } else if (gd.grid[next].loc != LOC_GRID && f_temp < gd.grid[next].f) {
                    gd.grid[next] = (Node){current, f_temp, g_temp, h_temp, gd.grid[next].loc};
                }
            }
        }
    }
    return ALGO_COMPLETE;
}

int main() {
    int screen_width = 800, screen_height = 800;
    InitWindow(screen_width, screen_height, "A-Star Demo");
    SetTargetFPS(144);

    UI_Element camera = {0, 0, 800, 700, 0, UI_WINDOW, UI_INVISIBLE, UI_NONINTERACTABLE}; // this is stupid
    UI_Element buttons[8] = {
        (UI_Element){10, 10, 90, 35, "Start", UI_BUTTON, UI_VISIBLE, UI_INTERACTABLE},
        (UI_Element){10, 55, 90, 35, "Clear All", UI_BUTTON, UI_VISIBLE, UI_INTERACTABLE},
        (UI_Element){110, 55, 90, 35, "Clear Walls", UI_BUTTON, UI_VISIBLE, UI_INTERACTABLE},
        (UI_Element){210, 55, 90, 35, "Clear Paths", UI_BUTTON, UI_VISIBLE, UI_INTERACTABLE},
        (UI_Element){0},
        (UI_Element){0},
        (UI_Element){0},
        (UI_Element){0}
    };
    int cell_width = 8;

    // color legend for grid
    Color color_palette[8] = {WHITE, LIGHTGRAY, GREEN, BLACK, YELLOW, RED, BLUE};


    Node grid[10000];
    GridData gd = {grid, 100, 100, 10000, 982, 1275};
    for (int i = 0; i < gd.size; i++) grid[i] = (Node){-1, -1, -1, -1, LOC_GRID};

    int open_raw[10000];
    NList openList = (NList){open_raw, 0, 10000};


    int count = 0, found = 0;
    int mouse_x_last = 0, mouse_y_last = 0, mouse_x = 0, mouse_y = 0;
    char left_mouse_pressed = 0, dragging_start = 0, dragging_end = 0;


    while (!WindowShouldClose()) {
        mouse_x_last = mouse_x;
        mouse_y_last = mouse_y;
        mouse_x = GetMouseX();
        mouse_y = GetMouseY();



        if (IsKeyDown(KEY_RIGHT)) {
            camera.x += 10;
            if (camera.x + camera.width > gd.width * cell_width) camera.x = gd.width * cell_width - camera.width;
        }
        if (IsKeyDown(KEY_LEFT)) {
            camera.x -= 10;
            if (camera.x < 0) camera.x = 0;
        }
        if (IsKeyDown(KEY_UP)) {
            camera.y -= 10;
            if (camera.y < 0) camera.y = 0;
        }
        if (IsKeyDown(KEY_DOWN)) {
            camera.y += 10;
            if (camera.y + camera.height > gd.height * cell_width) camera.y = gd.height * cell_width - camera.height;
        }


        if (IsMouseButtonDown(0)) {
            for (int i = 0; i < 4; i++) if (!left_mouse_pressed && mouse_x >= buttons[i].x && mouse_x < buttons[i].x + buttons[i].width && mouse_y >= buttons[i].y && mouse_y < buttons[i].y + buttons[i].height) {
                printf("[%s] button clicked\n", buttons[i].text);
                if (i == 0 && !found) { // start button
                    found = AStarFull(gd, &openList);
                    break;
                } else if (i == 1) { // reset all button
                    for (int i = 0; i < gd.size; i++) {
                        gd.grid[i].loc = LOC_GRID;
                        gd.grid[i].parent = -1;
                    }
                    openList.length = 0;
                    found = 0;
                    break;
                } else if (i == 2) {
                    for (int i = 0; i < gd.size; i++) if (gd.grid[i].loc == LOC_WALL) gd.grid[i].loc = LOC_GRID;
                    break;
                } else if (i == 3) {
                    for (int i = 0; i < gd.size; i++) if (gd.grid[i].loc != LOC_WALL) gd.grid[i].loc = LOC_GRID;
                    openList.length = 0;
                    found = 0;
                    break;
                }
            }
            // this is stupid but its late and i cant think of anything else
            if (dragging_start) {
                gd.start_pos = (mouse_y + camera.y - 100) / cell_width * gd.width + (mouse_x + camera.x) / cell_width;
            }else if (dragging_end) {
                gd.end_pos = (mouse_y + camera.y - 100) / cell_width * gd.width + (mouse_x + camera.x) / cell_width;
            } else if ((mouse_y + camera.y - 100) / cell_width * gd.width + (mouse_x + camera.x) / cell_width == gd.start_pos) {
                dragging_start = 1;
            } else if ((mouse_y + camera.y - 100) / cell_width * gd.width + (mouse_x + camera.x) / cell_width == gd.end_pos) {
                dragging_end = 1;
            } else { // paint on grid
                int dist = maxi_mag((mouse_x / cell_width - mouse_x_last / cell_width), (mouse_y / cell_width - mouse_y_last / cell_width));
                for (int i = 0; i <= dist; i++) {
                    float t = dist == 0 ? 0 : (float)i / dist;
                    int lerp_x = (float)mouse_x_last * (1.0f - t) + (float)mouse_x * t, lerp_y = (float)mouse_y_last * (1.0f - t) + (float)mouse_y * t;
                    grid[(lerp_y + camera.y - 100) / cell_width * gd.width + (lerp_x + camera.x) / cell_width].loc = LOC_WALL;
                }
            }
            left_mouse_pressed = 1;
        } else {
            left_mouse_pressed = 0;
            if (dragging_start) dragging_start = 0;
            if (dragging_end) dragging_end = 0;
        }

        if (IsMouseButtonDown(1)) { // move camera
            camera.x += mouse_x_last - mouse_x;
            camera.y += mouse_y_last - mouse_y;

            if (camera.x + camera.width > (gd.width - 1) * cell_width + screen_width) camera.x = (gd.width - 1) * cell_width;
            if (camera.x < cell_width - camera.width) camera.x = cell_width - camera.width;
            if (camera.y + camera.height > (gd.height - 1) * cell_width + camera.height) camera.y = (gd.height - 1) * cell_width;
            if (camera.y < cell_width - camera.height) camera.y = cell_width - camera.height;
        }


        if (IsKeyDown(KEY_ENTER) && found <= 1) for (int i = 0; i < 10 && found <= 1; i++) found = AStarStep(gd, &openList, found);

        int size_diff = (int)GetMouseWheelMove();
        if (size_diff != 0) {
            int cell_width_previous = cell_width;
            cell_width += size_diff;
            if (cell_width > 50) cell_width = 50;
            //if (cell_width * gd.width < camera.width) cell_width = camera.width / gd.width;
            //if (cell_width * gd.height < camera.height) cell_width = camera.height / gd.height;
            if (cell_width < 1) cell_width = 1;

            camera.x = (camera.x + mouse_x) * cell_width / cell_width_previous - mouse_x;
            camera.y = (camera.y + mouse_y) * cell_width / cell_width_previous - mouse_y;
            if (camera.x + camera.width > (gd.width - 1) * cell_width + screen_width) camera.x = (gd.width - 1) * cell_width;
            if (camera.x < cell_width - camera.width) camera.x = cell_width - camera.width;
            if (camera.y + camera.height > (gd.height - 1) * cell_width + camera.height) camera.y = (gd.height - 1) * cell_width;
            if (camera.y < cell_width - camera.height) camera.y = cell_width - camera.height;

        }




        BeginDrawing();
            ClearBackground(BLACK);
            for (int i = 0; i < gd.size; i++) {
                if (i % gd.width * cell_width >= camera.x - cell_width && i % gd.width * cell_width <= camera.x + camera.width + cell_width && i / gd.width * cell_width >= camera.y - cell_width && i / gd.width * cell_width <= camera.y + camera.height + cell_width) {
                    DrawRectangle(i % gd.width * cell_width - camera.x, 100 + i / gd.width * cell_width - camera.y, cell_width, cell_width, color_palette[grid[i].loc]);
                }
            }
            DrawRectangle(gd.end_pos % gd.width * cell_width - camera.x, 100 + gd.end_pos / gd.width * cell_width - camera.y, cell_width, cell_width, color_palette[6]);
            DrawRectangle(gd.start_pos % gd.width * cell_width - camera.x, 100 + gd.start_pos / gd.width * cell_width - camera.y, cell_width, cell_width, color_palette[5]);

            DrawRectangle(0, 0, 800, 100, LIGHTGRAY);

            for (int i = 0; i < 4; i++) {
                DrawRectangle(buttons[i].x, buttons[i].y, buttons[i].width, buttons[i].height, WHITE);
                DrawText(buttons[i].text, buttons[i].x, buttons[i].y, 20, BLACK);
            }
        EndDrawing();
    }

}