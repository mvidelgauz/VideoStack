CXX   = g++ 

LDFLAGS += -Wl,-Bsymbolic -pthread  -ltheora -lwavpack  -lm -lstdc++ -lrt -lz -lvpx -lx264 -lx265 -lspeex -lxvidcore -lmp3lame -lasound -lfdk-aac -lm  -lwebp -lgme -lasound -lmp3lame -lpng -ldl -lX11 -lXmu -lglut -fPIC -llzma -lmp3lame -lfdk-aac -L/usr/lib64 -ldl -lX11 -lXmu -lglut -fPIC -llzma  -lasound -lvorbis  -L/usr/lib64 -lopus -ltwolame -lsoxr -lvdpau -lGL
CFLAGS  =  -g -MD -Wall -I. -D _NO_NV -D _LIN -masm=intel 
INC = -I/Projects -I/ProjectsP/ffmpeg-3.2.5
INCL = /ProjectsP/ffmpeg-3.2.5 

OBJS = server.o tcpstream.o tcpacceptor.o ../WebSrv/rio.o ../WebSrv/web_srv.o ../WebSrv/network.o ../WebSrv/static.o ../sha1/sha1.o  /Projects/vSystem/VS_Reader.o /Projects/vSystem/VS_Controller.o /Projects/vSystem/VS_Sound.o /Projects/vSystem/VS_Writer.o ../X264/vx264.o
TARGET = mc_server


all: $(TARGET)

.cpp.o:
	$(CXX) $(CFLAGS) $(INC) -c $< -o $@ 2> sk
## 2> sk

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(INC) $(LDFLAGS) -lavcodec -lavformat -lavutil -lswscale -lavcodec -lavutil -lswresample -lavresample 2> sk
