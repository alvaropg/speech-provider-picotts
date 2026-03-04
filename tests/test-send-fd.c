#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>
#include <unistd.h>
#include <stdio.h>

int main(void)
{
    GError *error = NULL;

    GDBusConnection *c = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!c) {
        g_printerr("bus_get: %s\n", error->message);
        return 1;
    }

    int p[2];
    if (pipe(p) != 0) {
      perror("pipe");
      return 2;
    }

    GUnixFDList *fd_list = g_unix_fd_list_new();
    g_assert(fd_list != NULL);
    g_assert(G_IS_UNIX_FD_LIST(fd_list));
    
    int idx = g_unix_fd_list_append(fd_list, p[1], &error);
    if (idx < 0) {
      g_printerr("append fd: %s\n", error->message);
      g_clear_error(&error);
        return 3;
    }
    close(p[1]);

    /* handle = idx */
    GVariant *params = g_variant_new("(hssddbs)",
                                     idx,
                                     "hola",
                                     "picotts-es_ES",
                                     0.0, 0.0,
                                     FALSE,
                                     "es-ES");

    GUnixFDList *out_fd_list = NULL;
    GVariant *reply = g_dbus_connection_call_with_unix_fd_list_sync(
        c,
        "org.Picotts.Speech.Provider",
        "/org/Picotts/Speech/Provider",
        "org.freedesktop.Speech.Provider",
        "Synthesize",
        params,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        fd_list,
        &out_fd_list,
        NULL,
        &error);

    if (!reply) {
        g_printerr("call: %s\n", error->message);
        return 4;
    }

    g_print("call ok\n");
    g_variant_unref(reply);
    g_object_unref(fd_list);

    /* lee algo del pipe (si el provider escribió) */
    char buf[16];
    ssize_t n = read(p[0], buf, sizeof buf);
    g_print("read() from pipe returned %zd\n", n);
    return 0;
}
