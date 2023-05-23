/*
 * Copyright (C) 2014-2015 Collabora Ltd.
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 * Copyright (C) 2019, STMicroelectronics - All Rights Reserved
 *   @author Christophe Priouzeau <christophe.priouzeau@st.com>
 *
 * SPDX-License-Identufier: GPL-2.0+
 *
 * NOTE: inspired from https://github.com/GStreamer/gst-plugins-bad/tree/master/tests/examples/waylandsink
 */

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

static gchar *graph = NULL;
static gchar *shader_file = NULL;
static gboolean nofullscreen = FALSE;

static GOptionEntry entries[] = {
	{"nofullscreen", 'F', 0, G_OPTION_ARG_NONE, &nofullscreen,
		"Do not put video on fullscreeen", NULL},
	{"width", 'w', 0, G_OPTION_ARG_INT, &window_width, "Windows Width", NULL},
	{"height", 'h', 0, G_OPTION_ARG_INT, &window_height, "Windows Height", NULL},
	{"graph", 0, 0, G_OPTION_ARG_STRING, &graph, "Gstreamer graph to use", NULL},
	{"shader", 0, 0, G_OPTION_ARG_STRING, &shader_file, "Gstreamer shader graph to use", NULL},

	{NULL}
};

typedef struct
{
	GtkWidget *window_widget;

	GstElement *pipeline;

	GMainLoop *loop;
	guint io_watch_id;

	gchar **argv;
	gint current_uri;             /* index for argv */

	guint32 last_touch_tap;
} DemoApp;

static void
on_about_to_finish (GstElement * playbin, DemoApp * d)
{
	if (d->argv[++d->current_uri] == NULL)
		d->current_uri = 1;

	g_print ("Now playing %s\n", d->argv[d->current_uri]);
	g_object_set (playbin, "uri", d->argv[d->current_uri], NULL);
}

static void
msg_state_changed (GstBus * bus, GstMessage * message, gpointer user_data)
{
	const GstStructure *s;
	DemoApp *d = user_data;

	s = gst_message_get_structure (message);

	/* We only care about state changed on the pipeline */
	if (s && GST_MESSAGE_SRC (message) == GST_OBJECT_CAST (d->pipeline)) {
		GstState old, new, pending;

		gst_message_parse_state_changed (message, &old, &new, &pending);

		switch (new){
		case GST_STATE_VOID_PENDING:
			g_print("new state: GST_STATE_VOID_PENDING\n");
			break;
		case GST_STATE_NULL:
			g_print("new state: GST_STATE_NULL\n");
			break;
		case GST_STATE_READY:
			g_print("new state: GST_STATE_READY\n");
			break;
		case GST_STATE_PAUSED:
			g_print("new state: GST_STATE_PAUSED\n");
			break;
		case GST_STATE_PLAYING:
			g_print("new state: GST_STATE_PLAYING\n");
			break;
		default:
			break;
		}
	}
}

static gboolean
gstreamer_bus_callback (GstBus * bus, GstMessage * message, void *data)
{
	DemoApp *d = data;

	//g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

	switch (GST_MESSAGE_TYPE (message)) {
	case GST_MESSAGE_ERROR:{
		GError *err;
		gchar *debug;

		g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

		gst_message_parse_error (message, &err, &debug);
		g_print ("Error: %s\n", err->message);
		g_error_free (err);
		if (debug) {
			g_print ("Debug details: %s\n", debug);
			g_free (debug);
		}
		g_main_loop_quit (d->loop);
		break;
	}

	case GST_MESSAGE_STATE_CHANGED:
		msg_state_changed (bus, message, data);
		break;

	case GST_MESSAGE_EOS:
		/* end-of-stream */
		g_print ("EOS\n");
		g_main_loop_quit (d->loop);
		break;

	default:
		/* unhandled message */
		break;
	}
	return TRUE;
}

static gboolean
button_notify_event_cb (GtkWidget * widget, GdkEventButton * eventButton,
	gpointer data)
{
	DemoApp *d = data;

	if (eventButton->type == GDK_BUTTON_PRESS) {
		GstState actual_state;

		gst_element_get_state(d->pipeline, &actual_state, NULL, -1);
		if (actual_state == GST_STATE_PAUSED)
			gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
		else
			gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
	} else if (eventButton->type == GDK_2BUTTON_PRESS) {
		g_main_loop_quit (d->loop);
	}

	/* We've handled the event, stop processing */
	return TRUE;
}

static gboolean
touch_notify_event_cb (GtkWidget * widget, GdkEvent * event, gpointer data)
{
	DemoApp *d = data;
	guint32 diff;
	GstState actual_state;

	g_print("--> %s\n", __FUNCTION__);
	switch(event->touch.type) {
	case GDK_TOUCH_BEGIN:
		if (d->last_touch_tap == 0) {
			d->last_touch_tap = event->touch.time;
			gst_element_get_state(d->pipeline, &actual_state, NULL, -1);
			if (actual_state == GST_STATE_PAUSED)
				gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
			else
				gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
		} else {
			diff = event->touch.time - d->last_touch_tap;
			if (d->last_touch_tap != 0) {
				d->last_touch_tap = event->touch.time;
				if (diff < 600) {
					g_print("--> DOUBLE TAP\n");
					g_main_loop_quit (d->loop);
				} else {
					gst_element_get_state(d->pipeline, &actual_state, NULL, -1);
					if (actual_state == GST_STATE_PAUSED)
						gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
					else
						gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
					g_print("--> SIMPLE TAP\n");
				}
				g_print("--> BEGIN diff = %d\n", diff);
			}
		}
		break;
	case GDK_TOUCH_UPDATE:
		//g_print("--> UPDATE\n");
		break;
	case GDK_TOUCH_END:
		//g_print("--> END\n");
		break;
	case GDK_TOUCH_CANCEL:
		//g_print("--> CANCEL\n");
		break;
	default:
		break;
		//g_print("--> something else \n");
	}
	/* We've handled it, stop processing */
	return TRUE;
}

static void
build_window (DemoApp * d)
{
	GtkCssProvider* provider;
	GstElement *sink;
	GtkWidget *video_widget;

	/* windows */
	d->window_widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(d->window_widget), "GStreamer Wayland GTK ");
	g_signal_connect (d->window_widget, "destroy",
			G_CALLBACK (gtk_widget_destroyed), &d->window_widget);
	g_signal_connect_swapped (d->window_widget, "destroy",
			G_CALLBACK (g_main_loop_quit), d->loop);

	if (!nofullscreen)
		gtk_window_fullscreen(GTK_WINDOW(d->window_widget));
	//else {
	//	gtk_window_set_decorated (GTK_WINDOW(d->window_widget), FALSE);
	//}

	/* styling background color to black */
	const char *data = "#transparent_bg,GtkDrawingArea {\n"
			"    background-color: rgba (88%, 88%, 88%, 1.0);\n"
			"}";

	provider = gtk_css_provider_new();
	gtk_css_provider_load_from_data(provider, data, -1, NULL);
	gtk_style_context_add_provider(gtk_widget_get_style_context(d->window_widget),
                                   GTK_STYLE_PROVIDER(provider),
                                   GTK_STYLE_PROVIDER_PRIORITY_USER);
	g_object_unref(provider);

	gtk_widget_set_name(d->window_widget, "transparent_bg");

	sink = gst_bin_get_by_name (GST_BIN (d->pipeline), "gtkwsink");
	if (!sink && !g_strcmp0 (G_OBJECT_TYPE_NAME (d->pipeline), "GstPlayBin")) {
		g_object_get (d->pipeline, "video-sink", &sink, NULL);
		if (sink && g_strcmp0 (G_OBJECT_TYPE_NAME (sink), "GstGtkWaylandSink") != 0
				&& GST_IS_BIN (sink)) {
			GstBin *sinkbin = GST_BIN (sink);
			sink = gst_bin_get_by_name (sinkbin, "gtkwsink");
			gst_object_unref (sinkbin);
		}
	}
	g_assert (sink);
	g_assert (!g_strcmp0 (G_OBJECT_TYPE_NAME (sink), "GstGtkWaylandSink"));

	g_object_get (sink, "widget", &video_widget, NULL);
	gtk_widget_set_support_multidevice (video_widget, TRUE);
	gtk_widget_set_vexpand (video_widget, TRUE);
	g_signal_connect (video_widget, "touch-event",
			G_CALLBACK (touch_notify_event_cb), d);
	g_signal_connect (video_widget, "button-press-event",
			G_CALLBACK (button_notify_event_cb), d);

	// Override the system settings to match other demos more closely
	g_object_set (gtk_settings_get_default (),
			"gtk-double-click-time", 600,
			"gtk-double-click-distance", 100,
			NULL);

	gtk_container_add(GTK_CONTAINER (d->window_widget), video_widget);
	gtk_widget_show_all (d->window_widget);

	g_object_unref (video_widget);
	gst_object_unref (sink);
}

static void
print_keyboard_help (void)
{
	g_print ("\n\nInteractive mode - keyboard controls:\n\n");
	g_print ("\tp:   Pause/Play\n");
	g_print ("\tq:   quit\n");
	g_print ("\n");
}

static void
keyboard_cb (const gchar key_input, gpointer user_data)
{
	DemoApp *d = user_data;
	gchar key = '\0';

	/* only want to switch/case on single char, not first char of string */
	if (key_input != '\0')
		key = g_ascii_tolower (key_input);

	switch (key) {
	case 'h':
		print_keyboard_help ();
		break;
	case 'p':
	{
		GstState actual_state;
		gst_element_get_state(d->pipeline, &actual_state, NULL, -1);
		if (actual_state == GST_STATE_PLAYING)
			gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
		else
			gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
		break;
	}
	case 'q':
		g_main_loop_quit(d->loop);
		break;
	default:
		break;
	}
}

static gboolean
io_callback (GIOChannel * io, GIOCondition condition, gpointer data)
{
	gchar in;
	GError *error = NULL;

	switch (g_io_channel_read_chars (io, &in, 1, NULL, &error)) {
	case G_IO_STATUS_NORMAL:
		keyboard_cb(in, data);
		return TRUE;
	case G_IO_STATUS_ERROR:
		g_printerr ("IO error: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	case G_IO_STATUS_EOF:
		g_warning ("No input data available");
		return TRUE;
	case G_IO_STATUS_AGAIN:
		return TRUE;
	default:
		g_return_val_if_reached (FALSE);
		break;
	}

	return FALSE;
}

static void
local_kb_set_key_handler (DemoApp *d, gboolean enable)
{
	GIOChannel *io;
	if (d->io_watch_id > 0) {
		g_source_remove (d->io_watch_id);
		d->io_watch_id = 0;
	}
	if (enable) {
		io = g_io_channel_unix_new (STDIN_FILENO);
		d->io_watch_id = g_io_add_watch (io, G_IO_IN, io_callback, d);
		g_io_channel_unref (io);
	}
}

int
main (int argc, char **argv)
{
	DemoApp app = {NULL}, *d = &app;
	GOptionContext *context;
	GstBus *bus;
	GError *error = NULL;
	guint bus_watch_id = 0;
	int ret = 0;

	gtk_init (&argc, &argv);
	gst_init (&argc, &argv);

	context = g_option_context_new ("- waylandsink gtk demo");
	g_option_context_add_main_entries (context, entries, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_option_context_free (context);
		goto out;
	}
	g_option_context_free (context);

	if (argc > 1) {
		d->argv = argv;
		d->current_uri = 1;

		d->pipeline = gst_parse_launch ("playbin video-sink=gtkwaylandsink", &error);
		if (error)
			goto out;

		g_object_set (d->pipeline, "uri", argv[d->current_uri], NULL);

		/* enable looping */
		g_signal_connect (d->pipeline, "about-to-finish",
						  G_CALLBACK (on_about_to_finish), d);
	} else {
		if (graph != NULL) {
			if (strstr(graph, "gtkwaylandsink") != NULL) {
				d->pipeline = gst_parse_launch (graph, NULL);
			} else {
				g_print("ERROR: graph does not contain gtkwaylandsink !!!\n");
				ret = 1;
				goto out;
			}
		} else if (shader_file != NULL) {
			gchar *fragment_content;
			GFile *file;
			GstElement *customshader;

			d->pipeline = gst_parse_launch ("v4l2src ! "
				"video/x-raw, format=YUY2, width=320, height=240, framerate=(fraction)15/1 ! "
				"videorate  ! video/x-raw,framerate=(fraction)5/1  ! "
				"queue ! videoconvert ! video/x-raw,format=RGBA ! "
				"queue ! glupload ! queue ! glshader name=customshader ! queue ! gldownload ! "
				"queue ! videoconvert ! "
				"queue ! gtkwaylandsink name=gtkwsink", &error);
			if (error)
				goto out;

			file = g_file_new_for_path (shader_file);
			g_file_load_contents (file, NULL, &fragment_content, NULL, NULL, &error);
			g_object_unref (file);
			if (error)
				goto out;

			customshader = gst_bin_get_by_name(GST_BIN(d->pipeline), "customshader");
			g_assert(customshader);

			//g_print("set fragment, content: \n%s\n", fragment_content);
			g_object_set(customshader, "fragment", fragment_content, NULL);

			gst_object_unref (customshader);
			g_free (fragment_content);
		} else {
			d->pipeline = gst_parse_launch ("videotestsrc pattern=18 "
					"background-color=0x000062FF ! gtkwaylandsink name=gtkwsink",
					&error);
			if (error)
				goto out;
		}
	}

	d->loop = g_main_loop_new (NULL, FALSE);
	build_window (d);

	bus = gst_pipeline_get_bus (GST_PIPELINE (d->pipeline));
	bus_watch_id = gst_bus_add_watch (bus, gstreamer_bus_callback, d);
	gst_object_unref (bus);

	local_kb_set_key_handler (d, TRUE);
	g_print ("Press 'h' to see a list of keyboard shortcuts.\n");

	gst_element_set_state (d->pipeline, GST_STATE_PLAYING);

	g_main_loop_run (d->loop);

	local_kb_set_key_handler (d, FALSE);

	gst_element_set_state (d->pipeline, GST_STATE_NULL);

out:
	if (error) {
		g_printerr ("ERROR: %s\n", error->message);
		ret = 1;
	}
	g_clear_error (&error);

	if (bus_watch_id)
		g_source_remove (bus_watch_id);

	gst_clear_object (&d->pipeline);
	if (d->window_widget) {
		g_print ("destroy window\n");
		g_signal_handlers_disconnect_by_data (d->window_widget, d->loop);
		gtk_widget_destroy (d->window_widget);
	}
	g_clear_pointer (&d->loop, g_main_loop_unref);

	g_free(graph);
	g_free(shader_file);

	return ret;
}
