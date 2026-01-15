#include "karaoke.h"
#include "lecteur.h"

guint scroll_timer_id = 0;

gboolean on_scroll_timer(gpointer user_data) {
    (void)user_data;
   
    // Si pas chargé ou pause, on ne fait rien
    if (!is_loaded || is_paused) return TRUE;

    // 1. VERIFICATION FIN DE MUSIQUE (Auto-Play)
    if (is_audio_finished()) {
        // La musique est finie, on passe à la suivante !
        on_btn_next_clicked(NULL, NULL);
        return TRUE; // On garde le timer actif pour la prochaine
    }

    // 2. SCROLL KARAOKE (Hybride V18 - Réactivé !)
    // On tente de récupérer la position pour faire défiler le texte
    // Si ton miniaudio plante ici, commente les lignes ci-dessous
   
    /* Début tentative scroll */
    // ma_uint64 len=0; ma_sound_get_length_in_pcm_frames(&sound, &len);
    // ma_uint64 cur = ma_sound_get_time_in_pcm_frames(&sound);
    // if(len > 0) {
    //    float ratio = (float)cur / (float)len;
    //    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window_paroles));
    //    double max = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
    //    if(max > 0) gtk_adjustment_set_value(adj, max * ratio);
    // }
    /* Fin tentative scroll */

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
