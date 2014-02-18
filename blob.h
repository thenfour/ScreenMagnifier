/*
    August 11, 2004 carlc
    very FAST wrapper for quick malloc/free.  Used for quick memory buffers.
*/


#pragma once

#include <windows.h>

namespace LibCC
{
  template<bool StaticBufferSupport = true, long StaticBufferSize = 100>
  class BlobTraits
  {
  public:
    static const bool _StaticBufferSupport = StaticBufferSupport;
    static const long _StaticBufferSize = StaticBufferSize;

    // return the new size, in elements
    static long GetNewSize(long current_size, long requested_size)
    {
      if(!current_size)
      {
        return requested_size;
      }

      // same as (current_size * 1.5)
      while(current_size < requested_size)
      {
        current_size += (current_size >> 1);
      }
      return current_size;
    }
  };

  // manages a simple memory blob.
  // an unallocated state will have m_p = 0 and m_size = 0
  //
  // if the class is "not lockable" that means in exchange for removing all the "lockable" checks (for performance), this class
  // will allow direct access to the buffer via GetLockedBuffer()).
  template<typename Tel, bool Lockable = true, typename Ttraits = BlobTraits<true, 100> >
  class Blob
  {
  public:

    typedef Tel _El;
    typedef Ttraits _Traits;
    static const bool _Lockable = Lockable;
    static const bool _StaticBufferSupport = _Traits::_StaticBufferSupport;
    static const long _StaticBufferSize = _Traits::_StaticBufferSize;

    Blob() :
      m_p(_StaticBufferSupport ? m_StaticBuffer : 0),
      m_size(_StaticBufferSupport ? _StaticBufferSize : 0),
      m_locked(false),
      m_hHeap(GetProcessHeap())
    {
    }

    ~Blob()
    {
      Free();
    }

    long size() const
    {
      return m_size;
    }

    // these are just to break up some if() statements.
    inline bool CurrentlyUsingStaticBuffer() const
    {
      // is static buffer support enabled, and are we using the static buffer right now?
      return _StaticBufferSupport && (m_p == m_StaticBuffer);
    }
    inline bool CompletelyUnallocated() const
    {
      // is static buffer support disabled, and do we have NOTHING allocated now?
      return (!_StaticBufferSupport) && (m_p == 0);
    }
    inline bool CurrentlyLocked() const
    {
      return _Lockable && m_locked;
    }
    inline bool LockableAndUnlocked() const
    {
      return _Lockable && (!m_locked);
    }

    // frees memory if its not locked.
    bool Free()
    {
      bool r = false;
      // why do i check both TLockable and m_locked?  Because if the compiler is smart enough,
      // it will totally eliminate the comparison if TLockable is always false (in the template param).
      // msvc 7.1 has been verified to successfully remove the compares.
      if(!CurrentlyLocked())
      {
        if(_StaticBufferSupport)
        {
          if(m_StaticBuffer != m_p)
          {
            HeapFree(m_hHeap, 0, m_p);
            m_p = m_StaticBuffer;
            m_size = _StaticBufferSize;
          }
        }
        else
        {
          if(m_p)
          {
            HeapFree(m_hHeap, 0, m_p);
            m_p = 0;
            m_size = 0;
          }
        }
        r = true;
      }
      return r;
    }

    bool Alloc(long n)
    {
      bool r = false;
      if(Free())
      {
        if(CurrentlyUsingStaticBuffer())
        {
          if(m_size < n)
          {
            // we need to allocate on the heap.
            Tel* pNew;
            long nNewSize = Ttraits::GetNewSize(0, n);
            pNew = static_cast<Tel*>(HeapAlloc(m_hHeap, 0, sizeof(Tel) * nNewSize));
            if(pNew)
            {
              m_p = pNew;
              m_size = nNewSize;
              r = true;
            }
          }
        }
        else
        {
          // we need to allocate on the heap.
          Tel* pNew;
          long nNewSize = Ttraits::GetNewSize(0, n);
          pNew = static_cast<Tel*>(HeapAlloc(m_hHeap, 0, sizeof(Tel) * nNewSize));
          if(pNew)
          {
            m_p = pNew;
            m_size = nNewSize;
            r = true;
          }
        }
      }
      return r;
    }

    bool Realloc(long n)
    {
      bool r = false;
      if(!CurrentlyLocked())
      {
        if(m_size >= n)
        {
          // no need to allocate;
          r = true;
        }
        else
        {
          // we definitely need to allocate now.
          long nNewSize = Ttraits::GetNewSize(m_size, n);
          Tel* pNew;

          if(CurrentlyUsingStaticBuffer() || CompletelyUnallocated())
          {
            // allocate for the first time.
            pNew = static_cast<Tel*>(HeapAlloc(m_hHeap, 0, sizeof(Tel) * nNewSize));
            if(pNew)
            {
              if(CurrentlyUsingStaticBuffer())
              {
                // copy the contents of the static buffer into the new heap memory.
                CopyMemory(pNew, m_p, _StaticBufferSize);
              }

              m_p = pNew;
              m_size = nNewSize;

              r = true;
            }
          }
          else
          {
            // realloc, because we already have a heap buffer.
            pNew = static_cast<Tel*>(HeapReAlloc(m_hHeap, 0, m_p, sizeof(Tel) * nNewSize));
            if(pNew)
            {
              m_p = pNew;
              m_size = nNewSize;
              r = true;
            }
          }
        }
      }
      return r;
    }

    // locks the buffer so it can be written to.  this will allow GetBuffer(void) to be used.
    bool Lock()
    {
      bool r = false;
      // if its not lockable or the size is 0, you cant lock!
      if(_Lockable || m_size)
      {
          m_locked = true;
          r = true;
      }
      return r;
    }
    bool Unlock()
    {
      if(_Lockable) m_locked = false;// once again this is necessary for optimization.
      return true;
    }

    // returns a const buffer
    const Tel* GetBuffer() const
    {
      return m_p;
    }

    // returns 0 if its not allowed (buffer must be locked!)
    Tel* GetWritableBuffer() const
    {
      Tel* r = 0;
      if((!_Lockable) || (CurrentlyLocked()))// look at Lock() to see why i have this.
      {
        r = m_p;
      }
      return r;
    }

    // combination of Alloc(), Lock() and GetLockedBuffer(void)
    // call Unlock() after this!
    Tel* GetBuffer(long n)
    {
      Tel* r = 0;
      if(LockableAndUnlocked())
      {
        if(Alloc(n))
        {
          if(Lock())
          {
            r = m_p;
          }
        }
      }
      return r;
    }

  private:
    long m_size;
    Tel* m_p;
    bool m_locked;
    Tel m_StaticBuffer[_StaticBufferSize];
    HANDLE m_hHeap;
  };
  typedef Blob<wchar_t, false, BlobTraits<true, 300> > PathBlobW;
  typedef Blob<char, false, BlobTraits<true, 300> > PathBlobA;
  typedef Blob<TCHAR, false, BlobTraits<true, 300> > PathBlob;


}


