/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * PicoTTS Speech Provider
 * Copyright (C) Alvaro Peña 2026 <alvaropg@gmail.com>
 *
 * PicoTTS Speech Provider is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 2.1 of the License.
 *
 * PicoTTS Speech Provider is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with PicoTTS Speech Provider. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <gio/gio.h>
#include <picoapi.h>
#include <stdlib.h>

#include "picotts-speech-provider.h"

gboolean on_handle_synthesize(PicottsSpeechProvider *object,
                              GDBusMethodInvocation *invocation,
                              GVariant *pipe_fd,
                              const gchar *text,
                              const gchar *voice_id,
                              gdouble pitch,
                              gdouble rate,
                              gboolean is_ssml,
                              const gchar *language)
{
	g_print("Required to synthesize \"%s\" in language \"%s\"\n", text, language);

	return TRUE;
}

static void
on_name_acquired (GDBusConnection *connection,
		  const gchar *name,
		  gpointer user_data)
{
	PicottsSpeechProvider *skeleton;
        GVariantBuilder *builder_voice;
	GVariant *voices;

        skeleton = picotts_speech_provider_skeleton_new();

	g_signal_connect(skeleton, "handle-synthesize", G_CALLBACK(on_handle_synthesize), NULL);

        picotts_speech_provider_set_name(skeleton, "PicoTTS");

        /* audio_format = "audio/x-raw,format=S16LE,channels=1,rate=256000" */
	/* "a(ssstas)" */
        builder_voice = g_variant_builder_new(G_VARIANT_TYPE_ARRAY);
        /* ["Charlie", "gmw/en-US", audio_format, features, ["en-US"]], */
	/* "EVENTS_SENTENCE" 0x2 */
	g_variant_builder_add (builder_voice, "{ssstas}", "Paco", "es-ES", "audio/x-raw,format=S16LE,channels=1,rate=256000", G_GUINT64_CONSTANT(0x2), "es-ES");
	voices = g_variant_new ("as", builder_voice);

        picotts_speech_provider_set_voices(skeleton, voices);
        g_variant_unref(voices);
	g_variant_unref(builder_voice);

        g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skeleton), connection, "/org/Picotts/Speech/Provider", NULL);
}

int main(int argc, char *argv[])
{
	GMainLoop *loop;

	loop = g_main_loop_new(NULL, FALSE);

	g_bus_own_name (G_BUS_TYPE_SYSTEM,
			"org.Picotts.Speech.Provider",
			G_BUS_NAME_OWNER_FLAGS_NONE,
			NULL,
			on_name_acquired,
			NULL,
			NULL,
			NULL);

	g_main_loop_run (loop);

	return 0;
}
