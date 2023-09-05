#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"
#include <math.h>

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

typedef struct Point2D {
    int x, y;
} Point2D;

typedef struct node {
    int parent;
    float f, g, h;
    int loc;
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
    UI_TEXT = 3,
    UI_SLIDER = 4,
    UI_DROPDOWN = 5,
    UI_VISIBLE = 1,
    UI_INVISIBLE = 0,
    UI_INTERACTABLE = 1,
    UI_NONINTERACTABLE = 0
};

typedef struct ui_element {
    int x, y, width, height;
    char *text;
    char type, visible, capture;
    void *data;
} UI_Element;

typedef struct ui_slider_data {
    int min, max, val;
} UI_SliderData;

typedef struct ui_dropdown_data {
    int numelements, selectedelement;
} UI_DropdownData;

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
    return rng - (-i) % rng;
}

float dist(int i1, int i2, int width) {
    return sqrt((i1 % width - i2 % width) * (i1 % width - i2 % width) + (i1 / width - i2 / width) * (i1 / width - i2 / width));
}

int maxi_mag(int lhs, int rhs) {
    if (lhs < 0) lhs = -lhs;
    if (rhs < 0) rhs = -rhs;
    if (lhs > rhs) return lhs;
    else return rhs;
}

char ContainsPoint(GridData gd, Point2D pt) {
    return (pt.x >= 0 && pt.x < gd.width && pt.y >= 0 && pt.y < gd.height);
}

char UI_ContainsPoint(UI_Element ele, Point2D pt) {
    return (pt.x >= ele.x && pt.x < ele.x + ele.width && pt.y >= ele.y && pt.y < ele.y + ele.height);
}

// ALGORITHMS
//*
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

            float g_temp = gd.grid[current].g + dist(current, next, gd.width);
            float h_temp = dist(next, gd.end_pos, gd.width);
            float f_temp = g_temp + h_temp;
            if (gd.grid[next].loc == LOC_GRID) {
                gd.grid[next] = (Node){current, f_temp, g_temp, h_temp, LOC_OPEN};
                list_add(openList, next);
            } else if (gd.grid[next].loc == LOC_OPEN && g_temp < gd.grid[next].g) {
                gd.grid[next] = (Node){current, f_temp, g_temp, h_temp, gd.grid[next].loc};
            }
        }
    }
    return ALGO_INPROGRESS;
}
//*/
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

                float g_temp = gd.grid[current].g + dist(current, next, gd.width);

                if (gd.grid[next].loc == LOC_GRID) {
                    gd.grid[next] = (Node){current, g_temp + dist(next, gd.end_pos, gd.width), g_temp, -1, LOC_OPEN};
                    list_add(openList, next);
                } else if (gd.grid[next].loc == LOC_OPEN && g_temp < gd.grid[next].g) {
                    gd.grid[next] = (Node){current, g_temp + dist(next, gd.end_pos, gd.width), g_temp, -1, gd.grid[next].loc};
                }
            }
        }
    }
    return ALGO_COMPLETE;
}

int main() {
    int screen_width = 800, screen_height = 800;
    InitWindow(screen_width, screen_height, "A-Star Demo");
    SetTargetFPS(60);

    UI_Element menu_area = {0, 0, 800, 100, 0, UI_WINDOW, UI_INVISIBLE, UI_NONINTERACTABLE, 0};
    UI_Element window_area = {0, 100, 800, 700, 0, UI_WINDOW, UI_INVISIBLE, UI_NONINTERACTABLE, 0};
    UI_SliderData speedslider_data = {0, 120, 0};
    char str[50] = "Speed: ";
    UI_Element buttons[8] = {
        (UI_Element){10, 10, 120, 35, "Start", UI_BUTTON, UI_VISIBLE, UI_INTERACTABLE, 0},
        (UI_Element){10, 55, 120, 35, "Clear All", UI_BUTTON, UI_VISIBLE, UI_INTERACTABLE, 0},
        (UI_Element){140, 55, 120, 35, "Clear Paths", UI_BUTTON, UI_VISIBLE, UI_INTERACTABLE, 0},
        (UI_Element){140, 10, 120, 35, str, UI_TEXT, UI_VISIBLE, UI_NONINTERACTABLE, 0},
        (UI_Element){270, 10, 120, 35, "SPEEDSLIDER", UI_SLIDER, UI_VISIBLE, UI_INTERACTABLE, &speedslider_data},
        (UI_Element){0},
        (UI_Element){0},
        (UI_Element){0}
    };

    // color legend for grid
    Color color_palette[8] = {WHITE, LIGHTGRAY, GREEN, BLACK, YELLOW, RED, BLUE};

    Node grid[10000];
    GridData gd = {grid, 100, 100, 10000, 912, 1225};
    for (int i = 0; i < gd.size; i++) grid[i] = (Node){-1, -1, -1, -1, LOC_GRID};

    int open_raw[10000];
    NList openList = (NList){open_raw, 0, 10000};

    int count = 0, status = 0;
    char left_mouse_pressed = 0, dragging_start = 0, dragging_end = 0;

    Point2D mouse = {0, 0}, mouse_last = {0, 0}, camera = {200, 0};
    int cell_width = 20;

    char started = 0;

    while (!WindowShouldClose()) {
        mouse_last.x = mouse.x;
        mouse_last.y = mouse.y;
        mouse.x = GetMouseX();
        mouse.y = GetMouseY();

        if (IsKeyPressed(KEY_S)) {
            printf("saving\n");
            FILE *fp;
            fp = fopen("grid.sav", "wb");
            fwrite(&gd, sizeof(GridData), 1, fp);
            fwrite(grid, sizeof(Node), 10000, fp);
            fclose(fp);
        }
        if (IsKeyPressed(KEY_L)) {
            printf("loading\n");
            FILE *fp;
            fp = fopen("grid.sav", "rb");
            fread(&gd, sizeof(GridData), 1, fp);
            fread(grid, sizeof(Node), 10000, fp);
            fclose(fp);
        }


        if (IsKeyDown(KEY_RIGHT)) {
            camera.x += 10;
            if (camera.x + window_area.width > gd.width * cell_width) camera.x = gd.width * cell_width - window_area.width;
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
            if (camera.y + window_area.height > gd.height * cell_width) camera.y = gd.height * cell_width - window_area.height;
        }


        if (IsMouseButtonDown(0)) {
            if (UI_ContainsPoint(menu_area, mouse)) {
                for (int i = 0; i < 4; i++) if (!left_mouse_pressed && UI_ContainsPoint(buttons[i], mouse)) {
                    printf("[%s] button clicked\n", buttons[i].text);
                    if (i == 0 && status < ALGO_FOUND) { // start button
                        started = 1;
                        break;
                    } else if (i == 1) { // reset all button
                        for (int i = 0; i < gd.size; i++) {
                            gd.grid[i].loc = LOC_GRID;
                            gd.grid[i].parent = -1;
                        }
                        openList.length = 0;
                        status = 0;
                        break;
                    } else if (i == 2) { // clear paths
                        for (int i = 0; i < gd.size; i++) if (gd.grid[i].loc != LOC_WALL) gd.grid[i].loc = LOC_GRID;
                        openList.length = 0;
                        status = 0;
                        printf("paths cleared\n");
                        break;
                    }
                }
                if (UI_ContainsPoint(buttons[4], mouse)) {
                    (*(UI_SliderData *)(buttons[4].data)).val = (int)((float)(mouse.x - buttons[4].x) / buttons[4].width * ((*(UI_SliderData *)(buttons[4].data)).max - (*(UI_SliderData *)(buttons[4].data)).min) + (*(UI_SliderData *)(buttons[4].data)).min);
                }
            }
            else if (UI_ContainsPoint(window_area, mouse)) { // clicked on canvas area
                int pos = (mouse.y + camera.y - 100) / cell_width * gd.width + (mouse.x + camera.x) / cell_width;

                if (IsKeyDown(KEY_SPACE)) {
                    camera.x += mouse_last.x - mouse.x;
                    camera.y += mouse_last.y - mouse.y;

                    if (camera.x + window_area.width > (gd.width - 1) * cell_width + screen_width) camera.x = (gd.width - 1) * cell_width;
                    if (camera.x < cell_width - window_area.width) camera.x = cell_width - window_area.width;
                    if (camera.y + window_area.height > (gd.height - 1) * cell_width + window_area.height) camera.y = (gd.height - 1) * cell_width;
                    if (camera.y < cell_width - window_area.height) camera.y = cell_width - window_area.height;
                }
                else if (dragging_start) gd.start_pos = (mouse.y + camera.y - 100) / cell_width * gd.width + (mouse.x + camera.x) / cell_width;
                else if (dragging_end) gd.end_pos = (mouse.y + camera.y - 100) / cell_width * gd.width + (mouse.x + camera.x) / cell_width;
                else if (!left_mouse_pressed && pos == gd.start_pos) dragging_start = 1;
                else if (!left_mouse_pressed && pos == gd.end_pos) dragging_end = 1;
                else {
                    Point2D wpos = {mouse.x + camera.x, mouse.y + camera.y - 100}, wpos_last = {mouse_last.x + camera.x, mouse_last.y + camera.y - 100};
                    int dist = maxi_mag((wpos.x / cell_width - wpos_last.x / cell_width), (wpos.y / cell_width - wpos_last.y / cell_width));
                    for (int i = 0; i <= dist; i++) {
                        float t = dist == 0 ? 0 : (float)i / (float)dist;
                        int lerp_x = ((float)wpos_last.x / cell_width * (1.0f - t) + (float)wpos.x / cell_width * t),
                        lerp_y = ((float)wpos_last.y / cell_width * (1.0f - t) + (float)wpos.y / cell_width * t);
                        if (ContainsPoint(gd, (Point2D){lerp_x, lerp_y}) && lerp_y * gd.width + lerp_x != gd.start_pos && lerp_y * gd.width + lerp_x != gd.end_pos) grid[lerp_y * gd.width + lerp_x].loc = LOC_WALL;
                    }
                }
            }
            left_mouse_pressed = 1;

        } else if (left_mouse_pressed) {
            left_mouse_pressed = 0;
            if (dragging_start) dragging_start = 0;
            if (dragging_end) dragging_end = 0;
        }



        //if (IsKeyDown(KEY_ENTER) && status <= 1) for (int i = 0; i < 10 && status <= 1; i++) status = AStarStep(gd, &openList, status);

        int size_diff = (int)GetMouseWheelMove();
        if (size_diff != 0) {
            int cell_width_previous = cell_width;
            cell_width += size_diff;
            if (cell_width > 50) cell_width = 50;
            //if (cell_width * gd.width < window_area.width) cell_width = window_area.width / gd.width;
            //if (cell_width * gd.height < window_area.height) cell_width = window_area.height / gd.height;
            if (cell_width < 1) cell_width = 1;

            camera.x = (camera.x + mouse.x) * cell_width / cell_width_previous - mouse.x;
            camera.y = (camera.y + mouse.y - window_area.y) * cell_width / cell_width_previous - mouse.y + window_area.y;

            if (camera.x + window_area.width > (gd.width - 1) * cell_width + screen_width) camera.x = (gd.width - 1) * cell_width;
            if (camera.x < cell_width - window_area.width) camera.x = cell_width - window_area.width;
            if (camera.y + window_area.height > (gd.height - 1) * cell_width + window_area.height) camera.y = (gd.height - 1) * cell_width;
            if (camera.y < cell_width - window_area.height) camera.y = cell_width - window_area.height;

        }

        if (started) {
            int v = (*(UI_SliderData *)(buttons[4].data)).val;
            if (v == 0) status = AStarFull(gd, &openList);
            else if (v < 10) for (int i = 0; i < 10 / v; i++) status = AStarStep(gd, &openList, status);
            else if (count % (v / 10) == 0) status = AStarStep(gd, &openList, status);
            if (status >= ALGO_FOUND) started = 0;
        }

        // only update when slider changes
        sprintf(buttons[3].text, "Speed: %i", (*(UI_SliderData *)(buttons[4].data)).val);


        BeginDrawing();
            ClearBackground(BLACK);
            for (int i = 0; i < gd.size; i++) {
                if (i % gd.width * cell_width >= camera.x - cell_width && i % gd.width * cell_width <= camera.x + window_area.width + cell_width && i / gd.width * cell_width >= camera.y - cell_width && i / gd.width * cell_width <= camera.y + window_area.height + cell_width) {
                    DrawRectangle(i % gd.width * cell_width - camera.x, 100 + i / gd.width * cell_width - camera.y, cell_width, cell_width, color_palette[grid[i].loc]);
                }
            }
            DrawRectangle(gd.end_pos % gd.width * cell_width - camera.x, 100 + gd.end_pos / gd.width * cell_width - camera.y, cell_width, cell_width, color_palette[6]);
            DrawRectangle(gd.start_pos % gd.width * cell_width - camera.x, 100 + gd.start_pos / gd.width * cell_width - camera.y, cell_width, cell_width, color_palette[5]);

            DrawRectangle(0, 0, 800, 100, LIGHTGRAY);

            for (int i = 0; i < 5; i++) {
                switch (buttons[i].type) {
                    case UI_BUTTON: {
                        DrawRectangle(buttons[i].x, buttons[i].y, buttons[i].width, buttons[i].height, WHITE);
                        DrawText(buttons[i].text, buttons[i].x, buttons[i].y, 20, BLACK);
                        break;
                    }
                    case UI_TEXT: {
                        DrawText(buttons[i].text, buttons[i].x, buttons[i].y, 20, BLACK);
                        break;
                    }
                    case UI_SLIDER: {
                        DrawRectangle(buttons[i].x, buttons[i].y, buttons[i].width, buttons[i].height, WHITE);
                        DrawRectangle(buttons[i].x + (*(UI_SliderData *)(buttons[4].data)).val - 5, buttons[i].y + 15, 10, 10, BLACK);
                        break;
                    }
                }

            }
        EndDrawing();


        count++;
    }

}
