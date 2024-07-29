// Codetastic 2024
// ChatGPT was used to create this.

#ifndef PatternSphereSpin_H
#define PatternSphereSpin_H

#include <vector>
#include <cmath>

struct Point3D {
    float x, y, z;
};

struct Point2D {
    int x, y;
};

class PatternSphereSpin : public Drawable {

  private:

    std::vector<Point3D> points;

    float angleX = 0.0f;
    float angleY = 0.0f;
    float distance = 3.0f;


    Point2D project(Point3D point, float distance) {
        Point2D projected;
        projected.x = static_cast<int>((point.x / (distance - point.z)) * VPANEL_W / 2 + VPANEL_W / 2);
        projected.y = static_cast<int>((point.y / (distance - point.z)) * VPANEL_H / 2 + VPANEL_H / 2);
        return projected;
    }

    std::vector<Point3D> generateSpherePoints(int numPoints) {
        std::vector<Point3D> points;
        for (int i = 0; i < numPoints; ++i) {
            float theta = 2 * PI * (i / static_cast<float>(numPoints));
            for (int j = 0; j < numPoints / 2; ++j) {
                float phi = PI * (j / static_cast<float>(numPoints / 2));
                points.push_back({ cos(theta) * sin(phi), sin(theta) * sin(phi), cos(phi) });
            }
        }
        return points;
    }

    void rotatePoints(std::vector<Point3D>& points, float angleX, float angleY) {
        for (auto& point : points) {
            // Rotate around the X axis
            float y = point.y * cos(angleX) - point.z * sin(angleX);
            float z = point.y * sin(angleX) + point.z * cos(angleX);
            point.y = y;
            point.z = z;
            
            // Rotate around the Y axis
            float x = point.x * cos(angleY) + point.z * sin(angleY);
            z = -point.x * sin(angleY) + point.z * cos(angleY);
            point.x = x;
            point.z = z;
        }
    }


  public:
    PatternSphereSpin() {
      name = (char *)"Sphere Spin";
    }

    void start()
    {
        points = generateSpherePoints(30);
        angleX = 0.0f;
        angleY = 0.0f;
        distance = 3.0f;
    };

    unsigned int drawFrame() {

        effects.ClearFrame();

        // Rotate points
        rotatePoints(points, angleX, angleY);

        // Project and draw lines between points
        for (size_t i = 0; i < points.size(); ++i) {
            Point2D p1 = project(points[i], distance);
            for (size_t j = i + 1; j < points.size(); ++j) {
                Point2D p2 = project(points[j], distance);
                effects.drawLine(p1.x, p1.y, p2.x, p2.y, CRGB(255,255,255));
            }
        }

        // Update angles for rotation
        angleX += 0.05f;
        angleY += 0.05f;

      effects.ShowFrame();
      return 0;
    }
};

#endif
