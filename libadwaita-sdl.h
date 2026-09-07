// libadwaita-sdl.h
#ifndef LIBADWAITA_SDL_H
#define LIBADWAITA_SDL_H

#include <glib.h>

// Transparent opaque pointers for safety
typedef struct _AdwApplicationWindow AdwApplicationWindow;
typedef struct _AdwPreferencesPage   AdwPreferencesPage;
typedef struct _AdwPreferencesGroup  AdwPreferencesGroup;
typedef struct _AdwPreferencesRow    AdwPreferencesRow; // Base type for rows

typedef enum {
    ADW_ROW_TYPE_SWITCH,
    ADW_ROW_TYPE_ENTRY
} AdwRowType;

// --- Framework Lifecycle ---
void                  adw_init(void);
AdwApplicationWindow* adw_application_window_new(const gchar *title, gint width, gint height);
void                  adw_application_window_present(AdwApplicationWindow *window);
gboolean              adw_application_window_handle_event(AdwApplicationWindow *window, void *sdl_event);

// --- Component Allocation ---
AdwPreferencesPage*   adw_preferences_page_new(void);
AdwPreferencesGroup*  adw_preferences_group_new(const gchar *title);

// Row Constructors
AdwPreferencesRow*    adw_switch_row_new(const gchar *title, const gchar *subtitle);
AdwPreferencesRow*    adw_entry_row_new(const gchar *title, const gchar *subtitle);

// --- Parent-Child Relationship Mappings ---
void adw_application_window_set_content(AdwApplicationWindow *window, AdwPreferencesPage *page);
void adw_preferences_page_add(AdwPreferencesPage *page, AdwPreferencesGroup *group);
void adw_preferences_group_add(AdwPreferencesGroup *group, AdwPreferencesRow *row);

// --- State Getters / Setters ---
void     adw_switch_row_set_active(AdwPreferencesRow *row, gboolean is_active);
gboolean adw_switch_row_get_active(AdwPreferencesRow *row);
const gchar* adw_entry_row_get_text(AdwPreferencesRow *row);

#endif // LIBADWAITA_SDL_H
