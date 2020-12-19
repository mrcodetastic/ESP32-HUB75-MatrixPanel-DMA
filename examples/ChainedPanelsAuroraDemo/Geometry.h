/*
 * Aurora: https://github.com/pixelmatix/aurora
 * Copyright (c) 2014 Jason Coon
 *
 * Portions of this code are adapted from Noel Bundy's work: https://github.com/TwystNeko/Object3d
 * Copyright (c) 2014 Noel Bundy
 * 
 * Portions of this code are adapted from the Petty library: https://code.google.com/p/peggy/
 * Copyright (c) 2008 Windell H Oskay.  All right reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef Geometry_H
#define Geometry_H

struct Vertex
{
    float x, y, z;
    Vertex()
    {
        this->set(0, 0, 0);
    }

    Vertex(float x, float y, float z)
    {
        this->set(x, y, z);
    }

    void set(float x, float y, float z)
    {
        this->x = x;
        this->y = y;
        this->z = z;
    }
};

struct EdgePoint
{
    int x, y;
    boolean visible;

    EdgePoint()
    {
        this->set(0, 0);
        this->visible = false;
    }

    void set(int a, int b)
    {
        this->x = a;
        this->y = b;
    }
};

struct Point
{
    float x, y;

    Point()
    {
        set(0, 0);
    }

    Point(float x, float y)
    {
        set(x, y);
    }

    void set(float x, float y)
    {
        this->x = x;
        this->y = y;
    }

};

struct squareFace
{
    int length;
    int sommets[4];
    int ed[4];

    squareFace()
    {
        set(-1, -1, -1, -1);
    }

    squareFace(int a, int b, int c, int d)
    {
        this->length = 4;
        this->sommets[0] = a;
        this->sommets[1] = b;
        this->sommets[2] = c;
        this->sommets[3] = d;
    }

    void set(int a, int b, int c, int d)
    {
        this->length = 4;
        this->sommets[0] = a;
        this->sommets[1] = b;
        this->sommets[2] = c;
        this->sommets[3] = d;
    }

};

struct triFace
{
  int length;
  int sommets[3];
  int ed[3];

  triFace()
  {
    set(-1,-1,-1);
  }
  triFace(int a, int b, int c)
  {
    this->length =3;
    this->sommets[0]=a;
    this->sommets[1]=b;
    this->sommets[2]=c;
  }
  void set(int a, int b, int c)
  { 
    this->length =3;
    this->sommets[0]=a;
    this->sommets[1]=b;
    this->sommets[2]=c;
  }
};

#endif