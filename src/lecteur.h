#ifndef LECTEUR_H
#define LECTEUR_H
#include "modeles.h"
#include "miniaudio.h"

extern ma_engine engine;
extern ma_sound sound;
extern int is_loaded;
extern int is_paused;
extern int shuffle_mode;

void init_audio();
void stop_musique();
void jouer_musique(GtkListBoxRow *r);

// V27 : Fonctions temps
float get_audio_duration();
float get_audio_position();

void on_btn_play_pause_clicked(GtkButton *b, gpointer u);
void on_btn_next_clicked(GtkButton *b, gpointer u);
void on_btn_prev_clicked(GtkButton *b, gpointer u);
void on_btn_shuffle_clicked(GtkButton *b, gpointer u);
int is_audio_finished();

#endif
