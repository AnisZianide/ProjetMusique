#include <gtk/gtk.h>

// Cette fonction se lance quand l'application démarre
static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;

    // Création de la fenêtre
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Mon Projet Musique ESIEA");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    // Afficher la fenêtre
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    // Initialisation de l'application (l'identifiant doit être unique comme une URL inversée)
    app = gtk_application_new("org.esiea.projet.musique", G_APPLICATION_DEFAULT_FLAGS);

    // Connexion du signal "activate" à notre fonction
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    // Lancement de la boucle
    status = g_application_run(G_APPLICATION(app), argc, argv);

    // Nettoyage
    g_object_unref(app);

    return status;
}
