/*
 * Java YUV Image Player
 * Copyright (C) 2010 Luuvish <luuvish@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


public class YUVImage {

  enum Format {
    YUV_400,
    YUV_420,
    YUV_422,
    YUV_224, // vertical YUV422
    YUV_444,
  };

  protected Format format;
  protected int    width;
  protected int    height;

  protected int    size;
  protected byte[] data;

  public YUVImage(Format format, int width, int height) {
    this.format = format;
    this.width  = width;
    this.height = height;

    size = width * height;
    switch (format) {
    case YUV_400:                    break;
    case YUV_420: size = size * 3/2; break;
    case YUV_422: size = size * 2;   break;
    case YUV_224: size = size * 2;   break;
    case YUV_444: size = size * 3;   break;
    }
    data = new byte[size];
  }

  public int    getWidth()  { return width; }
  public int    getHeight() { return height; }
  public int    getSize()   { return size; }
  public byte[] getData()   { return data; }

  public void convertYUVtoRGB(int[] pixels) {
    int scaleX;
    int scaleY;

    switch (format) {
    case YUV_400: scaleX = 1; scaleY = 1; break;
    case YUV_420: scaleX = 2; scaleY = 2; break;
    case YUV_422: scaleX = 1; scaleY = 2; break;
    case YUV_224: scaleX = 2; scaleY = 1; break;
    case YUV_444: scaleX = 1; scaleY = 1; break;
    default:      scaleX = 1; scaleY = 1; break;
    }

    int base_y = 0;
    int base_u = base_y + width * height;
    int base_v = base_u + (width/scaleX) * (height/scaleY);
    int stride_y = width;
    int stride_u = width / scaleX;
    int stride_v = width / scaleX;
    byte by = (byte)128;
    byte bu = (byte)128;
    byte bv = (byte)128;

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        by = data[base_y + stride_y * y + x];
        if (format != Format.YUV_400) {
          bu = data[base_u + stride_u * (y/scaleY) + (x/scaleX)];
          bv = data[base_v + stride_v * (y/scaleY) + (x/scaleX)];
        }
        pixels[width * y + x] = convertPixel(by, bu, bv);
      }
    }
  }

  protected int convertPixel(byte y, byte u, byte v) {
    int iy = y & 0xff;
    int iu = u & 0xff;
    int iv = v & 0xff;

    float fr = 1.164f * (iy-16)                     + 1.596f * (iv-128);
    float fg = 1.164f * (iy-16) - 0.391f * (iu-128) - 0.813f * (iv-128);
    float fb = 1.164f * (iy-16) + 2.018f * (iu-128)                    ;

    int ir = (int)(fr > 255 ? 255 : fr < 0 ? 0 : fr);
    int ig = (int)(fg > 255 ? 255 : fg < 0 ? 0 : fg);
    int ib = (int)(fb > 255 ? 255 : fb < 0 ? 0 : fb);

    return (ir << 16) | (ig << 8) | (ib);
  }
}