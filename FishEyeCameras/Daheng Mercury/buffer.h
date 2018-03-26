#ifndef BUFFER_H_DEF
#define BUFFER_H_DEF

class Buffer {
  char *_out;
  unsigned long _size;
public:
  Buffer(unsigned long size) {
    _out = new char[size];
    this->_size = size;
  }
  ~Buffer() {
    delete [] _out;
  }
  char* buffer() {return _out;}
  unsigned long size() {return _size;}
};

#endif //BUFFER_H_DEF
