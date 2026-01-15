#include "modeles.h"
#include "base_sqlite.h"
#include "lecteur.h"
#include "importation.h"
#include "catalogue.h"
#include "playlists.h"
#include "karaoke.h"

// --- VARIABLES GLOBALES ---
const char *DB_FILE = "data/musique.db";
GtkWidget *main_window;
GtkWidget *lbl_status;
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
GtkWidget *btn_play_pause;

// --- CSS LOADER ---
void load_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

// --- CALLBACKS GÉNÉRAUX ---
void on_search_changed(GtkSearchEntry *e, gpointer u) {
    (void)e; (void)u;
    charger_catalogue();
}

void on_sort_changed(GObject *o, GParamSpec *p, gpointer u) {
    (void)o; (void)p; (void)u;
    charger_catalogue();
}

void on_row_activated(GtkListBox *b, GtkListBoxRow *r, gpointer u) {
    (void)b; (void)u;
    jouer_musique(r);
}

// --- INITIALISATION DE L'INTERFACE ---
static void activate(GtkApplication *app, gpointer u) {
    (void)u;
    init_audio();
    init_db();
    load_css();
   
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Projet Musique - V25 (Lecture Intelligente)");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 950, 700);
   
    GtkWidget *vb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(main_window), vb);
   
    GtkWidget *hb = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(hb, TRUE);
    gtk_box_append(GTK_BOX(vb), hb);
   
    GtkWidget *stack = gtk_stack_new();
    GtkWidget *sb = gtk_stack_sidebar_new();
    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sb), GTK_STACK(stack));
    gtk_widget_set_size_request(sb, 180, -1);
    gtk_box_append(GTK_BOX(hb), sb);
    gtk_widget_set_hexpand(stack, TRUE);
    gtk_box_append(GTK_BOX(hb), stack);

    // --- ONGLET 1 : CATALOGUE ---
    GtkWidget *p1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *box_ctrl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
   
    extern GtkWidget *entry_recherche;
    extern GtkWidget *dd_tri;
   
    entry_recherche = gtk_search_entry_new();
    gtk_widget_set_hexpand(entry_recherche, TRUE);
    g_signal_connect(entry_recherche, "search-changed", G_CALLBACK(on_search_changed), NULL);
    gtk_box_append(GTK_BOX(box_ctrl), entry_recherche);

    const char *options[] = { "🕒 Récent", "🔤 Titre", "🎤 Artiste", NULL };
    dd_tri = gtk_drop_down_new_from_strings(options);
    g_signal_connect(dd_tri, "notify::selected", G_CALLBACK(on_sort_changed), NULL);
    gtk_box_append(GTK_BOX(box_ctrl), dd_tri);

    GtkWidget *b_add = gtk_button_new_with_label("📂 Importer");
    gtk_widget_add_css_class(b_add, "text-button");
    g_signal_connect(b_add, "clicked", G_CALLBACK(on_add_clicked), NULL);
    gtk_box_append(GTK_BOX(box_ctrl), b_add);

    gtk_box_append(GTK_BOX(p1), box_ctrl);
   
    GtkWidget *s1 = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(s1, TRUE);
    list_box_catalogue = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s1), list_box_catalogue);
    gtk_box_append(GTK_BOX(p1), s1);
    g_signal_connect(list_box_catalogue, "row-activated", G_CALLBACK(on_row_activated), NULL);
   
    gtk_stack_add_titled(GTK_STACK(stack), p1, "cat", "Catalogue");

    // --- ONGLET 2 : PLAYLISTS ---
    GtkWidget *p2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *b_pl = gtk_button_new_with_label("➕ Nouvelle Playlist");
    gtk_widget_add_css_class(b_pl, "text-button");
    g_signal_connect(b_pl, "clicked", G_CALLBACK(on_creer_pl_clicked), NULL);
    gtk_box_append(GTK_BOX(p2), b_pl);
   
    GtkWidget *pan = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_vexpand(pan, TRUE);
    gtk_box_append(GTK_BOX(p2), pan);
   
    GtkWidget *s2 = gtk_scrolled_window_new();
    list_box_playlists = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s2), list_box_playlists);
    gtk_paned_set_start_child(GTK_PANED(pan), s2);
    gtk_paned_set_resize_start_child(GTK_PANED(pan), TRUE);
    g_signal_connect(list_box_playlists, "row-activated", G_CALLBACK(on_pl_row_activated), NULL);
   
    GtkWidget *vb_pl = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(vb_pl), gtk_label_new("Contenu :"));
   
    GtkWidget *s3 = gtk_scrolled_window_new();
    list_box_contenu_playlist = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s3), list_box_contenu_playlist);
    gtk_widget_set_vexpand(s3, TRUE);
    gtk_box_append(GTK_BOX(vb_pl), s3);
    g_signal_connect(list_box_contenu_playlist, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_paned_set_end_child(GTK_PANED(pan), vb_pl);
    gtk_paned_set_resize_end_child(GTK_PANED(pan), TRUE);
   
    gtk_stack_add_titled(GTK_STACK(stack), p2, "pl", "Playlists");

    // --- ONGLET 3 : ARTISTES ---
    GtkWidget *p3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *pan_art = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_vexpand(pan_art, TRUE);
    gtk_box_append(GTK_BOX(p3), pan_art);
   
    GtkWidget *s4 = gtk_scrolled_window_new();
    list_box_artistes = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s4), list_box_artistes);
    gtk_paned_set_start_child(GTK_PANED(pan_art), s4);
    gtk_paned_set_resize_start_child(GTK_PANED(pan_art), TRUE);
    g_signal_connect(list_box_artistes, "row-activated", G_CALLBACK(on_artiste_row_activated), NULL);
   
    GtkWidget *box_bas_art = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(box_bas_art), gtk_label_new("Titres de cet artiste :"));
   
    GtkWidget *s5 = gtk_scrolled_window_new();
    list_box_contenu_artiste = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(s5), list_box_contenu_artiste);
    gtk_widget_set_vexpand(s5, TRUE);
    gtk_box_append(GTK_BOX(box_bas_art), s5);
    g_signal_connect(list_box_contenu_artiste, "row-activated", G_CALLBACK(on_row_activated), NULL);
    gtk_paned_set_end_child(GTK_PANED(pan_art), box_bas_art);
    gtk_paned_set_resize_end_child(GTK_PANED(pan_art), TRUE);
   
    gtk_stack_add_titled(GTK_STACK(stack), p3, "art", "Artistes");

    // --- ONGLET 4 : LECTURE ---
    GtkWidget *p4 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(p4, GTK_ALIGN_CENTER);
   
    img_pochette = gtk_image_new_from_icon_name("audio-x-generic");
    gtk_image_set_pixel_size(GTK_IMAGE(img_pochette), 150);
    gtk_widget_set_margin_top(img_pochette, 20);
    gtk_box_append(GTK_BOX(p4), img_pochette);
   
    lbl_lecture_titre = gtk_label_new("<b>...</b>");
    gtk_label_set_use_markup(GTK_LABEL(lbl_lecture_titre), 1);
    gtk_widget_set_margin_top(lbl_lecture_titre, 10);
    gtk_box_append(GTK_BOX(p4), lbl_lecture_titre);
   
    GtkWidget *frm = gtk_frame_new("Paroles");
    gtk_widget_set_margin_top(frm, 20);
    gtk_widget_set_size_request(frm, 400, 300);
   
    scrolled_window_paroles = gtk_scrolled_window_new();
    txt_paroles_buffer = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(txt_paroles_buffer), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(txt_paroles_buffer), GTK_WRAP_WORD);
    gtk_text_view_set_justification(GTK_TEXT_VIEW(txt_paroles_buffer), GTK_JUSTIFY_CENTER);
    gtk_widget_set_margin_start(txt_paroles_buffer, 10);
    gtk_widget_set_margin_end(txt_paroles_buffer, 10);
    gtk_widget_set_margin_top(txt_paroles_buffer, 10);
    gtk_widget_set_margin_bottom(txt_paroles_buffer, 10);
   
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window_paroles), txt_paroles_buffer);
    gtk_frame_set_child(GTK_FRAME(frm), scrolled_window_paroles);
    gtk_box_append(GTK_BOX(p4), frm);
   
    gtk_stack_add_titled(GTK_STACK(stack), p4, "lec", "Lecture");

    // --- FOOTER (Contrôles de lecture) ---
    GtkWidget *foot = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_add_css_class(foot, "footer");
   
    gtk_box_append(GTK_BOX(vb), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));
   
    lbl_current_title = gtk_label_new("Prêt");
    gtk_box_append(GTK_BOX(foot), lbl_current_title);
   
    GtkWidget *ctrl = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(ctrl, GTK_ALIGN_CENTER);
   
    // Bouton Shuffle (Aléatoire)
    GtkWidget *b_shuf = gtk_button_new_from_icon_name("media-playlist-shuffle-symbolic");
    g_signal_connect(b_shuf, "clicked", G_CALLBACK(on_btn_shuffle_clicked), NULL);
    gtk_box_append(GTK_BOX(ctrl), b_shuf);
   
    GtkWidget *bp = gtk_button_new_from_icon_name("media-skip-backward-symbolic");
    btn_play_pause = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    GtkWidget *bn = gtk_button_new_from_icon_name("media-skip-forward-symbolic");
   
    gtk_box_append(GTK_BOX(ctrl), bp);
    gtk_box_append(GTK_BOX(ctrl), btn_play_pause);
    gtk_box_append(GTK_BOX(ctrl), bn);
   
    gtk_box_append(GTK_BOX(foot), ctrl);
   
    lbl_status = gtk_label_new("");
    gtk_box_append(GTK_BOX(foot), lbl_status);
    gtk_box_append(GTK_BOX(vb), foot);
   
    g_signal_connect(btn_play_pause, "clicked", G_CALLBACK(on_btn_play_pause_clicked), NULL);
    g_signal_connect(bn, "clicked", G_CALLBACK(on_btn_next_clicked), NULL);
    g_signal_connect(bp, "clicked", G_CALLBACK(on_btn_prev_clicked), NULL);

    charger_catalogue();
    charger_listes_annexes();
    gtk_window_present(GTK_WINDOW(main_window));
}

int main(int c, char **v) {
    GtkApplication *a = gtk_application_new("com.projet.v25", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(a, "activate", G_CALLBACK(activate), NULL);
    int s = g_application_run(G_APPLICATION(a), c, v);
    g_object_unref(a);
    return s;
}
