#include <config.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include "pp-dns-window.h"
#include "pp-dns-row.h"

struct _PpDnsWindow
{
    GtkWindow  parent;

    GtkBuilder *builder;

    GtkButton   *dw_top_box_in;
    GtkLabel    *dw_main;
    GtkFixed    *dw_left;
    GtkButton   *dw_left_add_btn;
    GtkEntry    *dw_left_enter_text;
    GtkListBox  *dw_right_list;


    gboolean is_authorized;

    cups_dest_t dummy_print_dest;
};


G_DEFINE_TYPE(PpDnsWindow, pp_dns_window, GTK_TYPE_WINDOW);

static void
pp_dns_remove_row_cb(GtkWidget *widget, gpointer row){
  gtk_widget_destroy (row);
}

static void
dw_left_add_btn_cb(PpDnsWindow * self){

    GtkEntry* text_entry_to_add = (GtkEntry*)self->dw_left_enter_text;

    // debug
    //if ( text_entry_to_add == NULL) g_debug ("found nULL!");

    cups_dest_t temp_dest;
    temp_dest = self->dummy_print_dest;
    temp_dest.name = (char*)gtk_entry_get_text (text_entry_to_add);



    GtkWidget* row_list = (GtkWidget*) pp_printer_dns_entry_new (temp_dest, self->is_authorized);
    gtk_widget_show (row_list);
    gtk_entry_set_text(text_entry_to_add,"");

    gtk_list_box_insert (self->dw_right_list, row_list, -1);
}

static void
pp_dns_window_class_init(PpDnsWindowClass *klass){

  //GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/control-center/printers/pp-dns-window.ui");

  gtk_widget_class_bind_template_child (widget_class, PpDnsWindow, dw_top_box_in);
  gtk_widget_class_bind_template_child (widget_class, PpDnsWindow, dw_main);
  gtk_widget_class_bind_template_child (widget_class, PpDnsWindow, dw_left);
  gtk_widget_class_bind_template_child (widget_class, PpDnsWindow, dw_left_add_btn);
  gtk_widget_class_bind_template_child (widget_class, PpDnsWindow, dw_left_enter_text);
  gtk_widget_class_bind_template_child (widget_class, PpDnsWindow, dw_right_list);

  gtk_widget_class_bind_template_callback (widget_class, dw_left_add_btn_cb);
}



static void
pp_dns_window_init(PpDnsWindow *self){

  self->is_authorized = gtk_true();
  gtk_widget_init_template(GTK_WIDGET (self));

}

PpDnsWindow*
pp_dns_window_new(cups_dest_t dest_dummy)
{
  PpDnsWindow *self;
  self = g_object_new(PP_DNS_WINDOW_TYPE, NULL);
  g_signal_connect_object (self->dw_left_add_btn, "clicked", G_CALLBACK (dw_left_add_btn_cb), self, G_CONNECT_SWAPPED);
  self->dummy_print_dest = dest_dummy;
  return self;
}
