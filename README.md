# janus-plugin-rtmp

[Janus](https://janus.conf.meetecho.com/) [plugin](https://janus.conf.meetecho.com/docs/plugin_8h.html) to stream publisher audio and video merged into a single RTMP stream to YouTube, Facebook, Instagram, Twitch and any other live streaming service accepting RTMP protocol.

[See here for API documentation on how to communicate with the plugin.](docs/api.md)

PRs and GitHub issues are welcome.

## How do I use the plugin?

This is a plugin for Janus, so you'll need to install and run Janus first. The [installation instructions on GitHub](https://github.com/meetecho/janus-gateway#dependencies) are canonical. It's built for Janus version 0.10.7 and should support later versions.

This plugin should be compatible with any OS that can run Janus; that includes Linux, OS X, and Windows via WSL.

## Dependencies

These are the native dependencies necessary for building. The plugin is
based on the excellent and resource-efficient
[GStreamer](https://gstreamer.freedesktop.org/) and thus we need to
install some of dependencies.

```
$ sudo apt-get install \
    libgstreamer1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad
```

To build the plugin we also need headers from Janus. Assuming that the
sources of Janus are at `/janus-gateway` and it is installed into
`/usr/local`.

```shell
$ sudo cp /janus-gateway/{.,plugins}/*.h /usr/local/janus
```

## Building and installing

```
$ ./autogen.sh
$ ./configure --prefix=/usr/local
$ make
$ make install
```

## Configuration and usage

The plugin accepts a configuration file in the Janus configuration directory named `janus.plugin.rtmp.cfg` containing key/value pairs in INI format. An example configuration file is provided as `conf/janus.plugin.rtmp.cfg.sample`.

## Future plans

- Replace port-based solution with passing direct RTP packets to GStreamer
- Support for multiple URLs

## Give it a try and reach us

Take a look at our <a href="https://shakuro.com/services/web-dev/?utm_source=github&utm_medium=repository&utm_campaign=janus-plugin-rtmp">web development services</a>

If you need professional assistance with your mobile or web project, feel free to <a href="https://shakuro.com/get-in-touch/?utm_source=github&utm_medium=repository&utm_campaign=janus-plugin-rtmp">contact our team</a
