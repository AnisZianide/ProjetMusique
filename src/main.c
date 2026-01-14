#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#define MINIAUDIO_IMPLEMENTATION // OBLIGATOIRE pour activer le code audio
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

// --- VARIABLES AUDIO GLOBALES ---
ma_engine engine;  // Le moteur audio
ma_sound sound;    // Le son en cours de lecture
int is_playing = 0; // Est-ce qu'un son joue déjà ?

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

// --- AUDIO : FONCTIONS DE LECTURE ---

void stop_musique() {
    if (is_playing) {
        ma_sound_stop(&sound);
        ma_sound_uninit(&sound); // On nettoie la mémoire du son précédent
        is_playing = 0;
    }
}

void jouer_musique(const char *chemin) {
    // 1. Si une musique tourne déjà, on l'arrête
    stop_musique();

    // 2. On charge le nouveau fichier
    ma_result result = ma_sound_init_from_file(&engine, chemin, 0, NULL, NULL, &sound);
   
    if (result != MA_SUCCESS) {
        printf("[AUDIO] Erreur : Impossible de lire le fichier %s\n", chemin);
        // On met à jour l'interface pour dire qu'il y a un souci
        char *markup = g_markup_printf_escaped("<span foreground='red'><b>Erreur lecture audio !</b></span>");
        gtk_label_set_markup(GTK_LABEL(lbl_status), markup);
        g_free(markup);
        return;
    }

    // 3. On lance la lecture
    ma_sound_start(&sound);
    is_playing = 1;
   
    printf("[AUDIO] Lecture en cours : %s\n", chemin);
    char *markup = g_markup_printf_escaped("<span foreground='blue'><b>Lecture en cours... 🎵</b></span>");
    gtk_label_set_markup(GTK_LABEL(lbl_status), markup);
    g_free(markup);
}

// --- BDD ---
void init_db() {
    sqlite3 *db;
    char *err_msg = 0;
    int rc = sqlite3_open(DB_FILE, &db);
    if (rc != SQLITE_OK) return;

    const char *sql = "CREATE TABLE IF NOT EXISTS musiques ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "chemin TEXT UNIQUE, "
                      "titre TEXT, "
                      "artiste TEXT);";
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

// NOUVEAU : Fonction appelée quand on double-clique sur une ligne
void on_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    (void)box; (void)user_data;
   
    // On doit retrouver le bouton poubelle caché dans la ligne pour récupérer le chemin
    // C'est une astuce : on sait que le chemin est stocké dans le bouton "user-trash"
   
    GtkWidget *box_row = gtk_list_box_row_get_child(row); // La boite horizontale
    GtkWidget *btn_del = gtk_widget_get_last_child(box_row); // Le dernier élément est le bouton poubelle
   
    // On récupère le chemin stocké
    char *chemin = (char *)g_object_get_data(G_OBJECT(btn_del), "mon_chemin");
   
    if (chemin) {
        jouer_musique(chemin);
    }
}

void set_status(const char *message, int is_error) {
    char *markup;
    if (is_error) markup = g_markup_printf_escaped("<span foreground='red'><b>%s</b></span>", message);
    else markup = g_markup_printf_escaped("<span foreground='green'><b>%s</b></span>", message);
    gtk_label_set_markup(GTK_LABEL(lbl_status), markup);
    g_free(markup);
}

void on_supprimer_clicked(GtkButton *btn, gpointer user_data) {
    GtkWidget *row = GTK_WIDGET(user_data);
    char *chemin = (char *)g_object_get_data(G_OBJECT(btn), "mon_chemin");
    if (chemin) {
        supprimer_bdd(chemin);
        // Si on supprime la musique en cours de lecture, on l'arrête !
        stop_musique();
        set_status("Musique supprimée.", 0);
    }
    gtk_list_box_remove(GTK_LIST_BOX(list_box_catalogue), row);
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
   
    // --- INITIALISATION AUDIO ---
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
        printf("ATTENTION : Impossible d'initialiser le moteur audio !\n");
    }

    init_db();

    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Projet Musique - PLAYER V1");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 600, 500);
   
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
   
    // --- CONNEXION DU SIGNAL "ROW ACTIVATED" (Double Clic) ---
    g_signal_connect(list_box_catalogue, "row-activated", G_CALLBACK(on_row_activated), NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), hbox);
   
    GtkWidget *btn_explorer = gtk_button_new_with_label("📂 Ajouter un son");
    gtk_widget_set_size_request(btn_explorer, 200, 40);
    gtk_box_append(GTK_BOX(hbox), btn_explorer);
    g_signal_connect(btn_explorer, "clicked", G_CALLBACK(on_ouvrir_explorateur_clicked), NULL);

    lbl_status = gtk_label_new("Double-cliquez sur une musique pour la lire !");
    gtk_box_append(GTK_BOX(box), lbl_status);
   
    charger_catalogue();
    gtk_window_present(GTK_WINDOW(main_window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.projet.audio", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
   
    // Nettoyage audio à la fermeture
    ma_engine_uninit(&engine);
   
    g_object_unref(app);
    return status;
}
