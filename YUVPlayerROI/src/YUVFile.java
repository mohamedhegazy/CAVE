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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.RandomAccessFile;

public class YUVFile {

  protected String           name;
  protected RandomAccessFile file;
  protected long             size;

  protected YUVImage         image;
  protected long             frameNum;
  protected long             framePos;

  public YUVFile(File file, YUVImage image) {
    this.image = image;

    openFile(file);
    setFrameNum();
  }

  private void openFile(File file) {
    name = file.getName();

    try {
      this.file = new RandomAccessFile(file, "r");
    } catch (FileNotFoundException e) {
      System.out.println("file not found");
    }

    try {
      size = this.file.length();
    } catch (IOException e) {
      System.out.println("file length error");
    }
  }

  private void setFrameNum() {
    frameNum = 0;
    framePos = 0;

    if (size > 0 && image.getSize() > 0) {
      frameNum = size / image.getSize();
    }
  }

  public int     getWidth()    { return image.getWidth(); }
  public int     getHeight()   { return image.getHeight(); }

  public long    getFrameNum() { return frameNum; }
  public long    getFramePos() { return framePos; }
  public boolean isFirstPos()  { return framePos == 0; }
  public boolean isLastPos()   { return framePos == frameNum-1; }

  public void read(long framePos, int[] pixels) {
    if (framePos >= 0 && framePos < frameNum) {
      try {
        synchronized (image) {
          file.seek(framePos * image.getSize());
          file.read(image.getData(), 0, image.getSize());
          image.convertYUVtoRGB(pixels);
        }
        this.framePos = framePos;
      } catch (IOException e) {
        System.out.println("read io error");
      }
    }
  }
}