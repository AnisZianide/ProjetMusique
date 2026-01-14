#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <gtk/gtk.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

const char *DB_FILE = "data/musique.db";
GtkWidget *list_box_catalogue;
GtkWidget *lbl_status;
GtkWidget *main_window;
GtkWidget *btn_play_pause; // On garde une référence pour changer l'icône

// --- VARIABLES AUDIO ---
ma_engine engine;
ma_sound sound;
int is_loaded = 0;  // Est-ce qu'un fichier est chargé en mémoire ?
int is_paused = 0;  // Est-ce qu'on est en pause ?

// NOUVEAU : On retient quelle ligne joue actuellement pour faire Suivant/Précédent
GtkListBoxRow *current_row = NULL;

// --- UTILITAIRES ---
void trim(char *s) {
    if (!s) return;
    char *p = s;
    int l = strlen(p);
    if (l == 0) return;
    while(l > 0 && isspace(p[l - 1])) p[--l] = 0;
    while(*p && isspace(*p)) ++p, --l;
    memmove(s, p, l + 1);
}

void set_status(const char *message, int is_error) {
    char *markup;
    if (is_error) markup = g_markup_printf_escaped("<span foreground='red'><b>%s</b></span>", message);
    else markup = g_markup_printf_escaped("<span foreground='green'><b>%s</b></span>", message);
    gtk_label_set_markup(GTK_LABEL(lbl_status), markup);
    g_free(markup);
}

// --- LOGIQUE AUDIO ---

void stop_musique() {
    if (is_loaded) {
        ma_sound_stop(&sound);
        ma_sound_uninit(&sound);
        is_loaded = 0;
        is_paused = 0;
    }
}

// Fonction pour récupérer le chemin caché dans une ligne
char* get_chemin_from_row(GtkListBoxRow *row) {
    if (!row) return NULL;
    GtkWidget *box_row = gtk_list_box_row_get_child(row);
    GtkWidget *btn_del = gtk_widget_get_last_child(box_row);
    return (char *)g_object_get_data(G_OBJECT(btn_del), "mon_chemin");
}

void jouer_musique(GtkListBoxRow *row) {
    if (!row) return;

    char *chemin = get_chemin_from_row(row);
    if (!chemin) return;

    // 1. Reset
    stop_musique();

    // 2. Load
    ma_result result = ma_sound_init_from_file(&engine, chemin, 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        set_status("Erreur : Impossible de lire ce fichier.", 1);
        return;
    }

    // 3. Start
    ma_sound_start(&sound);
    is_loaded = 1;
    is_paused = 0;
    current_row = row; // On mémorise la ligne en cours

    // Mise à jour visuelle (Sélection de la ligne + Icone Pause)
    gtk_list_box_select_row(GTK_LIST_BOX(list_box_catalogue), row);
   
    // Change l'icône du bouton en "Pause" car ça joue
    gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-pause-symbolic");
   
    printf("[AUDIO] Lecture : %s\n", chemin);
    set_status("Lecture en cours... 🎵", 0);
}

// --- CONTROLES (PLAY/PAUSE/NEXT/PREV) ---

void on_btn_play_pause_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    if (!is_loaded) return; // Rien à faire si pas de musique

    if (is_paused) {
        // On reprend
        ma_sound_start(&sound);
        is_paused = 0;
        gtk_button_set_icon_name(btn, "media-playback-pause-symbolic");
        set_status("Lecture reprise.", 0);
    } else {
        // On met en pause
        ma_sound_stop(&sound); // Stop sans uninit = Pause
        is_paused = 1;
        gtk_button_set_icon_name(btn, "media-playback-start-symbolic");
        set_status("En pause.", 0);
    }
}

void on_btn_next_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    if (!current_row) return;

    // Calcul de l'index suivant
    int index = gtk_list_box_row_get_index(current_row);
    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_box_catalogue), index + 1);

    if (next_row) {
        jouer_musique(next_row);
    } else {
        set_status("Fin de la liste.", 1);
    }
}

void on_btn_prev_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    if (!current_row) return;

    int index = gtk_list_box_row_get_index(current_row);
    if (index > 0) {
        GtkListBoxRow *prev_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_box_catalogue), index - 1);
        if (prev_row) jouer_musique(prev_row);
    }
}

// --- BDD (Inchangé) ---
void init_db() {
    sqlite3 *db;
    char *err_msg = 0;
    int rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) return;
    const char *sql = "CREATE TABLE IF NOT EXISTS musiques (id INTEGER PRIMARY KEY AUTOINCREMENT, chemin TEXT UNIQUE, titre TEXT, artiste TEXT);";
    sqlite3_exec(db, sql, 0, 0, &err_msg);
    sqlite3_close(db);
}
int ajouter_bdd(const char *chemin, const char *titre, const char *artiste) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int succes = 0;
    sqlite3_open(DB_FILE, &db);
    const char *sql = "INSERT INTO musiques (chemin, titre, artiste) VALUES (?, ?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, chemin, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, titre, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, artiste, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) succes = 1;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return succes;
}
void supprimer_bdd(const char *chemin) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(DB_FILE, &db);
    const char *sql = "DELETE FROM musiques WHERE chemin = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, chemin, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

// --- INTERFACE ---

void on_supprimer_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *row = GTK_WIDGET(user_data);
    char *chemin = (char *)g_object_get_data(G_OBJECT(btn), "mon_chemin");
    if (chemin) {
        // Si c'est la musique en cours, on stop
        if (current_row == GTK_LIST_BOX_ROW(row)) stop_musique();
        supprimer_bdd(chemin);
        set_status("Musique supprimée.", 0);
    }
    gtk_list_box_remove(GTK_LIST_BOX(list_box_catalogue), row);
}

// Callback double clic
void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box; (void)user_data;
    jouer_musique(row);
}

void ajouter_ligne_visuelle(const char *chemin, const char *titre, const char *artiste) {
    if (!titre) titre = "Titre inconnu";
    if (!artiste) artiste = "Artiste inconnu";

    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name("audio-x-generic");
   
    char *label_text = g_markup_printf_escaped("<b>%s</b>\n<span size='small' foreground='gray'>%s</span>", titre, artiste);
    GtkWidget *label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), label_text);
    g_free(label_text);

    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
   
    GtkWidget *btn_del = gtk_button_new_from_icon_name("user-trash-symbolic");
    g_object_set_data_full(G_OBJECT(btn_del), "mon_chemin", g_strdup(chemin), g_free);
   
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), btn_del);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    gtk_list_box_append(GTK_LIST_BOX(list_box_catalogue), row);
    g_signal_connect(btn_del, "clicked", G_CALLBACK(on_supprimer_clicked), row);
}

void charger_catalogue() {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(DB_FILE, &db);
    const char *sql = "SELECT chemin, titre, artiste FROM musiques;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *c = (const char *)sqlite3_column_text(stmt, 0);
            const char *t = (const char *)sqlite3_column_text(stmt, 1);
            const char *a = (const char *)sqlite3_column_text(stmt, 2);
            ajouter_ligne_visuelle(c ? c : "", t ? t : "Inconnu", a ? a : "Inconnu");
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void on_fichier_choisi(GtkNativeDialog *dialog, int response_id, gpointer user_data) {
    (void)user_data;
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(chooser);
        char *chemin = g_file_get_path(file);
        if (chemin) {
            char *nom_fichier = g_path_get_basename(chemin);
            char *dot = strrchr(nom_fichier, '.');
            if (dot) *dot = '\0';
            char titre[256]; char artiste[256];
            char *sep = strstr(nom_fichier, " - ");
            if (sep) {
                *sep = '\0';
                snprintf(artiste, sizeof(artiste), "%s", nom_fichier);
                snprintf(titre, sizeof(titre), "%s", sep + 3);
            } else {
                snprintf(titre, sizeof(titre), "%s", nom_fichier);
                strcpy(artiste, "Artiste inconnu");
            }
            trim(titre); trim(artiste);
            if (ajouter_bdd(chemin, titre, artiste)) {
                ajouter_ligne_visuelle(chemin, titre, artiste);
                set_status("Succès : Importé !", 0);
            } else {
                set_status("Erreur : Déjà présent.", 1);
            }
            g_free(nom_fichier); g_free(chemin);
        }
        g_object_unref(file);
    }
    g_object_unref(dialog);
}

void on_ouvrir_explorateur_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    GtkFileChooserNative *native = gtk_file_chooser_native_new("Choisir MP3", GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Ouvrir", "_Annuler");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Fichiers Audio MP3");
    gtk_file_filter_add_pattern(filter, "*.mp3");
    gtk_file_filter_add_pattern(filter, "*.MP3");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(native), filter);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(native), filter);
    g_signal_connect(native, "response", G_CALLBACK(on_fichier_choisi), NULL);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) printf("Erreur audio init\n");
    init_db();

    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Player Musique V2 (Contrôles)");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 600, 600);
   
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 15);
    gtk_widget_set_margin_bottom(box, 15);
    gtk_widget_set_margin_start(box, 15);
    gtk_widget_set_margin_end(box, 15);
    gtk_window_set_child(GTK_WINDOW(main_window), box);
   
    GtkWidget *lbl_titre = gtk_label_new("<b>Ma Bibliothèque</b>");
    gtk_label_set_use_markup(GTK_LABEL(lbl_titre), TRUE);
    gtk_box_append(GTK_BOX(box), lbl_titre);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(box), scroll);
   
    list_box_catalogue = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list_box_catalogue);
    g_signal_connect(list_box_catalogue, "row-activated", G_CALLBACK(on_row_activated), NULL);

    // --- BARRE DE CONTRÔLE (NOUVEAU) ---
    GtkWidget *controls_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(controls_box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), controls_box);

    GtkWidget *btn_prev = gtk_button_new_from_icon_name("media-skip-backward-symbolic");
    btn_play_pause = gtk_button_new_from_icon_name("media-playback-start-symbolic"); // Variable globale
    GtkWidget *btn_next = gtk_button_new_from_icon_name("media-skip-forward-symbolic");

    // On grossit un peu les boutons
    gtk_widget_set_size_request(btn_play_pause, 60, 60);
    gtk_widget_set_size_request(btn_prev, 40, 40);
    gtk_widget_set_size_request(btn_next, 40, 40);

    gtk_box_append(GTK_BOX(controls_box), btn_prev);
    gtk_box_append(GTK_BOX(controls_box), btn_play_pause);
    gtk_box_append(GTK_BOX(controls_box), btn_next);

    // Connexion des signaux
    g_signal_connect(btn_play_pause, "clicked", G_CALLBACK(on_btn_play_pause_clicked), NULL);
    g_signal_connect(btn_next, "clicked", G_CALLBACK(on_btn_next_clicked), NULL);
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(on_btn_prev_clicked), NULL);

    // Bouton Ajouter
    GtkWidget *btn_explorer = gtk_button_new_with_label("📂 Ajouter un son");
    gtk_widget_set_halign(btn_explorer, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), btn_explorer);
    g_signal_connect(btn_explorer, "clicked", G_CALLBACK(on_ouvrir_explorateur_clicked), NULL);

    lbl_status = gtk_label_new("");
    gtk_box_append(GTK_BOX(box), lbl_status);
   
    charger_catalogue();
    gtk_window_present(GTK_WINDOW(main_window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.projet.controls", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    ma_engine_uninit(&engine);
    g_object_unref(app);
    return status;
}
