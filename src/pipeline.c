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

#include <gst/gst.h>

#include "janus/debug.h"

GstElement *create_pipeline(const char *url, int audio_port, int video_port) {
  GstElement *pipeline;

  gchar *pipeline_def = g_strdup_printf(
    "rtpbin name=rtpbin "
    "udpsrc address=localhost port=%d caps=\"application/x-rtp, media=audio, encoding-name=OPUS, clock-rate=48000\" ! rtpbin.recv_rtp_sink_1 "
    "udpsrc address=localhost port=%d caps=\"application/x-rtp, media=video, encoding-name=H264, clock-rate=90000\" ! rtpbin.recv_rtp_sink_0 "
    "rtpbin. ! rtph264depay ! flvmux streamable=true name=mux ! rtmpsink location=\"%s\" "
    "rtpbin. ! rtpopusdepay ! queue ! opusdec ! voaacenc bitrate=128000 ! mux.",
    audio_port, video_port, url);
  JANUS_LOG(LOG_INFO, "Pipeline definition: %s\n", pipeline_def);

  pipeline = gst_parse_launch(pipeline_def, NULL);

  g_free(pipeline_def);

  return pipeline;
}


GstElement* start_pipeline(const char* url, int audio_port, int video_port) {
  GstElement *pipeline = create_pipeline(url, audio_port, video_port);

  if (!pipeline) {
    JANUS_LOG(LOG_ERR, "Could not create the pipeline");
    return NULL;
  }

  JANUS_LOG(LOG_INFO, "Pipeline started (ports audio: %d video: %d)\n", audio_port, video_port);

  // Start playing
  gst_element_set_state(pipeline, GST_STATE_PLAYING);

  return pipeline;
}

