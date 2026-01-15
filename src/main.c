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
GtkWidget *list_box_playlists;
GtkWidget *list_box_contenu_playlist;
GtkWidget *list_box_artistes;
GtkWidget *list_box_contenu_artiste;
GtkWidget *lbl_lecture_titre;
GtkWidget *lbl_lecture_artiste;
GtkWidget *lbl_current_title;
GtkWidget *txt_paroles_buffer;
GtkWidget *img_pochette;
GtkWidget *scrolled_window_paroles;

// NOUVEAU V22 : Widgets de Recherche et Tri
GtkWidget *entry_recherche;
GtkWidget *dd_tri;

// Audio & Timer
ma_engine engine;
ma_sound sound;
int is_loaded = 0;
int is_paused = 0;
guint scroll_timer_id = 0;
GtkListBoxRow *current_row = NULL;
GtkListBox *current_playing_list = NULL;

// --- CSS LOADER ---
void load_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

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
    else markup = g_markup_printf_escaped("<span foreground='#1db954'><b>%s</b></span>", message);
    gtk_label_set_markup(GTK_LABEL(lbl_status), markup);
    g_free(markup);
}

// --- AUTO-SCROLL ---
gboolean on_scroll_timer(gpointer user_data) {
    (void)user_data;
    if (!is_loaded || is_paused) return TRUE;
    return TRUE;
}
void start_auto_scroll() { if (scroll_timer_id == 0) scroll_timer_id = g_timeout_add(100, on_scroll_timer, NULL); }
void stop_auto_scroll() { if (scroll_timer_id > 0) { g_source_remove(scroll_timer_id); scroll_timer_id = 0; } }

// --- FICHIERS ---
char* get_associated_file(const char *mp3_path, const char *new_ext) {
    char *new_path = g_strdup(mp3_path);
    char *dot = strrchr(new_path, '.');
    if (dot) { *dot = '\0'; char *final_path = g_strconcat(new_path, new_ext, NULL); g_free(new_path);
        if (access(final_path, F_OK) != -1) return final_path;
        g_free(final_path);
    } else g_free(new_path);
    return NULL;
}

char* lire_contenu_fichier(const char *path) {
    FILE *f = fopen(path, "r"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long length = ftell(f); fseek(f, 0, SEEK_SET);
    char *buffer = malloc(length + 1);
    if (buffer) { fread(buffer, 1, length, f); buffer[length] = '\0'; }
    fclose(f); return buffer;
}

// --- BDD ---
void init_db() {
    sqlite3 *db; char *err = 0; if(sqlite3_open(DB_FILE, &db) != SQLITE_OK) return;
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, 0);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS musiques (id INTEGER PRIMARY KEY AUTOINCREMENT, chemin TEXT UNIQUE, titre TEXT, artiste TEXT);", 0, 0, &err);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS playlists (id INTEGER PRIMARY KEY AUTOINCREMENT, nom TEXT UNIQUE);", 0, 0, &err);
    sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS compositions (playlist_id INTEGER, musique_id INTEGER, PRIMARY KEY (playlist_id, musique_id), FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE, FOREIGN KEY(musique_id) REFERENCES musiques(id) ON DELETE CASCADE);", 0, 0, &err);
    sqlite3_close(db);
}

// --- METADATA ---
void recuperer_infos_mp3(const char *chemin, char *t_out, char *a_out, size_t size) {
    int use_filename = 1; TagLib_File *f = taglib_file_new(chemin);
    if(f && taglib_file_is_valid(f)){
        TagLib_Tag *t = taglib_file_tag(f);
        if(t){
            char *tt = taglib_tag_title(t); char *ta = taglib_tag_artist(t);
            if(tt && strlen(tt) > 0 && ta && strlen(ta) > 0) { snprintf(t_out, size, "%s", tt); snprintf(a_out, size, "%s", ta); use_filename = 0; }
        } taglib_file_free(f);
    }
    if(use_filename) {
        char *nom = g_path_get_basename(chemin); char *dot = strrchr(nom, '.'); if(dot) *dot='\0';
        char *sep = strstr(nom, " - "); if(sep) { *sep = '\0'; snprintf(a_out, size, "%s", nom); snprintf(t_out, size, "%s", sep + 3); }
        else { snprintf(t_out, size, "%s", nom); snprintf(a_out, size, "Artiste inconnu"); }
        g_free(nom);
    } trim(t_out); trim(a_out);
}

int doublon_metadata_existe(const char *t, const char *a) {
    sqlite3 *db; sqlite3_stmt *st; int ex = 0; sqlite3_open(DB_FILE, &db);
    sqlite3_prepare_v2(db, "SELECT id FROM musiques WHERE titre=? AND artiste=? COLLATE NOCASE;", -1, &st, 0);
    sqlite3_bind_text(st, 1, t, -1, SQLITE_STATIC); sqlite3_bind_text(st, 2, a, -1, SQLITE_STATIC);
    if(sqlite3_step(st) == SQLITE_ROW) ex = 1; sqlite3_finalize(st); sqlite3_close(db); return ex;
}

int ajouter_bdd(const char *c, const char *t, const char *a) {
    sqlite3 *db; sqlite3_stmt *st; sqlite3_open(DB_FILE, &db);
    sqlite3_prepare_v2(db, "SELECT id FROM musiques WHERE chemin=?;", -1, &st, 0);
    sqlite3_bind_text(st, 1, c, -1, SQLITE_STATIC);
    if(sqlite3_step(st)==SQLITE_ROW){ sqlite3_finalize(st); sqlite3_close(db); return 0; }
    sqlite3_finalize(st); sqlite3_close(db);
    if(doublon_metadata_existe(t, a)) return 0;
    int s=0; sqlite3_open(DB_FILE, &db);
    sqlite3_prepare_v2(db, "INSERT INTO musiques (chemin, titre, artiste) VALUES (?, ?, ?);", -1, &st, 0);
    sqlite3_bind_text(st, 1, c, -1, SQLITE_STATIC); sqlite3_bind_text(st, 2, t, -1, SQLITE_STATIC); sqlite3_bind_text(st, 3, a, -1, SQLITE_STATIC);
    if(sqlite3_step(st)==SQLITE_DONE)s=1; sqlite3_finalize(st); sqlite3_close(db); return s;
}

void supprimer_bdd(const char *c) {
    sqlite3 *db; sqlite3_stmt *st; sqlite3_open(DB_FILE, &db);
    sqlite3_prepare_v2(db, "DELETE FROM musiques WHERE chemin=?;", -1, &st, 0);
    sqlite3_bind_text(st, 1, c, -1, SQLITE_STATIC); sqlite3_step(st); sqlite3_finalize(st); sqlite3_close(db);
}
int ajouter_playlist_bdd(const char *n){sqlite3 *d;sqlite3_stmt *s;int r=0;sqlite3_open(DB_FILE,&d);sqlite3_prepare_v2(d,"INSERT INTO playlists(nom)VALUES(?);",-1,&s,0);sqlite3_bind_text(s,1,n,-1,0);if(sqlite3_step(s)==SQLITE_DONE)r=1;sqlite3_finalize(s);sqlite3_close(d);return r;}
int get_id_playlist(const char *n){sqlite3 *d;sqlite3_stmt *s;int i=-1;sqlite3_open(DB_FILE,&d);sqlite3_prepare_v2(d,"SELECT id FROM playlists WHERE nom=?;",-1,&s,0);sqlite3_bind_text(s,1,n,-1,0);if(sqlite3_step(s)==SQLITE_ROW)i=sqlite3_column_int(s,0);sqlite3_finalize(s);sqlite3_close(d);return i;}
int get_id_musique(const char *c){sqlite3 *d;sqlite3_stmt *s;int i=-1;sqlite3_open(DB_FILE,&d);sqlite3_prepare_v2(d,"SELECT id FROM musiques WHERE chemin=?;",-1,&s,0);sqlite3_bind_text(s,1,c,-1,0);if(sqlite3_step(s)==SQLITE_ROW)i=sqlite3_column_int(s,0);sqlite3_finalize(s);sqlite3_close(d);return i;}
void ajouter_compo_bdd(int p,int m){sqlite3 *d;sqlite3_stmt *s;sqlite3_open(DB_FILE,&d);sqlite3_prepare_v2(d,"INSERT OR IGNORE INTO compositions(playlist_id,musique_id)VALUES(?,?);",-1,&s,0);sqlite3_bind_int(s,1,p);sqlite3_bind_int(s,2,m);if(sqlite3_step(s)==SQLITE_DONE)set_status("Ajouté !",0);else set_status("Erreur.",1);sqlite3_finalize(s);sqlite3_close(d);}

// --- AUDIO PLAYER ---
void stop_musique() { if(is_loaded){ ma_sound_stop(&sound); ma_sound_uninit(&sound); is_loaded=0; is_paused=0; } stop_auto_scroll(); }
char* get_chemin_from_row(GtkListBoxRow *r) { if(!r)return NULL; GtkWidget *b=gtk_list_box_row_get_child(r); GtkWidget *bt=gtk_widget_get_last_child(b); return (char*)g_object_get_data(G_OBJECT(bt), "mon_chemin"); }

void jouer_musique(GtkListBoxRow *r) {
    if(!r)return; char *c=get_chemin_from_row(r); if(!c)return;
    GtkWidget *bx=gtk_list_box_row_get_child(r); GtkWidget *lb=gtk_widget_get_next_sibling(gtk_widget_get_first_child(bx));
    const char *mk=gtk_label_get_label(GTK_LABEL(lb));

    stop_musique();
    if(ma_sound_init_from_file(&engine, c, 0, NULL, NULL, &sound)!=MA_SUCCESS){ set_status("Erreur fichier.", 1); return; }
    ma_sound_start(&sound); is_loaded=1; is_paused=0; start_auto_scroll();
   
    current_row=r; GtkWidget *p=gtk_widget_get_parent(GTK_WIDGET(r));
    if(GTK_IS_LIST_BOX(p)) current_playing_list=GTK_LIST_BOX(p);
   
    gtk_button_set_icon_name(GTK_BUTTON(btn_play_pause), "media-playback-pause-symbolic");
    if(mk) gtk_label_set_markup(GTK_LABEL(lbl_lecture_titre), mk);
    gtk_label_set_markup(GTK_LABEL(lbl_current_title), "Lecture en cours... 🎵");

    GtkTextBuffer *buf=GTK_TEXT_BUFFER(txt_paroles_buffer);
    char *pp=get_associated_file(c, ".txt");
    if(pp){ char *cnt=lire_contenu_fichier(pp); gtk_text_buffer_set_text(buf, cnt?cnt:"(Erreur lecture)", -1); if(cnt)free(cnt); g_free(pp); }
    else{ gtk_text_buffer_set_text(buf, "\n(Pas de paroles trouvées.\nVerifiez le nom exact du fichier .txt)", -1); }
   
    char *ip=get_associated_file(c, ".jpg"); if(!ip) ip=get_associated_file(c, ".png");
    if(ip){ gtk_image_set_from_file(GTK_IMAGE(img_pochette), ip); gtk_image_set_pixel_size(GTK_IMAGE(img_pochette), 200); g_free(ip); }
    else{ gtk_image_set_from_icon_name(GTK_IMAGE(img_pochette), "audio-x-generic"); gtk_image_set_pixel_size(GTK_IMAGE(img_pochette), 150); }
}

void on_btn_play_pause_clicked(GtkButton *b, gpointer u) { (void)u; (void)b; if(!is_loaded)return; if(is_paused){ ma_sound_start(&sound); is_paused=0; gtk_button_set_icon_name(b, "media-playback-pause-symbolic"); } else{ ma_sound_stop(&sound); is_paused=1; gtk_button_set_icon_name(b, "media-playback-start-symbolic"); } }
void on_btn_next_clicked(GtkButton *b, gpointer u) { (void)b; (void)u; if(current_row && current_playing_list){ int i=gtk_list_box_row_get_index(current_row); GtkListBoxRow *n=gtk_list_box_get_row_at_index(current_playing_list, i+1); if(n) jouer_musique(n); else set_status("Fin liste.",0); } }
void on_btn_prev_clicked(GtkButton *b, gpointer u) { (void)b; (void)u; if(current_row && current_playing_list){ int i=gtk_list_box_row_get_index(current_row); if(i>0){ GtkListBoxRow *p=gtk_list_box_get_row_at_index(current_playing_list, i-1); if(p) jouer_musique(p); } } }

// UI HELPERS
void on_supprimer_clicked(GtkButton *b, gpointer u) { GtkWidget *r=GTK_WIDGET(u); char *c=(char*)g_object_get_data(G_OBJECT(b), "mon_chemin"); if(c){ if(current_row==GTK_LIST_BOX_ROW(r)) stop_musique(); supprimer_bdd(c); } GtkWidget *p=gtk_widget_get_parent(r); gtk_list_box_remove(GTK_LIST_BOX(p), r); set_status("Supprimé.", 0); }
void on_playlist_select_resp(GtkDialog *d, int r, gpointer u) { int mid=(int)(intptr_t)u; if(r==GTK_RESPONSE_OK){ GtkWidget *c=gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *ch=gtk_widget_get_first_child(c); GtkWidget *cb=NULL; while(ch){if(GTK_IS_DROP_DOWN(ch)){cb=ch;break;}ch=gtk_widget_get_next_sibling(ch);} if(cb){ GtkStringList *m=GTK_STRING_LIST(gtk_drop_down_get_model(GTK_DROP_DOWN(cb))); const char *n=gtk_string_list_get_string(m, gtk_drop_down_get_selected(GTK_DROP_DOWN(cb))); int p=get_id_playlist(n); if(p!=-1) ajouter_compo_bdd(p, mid); } } gtk_window_destroy(GTK_WINDOW(d)); }
void on_add_to_pl_clicked(GtkButton *b, gpointer u) { (void)u; char *c=(char*)g_object_get_data(G_OBJECT(b), "mon_chemin"); int m=get_id_musique(c); if(m==-1)return; GtkWidget *d=gtk_dialog_new_with_buttons("Choisir Playlist", GTK_WINDOW(main_window), GTK_DIALOG_MODAL, "Annuler", GTK_RESPONSE_CANCEL, "Ajouter", GTK_RESPONSE_OK, NULL); GtkWidget *ct=gtk_dialog_get_content_area(GTK_DIALOG(d)); const char *a[100]; int k=0; sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db); sqlite3_prepare_v2(db, "SELECT nom FROM playlists;", -1, &s, 0); while(sqlite3_step(s)==SQLITE_ROW && k<99) a[k++]=g_strdup((const char*)sqlite3_column_text(s, 0)); a[k]=NULL; sqlite3_finalize(s); sqlite3_close(db); if(k==0){ set_status("Aucune playlist !", 1); gtk_window_destroy(GTK_WINDOW(d)); return; } GtkWidget *dd=gtk_drop_down_new_from_strings(a); gtk_widget_set_margin_top(dd,20); gtk_widget_set_margin_bottom(dd,20); gtk_widget_set_margin_start(dd,20); gtk_widget_set_margin_end(dd,20); gtk_box_append(GTK_BOX(ct), dd); g_signal_connect(d, "response", G_CALLBACK(on_playlist_select_resp), (gpointer)(intptr_t)m); gtk_window_present(GTK_WINDOW(d)); }
void ajouter_ligne_visuelle_generique(GtkListBox *lb, const char *c, const char *t, const char *a, int pl) { GtkWidget *r=gtk_list_box_row_new(); GtkWidget *x=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); gtk_widget_set_hexpand(x, TRUE); char *txt=g_markup_printf_escaped("<b>%s</b>\n<span size='small' foreground='gray'>%s</span>", t, a); GtkWidget *l=gtk_label_new(NULL); gtk_label_set_markup(GTK_LABEL(l), txt); g_free(txt); gtk_widget_set_hexpand(l, TRUE); gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_END); gtk_box_append(GTK_BOX(x), gtk_image_new_from_icon_name("audio-x-generic")); gtk_box_append(GTK_BOX(x), l); if(pl){ GtkWidget *ba=gtk_button_new_from_icon_name("list-add-symbolic"); g_object_set_data_full(G_OBJECT(ba), "mon_chemin", g_strdup(c), g_free); g_signal_connect(ba, "clicked", G_CALLBACK(on_add_to_pl_clicked), NULL); gtk_box_append(GTK_BOX(x), ba); } GtkWidget *bd=gtk_button_new_from_icon_name("user-trash-symbolic"); g_object_set_data_full(G_OBJECT(bd), "mon_chemin", g_strdup(c), g_free); g_signal_connect(bd, "clicked", G_CALLBACK(on_supprimer_clicked), r); gtk_box_append(GTK_BOX(x), bd); gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), x); gtk_list_box_append(lb, r); }

// --- CHARGEMENT CATALOGUE AVEC TRI ET RECHERCHE (V22) ---
void charger_catalogue() {
    GtkWidget *c=gtk_widget_get_first_child(list_box_catalogue);
    while(c){ GtkWidget *n=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_catalogue), c); c=n; }
   
    // Récupérer le texte de recherche et le tri
    const char *recherche = gtk_editable_get_text(GTK_EDITABLE(entry_recherche));
    int tri_id = gtk_drop_down_get_selected(GTK_DROP_DOWN(dd_tri));
   
    // Construire la requête SQL
    char sql[1024];
    char *order_by = "id DESC"; // Par défaut : Ajout récent
    if (tri_id == 1) order_by = "titre ASC"; // A-Z
    if (tri_id == 2) order_by = "artiste ASC"; // Artiste

    snprintf(sql, sizeof(sql),
        "SELECT chemin, titre, artiste FROM musiques WHERE (titre LIKE '%%%s%%' OR artiste LIKE '%%%s%%') ORDER BY %s;",
        recherche ? recherche : "",
        recherche ? recherche : "",
        order_by);

    sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db);
    sqlite3_prepare_v2(db, sql, -1, &s, 0);
    while(sqlite3_step(s)==SQLITE_ROW) {
        ajouter_ligne_visuelle_generique(GTK_LIST_BOX(list_box_catalogue),
            (const char*)sqlite3_column_text(s,0),
            (const char*)sqlite3_column_text(s,1),
            (const char*)sqlite3_column_text(s,2), 1);
    }
    sqlite3_finalize(s); sqlite3_close(db);
}

// Callbacks pour recherche et tri
void on_search_changed(GtkSearchEntry *e, gpointer u) { (void)e; (void)u; charger_catalogue(); }
void on_sort_changed(GObject *o, GParamSpec *p, gpointer u) { (void)o; (void)p; (void)u; charger_catalogue(); }

void charger_contenu_playlist(const char *n) { GtkWidget *c=gtk_widget_get_first_child(list_box_contenu_playlist); while(c){ GtkWidget *x=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_contenu_playlist), c); c=x; } sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db); char q[512]; sprintf(q, "SELECT m.chemin, m.titre, m.artiste FROM musiques m JOIN compositions c ON m.id=c.musique_id JOIN playlists p ON p.id=c.playlist_id WHERE p.nom='%s';", n); sqlite3_prepare_v2(db, q, -1, &s, 0); while(sqlite3_step(s)==SQLITE_ROW) ajouter_ligne_visuelle_generique(GTK_LIST_BOX(list_box_contenu_playlist), (const char*)sqlite3_column_text(s,0), (const char*)sqlite3_column_text(s,1), (const char*)sqlite3_column_text(s,2), 0); sqlite3_finalize(s); sqlite3_close(db); }
void charger_contenu_artiste(const char *nom_artiste) { GtkWidget *c = gtk_widget_get_first_child(list_box_contenu_artiste); while(c) { GtkWidget *x=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_contenu_artiste), c); c=x; } sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db); char q[512]; sprintf(q, "SELECT chemin, titre, artiste FROM musiques WHERE artiste='%s';", nom_artiste); sqlite3_prepare_v2(db, q, -1, &s, 0); while(sqlite3_step(s)==SQLITE_ROW) { ajouter_ligne_visuelle_generique(GTK_LIST_BOX(list_box_contenu_artiste), (const char*)sqlite3_column_text(s, 0), (const char*)sqlite3_column_text(s, 1), (const char*)sqlite3_column_text(s, 2), 1); } sqlite3_finalize(s); sqlite3_close(db); }
void on_artiste_row_activated(GtkListBox *b, GtkListBoxRow *r, gpointer u) { (void)b; (void)u; GtkWidget *label = gtk_list_box_row_get_child(r); const char *nom_artiste = gtk_label_get_text(GTK_LABEL(label)); charger_contenu_artiste(nom_artiste); }
void charger_listes_annexes() { GtkWidget *c; c=gtk_widget_get_first_child(list_box_artistes); while(c){ GtkWidget *n=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_artistes), c); c=n; } sqlite3 *db; sqlite3_stmt *s; sqlite3_open(DB_FILE, &db); sqlite3_prepare_v2(db, "SELECT DISTINCT artiste FROM musiques ORDER BY artiste;", -1, &s, 0); while(sqlite3_step(s)==SQLITE_ROW){ GtkWidget *r=gtk_list_box_row_new(); GtkWidget *l=gtk_label_new((const char*)sqlite3_column_text(s,0)); gtk_widget_set_margin_start(l,10); gtk_widget_set_margin_top(l,10); gtk_widget_set_margin_bottom(l,10); gtk_widget_set_halign(l, GTK_ALIGN_START); gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), l); gtk_list_box_append(GTK_LIST_BOX(list_box_artistes), r); } sqlite3_finalize(s); c=gtk_widget_get_first_child(list_box_playlists); while(c){ GtkWidget *n=gtk_widget_get_next_sibling(c); gtk_list_box_remove(GTK_LIST_BOX(list_box_playlists), c); c=n; } sqlite3_prepare_v2(db, "SELECT nom FROM playlists ORDER BY nom;", -1, &s, 0); while(sqlite3_step(s)==SQLITE_ROW){ GtkWidget *r=gtk_list_box_row_new(); GtkWidget *b=gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10); GtkWidget *l=gtk_label_new((const char*)sqlite3_column_text(s,0)); gtk_box_append(GTK_BOX(b), gtk_image_new_from_icon_name("folder-music-symbolic")); gtk_box_append(GTK_BOX(b), l); gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(r), b); gtk_list_box_append(GTK_LIST_BOX(list_box_playlists), r); } sqlite3_finalize(s); sqlite3_close(db); }
void on_row_activated(GtkListBox *b, GtkListBoxRow *r, gpointer u) { (void)b; (void)u; jouer_musique(r); }
void on_pl_row_activated(GtkListBox *b, GtkListBoxRow *r, gpointer u) { (void)b; (void)u; GtkWidget *x=gtk_list_box_row_get_child(r); GtkWidget *l=gtk_widget_get_last_child(x); charger_contenu_playlist(gtk_label_get_text(GTK_LABEL(l))); }
void on_creer_pl_resp(GtkDialog *d, int r, gpointer u) { if(r==GTK_RESPONSE_OK){ const char *t=gtk_editable_get_text(GTK_EDITABLE(u)); if(t&&strlen(t)>0){ ajouter_playlist_bdd(t); charger_listes_annexes(); set_status("Playlist créée !", 0); } } gtk_window_destroy(GTK_WINDOW(d)); }
void on_creer_pl_clicked(GtkWidget *b, gpointer u) { (void)b; (void)u; GtkWidget *d=gtk_dialog_new_with_buttons("Nouvelle", GTK_WINDOW(main_window), GTK_DIALOG_MODAL, "Annuler", GTK_RESPONSE_CANCEL, "Créer", GTK_RESPONSE_OK, NULL); GtkWidget *c=gtk_dialog_get_content_area(GTK_DIALOG(d)); GtkWidget *e=gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(e), "Nom..."); gtk_widget_set_margin_start(e,20); gtk_widget_set_margin_end(e,20); gtk_widget_set_margin_top(e,20); gtk_widget_set_margin_bottom(e,20); gtk_box_append(GTK_BOX(c), e); g_signal_connect(d, "response", G_CALLBACK(on_creer_pl_resp), e); gtk_window_present(GTK_WINDOW(d)); }
void on_file_resp(GtkNativeDialog *d, int r, gpointer u) { (void)u; if(r==GTK_RESPONSE_ACCEPT){ GFile *f=gtk_file_chooser_get_file(GTK_FILE_CHOOSER(d)); char *p=g_file_get_path(f); if(p){ char t[256], a[256]; recuperer_infos_mp3(p,t,a,256); if(ajouter_bdd(p,t,a)){ charger_catalogue(); charger_listes_annexes(); set_status("Importé.", 0); } else set_status("Doublon Métadonnée.", 1); g_free(p); } g_object_unref(f); } g_object_unref(d); }
void on_add_clicked(GtkButton *b, gpointer u) { (void)b; (void)u; GtkFileChooserNative *n=gtk_file_chooser_native_new("Ajouter", GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_OPEN, "Ouvrir", "Annuler"); GtkFileFilter *f=gtk_file_filter_new(); gtk_file_filter_add_pattern(f, "*.mp3"); gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(n), f); g_signal_connect(n, "response", G_CALLBACK(on_file_resp), NULL); gtk_native_dialog_show(GTK_NATIVE_DIALOG(n)); }

static void activate(GtkApplication *app, gpointer u) { (void)u;
    ma_engine_init(NULL, &engine); init_db(); load_css();
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Projet Musique - V22 (Recherche & Tri)");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 950, 700);
    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); gtk_window_set_child(GTK_WINDOW(main_window), vb);
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0); gtk_widget_set_vexpand(hb, TRUE); gtk_box_append(GTK_BOX(vb), hb);
    stack = gtk_stack_new(); GtkWidget *sb = gtk_stack_sidebar_new(); gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sb), GTK_STACK(stack));
    gtk_widget_set_size_request(sb, 180, -1); gtk_box_append(GTK_BOX(hb), sb); gtk_widget_set_hexpand(stack, TRUE); gtk_box_append(GTK_BOX(hb), stack);

    // CAT (MODIFIÉ POUR V22)
    GtkWidget *p1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
   
    // --- BARRE DE CONTROLE (Recherche + Tri + Bouton) ---
    GtkWidget *box_ctrl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
   
    // Recherche
    entry_recherche = gtk_search_entry_new();
    gtk_search_entry_set_key_capture_widget(GTK_SEARCH_ENTRY(entry_recherche), main_window);
    gtk_widget_set_hexpand(entry_recherche, TRUE);
    g_signal_connect(entry_recherche, "search-changed", G_CALLBACK(on_search_changed), NULL);
    gtk_box_append(GTK_BOX(box_ctrl), entry_recherche);

    // Tri
    const char *options[] = { "🕒 Récent", "🔤 Titre", "🎤 Artiste", NULL };
    dd_tri = gtk_drop_down_new_from_strings(options);
    g_signal_connect(dd_tri, "notify::selected", G_CALLBACK(on_sort_changed), NULL);
    gtk_box_append(GTK_BOX(box_ctrl), dd_tri);

    // Bouton Import
    GtkWidget *b_add = gtk_button_new_with_label("📂 Importer");
    gtk_widget_add_css_class(b_add, "text-button");
    g_signal_connect(b_add, "clicked", G_CALLBACK(on_add_clicked), NULL);
    gtk_box_append(GTK_BOX(box_ctrl), b_add);

    gtk_box_append(GTK_BOX(p1), box_ctrl);
    // ----------------------------------------------------

    GtkWidget *s1 = gtk_scrolled_window_new(); gtk_widget_set_vexpand(s1, TRUE); list_box_catalogue = gtk_list_box_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s1), list_box_catalogue); gtk_box_append(GTK_BOX(p1), s1); g_signal_connect(list_box_catalogue, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_stack_add_titled(GTK_STACK(stack), p1, "cat", "Catalogue");

    // PL
    GtkWidget *p2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); GtkWidget *b_pl = gtk_button_new_with_label("➕ Nouvelle Playlist"); gtk_widget_add_css_class(b_pl, "text-button");
    g_signal_connect(b_pl, "clicked", G_CALLBACK(on_creer_pl_clicked), NULL); gtk_box_append(GTK_BOX(p2), b_pl); GtkWidget *pan = gtk_paned_new(GTK_ORIENTATION_VERTICAL); gtk_widget_set_vexpand(pan, TRUE); gtk_box_append(GTK_BOX(p2), pan); GtkWidget *s2 = gtk_scrolled_window_new(); list_box_playlists = gtk_list_box_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s2), list_box_playlists); gtk_paned_set_start_child(GTK_PANED(pan), s2); gtk_paned_set_resize_start_child(GTK_PANED(pan), TRUE); g_signal_connect(list_box_playlists, "row-activated", G_CALLBACK(on_pl_row_activated), NULL); GtkWidget *vb_pl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_box_append(GTK_BOX(vb_pl), gtk_label_new("Contenu :")); GtkWidget *s3 = gtk_scrolled_window_new(); list_box_contenu_playlist = gtk_list_box_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s3), list_box_contenu_playlist); gtk_widget_set_vexpand(s3, TRUE); gtk_box_append(GTK_BOX(vb_pl), s3); g_signal_connect(list_box_contenu_playlist, "row-activated", G_CALLBACK(on_row_activated), NULL); gtk_paned_set_end_child(GTK_PANED(pan), vb_pl); gtk_paned_set_resize_end_child(GTK_PANED(pan), TRUE); gtk_stack_add_titled(GTK_STACK(stack), p2, "pl", "Playlists");

    // ART
    GtkWidget *p3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); GtkWidget *pan_art = gtk_paned_new(GTK_ORIENTATION_VERTICAL); gtk_widget_set_vexpand(pan_art, TRUE); gtk_box_append(GTK_BOX(p3), pan_art); GtkWidget *s4 = gtk_scrolled_window_new(); list_box_artistes = gtk_list_box_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s4), list_box_artistes); gtk_paned_set_start_child(GTK_PANED(pan_art), s4); gtk_paned_set_resize_start_child(GTK_PANED(pan_art), TRUE); g_signal_connect(list_box_artistes, "row-activated", G_CALLBACK(on_artiste_row_activated), NULL); GtkWidget *box_bas_art = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_box_append(GTK_BOX(box_bas_art), gtk_label_new("Titres de cet artiste :")); GtkWidget *s5 = gtk_scrolled_window_new(); list_box_contenu_artiste = gtk_list_box_new(); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s5), list_box_contenu_artiste); gtk_widget_set_vexpand(s5, TRUE); gtk_box_append(GTK_BOX(box_bas_art), s5); g_signal_connect(list_box_contenu_artiste, "row-activated", G_CALLBACK(on_row_activated), NULL); gtk_paned_set_end_child(GTK_PANED(pan_art), box_bas_art); gtk_paned_set_resize_end_child(GTK_PANED(pan_art), TRUE); gtk_stack_add_titled(GTK_STACK(stack), p3, "art", "Artistes");

    // LECTURE
    GtkWidget *p4 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); gtk_widget_set_halign(p4, GTK_ALIGN_CENTER);
    img_pochette = gtk_image_new_from_icon_name("audio-x-generic"); gtk_image_set_pixel_size(GTK_IMAGE(img_pochette), 150); gtk_widget_set_margin_top(img_pochette, 20); gtk_box_append(GTK_BOX(p4), img_pochette);
    lbl_lecture_titre = gtk_label_new("<b>...</b>"); gtk_label_set_use_markup(GTK_LABEL(lbl_lecture_titre), 1); gtk_widget_set_margin_top(lbl_lecture_titre, 10); gtk_box_append(GTK_BOX(p4), lbl_lecture_titre);
    GtkWidget *frm = gtk_frame_new("Paroles"); gtk_widget_set_margin_top(frm, 20); gtk_widget_set_size_request(frm, 400, 300);
    scrolled_window_paroles = gtk_scrolled_window_new();
    txt_paroles_buffer = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(txt_paroles_buffer), FALSE); gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_paroles_buffer), GTK_WRAP_WORD); gtk_text_view_set_justification(GTK_TEXT_VIEW(txt_paroles_buffer), GTK_JUSTIFY_CENTER); gtk_widget_set_margin_start(txt_paroles_buffer, 10); gtk_widget_set_margin_end(txt_paroles_buffer, 10); gtk_widget_set_margin_top(txt_paroles_buffer, 10); gtk_widget_set_margin_bottom(txt_paroles_buffer, 10);
    GtkCssProvider *cp = gtk_css_provider_new(); gtk_css_provider_load_from_data(cp, "textview { font-size: 20px; font-family: Sans; }", -1); gtk_style_context_add_provider(gtk_widget_get_style_context(txt_paroles_buffer), GTK_STYLE_PROVIDER(cp), GTK_STYLE_PROVIDER_PRIORITY_USER); g_object_unref(cp);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window_paroles), txt_paroles_buffer); gtk_frame_set_child(GTK_FRAME(frm), scrolled_window_paroles); gtk_box_append(GTK_BOX(p4), frm);
    gtk_stack_add_titled(GTK_STACK(stack), p4, "lec", "Lecture");

    // FOOTER
    GtkWidget *foot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5); gtk_widget_add_css_class(foot, "footer");
    gtk_box_append(GTK_BOX(vb), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL)); lbl_current_title = gtk_label_new("Prêt"); gtk_box_append(GTK_BOX(foot), lbl_current_title); GtkWidget *ctrl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20); gtk_widget_set_halign(ctrl, GTK_ALIGN_CENTER); GtkWidget *bp = gtk_button_new_from_icon_name("media-skip-backward-symbolic"); btn_play_pause = gtk_button_new_from_icon_name("media-playback-start-symbolic"); GtkWidget *bn = gtk_button_new_from_icon_name("media-skip-forward-symbolic"); gtk_box_append(GTK_BOX(ctrl), bp); gtk_box_append(GTK_BOX(ctrl), btn_play_pause); gtk_box_append(GTK_BOX(ctrl), bn); gtk_box_append(GTK_BOX(foot), ctrl); lbl_status = gtk_label_new(""); gtk_box_append(GTK_BOX(foot), lbl_status); gtk_box_append(GTK_BOX(vb), foot);
    g_signal_connect(btn_play_pause, "clicked", G_CALLBACK(on_btn_play_pause_clicked), NULL); g_signal_connect(bn, "clicked", G_CALLBACK(on_btn_next_clicked), NULL); g_signal_connect(bp, "clicked", G_CALLBACK(on_btn_prev_clicked), NULL);

    charger_catalogue(); charger_listes_annexes(); gtk_window_present(GTK_WINDOW(main_window));
}

int main(int c, char **v) { GtkApplication *a = gtk_application_new("com.projet.v22", G_APPLICATION_DEFAULT_FLAGS); g_signal_connect(a, "activate", G_CALLBACK(activate), NULL); int s = g_application_run(G_APPLICATION(a), c, v); ma_engine_uninit(&engine); g_object_unref(a); return s; }
