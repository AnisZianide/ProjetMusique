#include "karaoke.h"
#include "lecteur.h"

guint scroll_timer_id = 0;

// Version Safe pour V23 (ne plante pas la compilation)
gboolean on_scroll_timer(gpointer user_data) {
    (void)user_data;
    if (!is_loaded || is_paused) return TRUE;
    // Calcul désactivé temporairement pour compatibilité
    return TRUE;
}

void start_auto_scroll() {
    if (scroll_timer_id == 0) {
        scroll_timer_id = g_timeout_add(100, on_scroll_timer, NULL);
    }
}

void stop_auto_scroll() {
    if (scroll_timer_id > 0) {
        g_source_remove(scroll_timer_id);
        scroll_timer_id = 0;
    }
}
