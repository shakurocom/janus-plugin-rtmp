# RTMP plugin documentation

RTMP plugin is intended to relay publisher audio / video streams to any RTMP-compatible server, like YouTube, Facebook, Twitch to name a few.

In its present incarnation this plugin uses GStreamer to build a simple pipeline that accepts RTP packets from Janus on two different ports, mixes them into a single stream and sends the stream to a given URL. This URL is specific to the destination service.


## Start

To start relaying A/V stream (broadcasting), a publishing user sends `start` request to the plugin and specifies the URL.

```json
{
  "request": "start",
  "url": "rtmp://a.rtmp.youtube.com/live2/<youtube_key>"
}
```

Response contains two ports (audio and video) that publisher streams should be forwarded by Janus to. Forwarding is supported by the standard [VideoRoom plugin](https://janus.conf.meetecho.com/docs/videoroom.html) or any other plugin ([janus-rtpforward-plugin](https://github.com/michaelfranzl/janus-rtpforward-plugin) is another example).


```json
{
  "streaming": "started",
  "audio_port": 9000,
  "video_port": 9001
}

```

> NOTE: Currently the broadcast can be sent to only one URL. This may change in the future.


## Stop

Stops broadcasting.

```json
{
  "request": "stop"
}
```
