#ifndef CONVERTER_H_DEF
#define CONVERTER_H_DEF

#include "buffer.h"
#include <string>
#include <memory>

#define DTOR            0.0174532925
#define RTOD            57.2957795
#define TWOPI           6.283185307179586476925287
#define PI              3.141592653589793238462643
#define PID2            1.570796326794896619231322

struct FileParameters {
  enum class TypeConverter {
    Perspective = 0, Spherical = 1
  };
  TypeConverter type = TypeConverter::Perspective;
  unsigned output_width = 1280;
  unsigned output_height = 1080;
  double output_fov = 200;
  unsigned antialiasing = 1;
  float correction[4] = {0, 0, 0, 0};
  double camera_fov = 180;
  unsigned camera_width = 0;
  unsigned camera_height = 0;
  unsigned camera_centerx = 0;
  unsigned camera_centery = 0;
  unsigned camera_radiusx = 0;
  unsigned camera_radiusy = 0;
  unsigned maxqueue = 10;
  double camera_aspect = 0;
  bool debug = false;
  bool debayer = true;
  bool prepaired = false;
  std::string socket;
  std::string file;
  int fps = 30;
  void set_image_size(unsigned w, unsigned h);
  bool prepair();
  std::string to_string();
  bool has_correction();
};

typedef struct {
   double x,y,z;
} XYZ;

typedef struct {
   int r,g,b;
} RGB;

typedef struct {
   unsigned char r,g,b;
} RGBELEM;

typedef struct {
   int u,v;
} UV;

typedef struct {
   int axis;
   double value;
   double cvalue,svalue;
} TRANSFORM;

class FishEye {
  FileParameters parameters;
  UV *mesh;
  RGB zero;
  XYZ p1,p2,p3,p4; // Corners of the view frustum
  XYZ deltah,deltav;
  double inversew,inverseh;
  bool create_mesh_persp();
  bool create_mesh_sphere();
  XYZ vector_sum(double d1, XYZ p1, double d2, XYZ p2, double d3, XYZ p3, double d4, XYZ p4);
public:
  void camera_ray(double x, double y, XYZ *);
  FishEye(FileParameters parameters);
  ~FishEye();
  bool create_mesh();
  std::unique_ptr<Buffer> convert(char *in);
};

#endif //CONVERTER_H_DEF
