/*
 * Copyright (C) 2014-2015 Collabora Ltd.
 *   @author George Kiagiadakis <george.kiagiadakis@collabora.com>
 * Copyright (C) 2019, STMicroelectronics - All Rights Reserved
 *   @author Christophe Priouzeau <christophe.priouzeau@st.com>
 *
 * SPDX-License-Identufier: GPL-2.0+
 *
 * NOTE: inspirated from https://github.com/GStreamer/gst-plugins-bad/tree/master/tests/examples/waylandsink
 */

#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#else
#error "Wayland is not supported in GTK+"
#endif

#include <gst/video/videooverlay.h>
#include <wayland/wayland.h> // for gst/wayland/wayland.h

gboolean local_kb_set_key_handler(gpointer user_data);

static gchar *graph = NULL;
static gchar *shader_file = NULL;
static gboolean nofullscreen = FALSE;

static GOptionEntry entries[] = {
	{"No Fullscreen", 'F', 0, G_OPTION_ARG_NONE, &nofullscreen,
		"Do not put video on fullscreeen", NULL},
	{"graph", 0, 0, G_OPTION_ARG_STRING, &graph, "Gstreamer graph to use", NULL},
	{"shader", 0, 0, G_OPTION_ARG_STRING, &shader_file, "Gstreamer shader graph to use", NULL},

	{NULL}
};

typedef struct
{
	GtkWidget *app_widget;
	GtkWidget *video_widget;

	GstElement *pipeline;
	GstVideoOverlay *overlay;

	gchar **argv;
	gint current_uri;             /* index for argv */
	gboolean to_start;
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
			g_print("new state: GST_STATE_VOID_PENDING\n");break;
		case GST_STATE_NULL:
			g_print("new state: GST_STATE_NULL\n");break;
		case GST_STATE_READY:
			g_print("new state: GST_STATE_READY\n");
			if (d->to_start)
				gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
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

static void
error_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
	DemoApp *d = user_data;
	gchar *debug = NULL;
	GError *err = NULL;

	gst_message_parse_error (msg, &err, &debug);

	g_print ("Error: %s\n", err->message);
	g_error_free (err);

	if (debug) {
		g_print ("Debug details: %s\n", debug);
		g_free (debug);
	}

	gst_element_set_state (d->pipeline, GST_STATE_NULL);
}


static guint32 last_touch_tap = 0;

static gboolean
button_notify_event_cb (GtkWidget      *widget,
			GdkEventButton *event,
			gpointer        data)
{
	DemoApp *d = data;
	guint32 diff;
	GstState actual_state;

	if (event->button == GDK_BUTTON_PRIMARY) {
		if (last_touch_tap == 0) {
			last_touch_tap = event->time;
			gst_element_get_state(d->pipeline, &actual_state, NULL, -1);
			if (actual_state == GST_STATE_PAUSED)
				gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
			else
				gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
		} else {
			diff = event->time - last_touch_tap;
			if (last_touch_tap != 0) {
				last_touch_tap = event->time;
				if (diff < 600) {
					//g_print("--> DOUBLE TAP\n");
					gst_element_set_state (d->pipeline, GST_STATE_NULL);
					gtk_main_quit();
				} else {
					gst_element_get_state(d->pipeline, &actual_state, NULL, -1);
					if (actual_state == GST_STATE_PAUSED)
						gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
					else
						gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
					//g_print("--> SIMPLE TAP\n");
				}
				//g_print("--> BEGIN diff = %d\n", diff);
			}
		}
	}

	/* We've handled the event, stop processing */
	return TRUE;
}

static gboolean
touch_notify_event_cb (GtkWidget      *widget,
					   GdkEvent *event,
					   gpointer        data)
{
	DemoApp *d = data;
	guint32 diff;
	GstState actual_state;

	//g_print("--> %s\n", __FUNCTION__);
	switch(event->touch.type) {
	case GDK_TOUCH_BEGIN:
		if (last_touch_tap == 0) {
			last_touch_tap = event->touch.time;
			gst_element_get_state(d->pipeline, &actual_state, NULL, -1);
			if (actual_state == GST_STATE_PAUSED)
				gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
			else
				gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
		} else {
			diff = event->touch.time - last_touch_tap;
			if (last_touch_tap != 0) {
				last_touch_tap = event->touch.time;
				if (diff < 600) {
					//g_print("--> DOUBLE TAP\n");
					gst_element_set_state (d->pipeline, GST_STATE_NULL);
					gtk_main_quit();
				} else {
					gst_element_get_state(d->pipeline, &actual_state, NULL, -1);
					if (actual_state == GST_STATE_PAUSED)
						gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
					else
						gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
					//g_print("--> SIMPLE TAP\n");
				}
				//g_print("--> BEGIN diff = %d\n", diff);
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

static GstBusSyncReply
bus_sync_handler (GstBus * bus, GstMessage * message, gpointer user_data)
{
	DemoApp *d = user_data;

	if (gst_is_wayland_display_handle_need_context_message (message)) {
		GstContext *context;
		GdkDisplay *display;
		struct wl_display *display_handle;

		display = gtk_widget_get_display (d->video_widget);
		display_handle = gdk_wayland_display_get_wl_display (display);
		context = gst_wayland_display_handle_context_new (display_handle);
		gst_element_set_context (GST_ELEMENT (GST_MESSAGE_SRC (message)), context);

		goto drop;
	} else if (gst_is_video_overlay_prepare_window_handle_message (message)) {
		GtkAllocation allocation;
		GdkWindow *window;
		struct wl_surface *window_handle;

		/* GST_MESSAGE_SRC (message) will be the overlay object that we have to
		 * use. This may be waylandsink, but it may also be playbin. In the latter
		 * case, we must make sure to use playbin instead of waylandsink, because
		 * playbin resets the window handle and render_rectangle after restarting
		 * playback and the actual window size is lost */
		d->overlay = GST_VIDEO_OVERLAY (GST_MESSAGE_SRC (message));

		gtk_widget_get_allocation (d->video_widget, &allocation);
		window = gtk_widget_get_window (d->video_widget);
		window_handle = gdk_wayland_window_get_wl_surface (window);

		g_print ("setting window handle and size (%d x %d)\n",
				 allocation.width, allocation.height);

		gst_video_overlay_set_window_handle (d->overlay, (guintptr) window_handle);
		gst_video_overlay_set_render_rectangle (d->overlay, allocation.x,
							allocation.y, allocation.width, allocation.height);

		/* Ask to receive events the drawing area doesn't normally
		 * subscribe to. In particular, we need to ask for the
		 * button press and motion notify events that want to handle.
		 */
		gtk_widget_add_events (d->video_widget, GDK_TOUCH_MASK);
		gtk_widget_add_events (d->video_widget, GDK_BUTTON_PRESS_MASK);
		g_signal_connect (d->video_widget, "touch-event",
				 G_CALLBACK (touch_notify_event_cb), d);
		g_signal_connect(d->video_widget, "button-press-event",
				 G_CALLBACK(button_notify_event_cb), d);
		if (d->to_start) {
			gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
		}

		goto drop;
	}

	return GST_BUS_PASS;

drop:
	gst_message_unref (message);
	return GST_BUS_DROP;
}

/* We use the "draw" callback to change the size of the sink
 * because the "configure-event" is only sent to top-level widgets. */
static gboolean
video_widget_draw_cb (GtkWidget * widget, cairo_t * cr, gpointer user_data)
{
	DemoApp *d = user_data;
	GtkAllocation allocation;

	gtk_widget_get_allocation (widget, &allocation);

	//g_print ("draw_cb x %d, y %d, w %d, h %d\n",
	//    allocation.x, allocation.y, allocation.width, allocation.height);

	if (d->overlay) {
		gst_video_overlay_set_render_rectangle (d->overlay, allocation.x,
							allocation.y, allocation.width, allocation.height);
	}

	/* There is no need to call gst_video_overlay_expose().
	 * The wayland compositor can always re-draw the window
	 * based on its last contents if necessary */

	return FALSE;
}

static void
build_window (DemoApp * d)
{
	GtkWidget *box;

	/* windows */
	d->app_widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW(d->app_widget), "GStreamer Wayland GTK ");
	g_signal_connect (d->app_widget, "destroy", G_CALLBACK (gtk_main_quit), NULL);
	if (!nofullscreen)
		gtk_window_fullscreen(GTK_WINDOW(d->app_widget));
	else {
		//gtk_window_maximize(GTK_WINDOW(d->app_widget));
		gtk_window_set_decorated (GTK_WINDOW(d->app_widget), FALSE);
	}


	box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	d->video_widget = gtk_event_box_new ();
	gtk_widget_set_app_paintable (d->video_widget, TRUE);
	gtk_widget_set_vexpand (d->video_widget, TRUE);
	g_signal_connect (d->video_widget, "draw",
					  G_CALLBACK (video_widget_draw_cb), d);

	gtk_container_add(GTK_CONTAINER (box), d->video_widget);
	gtk_container_add(GTK_CONTAINER (d->app_widget), box);
	gtk_widget_show (d->video_widget);
	gtk_widget_show (box);
	gtk_widget_show_all (d->app_widget);
}

static void
print_keyboard_help (void)
{
	g_print ("\n\nInteractive mode - keyboard controls:\n\n");
	g_print ("\tp:   Pause/Play\n");
	g_print ("\tq:   quit\n");
	g_print ("\n");
}

static gulong io_watch_id;
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
		if (actual_state == GST_STATE_PAUSED)
			gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
		else
			gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
	}
	break;
	case 'q':
		gst_element_set_state (d->pipeline, GST_STATE_NULL);
		local_kb_set_key_handler (NULL);
		gtk_main_quit();
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
gboolean
local_kb_set_key_handler(gpointer user_data)
{
	GIOChannel *io;
	if (io_watch_id > 0) {
		g_source_remove (io_watch_id);
		io_watch_id = 0;
	}
	io = g_io_channel_unix_new (STDIN_FILENO);
	io_watch_id = g_io_add_watch (io, G_IO_IN, io_callback, user_data);
	g_io_channel_unref (io);
	return TRUE;
}
int
main (int argc, char **argv)
{
	DemoApp *d;
	GOptionContext *context;
	GstBus *bus;
	GError *error = NULL;

	gtk_init (&argc, &argv);
	gst_init (&argc, &argv);

	context = g_option_context_new ("- waylandsink gtk demo");
	g_option_context_add_main_entries (context, entries, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		g_printerr ("option parsing failed: %s\n", error->message);
		return 1;
	}

	d = g_slice_new0 (DemoApp);
	build_window (d);

	if (argc > 1) {
		d->argv = argv;
		d->current_uri = 1;
		d->to_start = TRUE;

		if (!nofullscreen)
			d->pipeline = gst_parse_launch ("playbin video-sink='waylandsink fullscreen=true'", NULL);
		else
			d->pipeline = gst_parse_launch ("playbin video-sink='waylandsink'", NULL);

		g_object_set (d->pipeline, "uri", argv[d->current_uri], NULL);

		/* enable looping */
		g_signal_connect (d->pipeline, "about-to-finish",
						  G_CALLBACK (on_about_to_finish), d);
	} else {
		if (graph != NULL) {
			d->to_start = TRUE;
			if (strstr(graph, "waylandsink") != NULL) {
				d->pipeline = gst_parse_launch (graph, NULL);
			} else {
				g_print("ERROR: grap does not contains waylandsink !!!\n");
				g_free(graph);
				return 1;
			}
		} else if (shader_file != NULL) {
			gchar *shader_graph;
			gchar fragment_content[8096];
			FILE *fp;
			size_t nread, data_read, len=0;
			char *line;
			GstElement *customshader;

			d->to_start = TRUE;
			shader_graph = g_strdup_printf("v4l2src ! video/x-raw, format=YUY2, width=320, height=240, framerate=(fraction)15/1 ! videorate  ! video/x-raw,framerate=(fraction)5/1  !  queue ! videoconvert ! video/x-raw,format=RGBA ! queue ! glupload ! queue ! glshader name=customshader ! queue ! gldownload ! queue ! videoconvert ! queue ! waylandsink sync=false fullscreen=true");
			d->pipeline = gst_parse_launch (shader_graph, NULL);

			customshader = gst_bin_get_by_name(GST_BIN(d->pipeline), "customshader");
			g_assert(customshader);

			fp = fopen(shader_file, "r");
			if (fp == NULL) {
				g_print("ERROR: file cannot be openned, please specify absolute path!!\n");
				g_free(shader_file);
				return 1;
			} else {
				data_read = 0;
				bzero(fragment_content, sizeof fragment_content);
				while ((nread = getline(&line, &len, fp) ) != -1) {
					snprintf(fragment_content + data_read, 8096 - data_read, "%s", line);
					data_read += nread;
				}
				free(line);
				fclose(fp);
				//g_print("content: \n%s\n", fragment_content);
			}
			if (customshader) {
				//g_print("set fragment\n");
				g_object_set(customshader, "fragment", fragment_content, NULL);
			}

		} else {
			d->pipeline = gst_parse_launch ("videotestsrc pattern=18 "
											"background-color=0x000062FF ! waylandsink fullscreen=true", NULL);
		}
	}

	bus = gst_pipeline_get_bus (GST_PIPELINE (d->pipeline));
	gst_bus_add_signal_watch (bus);
	g_signal_connect (bus, "message::error", G_CALLBACK (error_cb), d);
	g_signal_connect (bus, "message::state-changed", G_CALLBACK (msg_state_changed), d);

	gst_bus_set_sync_handler (bus, bus_sync_handler, d, NULL);
	gst_object_unref (bus);

	if (nofullscreen) {
		GstPad *pad;
		GstCaps *caps;
		gint width, height;
		const GstStructure *str;
		GstState state;
		gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
		gst_element_get_state (d->pipeline, &state, NULL, GST_SECOND * 5);
		gst_element_seek_simple (d->pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH, GST_SECOND * 1);
		g_signal_emit_by_name (d->pipeline, "get-video-pad", 0, &pad, NULL);
		if (pad) {
			caps =  gst_pad_get_current_caps (pad);
			str = gst_caps_get_structure (caps, 0);
			gst_structure_get_int (str, "width", &width);
			gst_structure_get_int (str, "height", &height);
			g_print("main:set size to %d %d \n", width, height );

			gtk_widget_set_size_request (d->app_widget, width, height);
		}
		gst_element_seek_simple (d->pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_KEY_UNIT | GST_SEEK_FLAG_FLUSH, 0);
		gst_element_set_state (d->pipeline, GST_STATE_NULL);
		gst_element_set_state (d->pipeline, GST_STATE_PAUSED);
	}

	if (local_kb_set_key_handler (d)) {
		g_print ("Press 'h' to see a list of keyboard shortcuts.\n");
	} else {
		g_print ("Interactive keyboard handling in terminal not available.\n");
	}

	gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
	{
		GstState actual_state;

		while (TRUE) {
			gst_element_get_state(d->pipeline, &actual_state, NULL, 2);
			switch (actual_state){
			case GST_STATE_VOID_PENDING:
				g_print("main state: GST_STATE_VOID_PENDING\n");
				break;
			case GST_STATE_NULL:
				g_print("main state: GST_STATE_NULL\n");
				break;
			case GST_STATE_READY:
				g_print("main state: GST_STATE_READY\n");
				break;
			case GST_STATE_PAUSED:
				g_print("main state: GST_STATE_PAUSED\n");
				break;
			case GST_STATE_PLAYING:
				g_print("main state: GST_STATE_PLAYING\n");
				break;
			default:
				break;
			}
			if (actual_state == GST_STATE_PLAYING)
				break;

			sleep(1);
		}
	}

	gtk_main ();

	local_kb_set_key_handler (NULL);

	gst_element_set_state (d->pipeline, GST_STATE_NULL);
	gst_object_unref (d->pipeline);
	g_object_unref (d->app_widget);
	g_slice_free (DemoApp, d);

	g_free(graph);

	return 0;
}
