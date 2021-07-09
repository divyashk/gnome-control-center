#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PP_TYPE_DNS_ROW (pp_dns_row_get_type())
G_DECLARE_FINAL_TYPE (PpDnsRow, pp_dns_row, PP, DNS_ROW, GtkListBoxRow)

PpDnsRow* pp_dns_row_new(gchar* name);

G_END_DECLS
