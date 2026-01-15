#include "karaoke.h"
#include "lecteur.h"
#include <stdio.h>

guint scroll_timer_id = 0;

gboolean on_scroll_timer(gpointer user_data) {
    (void)user_data;
   
    if (!is_loaded || is_paused) return TRUE;

    // 1. Mise à jour de la barre et du texte (V27)
    float pos = get_audio_position();
    float dur = get_audio_duration();

    if(dur > 0) {
        // Barre
        gtk_range_set_range(GTK_RANGE(scale_progress), 0, dur);
        gtk_range_set_value(GTK_RANGE(scale_progress), pos);
       
        // Texte "01:20 / 03:45"
        int m_cur = (int)pos / 60; int s_cur = (int)pos % 60;
        int m_tot = (int)dur / 60; int s_tot = (int)dur % 60;
       
        char *txt = g_strdup_printf("%02d:%02d / %02d:%02d", m_cur, s_cur, m_tot, s_tot);
        gtk_label_set_text(GTK_LABEL(lbl_duree), txt);
        g_free(txt);
    }

    // 2. Auto-Play (Fin de piste)
    if (is_audio_finished()) {
        on_btn_next_clicked(NULL, NULL);
        return TRUE;
    }

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
