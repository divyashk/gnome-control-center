#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <glib.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/error.h>
#include <avahi-common/timeval.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-glib/glib-malloc.h>

/* Callback for Avahi API Timeout Event */
static void
avahi_timeout_event (AVAHI_GCC_UNUSED AvahiTimeout *timeout, AVAHI_GCC_UNUSED void *userdata)
{
    g_message ("Avahi API Timeout reached!");
}

/* Callback for GLIB API Timeout Event */
static gboolean
avahi_timeout_event_glib (void *userdata)
{
    GMainLoop *loop = userdata;
    g_message ("GLIB API Timeout reached, quitting main loop!");
    /* Quit the application */
    g_main_loop_quit (loop);
    return FALSE; /* Don't re-schedule timeout event */
}

/* Callback for state changes on the Client */
static void
avahi_client_callback (AVAHI_GCC_UNUSED AvahiClient *client, AvahiClientState state, void *userdata)
{
    GMainLoop *loop = userdata;
    g_message ("Avahi Client State Change: %d", state);
    if (state == AVAHI_CLIENT_FAILURE)
    {
        /* We we're disconnected from the Daemon */
        g_message ("Disconnected from the Avahi Daemon: %s", avahi_strerror(avahi_client_errno(client)));
        /* Quit the application */
        g_main_loop_quit (loop);
    }
}

static void resolve_callback(
    AvahiServiceResolver *r,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    AVAHI_GCC_UNUSED void *userdata)
{
    assert(r);
    /* Called whenever a service has been resolved successfully or timed out */
    switch (event)
    {
    case AVAHI_RESOLVER_FAILURE:
        fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
        break;
    case AVAHI_RESOLVER_FOUND:
    {
        char a[AVAHI_ADDRESS_STR_MAX], *t;
        fprintf(stderr, "Service '%s' of type '%s' in domain '%s':\n", name, type, domain);
        avahi_address_snprint(a, sizeof(a), address);
        t = avahi_string_list_to_string(txt);
        fprintf(stderr,
                "\t%s:%u (%s)\n"
                "\tTXT=%s\n"
                "\tcookie is %u\n"
                "\tis_local: %i\n"
                "\tour_own: %i\n"
                "\twide_area: %i\n"
                "\tmulticast: %i\n"
                "\tcached: %i\n",
                host_name, port, a,
                t,
                avahi_string_list_get_service_cookie(txt),
                !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
                !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
                !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
                !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
                !!(flags & AVAHI_LOOKUP_RESULT_CACHED));
        avahi_free(t);
    }
    }
    avahi_service_resolver_free(r);
}

static void browse_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void *userdata)
{
    AvahiClient *c = userdata;
    assert(b);
    /* Called whenever a new services becomes available on the LAN or is removed from the LAN */
    switch (event)
    {
    case AVAHI_BROWSER_FAILURE:
        fprintf(stderr, "(Browser) %s\n", avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
        return;
    case AVAHI_BROWSER_NEW:
        fprintf(stderr, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
        /* We ignore the returned resolver object. In the callback
               function we free it. If the server is terminated before
               the callback function is called the server will free
               the resolver for us. */
        if (!(avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, c)))
            fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(c)));
        break;
    case AVAHI_BROWSER_REMOVE:
        fprintf(stderr, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
        break;
    case AVAHI_BROWSER_ALL_FOR_NOW:
    case AVAHI_BROWSER_CACHE_EXHAUSTED:
        fprintf(stderr, "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
        break;
    }
}

int
main (AVAHI_GCC_UNUSED int argc, AVAHI_GCC_UNUSED char *argv[])
{
    GMainLoop *loop = NULL;
    const AvahiPoll *poll_api;
    AvahiGLibPoll *glib_poll;
    AvahiClient *client;
    struct timeval tv;
    const char *version;
    int error;
    AvahiServiceBrowser *sb = NULL;
    /* Optional: Tell avahi to use g_malloc and g_free */
    avahi_set_allocator (avahi_glib_allocator ());

    /* Create the GLIB main loop */
    loop = g_main_loop_new (NULL, FALSE);

    /* Create the GLIB Adaptor */
    glib_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
    poll_api = avahi_glib_poll_get (glib_poll);

        /* Create a new AvahiClient instance */
    client = avahi_client_new (poll_api,            /* AvahiPoll object from above */
                               0,
            avahi_client_callback,                  /* Callback function for Client state changes */
            loop,                                   /* User data */
            &error);                                /* Error return */

    /* Check the error return code */
    if (client == NULL)
    {
        /* Print out the error string */
        g_warning ("Error initializing Avahi: %s", avahi_strerror (error));
        goto fail;
    }

    /* Create the service browser */
    if (!(sb = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_ftp._tcp", NULL, 0, browse_callback, client)))
    {
        fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
        goto fail;
    }

    /* Make a call to get the version string from the daemon */
    version = avahi_client_get_version_string (client);

    /* Check if the call suceeded */
    if (version == NULL)
    {
        g_warning ("Error getting version string: %s", avahi_strerror (avahi_client_errno (client)));
        goto fail;
    }
    g_message ("Avahi Server Version: %s", version);

    /* Start the GLIB Main Loop */
    g_main_loop_run (loop);

fail:
    /* Clean up */
    g_main_loop_unref (loop);
    avahi_client_free (client);
    avahi_glib_poll_free (glib_poll);
    return 0;
}
