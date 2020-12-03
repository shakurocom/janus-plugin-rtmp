/*
 * Janus RTMP Plugin - broadcasts publisher's streams to external RTMP services.
 * Copyright (C) 2020 Shakuro (https://shakuro.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Aleksey Gureev <agureiev@shakuro.com>
 */

#include "janus/plugin.h"
#include "janus/record.h"
#include "janus/utils.h"

#include <errno.h>
#include <gst/gst.h>

#include "pipeline.h"

/* Plugin information */
#define PLUGIN_VERSION         1
#define PLUGIN_VERSION_STRING  "0.0.1"
#define PLUGIN_DESCRIPTION     "This is a live streaming plugin for Janus, allowing WebRTC peers to send their media to RTMP servers via Gstreamer."
#define PLUGIN_NAME            "JANUS SH live video plugin"
#define PLUGIN_AUTHOR          "agureiev@shakuro.com"
#define PLUGIN_PACKAGE         "janus.plugin.rtmp"

/* Plugin methods */
janus_plugin *create(void);
int plugin_init(janus_callbacks *callback, const char *config_path);
void plugin_destroy(void);
int plugin_get_api_compatibility(void);
int plugin_get_version(void);
const char *plugin_get_version_string(void);
const char *plugin_get_description(void);
const char *plugin_get_name(void);
const char *plugin_get_author(void);
const char *plugin_get_package(void);
void plugin_create_session(janus_plugin_session *handle, int *error);
struct janus_plugin_result *plugin_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
void plugin_setup_media(janus_plugin_session *handle);
void plugin_hangup_media(janus_plugin_session *handle);
void plugin_destroy_session(janus_plugin_session *handle, int *error);
json_t *plugin_query_session(janus_plugin_session *handle);



static janus_plugin plugin_plugin =
  JANUS_PLUGIN_INIT (
    .init = plugin_init,
    .destroy = plugin_destroy,

    .get_api_compatibility = plugin_get_api_compatibility,
    .get_version = plugin_get_version,
    .get_version_string = plugin_get_version_string,
    .get_description = plugin_get_description,
    .get_name = plugin_get_name,
    .get_author = plugin_get_author,
    .get_package = plugin_get_package,

    .create_session = plugin_create_session,
    .handle_message = plugin_handle_message,
    .setup_media = plugin_setup_media,
    .hangup_media = plugin_hangup_media,
    .destroy_session = plugin_destroy_session,
    .query_session = plugin_query_session,
  );

static volatile gint initialized = 0, stopping = 0;
static volatile gint next_port = 11000;

/* Plugin creator */
janus_plugin *create(void) {
  JANUS_LOG(LOG_VERB, "%s created!\n", PLUGIN_NAME);
  return &plugin_plugin;
}

/* Parameter validation */
static struct janus_json_parameter request_parameters[] = {
  {"request", JSON_STRING, JANUS_JSON_PARAM_REQUIRED}
};
static struct janus_json_parameter start_live[] = {
  {"url", JSON_STRING, JANUS_JSON_PARAM_REQUIRED}
};


/** Sessions */
typedef struct plugin_session {
  janus_plugin_session *handle;

  gboolean started;
  GstElement *pipeline;
  GstBus *bus;

  janus_mutex mutex;
} plugin_session;
static GHashTable *sessions;
static janus_mutex sessions_mutex;


/* Error codes */
#define ERROR_INVALID_REQUEST   411
#define ERROR_INVALID_ELEMENT   412
#define ERROR_MISSING_ELEMENT   413
#define ERROR_UNKNOWN_ERROR     499

/* Internal functions forward declaration. */
void stop_session_pipeline(plugin_session *session);
static gboolean bus_callback(GstBus * bus, GstMessage * message, gpointer data);
json_t *create_error_response(int error_code, char *error_cause);
plugin_session *session_from_handle(janus_plugin_session *handle);
gboolean is_valid_url(const char *url);

/* Message handlers. */
janus_plugin_result *handle_message(plugin_session *session, json_t *root);
janus_plugin_result *handle_message_start(plugin_session *session, json_t* root);
janus_plugin_result *handle_message_stop(plugin_session *session, json_t *root);


// ------------------------------------------------------------------------------------------------
// Plugin implementation
// ------------------------------------------------------------------------------------------------

int plugin_init(janus_callbacks *callback, const char *config_path) {
  JANUS_LOG(LOG_INFO, "%s initialized!\n", PLUGIN_NAME);
  sessions = g_hash_table_new(NULL, NULL);
  g_atomic_int_set(&initialized, 1);

  gst_init(NULL, NULL);

  return 0;
}

void plugin_destroy(void) {
  JANUS_LOG(LOG_INFO, "%s destroyed!\n", PLUGIN_NAME);
  g_hash_table_destroy(sessions);

  gst_deinit();
}

int plugin_get_api_compatibility(void) {
  return JANUS_PLUGIN_API_VERSION;
}

int plugin_get_version(void) {
  return PLUGIN_VERSION;
}

const char *plugin_get_version_string(void) {
  return PLUGIN_VERSION_STRING;
}

const char *plugin_get_description(void) {
  return PLUGIN_DESCRIPTION;
}

const char *plugin_get_name(void) {
  return PLUGIN_NAME;
}

const char *plugin_get_author(void) {
  return PLUGIN_AUTHOR;
}

const char *plugin_get_package(void) {
  return PLUGIN_PACKAGE;
}

static plugin_session *plugin_lookup_session(janus_plugin_session *handle) {
  plugin_session *session = NULL;
  if (g_hash_table_contains(sessions, handle)) {
    session = (plugin_session *)handle->plugin_handle;
  }
  return session;
}

void plugin_create_session(janus_plugin_session *handle, int *error) {
  if (g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
    *error = -1;
    return;
  }

  plugin_session *session = g_malloc0(sizeof(plugin_session));
  session->handle = handle;
  session->started = FALSE;
  session->pipeline = NULL;
  handle->plugin_handle = session;

  janus_mutex_lock(&sessions_mutex);
  g_hash_table_insert(sessions, handle, session);
  janus_mutex_unlock(&sessions_mutex);

  return;
}

void plugin_destroy_session(janus_plugin_session *handle, int *error) {
  if (g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
    *error = -1;
    return;
  }

  janus_mutex_lock(&sessions_mutex);

  plugin_session *session = plugin_lookup_session(handle);
  if (!session) {
    JANUS_LOG(LOG_ERR, "No Live session associated with this handle...\n");
    *error = -2;
  } else {
    JANUS_LOG(LOG_VERB, "Removing Live session...\n");
    stop_session_pipeline(session);
    g_hash_table_remove(sessions, handle);
  }

  janus_mutex_unlock(&sessions_mutex);
  return;
}

json_t *plugin_query_session(janus_plugin_session *handle) {
  json_t *result;

  if (!g_atomic_int_get(&stopping) && g_atomic_int_get(&initialized) && session_from_handle(handle) != NULL) {
    result = json_object();
  }

  return result;
}




struct janus_plugin_result *plugin_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep) {
  janus_plugin_result *result;

  // Check we aren't stopping and initialized
  if (g_atomic_int_get(&stopping) || !g_atomic_int_get(&initialized)) {
    result = janus_plugin_result_new(JANUS_PLUGIN_ERROR, g_atomic_int_get(&stopping) ? "Shutting down" : "Plugin not initialized", NULL);
  } else {
    plugin_session *session = session_from_handle(handle);

    if (session) {
      result = handle_message(session, message);
    } else {
      JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
      result = janus_plugin_result_new(JANUS_PLUGIN_ERROR, "No session associated with this handle", NULL);
    }
  }

  // Release resources
  if (message != NULL) json_decref(message);
  if (jsep != NULL) json_decref(jsep);
  g_free(transaction);

  return result;
}

void plugin_setup_media(janus_plugin_session *handle) {
  JANUS_LOG(LOG_INFO, "[%s-%p] WebRTC media is now available\n", PLUGIN_PACKAGE, handle);
}

void plugin_hangup_media(janus_plugin_session *handle) {
  JANUS_LOG(LOG_INFO, "[%s-%p] No WebRTC media anymore\n", PLUGIN_PACKAGE, handle);
  janus_mutex_lock(&sessions_mutex);
  plugin_session *session = plugin_lookup_session(handle);
  stop_session_pipeline(session);
  janus_mutex_unlock(&sessions_mutex);
}

// ------------------------------------------------------------------------------------------------
// Message handlers
// ------------------------------------------------------------------------------------------------

janus_plugin_result *handle_message(plugin_session *session, json_t *root) {
  janus_plugin_result *result;

  /* Increase the reference counter for this session: we'll decrease it after we handle the message */
  if (!root) return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "No message", NULL);
  if (!json_is_object(root)) return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "JSON error: not an object", NULL);

  /* Pre-parse the message */
  int error_code = 0;
  char error_cause[512];

  /* Get the request first */
  JANUS_VALIDATE_JSON_OBJECT(root, request_parameters,
    error_code, error_cause, TRUE,
    ERROR_MISSING_ELEMENT, ERROR_INVALID_ELEMENT);
  // TODO: do something with error_code / error_cause from parser

  json_t *request = json_object_get(root, "request");
  const char *request_text = json_string_value(request);

  if (!strcasecmp(request_text, "start")) {
    result = handle_message_start(session, root);
  } else if (!strcasecmp(request_text, "stop")) {
    result = handle_message_stop(session, root);
  }

  if (!result) {
    JANUS_LOG(LOG_VERB, "Unknown request '%s'\n", request_text);
    result = janus_plugin_result_new(JANUS_PLUGIN_ERROR, "Unknown request", NULL);
  }

  return result;
}

janus_plugin_result *handle_message_start(plugin_session *session, json_t* root) {
  JANUS_LOG(LOG_INFO, "[%s] Handling start\n", PLUGIN_PACKAGE);

  json_t *value = json_object_get(root, "url");
  const char *url = json_string_value(value);

  if (!is_valid_url(url)) {
    return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "Invalid URL format", NULL);
  }

  // generate ports
  janus_mutex_lock(&session->mutex);
  int audio_port = next_port++;
  int video_port = next_port++;
  GstElement *pipeline = start_pipeline(url, audio_port, video_port);

  // Start watching the pipeline bus for events
  GstBus *bus = gst_element_get_bus(pipeline);
  guint _bus_watch_id = gst_bus_add_watch(bus, bus_callback, NULL);

  session->pipeline = pipeline;
  session->bus = bus;
  janus_mutex_unlock(&session->mutex);


  // This releases the `url` the URL
  if (value != NULL) json_decref(value);

  json_t *response = json_object();
  json_object_set_new(response, "streaming", json_string("started"));
  json_object_set_new(response, "audio_port", json_integer(audio_port));
  json_object_set_new(response, "video_port", json_integer(video_port));

  return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
}


janus_plugin_result *handle_message_stop(plugin_session *session, json_t *root) {
  JANUS_LOG(LOG_INFO, "[%s] Handling stop\n", PLUGIN_PACKAGE);

  if (session->pipeline == NULL) {
    return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "Live streaming hasn't been started", NULL);
  }

  stop_session_pipeline(session);

  json_t *response = json_object();
  json_object_set_new(response, "streaming", json_string("stopped"));

  return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
}


void stop_session_pipeline(plugin_session *session) {
  janus_mutex_lock(&session->mutex);

  if (session->pipeline) {
    gst_element_send_event(session->pipeline, gst_event_new_eos());
    gst_element_set_state(session->pipeline, GST_STATE_NULL);
    gst_object_unref(GST_OBJECT(session->pipeline));
    session->pipeline = NULL;
  }

  if (session->bus) {
    gst_object_unref(GST_OBJECT(session->bus));
    session->bus = NULL;
  }

  janus_mutex_unlock(&session->mutex);
}


// ------------------------------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------------------------------

json_t *create_error_response(int error_code, char *error_cause) {
  char msg[512];
  g_snprintf(msg, 512, "%s", error_cause);

  json_t *event = json_object();
  json_object_set_new(event, "live", json_string("event"));
  json_object_set_new(event, "error_code", json_integer(error_code));
  json_object_set_new(event, "error", json_string(msg));

  return event;
}

plugin_session *session_from_handle(janus_plugin_session *handle) {
  plugin_session *session;

  janus_mutex_lock(&sessions_mutex);
  session = plugin_lookup_session(handle);
  janus_mutex_unlock(&sessions_mutex);

  return session;
}

// ------------------------------------------------------------------------------------------------
// Pipeline bus callback function
// ------------------------------------------------------------------------------------------------

static gboolean bus_callback(GstBus *bus, GstMessage *message, gpointer data) {
  GError *err;
  gchar *debug;

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &err, &debug);
      g_print("Error: %s\n", err->message);
      g_error_free (err);
      g_free (debug);
      break;

    case GST_MESSAGE_EOS:
      g_print("End of stream");
      break;

    case GST_MESSAGE_STATE_CHANGED: {
      GstState old_state, new_state, pending;

      gst_message_parse_state_changed(message, &old_state, &new_state, &pending);
      g_print("Element %s state changed from %s to %s\n", 
        GST_OBJECT_NAME (message->src),
        gst_element_state_get_name (old_state),
        gst_element_state_get_name (new_state));
      break;
    }

    default:
      /* unhandled message */
      g_print("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));
      break;
  }
  return TRUE;
}

// Validates it's RTMP(S) URL.
gboolean is_valid_url(const char *url) {
  return g_regex_match_simple("^rtmps?://.+", url, 0, 0);
}

