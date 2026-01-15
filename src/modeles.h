#ifndef MODELES_H
#define MODELES_H

#include <gtk/gtk.h>
#include <sqlite3.h>

// --- VARIABLES GLOBALES ---
extern GtkWidget *main_window;
extern GtkWidget *lbl_status;
extern GtkWidget *list_box_catalogue;
extern GtkWidget *list_box_playlists;
extern GtkWidget *list_box_contenu_playlist;
extern GtkWidget *list_box_artistes;
extern GtkWidget *list_box_contenu_artiste;
extern GtkWidget *lbl_lecture_titre;
extern GtkWidget *lbl_lecture_artiste;
extern GtkWidget *lbl_current_title;
extern GtkWidget *txt_paroles_buffer;
extern GtkWidget *img_pochette;
extern GtkWidget *scrolled_window_paroles;
extern GtkWidget *btn_play_pause;

// V27 : Nouveaux widgets pour la durée
extern GtkWidget *lbl_duree;      // Affiche "00:12 / 03:45"
extern GtkWidget *scale_progress; // Barre de défilement

extern const char *DB_FILE;

// --- FONCTIONS UTILES ---
void trim(char *s);
void set_status(const char *message, int is_error);
char* get_associated_file(const char *mp3_path, const char *new_ext);
char* lire_contenu_fichier(const char *path);

#endif
