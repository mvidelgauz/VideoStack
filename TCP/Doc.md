# Video Streamer

## Goal

The system is designed for the streaming video from the NG to the web-server that users can watch the NG window inside the browser.

## Current state

The current state of the development allows to stream the window of mc_player to the web-server and you can watch it in the chrome. Other browser are required the check for such support. Now the mc_playes encodes the palyed frame into h264 packet and send it to the web-server via the protocol, designed by me.
While first step development I added to watch the stream in another copy of mc_player. It allowed to make the protocol working.

## What is required to reach the goal

We need to add the support on Nvidia Encoder, check the support of other browser and understood how to take the content of NG window to stream it, like mc_player does.

## Parts of the system

* mc_player - TestApp2 directory contains it
* mc_server - TCP directory contains it
* WebSrv    - The directory contains a open source web-server, that I modified and added the web-socket support
* x264      - The directory contains the x264 encoder and the steeam in my protocol
* TCP/dist  - The directory contains web-service files and javascript library that transform h264 packets to mp4 packets to play it with MSE

## How to use in the current state

### Run mc_server with such arguments "mc_player -i 127.0.0.1:1090 -clnt 127.0.0.1:1091 -wbs 127.0.0.1:9090"
* -i means the address/port where mc_player streams to
* -clnt means the address/port that can be player another copy of mc_player
* -wbs means the address/port from which the browser javascript library requires video

### Run browser http://127.0.0.1/index.html

## HLS stream

In the inital development I added the support to stream the mc_player's windows as HLS stream. That ability exists and be used. 
To use it you can execute "mc_server -i 127.0.0.1:1090 -hlsv ./hls -fps 25 -parts 6 -sdur 5"
* hlsv means the live stream
* sdur means the duration of every segment
* parts means the number of segments that are stored in m3u file during playing



