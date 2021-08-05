#include <gtk/gtk.h>
#include "avahi-ui.h"

int main(int argc, char *argv[])
{
    GtkWidget *d;

    gtk_init(&argc, &argv);

    GtkWidget *window;
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show(window);

    GtkWidget *button;
    button = gtk_button_new_with_label("Start");

    d = aui_service_dialog_new("Choose Web Service", window, button);
    aui_service_dialog_set_browse_service_types(AUI_SERVICE_DIALOG(d), "_http._tcp", "_https._tcp", NULL);

    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK)
        g_message("Selected service name: %s; service type: %s; host name: %s; port: %u",
                  aui_service_dialog_get_service_name(AUI_SERVICE_DIALOG(d)),
                  aui_service_dialog_get_service_type(AUI_SERVICE_DIALOG(d)),
                  aui_service_dialog_get_host_name(AUI_SERVICE_DIALOG(d)),
                  aui_service_dialog_get_port(AUI_SERVICE_DIALOG(d)));
    else
        g_message("Canceled.");

    gtk_widget_destroy(d);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    return 0;
}