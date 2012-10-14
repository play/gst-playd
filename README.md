## What Is This?

`gst-playd` is a really simple cross-platform service whose sole job is to
play back media files to a list of targets (IceCast and AirPlay), using the
GStreamer API. 

It's a single exe, it's simple to manage and configure, and you can run as
many as you want in parallel for multiple audio streams.

## What *Doesn't* It Do

gst-playd knows nothing about library management. Its goal is to replace
Airfoil and Nicecast in the current Play implementation (Play v2)

## But can you explain it in ASCII art?

Sure.

```
 +------------+
 |            |
 |  source1   |+                                                         +----------------+
 |            ||                                                         |                |
 +------------+|         +------------------+                         +-+|    icecast     |
               +---------+                  |    +---------------+    |  |                |
 +------------+          |                  |    |               +----+  +----------------+
 |            |          |                  |    |               |
 | source2    +----------+     mixer        +---->    splitter   |       +----------------+
 |            |          |                  |    |               |+      |                |
 +------------+     +----+                  |    |               |+------+    airport1    |
                    |    +------------------+    +-------------+-+       |                |
     ...            |                                          |         +----------------+
                    |                                          |
 +------------+-----+                                          |         +----------------+
 |            |                                                |         |                |
 | source_n   +                                                +---------+    airport2    |
 |            |                                                          |                |
 +------------+                                                          +----------------+
```

## How does it work?

gst-playd will start up a REQ/REP ZeroMQ socket on a specified port - meaning,
it will boot up and wait for a request to come in. The requests are really
simple string-based, Redis-style commands, something like:

```
PLAY file:///home/foo/bar.mp3
STOP
```

And the responses will be equivalently structured:

```
OK
ERROR IceCast isn't installed. Install it via 'brew install icecast'
```

Internally, `gst-playd` will manage the GStreamer Pipeline as well as handle
starting up and configuring the Icecast server (including tying the lifetime
of the icecast process to gst-playd, so if gst-playd dies, it kills the
associated icecasts on its way out)

## How do I build this?

On OS X:

```sh
brew install zeromq --head
brew install gst-plugins-good gst-plugins-ugly gst-ffmpeg  ## Get some coffee.
brew link libxml2

./autogen.sh && make
```
