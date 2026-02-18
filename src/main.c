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

#define PICO_MEM_SIZE   2500000
#define PICO_VOICE_NAME "PicoVoice"

void           *pico_mem_area    = NULL;
pico_System     pico_system      = NULL;
pico_Engine     pico_engine      = NULL;
pico_Resource   pico_ta_resource = NULL;
pico_Resource   pico_sg_resource = NULL;


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

	if (is_ssml)
          g_info("SSML not supported\n");

	return TRUE;
}

static void
on_name_acquired (GDBusConnection *connection,
		  const gchar *name,
		  gpointer user_data)
{
	PicottsSpeechProvider *skeleton;
        GVariantBuilder voices;
        /* GVariantBuilder langs; */
	const gchar *langs_es[] = { "es-ES", NULL };

        skeleton = picotts_speech_provider_skeleton_new();
	g_signal_connect(skeleton, "handle-synthesize", G_CALLBACK(on_handle_synthesize), NULL);

        picotts_speech_provider_set_name(skeleton, "PicoTTS");

        /* audio_format = "audio/x-raw,format=S16LE,channels=1,rate=256000" */
        /* "a(ssstas)" */
        g_variant_builder_init(&voices, G_VARIANT_TYPE("a(ssstas)"));

        /* g_variant_builder_init(&langs, G_VARIANT_TYPE("as")); */
        /* g_variant_builder_add(&langs, "s", "es-ES"); */
        /* g_variant_builder_add(&voices, "(ssstas)", "Voz Española", "voice-es-1", */
        /*                       "audio/x-raw,format=S16LE,channels=1,rate=256000", */
        /*                       (guint64)0x02u, &langs); */


	/* Example voices */
	g_variant_builder_add(&voices, "(ssstaas)",
			      "PicoTTS Spanish (Spain)",                          /* name */
			      "picotts-es_ES",                                    /* id */
			      "audio/x-raw,format=S16LE,channels=1,rate=256000",  /* format */
			      (guint64)0x02u,                                     /* features: EVENTS_SENTENCE */
			      langs_es);                                          /* languages */

        picotts_speech_provider_set_voices(skeleton, g_variant_builder_end(&voices));

        g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skeleton), connection, "/org/Picotts/Speech/Provider", NULL);
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
        ret = pico_initialize(pico_mem_area, PICO_MEM_SIZE, &pico_system);
        if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot initialize pico (%i): %s\n", ret, outMessage);
	}

        /* load the text analysis Lingware resource file */
	resource_file = g_build_filename("usr", "share", "pico", "lang", "es-ES_ta.bin", NULL);
        ret = pico_loadResource(pico_system, (pico_Char *) resource_file, &pico_ta_resource);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot load text analysis resource file (%i): %s\n", ret, outMessage);
        }
	if (resource_file)
		g_free(resource_file);

        /* load the signal generation Lingware resource file */
        resource_file = g_build_filename("usr", "share", "pico", "lang", "es-ES_zl0_sg.bin", NULL);
	ret = pico_loadResource(pico_system, (pico_Char *) resource_file, &pico_sg_resource);
	if (ret) {
		pico_getSystemStatusMessage(pico_system, ret, outMessage);
		g_error("Cannot load signal generation Lingware resource file (%i): %s\n", ret, outMessage);
        }
	if (resource_file)
		g_free(resource_file);

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

	g_main_loop_run (loop);

	return 0;
}
