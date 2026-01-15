#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "lecteur.h"
#include "karaoke.h"
#include <stdlib.h>
#include <time.h> // Pour le random

ma_engine engine;
ma_sound sound;
int is_loaded = 0;
int is_paused = 0;
int shuffle_mode = 0; // Par défaut : Normal

GtkListBoxRow *current_row = NULL;
GtkListBox *current_playing_list = NULL;

void init_audio() {
    ma_engine_init(NULL, &engine);
    srand(time(NULL)); // Initialise le générateur aléatoire
}

void stop_musique() {
    if(is_loaded){ ma_sound_stop(&sound); ma_sound_uninit(&sound); is_loaded=0; is_paused=0; }
    stop_auto_scroll();
}

// Fonction pour vérifier si la musique est finie (utilisée par le timer)
int is_audio_finished() {
    if (!is_loaded) return 0;
    return ma_sound_at_end(&sound);
}

char* get_chemin_from_row(GtkListBoxRow *r) {
    if(!r)return NULL;
    GtkWidget *b=gtk_list_box_row_get_child(r);
    GtkWidget *bt=gtk_widget_get_last_child(b);
    return (char*)g_object_get_data(G_OBJECT(bt), "mon_chemin");
}

void jouer_musique(GtkListBoxRow *r) {
    if(!r)return; char *c=get_chemin_from_row(r); if(!c)return;
    GtkWidget *bx=gtk_list_box_row_get_child(r);
    GtkWidget *lb=gtk_widget_get_next_sibling(gtk_widget_get_first_child(bx));
    const char *mk=gtk_label_get_label(GTK_LABEL(lb));

    stop_musique();
    if(ma_sound_init_from_file(&engine, c, 0, NULL, NULL, &sound)!=MA_SUCCESS){ set_status("Erreur fichier.", 1); return; }
    ma_sound_start(&sound); is_loaded=1; is_paused=0;
    start_auto_scroll(); // Lance le timer qui surveillera aussi la fin de la piste
   
    current_row=r;
    GtkWidget *p=gtk_widget_get_parent(GTK_WIDGET(r));
    if(GTK_IS_LIST_BOX(p)) current_playing_list=GTK_LIST_BOX(p);
   
    gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-pause-symbolic");
    if(mk) gtk_label_set_markup(GTK_LABEL(lbl_lecture_titre), mk);
    gtk_label_set_markup(GTK_LABEL(lbl_current_title), "Lecture en cours... 🎵");

    // Chargement Paroles & Pochette
    GtkTextBuffer *buf=GTK_TEXT_BUFFER(txt_paroles_buffer);
    char *pp=get_associated_file(c, ".txt");
    if(pp){ char *cnt=lire_contenu_fichier(pp); gtk_text_buffer_set_text(buf, cnt?cnt:"(Erreur lecture)", -1); if(cnt)free(cnt); g_free(pp); }
    else{ gtk_text_buffer_set_text(buf, "\n(Pas de paroles trouvées.\nVerifiez le nom exact du fichier .txt)", -1); }
   
    char *ip=get_associated_file(c, ".jpg"); if(!ip) ip=get_associated_file(c, ".png");
    if(ip){ gtk_image_set_from_file(GTK_IMAGE(img_pochette), ip); gtk_image_set_pixel_size(GTK_IMAGE(img_pochette), 200); g_free(ip); }
    else{ gtk_image_set_from_icon_name(GTK_IMAGE(img_pochette), "audio-x-generic"); gtk_image_set_pixel_size(GTK_IMAGE(img_pochette), 150); }
}

void on_btn_play_pause_clicked(GtkButton *b, gpointer u) { (void)u; (void)b; if(!is_loaded)return; if(is_paused){ ma_sound_start(&sound); is_paused=0; gtk_button_set_icon_name(b, "media-playback-pause-symbolic"); } else{ ma_sound_stop(&sound); is_paused=1; gtk_button_set_icon_name(b, "media-playback-start-symbolic"); } }

// BOUTON SUIVANT INTELLIGENT
void on_btn_next_clicked(GtkButton *b, gpointer u) {
    (void)b; (void)u;
    if(current_row && current_playing_list){
        GtkListBoxRow *next_row = NULL;
        int i=gtk_list_box_row_get_index(current_row);
       
        // Calcul du nombre total de lignes
        int count = 0;
        while(gtk_list_box_get_row_at_index(current_playing_list, count) != NULL) count++;

        if (shuffle_mode) {
            // MODE ALEATOIRE : On prend un index au hasard
            int random_index = rand() % count;
            next_row = gtk_list_box_get_row_at_index(current_playing_list, random_index);
        } else {
            // MODE NORMAL : Suivant
            next_row = gtk_list_box_get_row_at_index(current_playing_list, i+1);
        }

        if(next_row) jouer_musique(next_row);
        else set_status("Fin de la liste.", 0);
    }
}

void on_btn_prev_clicked(GtkButton *b, gpointer u) { (void)b; (void)u; if(current_row && current_playing_list){ int i=gtk_list_box_row_get_index(current_row); if(i>0){ GtkListBoxRow *p=gtk_list_box_get_row_at_index(current_playing_list, i-1); if(p) jouer_musique(p); } } }

// BOUTON SHUFFLE
void on_btn_shuffle_clicked(GtkButton *b, gpointer u) {
    (void)u;
    shuffle_mode = !shuffle_mode; // Bascule ON/OFF
    if (shuffle_mode) {
        gtk_widget_add_css_class(GTK_WIDGET(b), "suggested-action"); // Met le bouton en couleur (GTK style)
        set_status("Mode Aléatoire : ON 🔀", 0);
    } else {
        gtk_widget_remove_css_class(GTK_WIDGET(b), "suggested-action");
        set_status("Mode Aléatoire : OFF ➡️", 0);
    }
}
