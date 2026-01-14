#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// On inclut la librairie de Tags (C Bindings)
#include <taglib/tag_c.h>

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
GtkWidget *btn_play_pause;

// --- AUDIO ---
ma_engine engine;
ma_sound sound;
int is_loaded = 0;
int is_paused = 0;
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

// --- LOGIQUE METADATA (TAGLIB) ---
void recuperer_infos_mp3(const char *chemin, char *titre_out, char *artiste_out, size_t size) {
    // 1. On essaie de lire les tags du fichier
    TagLib_File *file = taglib_file_new(chemin);
   
    int tags_trouves = 0;

    if (file != NULL && taglib_file_is_valid(file)) {
        TagLib_Tag *tag = taglib_file_tag(file);
        if (tag != NULL) {
            char *tag_title = taglib_tag_title(tag);
            char *tag_artist = taglib_tag_artist(tag);

            // Si les tags ne sont pas vides, on les utilise
            if (tag_title && strlen(tag_title) > 0) {
                snprintf(titre_out, size, "%s", tag_title);
                tags_trouves = 1;
            }
            if (tag_artist && strlen(tag_artist) > 0) {
                snprintf(artiste_out, size, "%s", tag_artist);
            } else {
                snprintf(artiste_out, size, "Artiste inconnu");
            }
        }
        taglib_file_free(file);
    }

    // 2. Si pas de tags (ou fichier invalide), on utilise l'ancienne méthode (Nom de fichier)
    if (!tags_trouves) {
        printf("[METADATA] Pas de tags trouvés, analyse du nom de fichier...\n");
        char *nom_base = g_path_get_basename(chemin);
        char *dot = strrchr(nom_base, '.');
        if (dot) *dot = '\0'; // Enlève .mp3

        char *sep = strstr(nom_base, " - ");
        if (sep) {
            *sep = '\0';
            snprintf(artiste_out, size, "%s", nom_base);
            snprintf(titre_out, size, "%s", sep + 3);
        } else {
            snprintf(titre_out, size, "%s", nom_base);
            snprintf(artiste_out, size, "Artiste inconnu");
        }
        g_free(nom_base);
    }

    // Nettoyage final
    trim(titre_out);
    trim(artiste_out);
}

// --- BDD ---
void init_db() {
    sqlite3 *db;
    char *err_msg = 0;
    int rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) return;
    const char *sql = "CREATE TABLE IF NOT EXISTS musiques (id INTEGER PRIMARY KEY AUTOINCREMENT, chemin TEXT UNIQUE, titre TEXT, artiste TEXT);";
    sqlite3_exec(db, sql, 0, 0, &err_msg);
    sqlite3_close(db);
}

// Vérifie si la chanson (Titre + Artiste) existe déjà, peu importe le chemin
int chanson_existe_deja(const char *titre, const char *artiste) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int existe = 0;
   
    sqlite3_open(DB_FILE, &db);
    // On cherche une ligne avec le même titre ET le même artiste (insensible à la casse)
    const char *sql = "SELECT id FROM musiques WHERE titre = ? AND artiste = ? COLLATE NOCASE;";
   
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, titre, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, artiste, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            existe = 1;
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return existe;
}

int ajouter_bdd(const char *chemin, const char *titre, const char *artiste) {
    // ÉTAPE IMPORTANTE : On vérifie les doublons sémantiques
    if (chanson_existe_deja(titre, artiste)) {
        printf("[BDD] Doublon détecté pour : %s - %s\n", artiste, titre);
        return 0; // On refuse l'ajout
    }

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

// --- LOGIQUE AUDIO ---
void stop_musique() {
    if (is_loaded) {
        ma_sound_stop(&sound);
        ma_sound_uninit(&sound);
        is_loaded = 0;
        is_paused = 0;
    }
}

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

    stop_musique();
    ma_result result = ma_sound_init_from_file(&engine, chemin, 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        set_status("Erreur : Fichier introuvable ou corrompu.", 1);
        return;
    }
    ma_sound_start(&sound);
    is_loaded = 1;
    is_paused = 0;
    current_row = row;

    gtk_list_box_select_row(GTK_LIST_BOX(list_box_catalogue), row);
    gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-pause-symbolic");
   
    // On récupère le titre pour l'afficher dans le statut
    // (Un peu hacky, on pourrait stocker le titre aussi, mais c'est pour l'exemple)
    set_status("Lecture en cours... 🎵", 0);
}

void on_btn_play_pause_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    if (!is_loaded) return;
    if (is_paused) {
        ma_sound_start(&sound);
        is_paused = 0;
        gtk_button_set_icon_name(btn, "media-playback-pause-symbolic");
        set_status("Lecture reprise.", 0);
    } else {
        ma_sound_stop(&sound);
        is_paused = 1;
        gtk_button_set_icon_name(btn, "media-playback-start-symbolic");
        set_status("En pause.", 0);
    }
}

void on_btn_next_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    if (!current_row) return;
    int index = gtk_list_box_row_get_index(current_row);
    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_box_catalogue), index + 1);
    if (next_row) jouer_musique(next_row);
    else set_status("Fin de la liste.", 1);
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

// --- INTERFACE ---
void on_supprimer_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *row = GTK_WIDGET(user_data);
    char *chemin = (char *)g_object_get_data(G_OBJECT(btn), "mon_chemin");
    if (chemin) {
        if (current_row == GTK_LIST_BOX_ROW(row)) stop_musique();
        supprimer_bdd(chemin);
        set_status("Musique supprimée.", 0);
    }
    gtk_list_box_remove(GTK_LIST_BOX(list_box_catalogue), row);
}

void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box; (void)user_data;
    jouer_musique(row);
}

void ajouter_ligne_visuelle(const char *chemin, const char *titre, const char *artiste) {
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
            char titre[256] = "";
            char artiste[256] = "";

            // NOUVEAU : On utilise TagLib pour lire les vraies infos
            recuperer_infos_mp3(chemin, titre, artiste, sizeof(titre));

            // Ajout BDD avec vérification anti-doublon (Titre/Artiste)
            if (ajouter_bdd(chemin, titre, artiste)) {
                ajouter_ligne_visuelle(chemin, titre, artiste);
                set_status("Succès : Importé avec Métadonnées !", 0);
            } else {
                set_status("Erreur : Ce morceau existe déjà (Doublon).", 1);
            }
            g_free(chemin);
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
    gtk_window_set_title(GTK_WINDOW(main_window), "Projet Musique - V6 (Metadata)");
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

    GtkWidget *controls_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(controls_box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), controls_box);

    GtkWidget *btn_prev = gtk_button_new_from_icon_name("media-skip-backward-symbolic");
    btn_play_pause = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    GtkWidget *btn_next = gtk_button_new_from_icon_name("media-skip-forward-symbolic");

    gtk_widget_set_size_request(btn_play_pause, 60, 60);
    gtk_widget_set_size_request(btn_prev, 40, 40);
    gtk_widget_set_size_request(btn_next, 40, 40);

    gtk_box_append(GTK_BOX(controls_box), btn_prev);
    gtk_box_append(GTK_BOX(controls_box), btn_play_pause);
    gtk_box_append(GTK_BOX(controls_box), btn_next);

    g_signal_connect(btn_play_pause, "clicked", G_CALLBACK(on_btn_play_pause_clicked), NULL);
    g_signal_connect(btn_next, "clicked", G_CALLBACK(on_btn_next_clicked), NULL);
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(on_btn_prev_clicked), NULL);

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
    GtkApplication *app = gtk_application_new("com.projet.meta", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    ma_engine_uninit(&engine);
    g_object_unref(app);
    return status;
}
