// libadwaita-sdl.c
#include "libadwaita-sdl.h"
#include <string.h>

// --- Helper Palette Constants (Libadwaita Dark Theme / AdwDark) ---
static const Clay_Color ADW_COLOR_BG =          { 30,  30,  30, 255 }; // Window base
static const Clay_Color ADW_COLOR_HEADER =      { 45,  45,  45, 255 }; // Headerbar fill
static const Clay_Color ADW_COLOR_CARD =        { 54,  54,  54, 255 }; // Card components
static const Clay_Color ADW_COLOR_ACCENT =      { 53, 132, 228, 255 }; // Blue accent (adw_accent)
static const Clay_Color ADW_COLOR_TEXT_MUTED =  { 150, 150, 150, 255 };
static const Clay_Color ADW_COLOR_TEXT_LIGHT =  { 255, 255, 255, 255 };
static const Clay_Color ADW_COLOR_BORDER =      { 20,  20,  20, 255 };

// --- AdwAdjustment Implementation ---
AdwAdjustment* adw_adjustment_new(double value, double lower, double upper, double step, double page) {
    AdwAdjustment *adj = malloc(sizeof(AdwAdjustment));
    adj->value = value;
    adj->lower = lower;
    adj->upper = upper;
    adj->step_increment = step;
    adj->page_size = page;
    return adj;
}

// --- AdwScrolledWindow Implementation ---
AdwScrolledWindow* adw_scrolled_window_new(AdwAdjustment *v_adjust) {
    AdwScrolledWindow *self = malloc(sizeof(AdwScrolledWindow));
    self->v_adjustment = v_adjust ? v_adjust : adw_adjustment_new(0, 0, 0, 1, 10);
    self->v_scrollbar_visible = true;
    self->child_data = NULL;
    self->child_render_callback = NULL;
    return self;
}

void adw_scrolled_window_set_child(AdwScrolledWindow *self, void *data, AdwWidgetRenderFunc callback) {
    self->child_data = data;
    self->child_render_callback = callback;
}

void adw_scrolled_window_render(AdwScrolledWindow *self) {
    AdwAdjustment *adj = self->v_adjustment;
    
    CLAY(
        CLAY_LAYOUT({ .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }),
        CLAY_ID("AdwScrolledWindow")
    ) {
        // Main view content panel
        CLAY(CLAY_LAYOUT({ .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }), CLAY_ID("AdwScrolledViewport")) {
            if (self->child_render_callback && self->child_data) {
                self->child_render_callback(self->child_data);
            }
        }
        
        // GNOME HIG Smooth Track overlay calculation
        if (self->v_scrollbar_visible && adj->upper > adj->page_size) {
            double range = adj->upper - adj->lower;
            double progress = adj->value / (range - adj->page_size);
            if (progress < 0.0) progress = 0.0;
            if (progress > 1.0) progress = 1.0;
            
            double thumb_ratio = adj->page_size / adj->upper;
            if (thumb_ratio < 0.1) thumb_ratio = 0.1;
            
            CLAY(
                CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(8), .height = CLAY_SIZING_GROW(0), .layoutDirection = CLAY_LAYOUT_DIRECTION_VERTICAL }),
                CLAY_BACKGROUND_COLOR(ADW_COLOR_HEADER),
                CLAY_ID("AdwScrollTrough")
            ) {
                if (progress > 0.0) {
                    CLAY(CLAY_LAYOUT({ .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(progress * (1.0 - thumb_ratio)) }));
                }
                CLAY(
                    CLAY_LAYOUT({ .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_PERCENT(thumb_ratio) }),
                    CLAY_BACKGROUND_COLOR(ADW_COLOR_TEXT_MUTED),
                    CLAY_CORNER_RADIUS(4),
                    CLAY_ID("AdwScrollThumb")
                ) {}
            }
        }
    }
}

void adw_scrolled_window_handle_event(AdwScrolledWindow *self, SDL_Event *event) {
    if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        AdwAdjustment *adj = self->v_adjustment;
        double max_scroll = adj->upper - adj->page_size;
        if (max_scroll < 0) max_scroll = 0;
        
        adj->value += -event->wheel.y * adj->step_increment;
        if (adj->value < adj->lower) adj->value = adj->lower;
        if (adj->value > max_scroll) adj->value = max_scroll;
        
        Clay_QueueRefresh();
    }
}

// --- AdwTabBar Implementation ---
AdwTabBar* adw_tab_bar_new(void) {
    AdwTabBar *self = calloc(1, sizeof(AdwTabBar));
    return self;
}

void adw_tab_bar_append(AdwTabBar *self, const char *title) {
    if (self->tab_count < ADW_MAX_TABS) {
        self->titles[self->tab_count] = strdup(title);
        self->tab_count++;
    }
}

void adw_tab_bar_render(AdwTabBar *self) {
    CLAY(
        CLAY_LAYOUT({ .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(36) }, .alignment = { .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER } }),
        CLAY_BACKGROUND_COLOR(ADW_COLOR_BORDER),
        CLAY_ID("AdwTabBarContainer")
    ) {
        for (int i = 0; i < self->tab_count; i++) {
            bool is_active = (i == self->active_tab_idx);
            
            CLAY(
                CLAY_LAYOUT({ .padding = { .left = 12, .right = 12 }, .height = CLAY_SIZING_GROW(0), .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, .alignment = { .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER } }),
                CLAY_BACKGROUND_COLOR(is_active ? ADW_COLOR_BG : ADW_COLOR_HEADER),
                CLAY_ID_INDEX("AdwTabItem", i)
            ) {
                CLAY_TEXT(Clay_String_FromCstring(self->titles[i]), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, .fontSize = 13 }));
                
                // Spacing to small close capsule
                CLAY(CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(8) }));
                
                CLAY(
                    CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(14), .height = CLAY_SIZING_FIXED(14), .alignment = { .alignmentHorizontal = CLAY_ALIGN_HORIZONTAL_CENTER, .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER } }),
                    CLAY_CORNER_RADIUS(7),
                    CLAY_BACKGROUND_COLOR(is_active ? ADW_COLOR_CARD : ADW_COLOR_BORDER),
                    CLAY_ID_INDEX("AdwTabCloseBtn", i)
                ) {
                    CLAY_TEXT(Clay_String_FromCstring("×"), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_MUTED, .fontSize = 11 }));
                }
            }
            // Vertical split thread border between inactive slots
            CLAY(CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(1), .height = CLAY_SIZING_GROW(0) }), CLAY_BACKGROUND_COLOR(ADW_COLOR_BORDER));
        }
    }
}

void adw_tab_bar_handle_event(AdwTabBar *self, SDL_Event *event) {
    if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        Clay_Vector2 mouse_pos = { event->button.x, event->button.y };
        
        for (int i = 0; i < self->tab_count; i++) {
            if (Clay_PointerOver(Clay_GetElementIdWithIndex(Clay_String_FromCstring("AdwTabCloseBtn"), i))) {
                self->tab_close_requested = true;
                self->target_close_idx = i;
                // Defer freeing structure array updates to application controller scope
                Clay_QueueRefresh();
                return;
            }
            if (Clay_PointerOver(Clay_GetElementIdWithIndex(Clay_String_FromCstring("AdwTabItem"), i))) {
                self->active_tab_idx = i;
                Clay_QueueRefresh();
                return;
            }
        }
    }
}

// --- AdwDropdown & Hamburger Menu Implementation ---
AdwDropdown* adw_dropdown_new(const char **items, int count) {
    AdwDropdown *self = malloc(sizeof(AdwDropdown));
    self->items = items;
    self->item_count = count;
    self->selected_idx = 0;
    self->is_open = false;
    self->last_click_pos = (Clay_Vector2){ 0, 0 };
    return self;
}

void adw_dropdown_render(AdwDropdown *self, const char *id_string, bool is_hamburger) {
    CLAY(
        CLAY_LAYOUT({ .padding = { .left = 10, .right = 10, .top = 6, .bottom = 6 }, .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL, .alignment = { .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER } }),
        CLAY_BACKGROUND_COLOR(ADW_COLOR_CARD),
        CLAY_CORNER_RADIUS(6),
        CLAY_ID(id_string)
    ) {
        if (is_hamburger) {
            // Standard GNOME HIG 3-line bar layout symbol tracking
            CLAY_TEXT(Clay_String_FromCstring("☰"), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, .fontSize = 14 }));
        } else {
            CLAY_TEXT(Clay_String_FromCstring(self->items[self->selected_idx]), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, .fontSize = 13 }));
            CLAY(CLAY_LAYOUT({ .width = CLAY_SIZING_FIXED(6) }));
            CLAY_TEXT(Clay_String_FromCstring("▾"), CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_MUTED, .fontSize = 12 }));
        }
    }

    // Floating overlay rendering when open
    if (self->is_open) {
        CLAY(
            CLAY_LAYOUT({ 
                .layoutDirection = CLAY_LAYOUT_DIRECTION_VERTICAL, 
                .sizing = { CLAY_SIZING_FIXED(160), CLAY_SIZING_FIT(0) },
                .padding = { .top = 4, .bottom = 4, .left = 4, .right = 4 },
                // Absolute positions calculating off the base activation trigger context bounds
                .floatingData = {
                    .pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH,
                    .attachment = { .parent = CLAY_ATTACHMENT_POINT_LEFT_BOTTOM, .widget = CLAY_ATTACHMENT_POINT_LEFT_TOP }
                }
            }),
            CLAY_BACKGROUND_COLOR(ADW_COLOR_HEADER),
            CLAY_CORNER_RADIUS(8),
            CLAY_BORDER({ .width = {1,1,1,1}, .color = ADW_COLOR_CARD }),
            CLAY_ID_INDEX("AdwDropdownMenuPopover", 1)
        ) {
            for (int i = 0; i < self->item_count; i++) {
                CLAY(
                    CLAY_LAYOUT({ .padding = { .left = 12, .right = 12, .top = 8, .bottom = 8 }, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) } }),
                    CLAY_BACKGROUND_COLOR((i == self->selected_idx) ? ADW_COLOR_ACCENT : (Clay_Color){0,0,0,0}),
                    CLAY_CORNER_RADIUS(4),
					CLAY_ID_INDEX("AdwDropdownItem", i)) 
					{
						CLAY_TEXT(Clay_String_FromCstring(self->items[i]), 
						CLAY_TEXT_CONFIG(
						{ .textColor = ADW_COLOR_TEXT_LIGHT, 
						  .fontSize = 13 }
						  )
						);
					}
				}
			}
	}
}

void adw_dropdown_handle_event(AdwDropdown *self, SDL_Event *event, const char *id_string) 
{
	if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && 
	    event->button.button == SDL_BUTTON_LEFT) 
	{
		if (Clay_PointerOver(Clay_GetElementId(Clay_String_FromCstring(id_string)))) 
		{
			self->is_open = !self->is_open;
			Clay_QueueRefresh();
			return;
		}
		
		if (self->is_open) 
		{
			for (int i = 0; i < self->item_count; i++) 
			{
				if (Clay_PointerOver(Clay_GetElementIdWithIndex(Clay_String_FromCstring("AdwDropdownItem"), i))) 
				{
					self->selected_idx = i;
					self->is_open = false;
					Clay_QueueRefresh();
					return;
				}
			}
			// Dismiss panel if click hits external viewport bounds
			self->is_open = false;
			Clay_QueueRefresh();
		}
	}
}

// --- AdwHeaderBar Implementation ---
AdwHeaderBar* adw_header_bar_new(const char *title) 
{
	AdwHeaderBar *self = malloc(sizeof(AdwHeaderBar));
	self->title = strdup(title);
	self->tab_bar = NULL;
	self->hamburger_menu = NULL;
	return self;
}

void adw_header_bar_render(AdwHeaderBar *self) 
{
	CLAY(CLAY_LAYOUT(
	{.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(46) },
	 .layoutDirection = CLAY_LAYOUT_DIRECTION_HORIZONTAL,
	 .alignment = { .alignmentVertical = CLAY_ALIGN_VERTICAL_CENTER },
	                .padding = { .left = 12, .right = 12 }
	}),
	CLAY_BACKGROUND_COLOR(ADW_COLOR_HEADER),
	CLAY_BORDER({ .width = { .bottom = 1 }, .color = ADW_COLOR_BORDER }),
	CLAY_ID("AdwHeaderBar")) 
	{
		// Left side branding or Tab section integration
		if (self->tab_bar) 
		{
			adw_tab_bar_render(self->tab_bar);
		} else 
		{
			CLAY_TEXT(Clay_String_FromCstring(self->title), 
			CLAY_TEXT_CONFIG({ .textColor = ADW_COLOR_TEXT_LIGHT, 
			                   .fontSize = 15, 
							   .fontId = 0 })
			);
		}
		// Variable elastic spacer shoving utilities rightward
		CLAY(CLAY_LAYOUT({ .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(1) } }));
		// Right hand menu section actions
		if (self->hamburger_menu) 
		{
			adw_dropdown_render(self->hamburger_menu, "AdwHeaderHamburgerBtn", true);
		}
	}
}

void adw_header_bar_handle_event(AdwHeaderBar *self, SDL_Event *event) 
{
	if (self->tab_bar) 
	{
		adw_tab_bar_handle_event(self->tab_bar, event);
	}
	if (self->hamburger_menu) 
	{
		adw_dropdown_handle_event(self->hamburger_menu, event, "AdwHeaderHamburgerBtn");
	}
}
