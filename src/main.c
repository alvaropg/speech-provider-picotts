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
#include <gio/gunixoutputstream.h>
#include <picoapi.h>
#include <stdlib.h>
#include <stdint.h>

#include "picotts-speech-provider.h"

#define MAX_OUTBUF_SIZE 128
#define PICO_MEM_SIZE   2500000
#define PICO_VOICE_NAME "PicoVoice"

void                  *pico_mem_area    = NULL;
pico_System            pico_system      = NULL;
pico_Engine            pico_engine      = NULL;
pico_Resource          pico_ta_resource = NULL;
pico_Resource          pico_sg_resource = NULL;
PicottsSpeechProvider *skeleton         = NULL;


gboolean
on_handle_synthesize(PicottsSpeechProvider *object,
                     GDBusMethodInvocation *invocation,
                     GVariant *pipe_fd,
                     const gchar *text,
                     const gchar *voice_id,
                     gdouble pitch,
                     gdouble rate,
                     gboolean is_ssml,
                     const gchar *language)
{
	pico_Char *inp = NULL;
	pico_Int16 bytes_sent = 0, bytes_recv = 0, text_remaining = 0, out_data_type = 0;
	pico_Retstring outMessage;
	int ret = 0, getstatus = 0;

	short outbuf[MAX_OUTBUF_SIZE / 2];

	/* Buffer temporal para agrupar audio antes de escribir al pipe */
	guint8 *buffer = NULL;
	gsize buffer_size = 256;
	gsize bufused = 0;

	gint32 pipe_fd_index = -1;
	GError *error = NULL;

	GDBusConnection *conn = g_dbus_method_invocation_get_connection(invocation);
	GIOStream *stream = g_dbus_connection_get_stream(conn);

	if (G_IS_SOCKET_CONNECTION(stream)) {
		GSocket *sock = g_socket_connection_get_socket(G_SOCKET_CONNECTION(stream));
		GSocketFamily fam = g_socket_get_family(sock);
		g_print("DBus connection socket family: %d (UNIX=%d)\n", fam, G_SOCKET_FAMILY_UNIX);
	} else {
		g_warning("DBus connection stream is not a GSocketConnection (type=%s)\n", G_OBJECT_TYPE_NAME(stream));
	}

	g_variant_get(pipe_fd, "h", &pipe_fd_index);

	g_print("Required to synthesize \"%s\" in language \"%s\"\n", text, language);
	if (is_ssml)
		g_warning("SSML not supported\n");

	/* 1) Obtener el fd real (UNIX fd) del argumento tipo 'h' */
	GDBusMessage *msg = g_dbus_method_invocation_get_message(invocation);
	GUnixFDList *fd_list = g_dbus_message_get_unix_fd_list(msg);
        if (fd_list == NULL) {
		g_warning("No unix fd list attached to message (0 fds received)");
		g_dbus_method_invocation_return_error(invocation,
		                                      G_IO_ERROR, G_IO_ERROR_FAILED,
		                                      "No UNIX FD list attached to message");
		return TRUE;
	} else {
		gint n_fds = g_unix_fd_list_get_length(fd_list);
		g_print("Received unix fd list with %d fd(s)\n", n_fds);
	}

	gint out_fd = g_unix_fd_list_get(fd_list, pipe_fd_index, &error);
        if (out_fd < 0) {
		g_printerr("%s\n", error->message);
		g_dbus_method_invocation_return_gerror(invocation, error);
		g_clear_error(&error);
		return TRUE;
	}

	/* 2) Envolver el fd en un GOutputStream y cerrarlo al terminar (EOF para el cliente) */
	GOutputStream *out = g_unix_output_stream_new(out_fd, TRUE);

	/* Reserva buffer */
	buffer = g_malloc(buffer_size);
        if (buffer == NULL) {
		g_warning("Out of memory allocating output buffer");
		g_dbus_method_invocation_return_error(invocation,
		                                      G_IO_ERROR, G_IO_ERROR_FAILED,
		                                      "Out of memory allocating output buffer");
		g_object_unref(out);
		return TRUE;
	}

	/* Helper local: vuelca 'bufused' bytes y resetea bufused */
#define FLUSH_BUFFER_OR_RETURN()                                                       \
	G_STMT_START {                                                                     \
		if (bufused > 0) {                                                             \
			gsize bytes_written = 0;                                                   \
			if (!g_output_stream_write_all(out, buffer, bufused,                        \
			                               &bytes_written, NULL, &error)) {            \
				g_dbus_method_invocation_return_gerror(invocation, error);             \
				g_clear_error(&error);                                                 \
				g_free(buffer);                                                        \
				g_object_unref(out);                                                   \
				return TRUE;                                                           \
			}                                                                           \
			/* write_all garantiza bytes_written == bufused si devuelve TRUE */         \
			bufused = 0;                                                               \
		}                                                                               \
	} G_STMT_END

	/* 3) Síntesis: alimentar texto y recoger audio */
	text_remaining = (pico_Int16)(strlen(text) + 1);
        inp = (pico_Char *)text;

        while (text_remaining > 0) {
		/* feed the text into the engine */
		ret = pico_putTextUtf8(pico_engine, inp, text_remaining, &bytes_sent);
		if (ret) {
			pico_getSystemStatusMessage(pico_system, ret, outMessage);
			g_warning("Cannot put Text (%i): %s\n", ret, outMessage);
			g_dbus_method_invocation_return_error(invocation,
			                                      G_IO_ERROR, G_IO_ERROR_FAILED,
			                                      "Cannot put Text (%i): %s", ret, outMessage);
			g_free(buffer);
			g_object_unref(out);
			return TRUE;
		}

		text_remaining -= bytes_sent;
		inp += bytes_sent;

                do {
			getstatus = pico_getData(pico_engine,
			                         (void *)outbuf,
			                         MAX_OUTBUF_SIZE,
			                         &bytes_recv,
			                         &out_data_type);

			if ((getstatus != PICO_STEP_BUSY) && (getstatus != PICO_STEP_IDLE)) {
				pico_getSystemStatusMessage(pico_system, getstatus, outMessage);
				g_warning("Cannot get Data (%i): %s\n", getstatus, outMessage);
				g_dbus_method_invocation_return_error(invocation,
				                                      G_IO_ERROR, G_IO_ERROR_FAILED,
				                                      "Cannot get Data (%i): %s", getstatus, outMessage);
				g_free(buffer);
				g_object_unref(out);
				return TRUE;
			}

			if (bytes_recv > 0) {
				/* Si no cabe lo que llega, vacía primero */
				if (bufused + (gsize)bytes_recv > buffer_size) {
					FLUSH_BUFFER_OR_RETURN();
				}

				/* Si aun así no cabe (bytes_recv > buffer_size), escribe directo */
				if ((gsize)bytes_recv > buffer_size) {
					gsize bytes_written = 0;
					if (!g_output_stream_write_all(out,
					                               (const guint8 *)outbuf,
					                               (gsize)bytes_recv,
					                               &bytes_written,
					                               NULL,
					                               &error)) {
						g_printerr("Error writting: %s\n", error->message);
						g_dbus_method_invocation_return_gerror(invocation, error);
						g_clear_error(&error);
						g_free(buffer);
						g_object_unref(out);
						return TRUE;
					}
				} else {
					memcpy(buffer + bufused, (const guint8 *)outbuf, (gsize)bytes_recv);
					bufused += (gsize)bytes_recv;
				}
			}

		} while (getstatus == PICO_STEP_BUSY);
	}

	/* Volcar lo que quede pendiente */
	FLUSH_BUFFER_OR_RETURN();

	/* Flush + close (EOF) */
	if (!g_output_stream_flush(out, NULL, &error)) {
		g_dbus_method_invocation_return_gerror(invocation, error);
		g_clear_error(&error);
		g_free(buffer);
		g_object_unref(out);
		return TRUE;
	}

	if (!g_output_stream_close(out, NULL, &error)) {
		g_dbus_method_invocation_return_gerror(invocation, error);
		g_clear_error(&error);
		g_free(buffer);
		g_object_unref(out);
		return TRUE;
	}

	/* Completar el método D-Bus (respuesta al cliente) */
	picotts_speech_provider_complete_synthesize(object, invocation);

	g_free(buffer);
	g_object_unref(out);
	return TRUE;

#undef FLUSH_BUFFER_OR_RETURN
}


static void
on_name_acquired (GDBusConnection *connection,
		  const gchar *name,
		  gpointer user_data)
{
        GVariantBuilder voices;
        GVariantBuilder langs;
	GError *error = NULL;

        skeleton = picotts_speech_provider_skeleton_new();
	g_signal_connect(skeleton, "handle-synthesize", G_CALLBACK(on_handle_synthesize), NULL);

        picotts_speech_provider_set_name(skeleton, "PicoTTS");

	/* Voices */
        g_variant_builder_init(&voices, G_VARIANT_TYPE("a(ssstas)"));
	g_variant_builder_init(&langs, G_VARIANT_TYPE("as"));
        g_variant_builder_add(&langs, "s", "es-ES");
	g_variant_builder_add(&voices, "(ssst@as)",
			      "PicoTTS Spanish (Spain)",
			      "picotts-es_ES",
			      "audio/x-raw,format=S16LE,channels=1,rate=16000",
			      (guint64)0x02u,
			      g_variant_builder_end(&langs));
        GVariant *voices_variant = g_variant_builder_end(&voices);
        picotts_speech_provider_set_voices(skeleton, voices_variant);

        if (!g_dbus_interface_skeleton_export( G_DBUS_INTERFACE_SKELETON(skeleton), connection, "/org/Picotts/Speech/Provider", &error)) {
		g_warning("Export failed: %s", error->message);
		g_clear_error(&error);
	}
}

int main(int argc, char *argv[])
{
	GMainLoop *loop;
        gchar *resource_file = NULL;
        pico_Status ret;
        pico_Retstring outMessage;
	pico_Char *pico_ta_resource_name  = NULL;
	pico_Char *pico_sg_resource_name  = NULL;

        /* initialize picotts */
	pico_mem_area = malloc(PICO_MEM_SIZE);
        ret = pico_initialize(pico_mem_area, PICO_MEM_SIZE, &pico_system);
        if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot initialize pico (%i): %s\n", ret, outMessage);
        }

	char *pico_data_dir = NULL;
        const gchar *const *system_dirs = g_get_system_data_dirs();
        for (; *system_dirs != NULL; system_dirs++) {
		pico_data_dir = g_build_filename(*system_dirs, "pico", NULL);
		if (g_file_test (pico_data_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))
			break;
		g_clear_pointer (&pico_data_dir, g_free);
        }

        if (pico_data_dir == NULL) {
		g_error("Cannot acces PicoTTS language files directory");
	}

        /* load the text analysis Lingware resource file */
	resource_file = g_build_filename(pico_data_dir, "lang", "es-ES_ta.bin", NULL);
        ret = pico_loadResource(pico_system, (pico_Char *) resource_file, &pico_ta_resource);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot load text analysis resource file (%i): %s\n", ret, outMessage);
        }
	if (resource_file)
		g_free(resource_file);

        /* load the signal generation Lingware resource file */
        resource_file = g_build_filename(pico_data_dir, "lang", "es-ES_zl0_sg.bin", NULL);
	ret = pico_loadResource(pico_system, (pico_Char *) resource_file, &pico_sg_resource);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot load signal generation Lingware resource file (%i): %s\n", ret, outMessage);
        }
	if (resource_file)
		g_free(resource_file);

	g_clear_pointer (&pico_data_dir, g_free);

	/* get the text analysis resource name */
	pico_ta_resource_name  = (pico_Char *) malloc(PICO_MAX_RESOURCE_NAME_SIZE);
        ret = pico_getResourceName(pico_system, pico_ta_resource, (char *) pico_ta_resource_name);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot get the text analysis resource name (%i): %s\n", ret, outMessage);
	}

	/* get the signal generation resource name */
	pico_sg_resource_name  = (pico_Char *) malloc( PICO_MAX_RESOURCE_NAME_SIZE );
        ret = pico_getResourceName(pico_system, pico_sg_resource, (char *) pico_sg_resource_name);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot get the signal generation resource name (%i): %s\n", ret, outMessage);
	}

	/* create a voice definition */
        ret = pico_createVoiceDefinition(pico_system, PICO_VOICE_NAME);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot create voice definition (%i): %s\n", ret, outMessage);
        }

	/* add the text analysis resource to the voice */
        ret = pico_addResourceToVoiceDefinition(pico_system, (const pico_Char *)PICO_VOICE_NAME, pico_ta_resource_name);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot add the text analysis resource to the voice (%i): %s\n", ret, outMessage);
	}

	/* add the signal generation resource to the voice. */
	ret = pico_addResourceToVoiceDefinition(pico_system, (const pico_Char *) PICO_VOICE_NAME, pico_sg_resource_name);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot add the signal generation resource to the voice (%i): %s\n", ret, outMessage);
	}

	/* create a new Pico engine. */
	ret = pico_newEngine(pico_system, (const pico_Char *) PICO_VOICE_NAME, &pico_engine);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot create a new pico engine (%i): %s\n", ret, outMessage);
        }

	loop = g_main_loop_new(NULL, FALSE);

	g_bus_own_name (G_BUS_TYPE_SESSION,
			"org.Picotts.Speech.Provider",
			G_BUS_NAME_OWNER_FLAGS_NONE,
			NULL,
			on_name_acquired,
			NULL,
			NULL,
			NULL);

        g_main_loop_run(loop);

	if (pico_system) {
		pico_terminate(&pico_system);
		pico_system = NULL;
	}

	return 0;
}
