/*
 * Copyright 2017 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#include <config.h>
#include <cups/cups.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "pp-details-dialog.h"
#include "pp-maintenance-command.h"
#include "pp-options-dialog.h"
#include "pp-printer.h"
#include "pp-utils.h"
#include "pp-dns-row.h"

#define SUPPLY_BAR_HEIGHT 8

struct _PpPrinterDnsEntry
{
  GtkListBoxRow parent;

  gchar    *printer_name;
  gchar    *printer_make_and_model;
  gchar    *printer_location;
  gchar    *printer_hostname;
  gboolean  is_authorized;
  gint      printer_state;
  gchar *printer_type;
  gchar *printer_domain;
  gchar *printer_port;

  /* Widgets */
  GtkImage       *printer_icon;
  GtkLabel       *printer_status;
  GtkLabel       *printer_name_label;
  GtkLabel       *printer_type_label;
  GtkLabel       *printer_model;
  GtkLabel       *printer_location_label;
  GtkLabel       *printer_location_address_label;
  GtkLabel       *printer_port_value;
  GtkModelButton *remove_printer_menuitem;
  GtkLabel       *printer_domain_value;
  GtkLabel       *printer_domain_label;
  PpDnsWindow    *window_dns;
};


struct _PpPrinterDnsEntryClass
{
  GtkListBoxRowClass parent_class;

  void (*printer_changed) (PpPrinterDnsEntry *printer_dns_entry);
  void (*printer_delete)  (PpPrinterDnsEntry *printer_dns_entry);
  void (*printer_renamed) (PpPrinterDnsEntry *printer_dns_entry, const gchar *new_name);
};

G_DEFINE_TYPE (PpPrinterDnsEntry, pp_printer_dns_entry, GTK_TYPE_LIST_BOX_ROW)

enum {
  IS_DEFAULT_PRINTER,
  PRINTER_DELETE,
  PRINTER_RENAMED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

static void
pp_printer_dns_entry_init (PpPrinterDnsEntry *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

typedef struct {
  gchar *color;
  gchar *type;
  gchar *name;
  gint   level;
} MarkerItem;

static gint
markers_cmp (gconstpointer a,
             gconstpointer b)
{
  MarkerItem *x = (MarkerItem*) a;
  MarkerItem *y = (MarkerItem*) b;

  if (x->level < y->level)
    return 1;
  else if (x->level == y->level)
    return 0;
  else
    return -1;
}

static gchar *
sanitize_printer_model (const gchar *printer_make_and_model)
{
  gchar            *breakpoint = NULL, *tmp2 = NULL;
  g_autofree gchar *tmp = NULL;
  gchar             backup;
  size_t            length = 0;
  gchar            *forbidden[] = {
    "foomatic",
    ",",
    "hpijs",
    "hpcups",
    "(recommended)",
    "postscript (recommended)",
    NULL };
  int     i;

  tmp = g_ascii_strdown (printer_make_and_model, -1);

  for (i = 0; i < g_strv_length (forbidden); i++)
    {
      tmp2 = g_strrstr (tmp, forbidden[i]);
      if (breakpoint == NULL ||
         (tmp2 != NULL && tmp2 < breakpoint))
           breakpoint = tmp2;
    }

  if (breakpoint)
    {
      backup = *breakpoint;
      *breakpoint = '\0';
      length = strlen (tmp);
      *breakpoint = backup;

      if (length > 0)
        return g_strndup (printer_make_and_model, length);
    }
  else
    return g_strdup (printer_make_and_model);

  return NULL;
}


static void
on_printer_rename_cb (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  PpPrinterDnsEntry *self = user_data;

  if (!pp_printer_rename_finish (PP_PRINTER (source_object), result, NULL))
    return;

  g_signal_emit_by_name (self, "printer-renamed", pp_printer_get_name (PP_PRINTER (source_object)));
}

static void
on_show_printer_details_dialog (GtkButton      *button,
                                PpPrinterDnsEntry *self)
{
  const gchar *new_name;
  const gchar *new_location;

  PpDetailsDialog *dialog = pp_details_dialog_new (self->printer_name,
                                                   self->printer_location,
                                                   self->printer_hostname,
                                                   self->printer_make_and_model,
                                                   self->is_authorized);

  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));

  gtk_dialog_run (GTK_DIALOG (dialog));

  new_location = pp_details_dialog_get_printer_location (dialog);
  if (g_strcmp0 (self->printer_location, new_location) != 0)
    printer_set_location (self->printer_name, new_location);

  new_name = pp_details_dialog_get_printer_name (dialog);
  if (g_strcmp0 (self->printer_name, new_name) != 0)
    {
      g_autoptr(PpPrinter) printer = pp_printer_new (self->printer_name);

      pp_printer_rename_async (printer,
                               new_name,
                               NULL,
                               on_printer_rename_cb,
                               self);
    }

  g_signal_emit_by_name (self, "printer-changed");

  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_show_printer_options_dialog (GtkButton      *button,
                                PpPrinterDnsEntry *self)
{
  PpOptionsDialog *dialog;

  dialog = pp_options_dialog_new (self->printer_name, self->is_authorized);

  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void
remove_printer (GtkButton      *button,
                PpPrinterDnsEntry *self)
{
  g_signal_emit_by_name (self, "printer-delete");
  gtk_widget_destroy (self);
  g_hash_table_remove (self->window_dns->service_hash_table, gconstpointer key)

}


enum
{
  PRINTER_READY = 3,
  PRINTER_PROCESSING,
  PRINTER_STOPPED
};

GSList *
pp_printer_dns_entry_get_size_group_widgets (PpPrinterDnsEntry *self)
{
  GSList *widgets = NULL;

  widgets = g_slist_prepend (widgets, self->printer_icon);
  widgets = g_slist_prepend (widgets, self->printer_location_label);
  return widgets;
}


void
pp_printer_dns_entry_update (PpPrinterDnsEntry *self,
                         char* name, char* type, char* domain, char* hostname, char* port,
                         gboolean        is_authorized)
{

  self->printer_name = name;
  gtk_label_set_text (self->printer_name_label,(gchar*) name);

  self->printer_type = type;
  gtk_label_set_text (self->printer_model,(gchar*)  type);

  self->printer_hostname = hostname;
  gtk_label_set_text (self->printer_location_address_label,(gchar*)  hostname);

  self->printer_domain = domain;
  gtk_label_set_text (self->printer_domain_value,(gchar*)  domain);

  self->printer_port = port;
  gtk_label_set_text (self->printer_port_value,(gchar*) port);
}



const gchar *
pp_dns_printer_dns_entry_get_name (PpPrinterDnsEntry *self)
{
  g_return_val_if_fail (PP_IS_PRINTER_DNS_ENTRY (self), NULL);
  return self->printer_name;
}

const gchar *
pp_dns_printer_dns_entry_get_port (PpPrinterDnsEntry *self)
{
  g_return_val_if_fail (PP_IS_PRINTER_DNS_ENTRY (self), NULL);
  return self->printer_port;
}


const gchar *
pp_dns_printer_dns_entry_get_hostname (PpPrinterDnsEntry *self)
{
  g_return_val_if_fail (PP_IS_PRINTER_DNS_ENTRY (self), NULL);
  return self->printer_hostname;
}


const gchar *
pp_dns_printer_dns_entry_get_type (PpPrinterDnsEntry *self)
{
  g_return_val_if_fail (PP_IS_PRINTER_DNS_ENTRY (self), NULL);
  return self->printer_type;
}


const gchar *
pp_dns_printer_dns_entry_get_domain (PpPrinterDnsEntry *self)
{
  g_return_val_if_fail (PP_IS_PRINTER_DNS_ENTRY (self), NULL);
  return self->printer_domain;
}


static void
pp_printer_dns_entry_dispose (GObject *object)
{
  PpPrinterDnsEntry *self = PP_PRINTER_DNS_ENTRY (object);




  g_clear_pointer (&self->printer_name, g_free);
  g_clear_pointer (&self->printer_location, g_free);
  g_clear_pointer (&self->printer_make_and_model, g_free);
  g_clear_pointer (&self->printer_hostname, g_free);

  G_OBJECT_CLASS (pp_printer_dns_entry_parent_class)->dispose (object);
}

static void
pp_printer_dns_entry_class_init (PpPrinterDnsEntryClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/printers/pp-dns-row.ui");

  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_icon);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_name_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_status);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_model);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_location_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_location_address_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_domain_value);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_domain_label);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, printer_port_value);
  gtk_widget_class_bind_template_child (widget_class, PpPrinterDnsEntry, remove_printer_menuitem);


  gtk_widget_class_bind_template_callback (widget_class, on_show_printer_details_dialog);
  gtk_widget_class_bind_template_callback (widget_class, on_show_printer_options_dialog);
  gtk_widget_class_bind_template_callback (widget_class, remove_printer);


  object_class->dispose = pp_printer_dns_entry_dispose;

  signals[IS_DEFAULT_PRINTER] =
    g_signal_new ("printer-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[PRINTER_DELETE] =
    g_signal_new ("printer-delete",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 0);

  signals[PRINTER_RENAMED] =
    g_signal_new ("printer-renamed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL, NULL,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);
}

PpPrinterDnsEntry *
pp_printer_dns_entry_new (char* name, char* type, char* domain, char* hostname, char* port,
                      gboolean     is_authorized, PpDnsWindow* window)
{
  PpPrinterDnsEntry *self;

  self = g_object_new (PP_PRINTER_DNS_ENTRY_TYPE, NULL);
  self->window_dns = window;


  is_authorized = 1;
  pp_printer_dns_entry_update (self,  name, type, domain, hostname, port, is_authorized);

  return self;
}
