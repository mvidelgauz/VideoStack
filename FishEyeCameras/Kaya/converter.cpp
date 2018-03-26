#include "converter.h"
#include <iostream>
#include <cmath>

FishEye::FishEye(FileParameters parameters)
  : parameters(parameters), mesh(nullptr), zero({0,0,0})
{
}

FishEye::~FishEye()
{
  if(mesh) delete [] mesh;
}

void FishEye::camera_ray(double x, double y, XYZ *p)
{
  double h = x * inversew;
  double v = (parameters.output_height - 1 - y) * inverseh;
  p->x = p1.x + h * deltah.x + v * deltav.x;
  p->y = p1.y + h * deltah.y + v * deltav.y;
  p->z = p1.z + h * deltah.z + v * deltav.z;
}

bool FishEye::create_mesh()
{
  if(!parameters.prepair())
    return false;
  if(parameters.type == FileParameters::TypeConverter::Perspective)
    return create_mesh_persp();
  else
    return create_mesh_sphere();
}

std::unique_ptr<Buffer> FishEye::convert(char *in)
{
  size_t index_out = 0;
  RGB rgbsum;
  int u, v, index;
  int antial = parameters.antialiasing * parameters.antialiasing;
  RGBELEM *fishimage = reinterpret_cast<RGBELEM *>(in);
  Buffer *buf = new Buffer(parameters.output_width*parameters.output_height*3);
  RGBELEM *perspimage = reinterpret_cast<RGBELEM *>(buf->buffer());
  for (unsigned j=0; j < parameters.output_height; j++) {
    for (unsigned i=0; i < parameters.output_width; i++) {
      rgbsum = zero;
      for (unsigned ai=0; ai < parameters.antialiasing; ai++) {
        for (unsigned aj=0; aj < parameters.antialiasing; aj++) {
          u = mesh[index_out].u;
          v = mesh[index_out].v;
          ++index_out;
          if (u < 0 || u >= parameters.camera_width)
            continue;
          if (v < 0 || v >= parameters.camera_height)
            continue;
          // Add up antialias contribution
          index = v * parameters.camera_width + u;
          rgbsum.r += fishimage[index].r;
          rgbsum.g += fishimage[index].g;
          rgbsum.b += fishimage[index].b;
        }
      }

      // Set the pixel
      index = j * parameters.output_width + i;
      perspimage[index].r = rgbsum.r / antial;
      perspimage[index].g = rgbsum.g / antial;
      perspimage[index].b = rgbsum.b / antial;
    }
  }
  return std::unique_ptr<Buffer>(buf);
}

bool FishEye::create_mesh_persp()
{
  double dh = tan(parameters.output_fov / 2);
  double dv = parameters.output_height * dh / parameters.output_width;
  XYZ vp = {0,0,0},vd = {0,1,0}, vu = {0,0,1}; // Camera view position, direction, and up
  XYZ right = {1,0,0};
  p1 = vector_sum(1.0,vp,1.0,vd,-dh,right, dv,vu);
  p2 = vector_sum(1.0,vp,1.0,vd,-dh,right,-dv,vu);
  p3 = vector_sum(1.0,vp,1.0,vd, dh,right,-dv,vu);
  p4 = vector_sum(1.0,vp,1.0,vd, dh,right, dv,vu);
  deltah.x = p4.x - p1.x;
  deltah.y = p4.y - p1.y;
  deltah.z = p4.z - p1.z;
  deltav.x = p2.x - p1.x;
  deltav.y = p2.y - p1.y;
  deltav.z = p2.z - p1.z;
  if (parameters.debug) {
    std::cout << "Projection plane" << std::endl;
    std::cout << "   angle: " << (parameters.output_fov*RTOD) << ", distance: 1" << std::endl;
    std::cout << "   width: " << (2*dh) << ", height: " << (2*dv) << std::endl;
    std::cout << "   p1: (" << p1.x << "," << p1.y << "," << p1.z << ")" << std::endl;
    std::cout << "   p2: (" << p2.x << "," << p2.y << "," << p2.z << ")" << std::endl;
    std::cout << "   p3: (" << p3.x << "," << p3.y << "," << p3.z << ")" << std::endl;
    std::cout << "   p4: (" << p4.x << "," << p4.y << "," << p4.z << ")" << std::endl;
  }
  inversew = 1.0 / parameters.output_width;
  inverseh = 1.0 / parameters.output_height;

  if(parameters.debug)
    std::cout << parameters.to_string() << std::endl;

  double x,y,r,phi,theta;
  XYZ p;
  if(mesh) delete [] mesh;
  mesh = new UV[
      parameters.output_height*parameters.output_width *
      parameters.antialiasing*parameters.antialiasing
      ];
  size_t index_out = 0;
  for (unsigned j=0; j < parameters.output_height; j++) {
    for (unsigned i=0; i < parameters.output_width; i++) {
      // Antialiasing loops, sub-pixel sampling
      for (unsigned ai=0; ai < parameters.antialiasing; ai++) {
        x = i + ai / (double)parameters.antialiasing;
        for (unsigned aj=0; aj < parameters.antialiasing; aj++) {
          y = j + aj / (double)parameters.antialiasing;
          // Calculate vector to each pixel in the perspective image
          camera_ray(x,y,&p);
          // Convert to fisheye image coordinates
          theta = atan2(p.z,p.x);
          phi = atan2(sqrt(p.x * p.x + p.z * p.z),p.y);
          if (!parameters.has_correction()) {
            r = phi / parameters.camera_fov;
          } else {
            r = phi * (parameters.correction[0] + phi * (parameters.correction[1]
                + phi * (parameters.correction[2] + phi * parameters.correction[3])));
            if (phi > parameters.camera_fov)
              r *= 10;
          }
          // Convert to fisheye texture coordinates
          mesh[index_out].u = parameters.camera_centerx + r * parameters.camera_radiusx * cos(theta);
          mesh[index_out].v = parameters.camera_centery + r * parameters.camera_radiusx /*?*/ * parameters.camera_aspect * sin(theta);
          ++index_out;
        }
      }
    }
  }
  return true;
}

bool FishEye::create_mesh_sphere()
{
  if(parameters.debug)
    std::cout << parameters.to_string() << std::endl;
  double x,y;
  double latitude,longitude;
  int k;
  XYZ p;
  double theta,phi,r;
  int u, v;
  if(mesh) delete [] mesh;
  mesh = new UV[
      parameters.output_height*parameters.output_width *
      parameters.antialiasing*parameters.antialiasing
      ];
  size_t index_out = 0;
  for (unsigned j = 0; j < parameters.output_height; j++) {
    for (unsigned i = 0; i < parameters.output_width; i++) {
      // Antialiasing loops
      for (unsigned ai=0; ai < parameters.antialiasing; ai++) {
        x = 2 * (i + ai/(double)parameters.antialiasing) / (double)parameters.output_width - 1; // -1 to 1
        longitude = x * PI;     // -pi <= x < pi
        for (unsigned aj=0; aj<parameters.antialiasing; aj++) {
          y = 2 * (j + aj/(double)parameters.antialiasing) / (double)parameters.output_height - 1; // -1 to 1
          latitude = y * PI/2;     // -pi/2 <= y < pi/2
          // Find the corresponding pixel in the fisheye image
          // Sum over the supersampling set

          // p is the ray from the camera position into the scene
          p.x = cos(latitude) * sin(longitude);
          p.y = cos(latitude) * cos(longitude);
          p.z = sin(latitude);
          // Calculate fisheye polar coordinates
          theta = atan2(p.z,p.x); // -pi ... pi
          phi = atan2(sqrt(p.x*p.x+p.z*p.z),p.y); // 0 ... fov
          if (!parameters.has_correction()) {
            r = phi / parameters.camera_fov; // 0 .. 1
          } else {
            //r = vars.a1*phi + vars.a2*phi*phi + vars.a3*phi*phi*phi + vars.a4*phi*phi*phi*phi;
            r = phi * (parameters.correction[0] + phi * (parameters.correction[1]
                + phi * (parameters.correction[2] + phi * parameters.correction[3])));
            if (phi > parameters.camera_fov)
              r *= 10;
          }
          // Determine the u,v coordinate
          mesh[index_out].u = parameters.camera_centerx + r * parameters.camera_radiusx * cos(theta);
          mesh[index_out].v = parameters.camera_centery + r * parameters.camera_radiusx * sin(theta);
          ++index_out;
        }
      }
    }
  }
  return true;
}

XYZ FishEye::vector_sum(double d1, XYZ p1, double d2, XYZ p2, double d3, XYZ p3, double d4, XYZ p4)
{
  XYZ sum;
  sum.x = d1 * p1.x + d2 * p2.x + d3 * p3.x + d4 * p4.x;
  sum.y = d1 * p1.y + d2 * p2.y + d3 * p3.y + d4 * p4.y;
  sum.z = d1 * p1.z + d2 * p2.z + d3 * p3.z + d4 * p4.z;

  return(sum);
}

void FileParameters::set_image_size(unsigned w, unsigned h) {
  camera_width = w; camera_height = h;
  output_width = static_cast<unsigned>((static_cast<double>(output_height)/camera_height)*camera_width);
}

bool FileParameters::prepair()
{
  if(output_width < 8 || output_height < 8) {
    if(debug)
      std::cerr << "Bad output perspective image size (must be > 8)" << std::endl;
    return false;
  }
  // Even
  output_width /= 2; output_width *= 2;
  output_height /= 2; output_height *= 2;
  if(camera_fov > 360) {
    if(debug)
      std::cerr << "Maximum fisheye FOV is 360 degrees" << std::endl;
    camera_fov = 360;
  }
  if(output_fov > 340) {
    if(debug)
      std::cerr << "Maximum/dangerous aperture, reset to 340" << std::endl;
    output_fov = 340;
  }
  if(antialiasing < 1)
    antialiasing = 1;
  if(camera_centerx == 0)
    camera_centerx = camera_width / 2;
  if(camera_centery == 0)
    camera_centery = camera_height / 2;
  if(camera_radiusx == 0)
    camera_radiusx = camera_width / 2;
  if(camera_radiusy == 0)
    camera_radiusy = camera_radiusx;
  if(!prepaired) {
    prepaired = true;
    camera_fov /= 2;
    camera_fov *= DTOR;
    camera_aspect = static_cast<double>(camera_radiusy)/camera_radiusx;
    output_fov /= 2;
    output_fov *= DTOR;
  }
  return true;
}

std::string FileParameters::to_string() {
  std::string ret("Parameters:\n");
  ret += (debug ? "  debug = true\n" : "  debug = false\n");
  ret += "  output size = " + std::to_string(output_width) + "x" + std::to_string(output_height) + "\n";
  ret += "  camera fov = " + std::to_string(camera_fov) + "\n";
  ret += "  antialiasing = " + std::to_string(antialiasing) + "\n";
  ret += "  correction = " + std::to_string(correction[0]) + " " + std::to_string(correction[1]) + " "
      + std::to_string(correction[2]) + " " + std::to_string(correction[3]) + "\n";
  ret += "  image size = " + std::to_string(camera_width) + "x" + std::to_string(camera_height) + "\n";
  ret += "  camera centerx = " + std::to_string(camera_centerx) + "\n";
  ret += "  camera centery = " + std::to_string(camera_centery) + "\n";
  ret += "  camera radiusx = " + std::to_string(camera_radiusx) + "\n";
  ret += "  camera radiusy = " + std::to_string(camera_radiusy) + "\n";
  ret += "  camera aspect = " + std::to_string(camera_aspect) + "\n";
  ret += "  camera fps = " + std::to_string(fps) + "\n";
  ret += "  socket = " + socket + "\n";
  ret += "  file = " + file + "\n";
  ret += (debayer ? "  debayer = true\n" : "  debayer = false\n");
  ret += "  application maxqueue = " + std::to_string(maxqueue) + "\n";
  return ret;
}

bool FileParameters::has_correction()
{
  return correction[0] != 0.0f || correction[1] != 0.0f ||correction[2] != 0.0f ||correction[3] != 0.0f;
}
