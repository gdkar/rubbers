#include "rubbers/RubbersFile.h"
#include "RubbersFileImpl.h"
RubbersFile::RubbersFile ( const char *filename, int nch, int srate)
  : m_d ( new RubbersFile::Impl(filename, nch, srate) )
{
}
RubbersFile::~RubbersFile ( )
{
  delete m_d;
}
void 
RubbersFile::channels ( int nch )
{
  m_d->channels ( nch );
}
int
RubbersFile::channels ( ) const
{
  return m_d->channels ();
}
void
RubbersFile::rate ( int srate )
{
  m_d->rate ( srate );
}
int
RubbersFile::rate ( ) const
{
  return m_d->rate ( );
}
off_t RubbersFile::seek ( off_t offset, int whence )
{
  return m_d->seek ( offset, whence );
}
off_t RubbersFile::tell ( ) const
{
  return m_d->tell ( );
}
size_t RubbersFile::pread(float **buf, size_t req, off_t off)
{
    return m_d->pread( buf, req, off);
}
size_t RubbersFile::read ( float **buf, size_t req )
{
  return m_d->read ( buf, req );
}
size_t RubbersFile::length () const
{
  return m_d->length ();
}
