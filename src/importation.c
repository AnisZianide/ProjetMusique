#include "importation.h"
#include "base_sqlite.h"
#include "catalogue.h"
#include "playlists.h"
#include "modeles.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

// Vérifie si le fichier est un audio supporté
int est_fichier_audio(const char *filename) {
    if (!filename) return 0;
    // On cherche l'extension
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;

    // Comparaison insensible à la casse (simplifiée pour C standard)
    // On utilise g_ascii_strcasecmp de GLib (inclus via GTK)
    if (g_ascii_strcasecmp(dot, ".mp3") == 0) return 1;
    if (g_ascii_strcasecmp(dot, ".flac") == 0) return 1;
    if (g_ascii_strcasecmp(dot, ".ogg") == 0) return 1;
    if (g_ascii_strcasecmp(dot, ".wav") == 0) return 1;
   
    return 0;
}

void recuperer_infos_mp3(const char *chemin, char *t_out, char *a_out, char *alb_out, int *dur_out, size_t size) {
    int use_filename = 1;
   
    snprintf(alb_out, size, "Album Inconnu");
    *dur_out = 0;

    TagLib_File *f = taglib_file_new(chemin);
   
    if(f && taglib_file_is_valid(f)){
        const TagLib_AudioProperties *p = taglib_file_audioproperties(f);
        if (p) { *dur_out = taglib_audioproperties_length(p); }

        TagLib_Tag *t = taglib_file_tag(f);
        if(t){
            char *tt = taglib_tag_title(t);
            char *ta = taglib_tag_artist(t);
            char *tal = taglib_tag_album(t);

            if(tt && strlen(tt) > 0 && ta && strlen(ta) > 0) {
                snprintf(t_out, size, "%s", tt);
                snprintf(a_out, size, "%s", ta);
                if(tal && strlen(tal) > 0) snprintf(alb_out, size, "%s", tal);
                use_filename = 0;
            }
        }
        taglib_file_free(f);
    }
   
    if(use_filename) {
        char *nom = g_path_get_basename(chemin);
        char *dot = strrchr(nom, '.'); if(dot) *dot='\0';
        char *sep = strstr(nom, " - ");
        if(sep) { *sep = '\0'; snprintf(a_out, size, "%s", nom); snprintf(t_out, size, "%s", sep + 3); }
        else { snprintf(t_out, size, "%s", nom); snprintf(a_out, size, "Artiste inconnu"); }
        g_free(nom);
    }
    trim(t_out); trim(a_out); trim(alb_out);
}

// FONCTION RÉCURSIVE : Scanne un dossier et ses enfants
void scanner_dossier_recursif(const char *folder_path, int *count_added) {
    GDir *dir;
    const char *name;
    GError *error = NULL;

    dir = g_dir_open(folder_path, 0, &error);
    if (!dir) {
        // Erreur d'accès au dossier, on ignore
        if(error) g_error_free(error);
        return;
    }

    while ((name = g_dir_read_name(dir)) != NULL) {
        // On construit le chemin complet
        char *full_path = g_build_filename(folder_path, name, NULL);
       
        // On vérifie si c'est un dossier ou un fichier
        if (g_file_test(full_path, G_FILE_TEST_IS_DIR)) {
            // C'est un dossier : RECURSION !
            scanner_dossier_recursif(full_path, count_added);
        } else {
            // C'est un fichier : Est-ce de la musique ?
            if (est_fichier_audio(name)) {
                char t[256], a[256], alb[256];
                int dur = 0;
               
                recuperer_infos_mp3(full_path, t, a, alb, &dur, 256);
               
                // On tente l'ajout en BDD
                if (ajouter_bdd(full_path, t, a, alb, dur)) {
                    (*count_added)++;
                   
                    // Gestion des paroles (V29)
                    int id = get_id_musique(full_path);
                    if(id != -1) {
                        char *txt_path = get_associated_file(full_path, ".txt");
                        if(txt_path) {
                            char *cnt = lire_contenu_fichier(txt_path);
                            if(cnt) { set_paroles_bdd(id, cnt); free(cnt); }
                            g_free(txt_path);
                        }
                    }
                }
            }
        }
        g_free(full_path);
    }
    g_dir_close(dir);
}

void on_folder_resp(GtkNativeDialog *d, int r, gpointer u) {
    (void)u;
    if(r == GTK_RESPONSE_ACCEPT){
        GFile *f = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(d));
        char *path = g_file_get_path(f);
       
        if(path){
            int count = 0;
            // On lance le scan massif
            scanner_dossier_recursif(path, &count);
           
            charger_catalogue();
            charger_listes_annexes();
           
            char msg[128];
            snprintf(msg, sizeof(msg), "Scan terminé : %d ajoutés.", count);
            set_status(msg, 0);
           
            g_free(path);
        }
        g_object_unref(f);
    }
    g_object_unref(d);
}

void on_add_clicked(GtkButton *b, gpointer u) {
    (void)b; (void)u;
    // Changement ici : ACTION_SELECT_FOLDER
    GtkFileChooserNative *n = gtk_file_chooser_native_new("Importer Dossier (Album/USB)", GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER, "Sélectionner", "Annuler");
   
    g_signal_connect(n, "response", G_CALLBACK(on_folder_resp), NULL);
    gtk_native_dialog_show(GTK_NATIVE_DIALOG(n));
}
