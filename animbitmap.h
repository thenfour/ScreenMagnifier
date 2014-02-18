/*
  Jun 30, 2004 carlc
  This is a class for "animated bitmaps".  Basically the idea is that you get easy low level
  access do a DIB and its meant to be drawn in frames.

  This is only meant for SCREEN purposes, and meant to be FAST!
*/


#pragma once


#include <windows.h>

extern void InitLUTs(void);
extern void UninitLUTs(void);
extern void hq2x_32( unsigned char * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
extern void hq3x_32( unsigned char * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );
extern void hq4x_32( unsigned char * pIn, unsigned char * pOut, int Xres, int Yres, int BpL );


template<long bpp>
struct BitmapTypes
{
  typedef unsigned __int8 RgbPixel;
};

template<>
struct BitmapTypes<16>
{
  typedef unsigned __int16 RgbPixel;
};

template<>
struct BitmapTypes<32>
{
  typedef unsigned __int32 RgbPixel;
};


template<long Tbpp>
class AnimBitmap
{
public:
  static const long bpp = Tbpp;
  typedef typename BitmapTypes<bpp>::RgbPixel RgbPixel;

	inline static BYTE R(RgbPixel d)
	{
		return static_cast<BYTE>((d & 0xF800) >> 11);
	}

	inline static BYTE G(RgbPixel d)
	{
		return static_cast<BYTE>((d & 0x07E0) >> 5);
	}

	inline static BYTE B(RgbPixel d)
	{
		return static_cast<BYTE>((d & 0x001F));
	}

	inline static RgbPixel MakeRgbPixel32(BYTE r, BYTE g, BYTE b)
	{
		return static_cast<RgbPixel>((r << 11) | (g << 5) | (b));
	}


  AnimBitmap() :
    m_y(0),
    m_x(0),
    m_bmp(0),
    m_pbuf(0)
  {
    // store our offscreen hdc
    HDC hscreen = ::GetDC(0);
    m_offscreen = CreateCompatibleDC(hscreen);
    ReleaseDC(0, hscreen);
  }

  ~AnimBitmap()
  {
    DeleteDC(m_offscreen);
    if(m_bmp)
    {
      DeleteObject(m_bmp);
    }
  }

  //CSize GetSize() const
  //{
  //  return CSize(m_x, m_y);
  //}

  long GetWidth() const
  {
    return m_x;
  }

  long GetHeight() const
  {
    return m_y;
  }

  // MUST be called at least once.  This will allocate the bmp object.
  bool SetSize(long x, long y)
  {
    bool r = false;

    if(x & 1) x ++;
    if(y & 1) y ++;

    if((x != m_x) || (y != m_y))
    {
      // delete our current bmp.
      if(m_bmp)
      {
        DeleteObject(m_bmp);
        m_bmp = 0;
      }

      long pbiSize = sizeof(BITMAPINFO) + sizeof(RGBQUAD) * 10;
      BITMAPINFO* pbi = reinterpret_cast<BITMAPINFO*>(HeapAlloc(GetProcessHeap(), 0, pbiSize));
      memset(pbi, 0, pbiSize);
      BITMAPINFO& bi = *pbi;

      bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
      bi.bmiHeader.biPlanes = 1;// must be 1

      if(bpp == 32)
      {
        bi.bmiHeader.biBitCount = 32;// for RGBQUAD
        bi.bmiHeader.biCompression = BI_RGB;
      }
      else if(bpp == 16)
      {
        bi.bmiHeader.biBitCount = 16;
        bi.bmiHeader.biCompression = BI_BITFIELDS;
        *(unsigned int*)(&bi.bmiColors[0]) = 0xF800;
        *(unsigned int*)(&bi.bmiColors[1]) = 0x07E0;
        *(unsigned int*)(&bi.bmiColors[2]) = 0x001F;
      }

      bi.bmiHeader.biSizeImage = 0;// dont need to specify because its uncompressed
      bi.bmiHeader.biXPelsPerMeter = 0;
      bi.bmiHeader.biYPelsPerMeter = 0;
      bi.bmiHeader.biClrUsed  = 0;
      bi.bmiHeader.biClrImportant = 0;
      bi.bmiHeader.biWidth = max(x,1);
      bi.bmiHeader.biHeight = -(max(y,1));

      m_bmp = CreateDIBSection(m_offscreen, &bi, DIB_RGB_COLORS, (void**)&m_pbuf, 0, 0);
      bi.bmiHeader.biHeight = (max(y,1));
      if(m_bmp)
      {
        r = true;
        m_x = bi.bmiHeader.biWidth;
        m_y = bi.bmiHeader.biHeight;
        SelectObject(m_offscreen, m_bmp);
      }

      HeapFree(GetProcessHeap(), 0, pbi);
    }
    return r;
  }

  // commits the changes
  bool Commit()
  {
    return true;
  }

  bool BeginDraw()
  {
    GdiFlush();
    return true;
  }

  HDC GetDC()
  {
    return m_offscreen;
  }

  // no boundschecking for speed.
  void SetPixel(long x, long y, RgbPixel c)
  {
    ATLASSERT(x >= 0);
    ATLASSERT(y >= 0);
    ATLASSERT(y < m_y);
    ATLASSERT(x < m_x);
    //y = m_y - y;
    m_pbuf[x + (y * m_x)] = c;
  }

  // xright is NOT drawn.
  void SafeHLine(long x1, long x2, long y, RgbPixel c)
  {
    long xleft = min(x1, x2);
    long xright = max(x1, x2);
    if(xleft > 0)
    {
      if(xright <= m_x)
      {
        if(y > 0)
        {
          if(y < m_y)
          {
            HLine(xleft, xright, y, c);
          }
        }
      }
    }
  }

  // xright is NOT drawn.
  void HLine(long x1, long x2, long y, RgbPixel c)
  {
    long xleft = min(x1, x2);
    long xright = max(x1, x2);
    RgbPixel* pbuf = &m_pbuf[(y * m_x) + xleft];
    while(xleft != xright)
    {
      *pbuf = c;
      pbuf ++;
      xleft ++;
    }
  }

  void VLine(long x, long y1, long y2, RgbPixel c)
  {
    long ytop = min(y1, y2);
    long ybottom = max(y1, y2);
    RgbPixel* pbuf = &m_pbuf[(ytop * m_x) + x];
    while(ytop != ybottom)
    {
      *pbuf = c;
      pbuf += m_x;
      ytop ++;
    }
  }

  // b and r are not drawn
  void Rect(long l, long t, long r, long b, RgbPixel c)
  {
    RgbPixel* pbuf = &m_pbuf[(t * m_x) + l];
    long h = r - l;// horizontal size
    // fill downwards
    while(t != b)
    {
      // draw a horizontal line
      for(long i = 0; i < h; i ++)
      {
        pbuf[i] = c;
      }
      pbuf += m_x;
      t ++;
    }
  }

  // b and r are not drawn
  void SafeRect(long l, long t, long r, long b, RgbPixel c)
  {
    clamp(l, 0, m_x);
    clamp(r, 0, m_x);
    clamp(t, 0, m_y);
    clamp(b, 0, m_y);
    Rect(l, t, r, b, c);
  }

  bool SetPixelSafe(long x, long y, RgbPixel c)
  {
    bool r = false;
    if(x > 0 && y > 0 && x < m_x && y < m_y)
    {
      m_pbuf[x + (y * m_x)] = c;
      r = true;
    }
    return r;
  }

  RgbPixel GetPixel(long x, long y)
  {
    return m_pbuf[x + (y * m_x)];
  }

  bool GetPixelSafe(RgbPixel& out, long x, long y)
  {
    bool r = false;
    if(x > 0 && y > 0 && x < m_x && y < m_y)
    {
      out = m_pbuf[x + (y * m_x)];
      r = true;
    }
    return r;
  }

  void Fill(RgbPixel c)
  {
    RgbPixel* pDest = m_pbuf;
    long n = m_x * m_y;
    for(long i = 0; i < n; i ++)
    {
      pDest[i] = c;
    }
  }

	inline static float GetIntensity(RgbPixel p)
	{
		return ((float)Rf(p)+Gf(p)+Bf(p)) / 3;
	}
	inline static RgbPixel ScaleToIntensity(RgbPixel p, float i)
	{
		float r = (float)R(p) / 0x1f;
		float g = (float)G(p) / 0x3f;
		float b = (float)B(p) / 0x1f;

		float iold = GetIntensity(p);
		r = r * i / iold;
		g = g * i / iold;
		b = b * i / iold;

			r *= 0x1f;
			g *= 0x3f;
			b *= 0x1f;
		clamp(r, 0, 0x1f);
		clamp(g, 0, 0x3f);
		clamp(b, 0, 0x1f);
		BYTE br = (BYTE)r,
			bg = (BYTE)g,
			bb = (BYTE)b;

		return MakeRgbPixel32(br, bg, bb);
	}

	struct Pixel
	{
		RgbPixel* p;
		Pixel(RgbPixel* p_) : p(p_)
		{
		}
		Pixel& operator = (const Pixel& rhs)
		{
			*p = *(rhs.p);
			return *this;
		}
		Pixel& operator = (const RgbPixel& rhs)
		{
			*p = rhs;
			return *this;
		}
		operator RgbPixel() const { return *p; }
		BYTE R() const { return AnimBitmap::R(*p); }
		BYTE G() const { return AnimBitmap::G(*p); }
		BYTE B() const { return AnimBitmap::B(*p); }
		float Rf() const { return (float)AnimBitmap::R(*p) / 0x1f; }
		float Gf() const { return (float)AnimBitmap::G(*p) / 0x3f; }
		float Bf() const { return (float)AnimBitmap::B(*p) / 0x1f; }
	};

	static float Rf(const RgbPixel& p) { return (float)R(p) / 0x1f; }
	static float Gf(const RgbPixel& p) { return (float)G(p) / 0x3f; }
	static float Bf(const RgbPixel& p) { return (float)B(p) / 0x1f; }

	static float absf(float a) { return a < 0.0f ? -a : a; }

	bool IsCloseTo(RgbPixel& lhs, RgbPixel& rhs, float thresh)
	{
		if(absf(Rf(lhs) - Rf(rhs)) > thresh)
			return false;
		if(absf(Gf(lhs) - Gf(rhs)) > thresh)
			return false;
		if(absf(Bf(lhs) - Bf(rhs)) > thresh)
			return false;
		return true;
	}

	template<typename T>
	inline static T minxx(T a, T b, RgbPixel& ifBthenSetThisVar, Pixel& to)
	{
		if(a < b)
		{
			return a;
		}
		ifBthenSetThisVar = to;
		return b;
	}

	template<typename T>
	inline static T maxxx(T a, T b, RgbPixel& ifBthenSetThisVar, Pixel& to)
	{
		if(a > b)
		{
			return a;
		}
		ifBthenSetThisVar = to;
		return b;
	}

	inline static void AssignClosest(Pixel& out, float i, const float& bias, RgbPixel& pa, float& ca, RgbPixel& pb, float& cb)
	{
		i *= bias;
		if(absf(ca - i) > absf(cb - i))
		{
			// closest to b
			out = pb;
		}
		else
		{
			out = pa;
		}
	}

	RgbPixel DoSomething(RgbPixel& x)
	{
		return MakeRgbPixel32(R(x), G(x), B(x)/2);
	}

	void RemoveClearType()
	{
		// another possible technique:
		// hq2x works great for low frequencies, and the nyquist. anything past a certain threshold needs to get sharpened.
		// http://www.gamedev.net/reference/programming/features/imageproc/page2.asp

/*		// technique: come up with a method of converting areas to 2 colors only. i can probably use a 3x3 grid type deal, similar to other scaling algorithms.
		// so, for each 3x3 grid, polarize the values into a set of 2 - "dark" and a "light" - colors. within a certain threshold, use the "current" dark and light.
		const float movingThreshold = 0.39f;// lower this is, the more frequently our palette changes
		const float actionThresh = 0.1f;// don't take any action unless there's this much variance in the grid.
		const float separation = 0.3f;// always keep light & dark this far apart.
		const float bias = 1.0001f;//multiplier to favor light (>1.0f) or dark(<1.0f)
		const int maxDistance = 100;

		for(int y = 1; y < (m_y - 1); y += 1)
		{
			RgbPixel currentDark;
			RgbPixel currentLight;
			float currentDarkI;
			float currentLightI;
			bool haveDark = false;
			bool haveLight = false;
			int distanceLight = 0;
			int distanceDark = 0;

			for(int x = 1; x < (m_x - 1); x += 1)
			{
				Pixel a(&tempCopy[x-1 + ((y-1) * m_x)]);
				Pixel b(&tempCopy[x-0 + ((y-1) * m_x)]);
				Pixel c(&tempCopy[x+1 + ((y-1) * m_x)]);
				Pixel d(&tempCopy[x-1 + ((y) * m_x)]);
				Pixel e(&tempCopy[x-0 + ((y) * m_x)]);
				Pixel f(&tempCopy[x+1 + ((y) * m_x)]);
				Pixel g(&tempCopy[x-1 + ((y+1) * m_x)]);
				Pixel h(&tempCopy[x-0 + ((y+1) * m_x)]);
				Pixel i(&tempCopy[x+1 + ((y+1) * m_x)]);
				// a b c
				// d e f
				// g h i
				float ia = GetIntensity(a);
				float ib = GetIntensity(b);
				float ic = GetIntensity(c);
				float id = GetIntensity(d);
				float ie = GetIntensity(e);
				float if_ = GetIntensity(f);
				float ig = GetIntensity(g);
				float ih = GetIntensity(h);
				float ii = GetIntensity(i);

				float lowestI = ia;
				RgbPixel lowestColor = a;
				lowestI = minxx(lowestI, ib, lowestColor, b);
				lowestI = minxx(lowestI, ic, lowestColor, c);
				lowestI = minxx(lowestI, id, lowestColor, d);
				lowestI = minxx(lowestI, ie, lowestColor, e);
				lowestI = minxx(lowestI, if_, lowestColor, f);
				lowestI = minxx(lowestI, ig, lowestColor, g);
				lowestI = minxx(lowestI, ih, lowestColor, h);
				lowestI = minxx(lowestI, ii, lowestColor, i);

				float highestI = ia;
				RgbPixel highestColor = a;
				highestI = maxxx(highestI, ib, highestColor, b);
				highestI = maxxx(highestI, ic, highestColor, c);
				highestI = maxxx(highestI, id, highestColor, d);
				highestI = maxxx(highestI, ie, highestColor, e);
				highestI = maxxx(highestI, if_, highestColor, f);
				highestI = maxxx(highestI, ig, highestColor, g);
				highestI = maxxx(highestI, ih, highestColor, h);
				highestI = maxxx(highestI, ii, highestColor, i);

				float oldDarkI = currentDarkI;
				RgbPixel oldDark = currentDark;
				int oldDarkDistance = distanceDark;

				if(!haveDark || distanceDark == maxDistance)
				{
					currentDark = lowestColor;
					currentDarkI = lowestI;
					haveDark = true;
					distanceDark = 0;
				}
				else
				{
					// if we're outside the threshold then switch to a new color as our "dark" color.
					if(!IsCloseTo(currentDark, lowestColor, movingThreshold))
					{
						currentDark = lowestColor;
						currentDarkI = lowestI;
						distanceDark = 0;
					}
				}

				if(!haveLight || distanceLight == maxDistance)
				{
					currentLight = highestColor;
					currentLightI = highestI;
					haveLight = true;
					distanceLight = 0;
				}
				else
				{
					// if we're outside the threshold then switch to a new color as our "dark" color.
					if(!IsCloseTo(currentLight, highestColor, movingThreshold))
					{
						if((highestI - currentDarkI) > separation)
						{
							currentLight = highestColor;
							currentLightI = highestI;
							distanceLight = 0;
						}
						else
						{
							currentDark  = oldDark;
							currentDarkI = oldDarkI;
							distanceDark = oldDarkDistance;
						}
					}
				}
				distanceLight ++;
				distanceDark ++;

				//if(currentLightI < currentDarkI)
				//	__asm int 3;

				if((highestI - lowestI) > actionThresh)
				{
					//Pixel a(&m_pbuf[x-1 + ((y-1) * m_x)]);
					//Pixel b(&m_pbuf[x-0 + ((y-1) * m_x)]);
					//Pixel c(&m_pbuf[x+1 + ((y-1) * m_x)]);
					//Pixel d(&m_pbuf[x-1 + ((y) * m_x)]);
					Pixel e(&m_pbuf[x-0 + ((y) * m_x)]);
					//Pixel f(&m_pbuf[x+1 + ((y) * m_x)]);
					//Pixel g(&m_pbuf[x-1 + ((y+1) * m_x)]);
					//Pixel h(&m_pbuf[x-0 + ((y+1) * m_x)]);
					//Pixel i(&m_pbuf[x+1 + ((y+1) * m_x)]);

					// now that we have currentLight and currentDark, fix up all the pixels.
					//AssignClosest(a, ia, currentLight, highestI, currentDark, lowestI);
					//AssignClosest(b, ib, currentLight, highestI, currentDark, lowestI);
					//AssignClosest(c, ic, currentLight, highestI, currentDark, lowestI);
					//AssignClosest(d, id, currentLight, highestI, currentDark, lowestI);
					AssignClosest(e, ie, bias, currentLight, highestI, currentDark, lowestI);
					//AssignClosest(f, if_, currentLight, highestI, currentDark, lowestI);
					//AssignClosest(g, ig, currentLight, highestI, currentDark, lowestI);
					//AssignClosest(h, ih, currentLight, highestI, currentDark, lowestI);
					//AssignClosest(i, ii, currentLight, highestI, currentDark, lowestI);
				}
			}
		}
		delete [] tempCopy;
*/

		//// examine 3 pixels at a time. if the 2nd pixel fits the profile of being a cleartype'd pixel, then operate.
		//// we always show intensity to the left. if we shove it either direction then things become too fat or skinny.
		//RgbPixel* begin = m_pbuf;
		//RgbPixel* end = m_pbuf + (m_x * m_y) - 3;// because we look 2 ahead
		//RgbPixel* p = begin;
  //  while(p < end)
  //  {
		//	// is it ascending or descending?
		//	float i1 = GetIntensity(*p);
		//	float i2 = GetIntensity(*(p+1));
		//	float i3 = GetIntensity(*(p+2));
		//	if((i1 < i2 && i2 < i3) || (i1 > i2 && i2 > i3))
		//	{
		//		// ascending or descending pattern. make the middle pixel the same intensity as the left, and
		//		// add the intensity to the right.
		//		*(p+1) = *p;
		//		*(p+2) = ScaleToIntensity(*(p+2), (i2 - i1) + i3);// add the missing intensity to the 3rd pixel.
		//	}
		//	p ++;
  //  }
	}

  template<long _Tbpp>
  bool StretchBlit(AnimBitmap<_Tbpp>& dest, long destx, long desty, long destw, long desth, long srcx, long srcy, long srcw, long srch)
  {
    int r = StretchBlt(
      dest.m_offscreen, destx, desty, destw, desth,
      m_offscreen, srcx, srcy, srcw, srch, SRCCOPY);
    return r != 0;
  }

  bool StretchBlit(AnimBitmap& dest, long x, long y, long w, long h)
  {
    int r = StretchBlt(dest.m_offscreen, x, y, w, h, m_offscreen, 0, 0, m_x, m_y, SRCCOPY);
    return r != 0;
  }

  bool StretchBlit(HDC hDest, long x, long y, long w, long h)
  {
    int r = StretchBlt(hDest, x, y, w, h, m_offscreen, 0, 0, m_x, m_y, SRCCOPY);
    return r != 0;
  }

  bool Blit(HDC hDest, long x, long y)
  {
    int r = BitBlt(hDest, x, y, x + m_x, y + m_y, m_offscreen, 0, 0, SRCCOPY);
    return r != 0;
  }

  template<long _Tbpp>
  bool Blit(AnimBitmap<_Tbpp>& dest, long x, long y)
  {
    int r = BitBlt(dest.m_offscreen, x, y, x + m_x, y + m_y, m_offscreen, 0, 0, SRCCOPY);
    return r != 0;
  }

  bool BlitFrom(HDC src, long x, long y, long w, long h, long DestX = 0, long DestY = 0)
  {
    int r = BitBlt(m_offscreen, DestX, DestY, w, h, src, x, y, SRCCOPY);
    return r != 0;
  }

  bool _DrawText(const char* s, long x, long y)
  {
    RECT rc;
    rc.left = x;
    rc.top = y;
    rc.right = m_x;
    rc.bottom = m_y;
    DrawText(m_offscreen, s, static_cast<int>(strlen(s)), &rc, DT_NOCLIP);
    return true;
  }

  RgbPixel* GetBuffer()
  {
    return m_pbuf;
  }

  bool hq2x(AnimBitmap<32>& out)
  {
    out.SetSize(m_x * 2, m_y * 2);
    hq2x_32(reinterpret_cast<BYTE*>(GetBuffer()), reinterpret_cast<BYTE*>(out.GetBuffer()), m_x, m_y, m_x * 8);
    return true;
  }

  bool hq3x(AnimBitmap<32>& out)
  {
    out.SetSize(m_x * 3, m_y * 3);
    hq3x_32(reinterpret_cast<BYTE*>(GetBuffer()), reinterpret_cast<BYTE*>(out.GetBuffer()), m_x, m_y, m_x * 12);
    return true;
  }

  bool hq4x(AnimBitmap<32>& out)
  {
    out.SetSize(m_x * 4, m_y * 4);
    hq4x_32(reinterpret_cast<BYTE*>(GetBuffer()), reinterpret_cast<BYTE*>(out.GetBuffer()), m_x, m_y, m_x * 16);
    return true;
  }

//private:
  long m_x;
  long m_y;
  HDC m_offscreen;
  HBITMAP m_bmp;
  RgbPixel* m_pbuf;

  template<typename T, typename Tmin, typename Tmax>
  inline static bool clamp(T& l, const Tmin& minval, const Tmax& maxval)
  {
    if(l < static_cast<T>(minval))
    {
      l = static_cast<T>(minval);
      return true;
    }
    if(l > static_cast<T>(maxval))
    {
      l = static_cast<T>(maxval);
      return true;
    }
    return false;
  }
};

