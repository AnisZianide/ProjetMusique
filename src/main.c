#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <taglib/tag_c.h>
#include <gtk/gtk.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

const char *DB_FILE = "data/musique.db";

// Widgets globaux
GtkWidget *main_window;
GtkWidget *stack;
GtkWidget *lbl_status;
GtkWidget *btn_play_pause;
GtkWidget *list_box_catalogue;
GtkWidget *list_box_playlists; // NOUVEAU
GtkWidget *list_box_artistes;  // NOUVEAU
GtkWidget *lbl_current_title;
GtkWidget *lbl_lecture_titre;  // Gros titre page lecture
GtkWidget *lbl_lecture_artiste; // Gros artiste page lecture

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
    if (is_error) markup = g_markup_printf_escaped("<span foreground='#ff5555'><b>%s</b></span>", message);
    else markup = g_markup_printf_escaped("<span foreground='#50fa7b'><b>%s</b></span>", message);
    gtk_label_set_markup(GTK_LABEL(lbl_status), markup);
    g_free(markup);
}

// --- TAGLIB ---
void recuperer_infos_mp3(const char *chemin, char *titre_out, char *artiste_out, size_t size) {
    TagLib_File *file = taglib_file_new(chemin);
    int tags_trouves = 0;
    if (file != NULL && taglib_file_is_valid(file)) {
        TagLib_Tag *tag = taglib_file_tag(file);
        if (tag != NULL) {
            char *tag_title = taglib_tag_title(tag);
            char *tag_artist = taglib_tag_artist(tag);
            if (tag_title && strlen(tag_title) > 0) {
                snprintf(titre_out, size, "%s", tag_title);
                tags_trouves = 1;
            }
            if (tag_artist && strlen(tag_artist) > 0) snprintf(artiste_out, size, "%s", tag_artist);
            else snprintf(artiste_out, size, "Artiste inconnu");
        }
        taglib_file_free(file);
    }
    if (!tags_trouves) {
        char *nom_base = g_path_get_basename(chemin);
        char *dot = strrchr(nom_base, '.');
        if (dot) *dot = '\0';
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
    trim(titre_out); trim(artiste_out);
}

// --- BDD ---
void init_db() {
    sqlite3 *db;
    char *err_msg = 0;
    int rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) return;

    const char *sql_musique = "CREATE TABLE IF NOT EXISTS musiques ("
                              "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                              "chemin TEXT UNIQUE, titre TEXT, artiste TEXT);";
    sqlite3_exec(db, sql_musique, 0, 0, &err_msg);

    const char *sql_playlist = "CREATE TABLE IF NOT EXISTS playlists ("
                               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                               "nom TEXT UNIQUE);";
    sqlite3_exec(db, sql_playlist, 0, 0, &err_msg);

    const char *sql_liaison = "CREATE TABLE IF NOT EXISTS compositions ("
                              "playlist_id INTEGER, musique_id INTEGER, "
                              "FOREIGN KEY(playlist_id) REFERENCES playlists(id), "
                              "FOREIGN KEY(musique_id) REFERENCES musiques(id));";
    sqlite3_exec(db, sql_liaison, 0, 0, &err_msg);

    sqlite3_close(db);
}

int ajouter_playlist_bdd(const char *nom) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int succes = 0;
    sqlite3_open(DB_FILE, &db);
    const char *sql = "INSERT INTO playlists (nom) VALUES (?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, nom, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_DONE) succes = 1;
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return succes;
}

int ajouter_bdd(const char *chemin, const char *titre, const char *artiste) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int succes = 0;
    sqlite3_open(DB_FILE, &db); // Pas de verif doublon complexe ici pour simplifier le code "dump"
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

// --- AUDIO LOGIC ---
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
    // Astuce : le bouton est le DERNIER enfant, l'icone le PREMIER, le Label le DEUXIEME
    // On va chercher le bouton suppression à la fin
    GtkWidget *btn_del = gtk_widget_get_last_child(box_row);
    return (char *)g_object_get_data(G_OBJECT(btn_del), "mon_chemin");
}

void jouer_musique(GtkListBoxRow *row) {
    if (!row) return;
    char *chemin = get_chemin_from_row(row);
    if (!chemin) return;

    // Récupérer titre et artiste pour l'affichage (depuis le label de la ligne)
    // C'est un peu "hacky" mais ça évite de refaire une requête BDD
    GtkWidget *box_row = gtk_list_box_row_get_child(row);
    GtkWidget *label_widget = gtk_widget_get_next_sibling(gtk_widget_get_first_child(box_row)); // Icon -> Label
    const char *texte_markup = gtk_label_get_label(GTK_LABEL(label_widget));

    stop_musique();
    ma_result result = ma_sound_init_from_file(&engine, chemin, 0, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        set_status("Fichier introuvable.", 1);
        return;
    }
    ma_sound_start(&sound);
    is_loaded = 1;
    is_paused = 0;
    current_row = row;

    gtk_list_box_select_row(GTK_LIST_BOX(list_box_catalogue), row);
    gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-pause-symbolic");
   
    // Mise à jour page lecture
    gtk_label_set_markup(GTK_LABEL(lbl_current_title), "Lecture en cours... 🎵");
    if (texte_markup) {
        gtk_label_set_markup(GTK_LABEL(lbl_lecture_titre), texte_markup);
        gtk_label_set_text(GTK_LABEL(lbl_lecture_artiste), ""); // Déjà dans le markup
    }
}

void on_btn_play_pause_clicked(GtkButton *btn, gpointer user_data) {
    (void)user_data;
    if (!is_loaded) return;
    if (is_paused) {
        ma_sound_start(&sound);
        is_paused = 0;
        gtk_button_set_icon_name(btn, "media-playback-pause-symbolic");
    } else {
        ma_sound_stop(&sound);
        is_paused = 1;
        gtk_button_set_icon_name(btn, "media-playback-start-symbolic");
    }
}

void on_btn_next_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    if (!current_row) return;
    int index = gtk_list_box_row_get_index(current_row);
    GtkListBoxRow *next_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_box_catalogue), index + 1);
    if (next_row) jouer_musique(next_row);
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

    // FIX LAYOUT : Ces deux lignes permettent au texte de se couper (...)
    // au lieu de pousser le bouton en dehors de l'écran
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
   
    GtkWidget *btn_del = gtk_button_new_from_icon_name("user-trash-symbolic");
    g_object_set_data_full(G_OBJECT(btn_del), "mon_chemin", g_strdup(chemin), g_free);
   
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), btn_del);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    gtk_list_box_append(GTK_LIST_BOX(list_box_catalogue), row);
    g_signal_connect(btn_del, "clicked", G_CALLBACK(on_supprimer_clicked), row);
}

// --- GESTION ARTISTES & PLAYLISTS ---

void ajouter_ligne_artiste(const char *artiste) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *lbl = gtk_label_new(artiste);
    gtk_widget_set_margin_start(lbl, 10);
    gtk_widget_set_margin_top(lbl, 10);
    gtk_widget_set_margin_bottom(lbl, 10);
    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), lbl);
    gtk_list_box_append(GTK_LIST_BOX(list_box_artistes), row);
}

void ajouter_ligne_playlist(const char *nom) {
    GtkWidget *row = gtk_list_box_row_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *icon = gtk_image_new_from_icon_name("folder-music-symbolic");
    GtkWidget *lbl = gtk_label_new(nom);
   
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), lbl);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
    gtk_list_box_append(GTK_LIST_BOX(list_box_playlists), row);
}

void charger_donnees() {
    // 1. Catalogue
    GtkWidget *child = gtk_widget_get_first_child(list_box_catalogue);
    while (child) { GtkWidget *n=gtk_widget_get_next_sibling(child); gtk_list_box_remove(GTK_LIST_BOX(list_box_catalogue), child); child=n; }
   
    // 2. Artistes
    child = gtk_widget_get_first_child(list_box_artistes);
    while (child) { GtkWidget *n=gtk_widget_get_next_sibling(child); gtk_list_box_remove(GTK_LIST_BOX(list_box_artistes), child); child=n; }

    // 3. Playlists
    child = gtk_widget_get_first_child(list_box_playlists);
    while (child) { GtkWidget *n=gtk_widget_get_next_sibling(child); gtk_list_box_remove(GTK_LIST_BOX(list_box_playlists), child); child=n; }

    sqlite3 *db;
    sqlite3_stmt *stmt;
    sqlite3_open(DB_FILE, &db);

    // Charger Musiques
    const char *sql = "SELECT chemin, titre, artiste FROM musiques;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ajouter_ligne_visuelle(
                (const char *)sqlite3_column_text(stmt, 0),
                (const char *)sqlite3_column_text(stmt, 1),
                (const char *)sqlite3_column_text(stmt, 2)
            );
        }
    }
    sqlite3_finalize(stmt);

    // Charger Artistes (DISTINCT)
    const char *sql_art = "SELECT DISTINCT artiste FROM musiques ORDER BY artiste;";
    if (sqlite3_prepare_v2(db, sql_art, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ajouter_ligne_artiste((const char *)sqlite3_column_text(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);

    // Charger Playlists
    const char *sql_pl = "SELECT nom FROM playlists ORDER BY nom;";
    if (sqlite3_prepare_v2(db, sql_pl, -1, &stmt, 0) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            ajouter_ligne_playlist((const char *)sqlite3_column_text(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

// Creation playlist
void on_dialog_response(GtkDialog *dialog, int response, gpointer user_data) {
    if (response == GTK_RESPONSE_OK) {
        GtkEntry *entry = GTK_ENTRY(user_data);
        const char *nom = gtk_editable_get_text(GTK_EDITABLE(entry));
        if (nom && strlen(nom) > 0) {
            if (ajouter_playlist_bdd(nom)) {
                ajouter_ligne_playlist(nom);
                set_status("Playlist créée !", 0);
            } else {
                set_status("Erreur playlist (Doublon ?)", 1);
            }
        }
    }
    gtk_window_destroy(GTK_WINDOW(dialog));
}

void on_creer_playlist_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    GtkWidget *dialog = gtk_dialog_new_with_buttons("Nouvelle Playlist", GTK_WINDOW(main_window), GTK_DIALOG_MODAL, "_Annuler", GTK_RESPONSE_CANCEL, "_Créer", GTK_RESPONSE_OK, NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Nom de la playlist...");
    gtk_box_append(GTK_BOX(content), entry);
    gtk_widget_set_margin_top(entry, 20);
    gtk_widget_set_margin_bottom(entry, 20);
    gtk_widget_set_margin_start(entry, 20);
    gtk_widget_set_margin_end(entry, 20);
    gtk_widget_show(entry);
    g_signal_connect(dialog, "response", G_CALLBACK(on_dialog_response), entry);
    gtk_window_present(GTK_WINDOW(dialog));
}

void on_fichier_choisi(GtkNativeDialog *dialog, int response_id, gpointer user_data) {
    (void)user_data;
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GFile *file = gtk_file_chooser_get_file(chooser);
        char *chemin = g_file_get_path(file);
        if (chemin) {
            char titre[256] = ""; char artiste[256] = "";
            recuperer_infos_mp3(chemin, titre, artiste, sizeof(titre));
            if (ajouter_bdd(chemin, titre, artiste)) {
                charger_donnees(); // On recharge tout pour mettre à jour la liste Artistes aussi
                set_status("Import réussi !", 0);
            } else {
                set_status("Déjà présent.", 1);
            }
            g_free(chemin);
        }
        g_object_unref(file);
    }
    g_object_unref(dialog);
}

void on_ouvrir_explorateur_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn; (void)user_data;
    GtkFileChooserNative *native = gtk_file_chooser_native_new("Ajouter", GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_OPEN, "_Ouvrir", "_Annuler");
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.mp3");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(native), filter);
    gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(native), filter);
    g_signal_connect(native, "response", G_CALLBACK(on_fichier_choisi), NULL);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) printf("Erreur audio\n");
    init_db();

    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Projet Musique - V8 (Complet)");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 800, 600);

    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(main_window), main_vbox);

    GtkWidget *content_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(content_hbox, TRUE);
    gtk_box_append(GTK_BOX(main_vbox), content_hbox);

    stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
   
    GtkWidget *sidebar = gtk_stack_sidebar_new();
    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(stack));
    gtk_widget_set_size_request(sidebar, 150, -1);
    gtk_box_append(GTK_BOX(content_hbox), sidebar);
    gtk_box_append(GTK_BOX(content_hbox), stack);

    // 1. CATALOGUE
    GtkWidget *box_catalogue = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *btn_add = gtk_button_new_with_label("📂 Importer un MP3");
    gtk_box_append(GTK_BOX(box_catalogue), btn_add);
    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_ouvrir_explorateur_clicked), NULL);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(box_catalogue), scroll);
    list_box_catalogue = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), list_box_catalogue);
    g_signal_connect(list_box_catalogue, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_stack_add_titled(GTK_STACK(stack), box_catalogue, "page_catalogue", "Catalogue");

    // 2. PLAYLISTS
    GtkWidget *box_playlists = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *btn_new_pl = gtk_button_new_with_label("➕ Nouvelle Playlist");
    gtk_box_append(GTK_BOX(box_playlists), btn_new_pl);
    g_signal_connect(btn_new_pl, "clicked", G_CALLBACK(on_creer_playlist_clicked), NULL);
    GtkWidget *scroll_pl = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_pl, TRUE);
    gtk_box_append(GTK_BOX(box_playlists), scroll_pl);
    list_box_playlists = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_pl), list_box_playlists);
    gtk_stack_add_titled(GTK_STACK(stack), box_playlists, "page_playlists", "Playlists");

    // 3. ARTISTES
    GtkWidget *box_artistes = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *scroll_art = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll_art, TRUE);
    gtk_box_append(GTK_BOX(box_artistes), scroll_art);
    list_box_artistes = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll_art), list_box_artistes);
    gtk_stack_add_titled(GTK_STACK(stack), box_artistes, "page_artistes", "Artistes");

    // 4. LECTURE
    GtkWidget *box_lecture = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_valign(box_lecture, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(box_lecture, GTK_ALIGN_CENTER);
   
    GtkWidget *icon_gros = gtk_image_new_from_icon_name("audio-x-generic");
    gtk_widget_set_size_request(icon_gros, 128, 128); // GRANDE ICONE
    gtk_image_set_pixel_size(GTK_IMAGE(icon_gros), 128);
   
    lbl_lecture_titre = gtk_label_new("<b>En attente...</b>");
    gtk_label_set_use_markup(GTK_LABEL(lbl_lecture_titre), TRUE);
    // On grossit le texte du titre
   
    lbl_lecture_artiste = gtk_label_new("");
   
    gtk_box_append(GTK_BOX(box_lecture), icon_gros);
    gtk_box_append(GTK_BOX(box_lecture), lbl_lecture_titre);
    gtk_box_append(GTK_BOX(box_lecture), lbl_lecture_artiste);
    gtk_stack_add_titled(GTK_STACK(stack), box_lecture, "page_lecture", "Lecture");

    // FOOTER (Controles)
    GtkWidget *footer_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(footer_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
    lbl_current_title = gtk_label_new("Aucune lecture");
    gtk_box_append(GTK_BOX(footer_box), lbl_current_title);

    GtkWidget *controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(controls, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_bottom(controls, 10);
    GtkWidget *btn_prev = gtk_button_new_from_icon_name("media-skip-backward-symbolic");
    btn_play_pause = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    GtkWidget *btn_next = gtk_button_new_from_icon_name("media-skip-forward-symbolic");
    gtk_box_append(GTK_BOX(controls), btn_prev);
    gtk_box_append(GTK_BOX(controls), btn_play_pause);
    gtk_box_append(GTK_BOX(controls), btn_next);
    gtk_box_append(GTK_BOX(footer_box), controls);
    lbl_status = gtk_label_new("");
    gtk_box_append(GTK_BOX(footer_box), lbl_status);
    gtk_box_append(GTK_BOX(main_vbox), footer_box);

    g_signal_connect(btn_play_pause, "clicked", G_CALLBACK(on_btn_play_pause_clicked), NULL);
    g_signal_connect(btn_next, "clicked", G_CALLBACK(on_btn_next_clicked), NULL);
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(on_btn_prev_clicked), NULL);

    charger_donnees();
    gtk_window_present(GTK_WINDOW(main_window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.projet.v8", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    ma_engine_uninit(&engine);
    g_object_unref(app);
    return status;
}
