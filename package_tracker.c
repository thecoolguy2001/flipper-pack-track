#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PACKAGES 5
#define VISIBLE_ROWS 4
#define ROW_HEIGHT 13

typedef enum {
    StatusPending,
    StatusInTransit,
    StatusOutForDelivery,
    StatusDelivered,
    StatusException,
} PackageStatus;

typedef struct {
    const char* carrier;
    const char* tracking;
    const char* label;
    const char* last_update;
    const char* location;
    PackageStatus status;
} Package;

typedef enum {
    ScreenList,
    ScreenDetail,
} Screen;

typedef enum {
    EventTypeInput,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} TrackerEvent;

typedef struct {
    Screen screen;
    uint8_t selected;
    uint8_t scroll;
    FuriMutex* mutex;
} TrackerState;

static const Package packages[MAX_PACKAGES] = {
    {
        .carrier = "UPS",
        .tracking = "1Z999AA10123456784",
        .label = "Flipper Case",
        .last_update = "Apr 17, 2:14 PM",
        .location = "Memphis, TN",
        .status = StatusInTransit,
    },
    {
        .carrier = "USPS",
        .tracking = "9400111899223596012345",
        .label = "Solder Paste",
        .last_update = "Apr 18, 8:02 AM",
        .location = "Local Facility",
        .status = StatusOutForDelivery,
    },
    {
        .carrier = "FedEx",
        .tracking = "771234567890",
        .label = "Oscilloscope",
        .last_update = "Apr 16, 9:41 PM",
        .location = "Delivered - Front Door",
        .status = StatusDelivered,
    },
    {
        .carrier = "DHL",
        .tracking = "1234567890",
        .label = "PCB Order",
        .last_update = "Apr 15, 5:30 AM",
        .location = "Shenzhen, CN",
        .status = StatusPending,
    },
    {
        .carrier = "UPS",
        .tracking = "1Z999AA10999999999",
        .label = "Replacement Screen",
        .last_update = "Apr 17, 11:00 AM",
        .location = "Address Issue",
        .status = StatusException,
    },
};

static const char* status_short(PackageStatus s) {
    switch(s) {
    case StatusPending: return "Pending";
    case StatusInTransit: return "In Transit";
    case StatusOutForDelivery: return "Out Delivery";
    case StatusDelivered: return "Delivered";
    case StatusException: return "Exception";
    }
    return "?";
}

static const char* status_full(PackageStatus s) {
    switch(s) {
    case StatusPending: return "Pending pickup";
    case StatusInTransit: return "In Transit";
    case StatusOutForDelivery: return "Out for Delivery";
    case StatusDelivered: return "Delivered";
    case StatusException: return "Delivery Exception";
    }
    return "Unknown";
}

static void draw_status_icon(Canvas* canvas, int x, int y, PackageStatus s) {
    switch(s) {
    case StatusDelivered:
        canvas_draw_disc(canvas, x + 3, y + 3, 3);
        break;
    case StatusOutForDelivery:
        canvas_draw_circle(canvas, x + 3, y + 3, 3);
        canvas_draw_dot(canvas, x + 3, y + 3);
        break;
    case StatusInTransit:
        canvas_draw_circle(canvas, x + 3, y + 3, 3);
        break;
    case StatusPending:
        canvas_draw_line(canvas, x, y + 3, x + 6, y + 3);
        break;
    case StatusException:
        canvas_draw_line(canvas, x, y, x + 6, y + 6);
        canvas_draw_line(canvas, x + 6, y, x, y + 6);
        break;
    }
}

static void draw_list(Canvas* canvas, TrackerState* state) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "Pack Track");
    canvas_draw_line(canvas, 0, 11, 127, 11);

    char count[16];
    snprintf(count, sizeof(count), "%d/%d", state->selected + 1, MAX_PACKAGES);
    canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, count);

    for(int i = 0; i < VISIBLE_ROWS && (i + state->scroll) < MAX_PACKAGES; i++) {
        int idx = i + state->scroll;
        int y = 13 + i * ROW_HEIGHT;

        if(idx == state->selected) {
            canvas_draw_box(canvas, 0, y, 128, ROW_HEIGHT);
            canvas_invert_color(canvas);
        }

        draw_status_icon(canvas, 3, y + 3, packages[idx].status);
        canvas_draw_str(canvas, 13, y + 9, packages[idx].label);

        const char* st = status_short(packages[idx].status);
        canvas_draw_str_aligned(canvas, 125, y + 9, AlignRight, AlignBottom, st);

        if(idx == state->selected) {
            canvas_invert_color(canvas);
        }
    }
}

static void draw_detail(Canvas* canvas, TrackerState* state) {
    const Package* p = &packages[state->selected];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, p->label);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    canvas_set_font(canvas, FontSecondary);

    char line[40];
    snprintf(line, sizeof(line), "%s  %s", p->carrier, status_full(p->status));
    canvas_draw_str(canvas, 2, 22, line);

    canvas_draw_str(canvas, 2, 33, "Track:");
    char trunc[20];
    strncpy(trunc, p->tracking, sizeof(trunc) - 1);
    trunc[sizeof(trunc) - 1] = '\0';
    canvas_draw_str(canvas, 30, 33, trunc);

    canvas_draw_str(canvas, 2, 44, "Where:");
    canvas_draw_str(canvas, 30, 44, p->location);

    canvas_draw_str(canvas, 2, 55, "When:");
    canvas_draw_str(canvas, 30, 55, p->last_update);

    canvas_draw_line(canvas, 0, 57, 127, 57);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "BACK: list");
}

static void render_callback(Canvas* canvas, void* ctx) {
    furi_assert(ctx);
    TrackerState* state = ctx;
    furi_mutex_acquire(state->mutex, FuriWaitForever);
    canvas_clear(canvas);

    if(state->screen == ScreenList) {
        draw_list(canvas, state);
    } else {
        draw_detail(canvas, state);
    }

    furi_mutex_release(state->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;
    TrackerEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(queue, &event, FuriWaitForever);
}

int32_t package_tracker_app(void* p) {
    UNUSED(p);

    TrackerState* state = malloc(sizeof(TrackerState));
    state->screen = ScreenList;
    state->selected = 0;
    state->scroll = 0;
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!state->mutex) {
        free(state);
        return 255;
    }

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(TrackerEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, state);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    TrackerEvent event;

    while(running) {
        if(furi_message_queue_get(queue, &event, FuriWaitForever) != FuriStatusOk) continue;
        if(event.type != EventTypeInput) continue;

        InputEvent* in = &event.input;
        if(in->type != InputTypeShort && in->type != InputTypeLong && in->type != InputTypeRepeat)
            continue;

        furi_mutex_acquire(state->mutex, FuriWaitForever);

        if(in->type == InputTypeLong && in->key == InputKeyBack) {
            running = false;
        } else if(state->screen == ScreenList) {
            if(in->key == InputKeyDown) {
                if(state->selected < MAX_PACKAGES - 1) {
                    state->selected++;
                    if(state->selected >= state->scroll + VISIBLE_ROWS) state->scroll++;
                }
            } else if(in->key == InputKeyUp) {
                if(state->selected > 0) {
                    state->selected--;
                    if(state->selected < state->scroll) state->scroll--;
                }
            } else if(in->key == InputKeyOk && in->type == InputTypeShort) {
                state->screen = ScreenDetail;
            } else if(in->key == InputKeyBack && in->type == InputTypeShort) {
                running = false;
            }
        } else {
            if(in->key == InputKeyBack && in->type == InputTypeShort) {
                state->screen = ScreenList;
            } else if(in->key == InputKeyLeft && in->type == InputTypeShort) {
                if(state->selected > 0) state->selected--;
            } else if(in->key == InputKeyRight && in->type == InputTypeShort) {
                if(state->selected < MAX_PACKAGES - 1) state->selected++;
            }
        }

        furi_mutex_release(state->mutex);
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(state->mutex);
    free(state);
    return 0;
}
