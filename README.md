# Video Stack

## This tools is distributed with BSD License and designed to work with video

## Parts of the system

* mc_player - TestApp2 directory contains it, this is the video-player
* mc_server - TCP directory contains it, the server receive the stream in mc_protocol and send it to another app or to web-socket
* mc_encoder - TestEnc directory contains the app that can reencode video or send the desktop top the server
* WebSrv    - The directory contains a open source web-server, that I modified and added the web-socket support
* x264      - The directory contains the x264 encoder and the steeam in my protocol
* TCP/dist  - The directory contains web-service files and javascript library that transform h264 packets to mp4 packets to play it with MSE
* vSystem   - The external repo vSystem contains a basic video-library for all operations

* FishEyeCamears - The directory contains the support for streaming of genicam cameras. Currently, supports Kaya and Dhaheng Mercury cameras.

## How to use in it for web-socket

### Run mc_server with such arguments "mc_player -i 127.0.0.1:1090 -clnt 127.0.0.1:1091 -wbs 127.0.0.1:9090"
* -i means the address/port where mc_player streams to
* -clnt means the address/port that can be player another copy of mc_player
* -wbs means the address/port from which the browser javascript library requires video

### Run browser http://127.0.0.1/index.html

## How to use it for HLS streaming

We can send a stream in mc_protocol to the mc_server that can convert it to the adaptive HLS stream. Currently, mc_player can send playing video to the server,
or mc_encoder can send the currendt Linux desktop or you can send the video stream from Kaya or Dhaheng cameras.  
To use it you can execute "mc_server -i 127.0.0.1:1090 -hlsv ./hls -fps 25 -parts 6 -sdur 5"
* hlsv means the live stream
* sdur means the duration of every segment
* parts means the number of segments that are stored in m3u file during playing


