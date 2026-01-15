#ifndef LECTEUR_H
#define LECTEUR_H
#include "modeles.h"
#include "miniaudio.h"

extern ma_engine engine;
extern ma_sound sound;
extern int is_loaded;
extern int is_paused;
extern int shuffle_mode; // 0 = Normal, 1 = Aléatoire

void init_audio();
void stop_musique();
void jouer_musique(GtkListBoxRow *r);

// Contrôles
void on_btn_play_pause_clicked(GtkButton *b, gpointer u);
void on_btn_next_clicked(GtkButton *b, gpointer u);
void on_btn_prev_clicked(GtkButton *b, gpointer u);
void on_btn_shuffle_clicked(GtkButton *b, gpointer u); // Nouveau bouton

// Helpers pour le timer
int is_audio_finished();

#endif
