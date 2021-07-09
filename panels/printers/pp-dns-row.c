#include <config.h>
#include <glib/gi18n.h>

#include "pp-dns-row.h"
#include "cc-printers-resources.h"

struct _PpDnsRow
{
    GtkListBoxRow parent;

    GtkButton   *pp_dns_row_del_btn;
    GtkLabel    *pp_dns_row_title_label;
    GtkImage    *icon;
};


G_DEFINE_TYPE(PpDnsRow, pp_dns_row, GTK_TYPE_LIST_BOX_ROW);

static void
pp_dns_remove_row_cb(GtkWidget *widget, gpointer row){
  gtk_widget_destroy (row);
}

static void
pp_dns_row_class_init(PpDnsRowClass *klass){

    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(widget_class, "/org/gnome/control-center/printers/pp-dns-row.ui");

    gtk_widget_class_bind_template_child (widget_class, PpDnsRow, pp_dns_row_title_label);
    gtk_widget_class_bind_template_child (widget_class, PpDnsRow, pp_dns_row_del_btn);

    gtk_widget_class_bind_template_callback (widget_class, pp_dns_remove_row_cb);
}



static void
pp_dns_row_init(PpDnsRow *self){
  gtk_widget_init_template(GTK_WIDGET (self));
}

PpDnsRow*
pp_dns_row_new(gchar* name)
{
  PpDnsRow *self;
  self = g_object_new(PP_TYPE_DNS_ROW, NULL);

  gtk_label_set_text(self->pp_dns_row_title_label, name);
  return self;
}




