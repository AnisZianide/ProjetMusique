#include <gtk/gtk.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>

sqlite3 *db;
GtkWidget *main_listbox; // On garde la liste en variable globale pour pouvoir la rafraîchir

// --- FONCTIONS DE BASE DE DONNÉES ---

// Fonction pour vider la liste actuelle et la re-remplir depuis la BDD
void rafraichir_liste() {
    // 1. On vide la liste actuelle (on supprime tous les enfants)
    GtkWidget *child = gtk_widget_get_first_child(main_listbox);
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(main_listbox), child);
        child = next;
    }

    // 2. On relit la BDD
    const char *sql = "SELECT titre, artiste, annee FROM chansons ORDER BY id DESC"; // Les plus récents en haut
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *titre = (const char *)sqlite3_column_text(stmt, 0);
        const char *artiste = (const char *)sqlite3_column_text(stmt, 1);
        int annee = sqlite3_column_int(stmt, 2);
        char annee_str[10];
        sprintf(annee_str, "%d", annee);

        // Création de la ligne visuelle
        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
        GtkWidget *lbl_titre = gtk_label_new(titre);
        GtkWidget *lbl_artiste = gtk_label_new(artiste);
        GtkWidget *lbl_annee = gtk_label_new(annee_str);

        // Mise en forme
        gtk_widget_set_hexpand(lbl_titre, TRUE);
        gtk_widget_set_halign(lbl_titre, GTK_ALIGN_START);
        gtk_label_set_markup(GTK_LABEL(lbl_titre), "<b>"); // Gras pour le titre (début)
        char markup[256];
        snprintf(markup, sizeof(markup), "<b>%s</b>", titre);
        gtk_label_set_markup(GTK_LABEL(lbl_titre), markup);

        gtk_box_append(GTK_BOX(box), lbl_titre);
        gtk_box_append(GTK_BOX(box), lbl_artiste);
        gtk_box_append(GTK_BOX(box), lbl_annee);

        gtk_list_box_append(GTK_LIST_BOX(main_listbox), box);
    }
    sqlite3_finalize(stmt);
}

// Fonction pour insérer dans la BDD
void ajouter_chanson_bdd(const char *t, const char *a, const char *an) {
    char sql[512];
    // Attention : ceci est une méthode simple, pour un vrai projet pro on utiliserait des "prepared statements" pour la sécurité
    snprintf(sql, sizeof(sql), "INSERT INTO chansons (titre, artiste, annee) VALUES ('%s', '%s', %s);", t, a, an);

    char *errMsg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &errMsg);

    if (rc != SQLITE_OK) {
        g_print("Erreur SQL: %s\n", errMsg);
        sqlite3_free(errMsg);
    } else {
        g_print("Chanson ajoutée !\n");
        rafraichir_liste(); // On met à jour l'affichage direct !
    }
}

// --- INTERFACE GRAPHIQUE ---

// Bouton "Enregistrer" dans la fenêtre d'ajout
static void on_save_clicked(GtkWidget *widget, gpointer data) {
    // On récupère les 3 champs de texte (passés dans un tableau)
    GtkWidget **entries = (GtkWidget **)data;
    GtkEditable *entry_titre = GTK_EDITABLE(entries[0]);
    GtkEditable *entry_artiste = GTK_EDITABLE(entries[1]);
    GtkEditable *entry_annee = GTK_EDITABLE(entries[2]);

    const char *titre = gtk_editable_get_text(entry_titre);
    const char *artiste = gtk_editable_get_text(entry_artiste);
    const char *annee = gtk_editable_get_text(entry_annee);

    // On sauvegarde
    if (g_utf8_strlen(titre, -1) > 0) { // Vérif simple que le titre n'est pas vide
        ajouter_chanson_bdd(titre, artiste, annee);

        // On ferme la fenêtre d'ajout (c'est le parent du parent du bouton... un peu tricky mais ça marche)
        GtkWidget *window = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
        gtk_window_close(GTK_WINDOW(window));
    }

    free(entries); // Nettoyage mémoire
}

// Bouton principal "+"
static void on_add_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Ajouter une musique");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE); // Bloque la fenêtre principale tant que celle-ci est ouverte
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 200);

    // Organisation verticale
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    // Champs de saisie
    GtkWidget *e_titre = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_titre), "Titre");

    GtkWidget *e_artiste = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_artiste), "Artiste");

    GtkWidget *e_annee = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e_annee), "Année");

    gtk_box_append(GTK_BOX(box), e_titre);
    gtk_box_append(GTK_BOX(box), e_artiste);
    gtk_box_append(GTK_BOX(box), e_annee);

    // Bouton Valider
    GtkWidget *btn_save = gtk_button_new_with_label("Enregistrer");
    gtk_widget_set_margin_top(btn_save, 10);
    gtk_box_append(GTK_BOX(box), btn_save);

    // On prépare les données à envoyer au bouton (les 3 champs)
    GtkWidget **entries = malloc(3 * sizeof(GtkWidget*));
    entries[0] = e_titre;
    entries[1] = e_artiste;
    entries[2] = e_annee;

    g_signal_connect(btn_save, "clicked", G_CALLBACK(on_save_clicked), entries);

    gtk_window_present(GTK_WINDOW(dialog));
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *scrolled_window;
    GtkWidget *header;
    GtkWidget *add_button;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Ma Playlist ESIEA");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    // Barre de titre personnalisée (HeaderBar)
    header = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    // Bouton Ajouter
    add_button = gtk_button_new_with_label("+ Ajouter");
    g_signal_connect(add_button, "clicked", G_CALLBACK(on_add_clicked), NULL);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), add_button);

    // Connexion BDD
    sqlite3_open("data/musique.db", &db);

    // Liste avec défilement
    scrolled_window = gtk_scrolled_window_new();
    gtk_window_set_child(GTK_WINDOW(window), scrolled_window);

    main_listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(main_listbox), GTK_SELECTION_NONE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), main_listbox);

    rafraichir_liste();

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("org.esiea.projet", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    if(db) sqlite3_close(db);
    return status;
}
