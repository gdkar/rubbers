_Pragma("once")

extern "C" {
  #include <libavutil/avutil.h>
  #include <libavformat/avformat.h>
  #include <libavcodec/avcodec.h>
  #include <libavdevice/avdevice.h>
  #include <libswresample/swresample.h>
};
#include <vector>
#include <memory>
#include <map>

struct avpacket_ptr {
    AVPacket     *m_d{nullptr};
    avpacket_ptr() = default;
    avpacket_ptr(AVPacket *f)
        : m_d(f) { }
    avpacket_ptr(avpacket_ptr &&o)
        : m_d(o.release()) { }
    avpacket_ptr(const avpacket_ptr &o)
        : m_d(av_packet_clone(o)) { }
    avpacket_ptr &operator = (AVPacket *f)
    {
        reset(f);
        return *this;
    }
    avpacket_ptr &operator =(avpacket_ptr &&o)
    {
        if(m_d) {
            ref(o);
        }else{
            reset(o.release());
        }
        return *this;
    }
    avpacket_ptr &operator = (const avpacket_ptr &o)
    {
        reset(av_packet_clone(o.m_d));
        return *this;
    }
    void swap(avpacket_ptr &o) { std::swap(m_d,o.m_d);}
    void reset(AVPacket *f = nullptr) { av_packet_free(&m_d); m_d = f;}
    AVPacket *release() { auto ret = m_d; m_d = nullptr; return ret;}
    AVPacket *get() const { return m_d;}
   ~avpacket_ptr() { reset();}
    AVPacket *operator ->() { return m_d; }
    AVPacket *operator ->() const { return m_d;}
    AVPacket &operator *() { return *m_d;}
    const AVPacket &operator *() const { return *m_d;}
    operator bool() const { return !!m_d;}
    operator AVPacket *() const { return m_d;}
    void alloc() { if(!m_d) m_d = av_packet_alloc(); else unref();}
    void unref() { av_packet_unref(m_d); }
    void ref(avpacket_ptr &o) { if(o.m_d != m_d) { unref(); av_packet_ref(m_d,o.m_d); } }
};

inline void swap(avpacket_ptr &lhs, avpacket_ptr &rhs){ lhs.swap(rhs);}
