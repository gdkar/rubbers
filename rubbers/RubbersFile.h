#ifndef _RUBBERS_RUBBERSFILE_H_
#define _RUBBERS_RUBBERSFILE_H_

#include <cstdio>
#include <algorithm>
#include <iterator>

#include "rubbers/RubbersStretcher.h"
#include "rubbers/libff/libff.h"
class RubbersFile {
  class Impl;
  Impl *m_d;
public:
  RubbersFile( const char *filename, int channels = -1, int rate = -1);
  RubbersFile( RubbersFile && ) = default;
  RubbersFile &operator=( RubbersFile && ) = default;
  virtual ~RubbersFile();
  virtual void    channels ( int nch );
  virtual int     channels () const;
  virtual void    rate ( int rate );
  virtual int     rate () const;
  virtual off_t   seek ( off_t offset, int whence = SEEK_SET );
  virtual off_t   tell ( ) const;
  virtual size_t  read       ( float **buf, size_t req  );
  virtual frame_ptr read_frame();
  virtual frame_ptr read_frame(size_t req);
  virtual size_t  pread      ( float **buf, size_t req, off_t off);
  virtual size_t  length () const;
};

#endif
