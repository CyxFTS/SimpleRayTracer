/* **************************
 * CSCI 420
 * Assignment 3 Raytracer
 * Name: Yunxuan Cai
 * *************************
*/

#ifdef WIN32
  #include <windows.h>
#endif

#if defined(WIN32) || defined(linux)
  #include <GL/gl.h>
  #include <GL/glut.h>
#elif defined(__APPLE__)
  #include <OpenGL/gl.h>
  #include <GLUT/glut.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glm/glm.hpp>
#ifdef WIN32
  #define strcasecmp _stricmp
#endif

#include <imageIO.h>
#include <random>
#include <vector>

#define MAX_TRIANGLES 20000
#define MAX_SPHERES 100
#define MAX_LIGHTS 1000

char * filename = NULL;

//different display modes
#define MODE_DISPLAY 1
#define MODE_JPEG 2

int mode = MODE_DISPLAY;

//you may want to make these smaller for debugging purposes
#define WIDTH 640
#define HEIGHT 480

//the field of view of the camera
#define fov 60.0

const double epsilon = 0.001;

// Supersampling antialiasing
#define SSAA 3

unsigned char buffer[HEIGHT][WIDTH][3];

struct Vertex
{
  double position[3];
  double color_diffuse[3];
  double color_specular[3];
  double normal[3];
  double shininess;
};

struct Triangle
{
  Vertex v[3];
};

struct Sphere
{
  double position[3];
  double color_diffuse[3];
  double color_specular[3];
  double shininess;
  double radius;
};

struct Light
{
  double position[3];
  double color[3];
};

Triangle triangles[MAX_TRIANGLES];
Sphere spheres[MAX_SPHERES];
Light lights[MAX_LIGHTS];
double ambient_light[3];

int num_triangles = 0;
int num_spheres = 0;
int num_lights = 0;

void plot_pixel_display(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel_jpeg(int x,int y,unsigned char r,unsigned char g,unsigned char b);
void plot_pixel(int x,int y,unsigned char r,unsigned char g,unsigned char b);

double sphereIntersection(const unsigned int& index, const glm::vec3& origin, const glm::vec3& ray) {
    glm::vec3 spherepos(spheres[index].position[0], spheres[index].position[1], spheres[index].position[2]);
    double radius = spheres[index].radius;
    double a = 1; // unit vector
    double b = 2 * glm::dot(ray, origin - spherepos);
    double c = glm::dot(origin - spherepos, origin - spherepos) - radius * radius;

    double descriminant = b * b - 4 * c; //  (b^2 - 4*a*c)
    if (descriminant < epsilon) 
        return -1;
    double sqroot = sqrt(descriminant);
    double negroot = (-b - sqroot) / 2;
    if (negroot > epsilon) 
        return negroot;
    else
    {
        double root = (-b + sqrt(descriminant)) / 2;
        return root;
    }
}

glm::vec4 triangleIntersection(const unsigned int& index, const glm::vec3& origin, const glm::vec3& rayDirection) {

    glm::vec3 p0(triangles[index].v[0].position[0], triangles[index].v[0].position[1], triangles[index].v[0].position[2]);
    glm::vec3 p1(triangles[index].v[1].position[0], triangles[index].v[1].position[1], triangles[index].v[1].position[2]);
    glm::vec3 p2(triangles[index].v[2].position[0], triangles[index].v[2].position[1], triangles[index].v[2].position[2]);
    glm::vec3 planeNormal = glm::normalize(glm::cross((p1 - p0), (p2 - p0)));

    double angle = glm::dot(planeNormal, rayDirection);
    if (abs(angle) < epsilon) // parallel
        return glm::vec4(-1, 0, 0, 0);

    double t = (glm::dot(planeNormal, p0) - glm::dot(planeNormal, origin)) / glm::dot(planeNormal, rayDirection);

    if (t < epsilon) 
        return glm::vec4(t, 0, 0, 0);

    glm::vec3 intersection = origin + (rayDirection * (float)t);
    glm::vec2 i0Proj, v0Proj, v1Proj, v2Proj;

    if (abs(glm::dot(glm::vec3(1, 0, 0), planeNormal)) >= epsilon) {
        // to x=0
        i0Proj = glm::vec2(intersection.y, intersection.z);
        v0Proj = glm::vec2(p0.y, p0.z);
        v1Proj = glm::vec2(p1.y, p1.z);
        v2Proj = glm::vec2(p2.y, p2.z);
    }
    else if (abs(glm::dot(glm::vec3(0, 1, 0), planeNormal)) >= epsilon) {
        // to y=0
        i0Proj = glm::vec2(intersection.x, intersection.z);
        v0Proj = glm::vec2(p0.x, p0.z);
        v1Proj = glm::vec2(p1.x, p1.z);
        v2Proj = glm::vec2(p2.x, p2.z);
    }
    else if (abs(glm::dot(glm::vec3(0, 0, 1), planeNormal)) >= epsilon) {
        // to z=0
        i0Proj = glm::vec2(intersection.x, intersection.y);
        v0Proj = glm::vec2(p0.x, p0.y);
        v1Proj = glm::vec2(p1.x, p1.y);
        v2Proj = glm::vec2(p2.x, p2.y);
    }

    // barycentric coordinates calculation
    double S = 0.5 * ((v1Proj.x - v0Proj.x) * (v2Proj.y - v0Proj.y) - (v2Proj.x - v0Proj.x) * (v1Proj.y - v0Proj.y));
    double a = 0.5 / S * ((v1Proj.x - i0Proj.x) * (v2Proj.y - i0Proj.y) - (v2Proj.x - i0Proj.x) * (v1Proj.y - i0Proj.y));
    double b = 0.5 / S * ((i0Proj.x - v0Proj.x) * (v2Proj.y - v0Proj.y) - (v2Proj.x - v0Proj.x) * (i0Proj.y - v0Proj.y));
    double c = 0.5 / S * ((v1Proj.x - v0Proj.x) * (i0Proj.y - v0Proj.y) - (i0Proj.x - v0Proj.x) * (v1Proj.y - v0Proj.y));

    // inside triangle
    if (a >= 0 && b >= 0 && c >= 0 && a <= 1 && b <= 1 && c <= 1 && abs(1 - a - b - c) <= epsilon)
        return glm::vec4(t, a, b, c);
    // outside triangle
    else
        return glm::vec4(-1, 0, 0, 0);
}

//MODIFY THIS FUNCTION
void draw_scene()
{
    unsigned char SSAA_buffer[WIDTH * SSAA][SSAA][3];

    double fovRad = fov * 3.14159265 / 180; //Deg2Rad

    double imageZ = -1;
    double imageY = tan(fovRad / 2);
    double imageX = (1.0 * WIDTH / HEIGHT) * imageY;

    double halfWidth = WIDTH / 2 * SSAA;
    double halfHeight = HEIGHT / 2 * SSAA;

    for (int y = HEIGHT * SSAA - 1; y >= 0 ; y--)
    {
        double camY = (y - halfHeight) / halfHeight * imageY; 
#pragma omp parallel for
        for (int x = 0; x < WIDTH * SSAA; x++)
        {
            double camX = (x - halfWidth) / halfWidth * imageX; 
            
            glm::vec3 viewRay = glm::normalize(glm::vec3(camX, camY, imageZ));

            double t0 = -1.0;
            bool isSphere = false;
            int index = -1;

            //find intersection
            for (int i = 0; i < num_spheres; ++i) {
                double t = sphereIntersection(i, glm::vec3(0, 0, 0), viewRay);
                if (t > epsilon && (t0 < 0 || t < t0)) {
                    t0 = t;
                    isSphere = true;
                    index = i;
                }
            }

            glm::vec3 pos;
            for (int i = 0; i < num_triangles; ++i) {
                glm::vec4 intersection = triangleIntersection(i, glm::vec3(0, 0, 0), viewRay);
                double t = intersection[0];
                if (t > epsilon && (t0 < 0 || t < t0)) {
                    t0 = t;
                    isSphere = false;
                    index = i;
                    pos = glm::vec3(intersection[1], intersection[2], intersection[3]);
                }
            }

            // phong lighting calculation
            if (t0 > 0) {
                glm::vec3 intersectPoint = viewRay * (float)t0;
                glm::vec3 pixelColor(0, 0, 0);

                glm::vec3 iNormal;
                glm::vec3 iDiffuse;
                glm::vec3 iSpecular;
                double pointShininess(0.0);
                if (isSphere) {
                    Sphere sphere = spheres[index];
                    iNormal = (intersectPoint - glm::vec3(sphere.position[0], sphere.position[1], sphere.position[2])) / (float)sphere.radius;
                    iDiffuse = glm::vec3(sphere.color_diffuse[0], sphere.color_diffuse[1], sphere.color_diffuse[2]);
                    iSpecular = glm::vec3(sphere.color_specular[0], sphere.color_specular[1], sphere.color_specular[2]);
                    pointShininess = sphere.shininess;
                }
                else {
                    for (int i = 0; i < 3; ++i) {
                        Vertex v = triangles[index].v[i];
                        glm::vec3 normal(v.normal[0], v.normal[1], v.normal[2]);
                        iNormal += glm::normalize(normal) * (float)pos[i];

                        for (int j = 0; j < 3; ++j) {
                            iDiffuse[j] += v.color_diffuse[j] * pos[i];
                            iSpecular[j] += v.color_specular[j] * pos[i];
                        }
                        pointShininess += v.shininess * pos[i];
                    }
                }

                // shadow calculation
                for (int i = 0; i < num_lights; ++i) {
                    bool inShadow = false;
                    glm::vec3 lightPos(lights[i].position[0], lights[i].position[1], lights[i].position[2]);
                    glm::vec3 shadowRay = lightPos - intersectPoint;
                    double len = sqrt(glm::dot(shadowRay, shadowRay));

                    shadowRay /= len; // normalize
                    double dis = len + epsilon; 

                    for (int j = 0; j < num_spheres && !inShadow; ++j) {
                        double t = sphereIntersection(j, intersectPoint, shadowRay);
                        if (t > epsilon && t < dis) {
                            inShadow = true;
                            break;
                        }
                    }
                    for (int j = 0; j < num_triangles && !inShadow; ++j) {
                        double t = triangleIntersection(j, intersectPoint, shadowRay)[0];
                        if (t > epsilon && t < dis) {
                            inShadow = true;
                            break;
                        }
                    }

                    if (inShadow)
                        continue;

                    // pixel color calculation
                    double ldotn = glm::dot(shadowRay, iNormal);
                    if (ldotn < 0)
                        ldotn = 0;

                    double rdotv = glm::dot(-shadowRay - (2 * glm::dot(-shadowRay, iNormal) * iNormal), -viewRay);
                    if (rdotv < 0)
                        rdotv = 0;

                    for (int j = 0; j < 3; ++j) 
                        pixelColor[j] += lights[i].color[j] * (iDiffuse[j] * ldotn + iSpecular[j] * pow(rdotv, pointShininess));
                }

                // convert to 8-bit rgb color
                for (int i = 0; i < 3; ++i) {
                    pixelColor[i] += ambient_light[i];
                    if (pixelColor[i] > 1.0)
                        pixelColor[i] = 1.0; 
                    SSAA_buffer[x][y % SSAA][i] = pixelColor[i] * 255;
                }
            }
            else
                for (int i = 0; i < 3; ++i) 
                    SSAA_buffer[x][y % SSAA][i] = 255;
        }
        if (y > 0 && y < HEIGHT * SSAA - 1 && y % SSAA == 0) {
            glPointSize(2.0);
            glBegin(GL_POINTS);

            for (int xpixel = 0; xpixel < WIDTH; ++xpixel) {
                int sumR = 0;
                int sumG = 0;
                int sumB = 0;
                for (int i = 0; i < SSAA; ++i) {
                    for (int j = xpixel * SSAA; j < xpixel * SSAA + SSAA; ++j) {
                        sumR += SSAA_buffer[j][i][0];
                        sumG += SSAA_buffer[j][i][1];
                        sumB += SSAA_buffer[j][i][2];
                    }
                }
                plot_pixel(xpixel, y / SSAA - 1, sumR / SSAA / SSAA, sumG / SSAA / SSAA, sumB / SSAA / SSAA);
            }

            glEnd();
            glFlush();
        }
    }
    glPointSize(2.0);
    glBegin(GL_POINTS);

    // draw last line
    for (int x = 0; x < WIDTH; ++x) {
        int sumR = 0, sumG = 0, sumB = 0;
        for (int i = 0; i < SSAA; ++i) {
            for (int j = x * SSAA; j < x * SSAA + SSAA; ++j) {
                sumR += SSAA_buffer[j][i][0];
                sumG += SSAA_buffer[j][i][1];
                sumB += SSAA_buffer[j][i][2];
            }
        }
        plot_pixel(x, 0, sumR / SSAA / SSAA, sumG / SSAA / SSAA, sumB / SSAA / SSAA);
    }

    glEnd();
    glFlush();
  printf("Done!\n"); fflush(stdout);
}

void plot_pixel_display(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  glColor3f(((float)r) / 255.0f, ((float)g) / 255.0f, ((float)b) / 255.0f);
  glVertex2i(x,y);
}

void plot_pixel_jpeg(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  buffer[y][x][0] = r;
  buffer[y][x][1] = g;
  buffer[y][x][2] = b;
}

void plot_pixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
  plot_pixel_display(x,y,r,g,b);
  if(mode == MODE_JPEG)
    plot_pixel_jpeg(x,y,r,g,b);
}

void save_jpg()
{
  printf("Saving JPEG file: %s\n", filename);

  ImageIO img(WIDTH, HEIGHT, 3, &buffer[0][0][0]);
  if (img.save(filename, ImageIO::FORMAT_JPEG) != ImageIO::OK)
    printf("Error in Saving\n");
  else 
    printf("File saved Successfully\n");
}

void parse_check(const char *expected, char *found)
{
  if(strcasecmp(expected,found))
  {
    printf("Expected '%s ' found '%s '\n", expected, found);
    printf("Parse error, abnormal abortion\n");
    exit(0);
  }
}

void parse_doubles(FILE* file, const char *check, double p[3])
{
  char str[100];
  fscanf(file,"%s",str);
  parse_check(check,str);
  fscanf(file,"%lf %lf %lf",&p[0],&p[1],&p[2]);
  printf("%s %lf %lf %lf\n",check,p[0],p[1],p[2]);
}

void parse_rad(FILE *file, double *r)
{
  char str[100];
  fscanf(file,"%s",str);
  parse_check("rad:",str);
  fscanf(file,"%lf",r);
  printf("rad: %f\n",*r);
}

void parse_shi(FILE *file, double *shi)
{
  char s[100];
  fscanf(file,"%s",s);
  parse_check("shi:",s);
  fscanf(file,"%lf",shi);
  printf("shi: %f\n",*shi);
}

int loadScene(char *argv)
{
  FILE * file = fopen(argv,"r");
  int number_of_objects;
  char type[50];
  Triangle t;
  Sphere s;
  Light l;
  fscanf(file,"%i", &number_of_objects);

  printf("number of objects: %i\n",number_of_objects);

  parse_doubles(file,"amb:",ambient_light);

  for(int i=0; i<number_of_objects; i++)
  {
    fscanf(file,"%s\n",type);
    printf("%s\n",type);
    if(strcasecmp(type,"triangle")==0)
    {
      printf("found triangle\n");
      for(int j=0;j < 3;j++)
      {
        parse_doubles(file,"pos:",t.v[j].position);
        parse_doubles(file,"nor:",t.v[j].normal);
        parse_doubles(file,"dif:",t.v[j].color_diffuse);
        parse_doubles(file,"spe:",t.v[j].color_specular);
        parse_shi(file,&t.v[j].shininess);
      }

      if(num_triangles == MAX_TRIANGLES)
      {
        printf("too many triangles, you should increase MAX_TRIANGLES!\n");
        exit(0);
      }
      triangles[num_triangles++] = t;
    }
    else if(strcasecmp(type,"sphere")==0)
    {
      printf("found sphere\n");

      parse_doubles(file,"pos:",s.position);
      parse_rad(file,&s.radius);
      parse_doubles(file,"dif:",s.color_diffuse);
      parse_doubles(file,"spe:",s.color_specular);
      parse_shi(file,&s.shininess);

      if(num_spheres == MAX_SPHERES)
      {
        printf("too many spheres, you should increase MAX_SPHERES!\n");
        exit(0);
      }
      spheres[num_spheres++] = s;
    }
    else if(strcasecmp(type,"light")==0)
    {
     printf("found light\n");
	  parse_doubles(file, "pos:", l.position);
	  parse_doubles(file, "col:", l.color);

      Light templ;

	  // give light source a volume
	  l.color[0] /= 27;
	  l.color[1] /= 27;
	  l.color[2] /= 27;
      
	  for (int i = -1; i <= 1; ++i) 
		  for (int j = -1; j <= 1; ++j) 
			  for (int k = -1; k <= 1; ++k) {
                  if (num_lights == MAX_LIGHTS)
                  {
                      printf("too many lights, you should increase MAX_LIGHTS!\n");
                      exit(0);
                  }
                  Light templ;
                  templ.color[0] = l.color[0];
                  templ.color[1] = l.color[1];
                  templ.color[2] = l.color[2];
                  templ.position[0] = l.position[0] + 0.02 * i;
                  templ.position[1] = l.position[1] + 0.02 * j;
                  templ.position[2] = l.position[2] + 0.02 * k;
                  lights[num_lights++] = templ;
			  }
    }
    else
    {
      printf("unknown type in scene description:\n%s\n",type);
      exit(0);
    }
  }
  return 0;
}

void display()
{
}

void init()
{
  glMatrixMode(GL_PROJECTION);
  glOrtho(0,WIDTH,0,HEIGHT,1,-1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  glClearColor(0,0,0,0);
  glClear(GL_COLOR_BUFFER_BIT);
}

void idle()
{
  //hack to make it only draw once
  static int once=0;
  if(!once)
  {
    draw_scene();
    if(mode == MODE_JPEG)
      save_jpg();
  }
  once=1;
}

int main(int argc, char ** argv)
{
  if ((argc < 2) || (argc > 3))
  {  
    printf ("Usage: %s <input scenefile> [output jpegname]\n", argv[0]);
    exit(0);
  }
  if(argc == 3)
  {
    mode = MODE_JPEG;
    filename = argv[2];
  }
  else if(argc == 2)
    mode = MODE_DISPLAY;

  glutInit(&argc,argv);
  loadScene(argv[1]);

  glutInitDisplayMode(GLUT_RGBA | GLUT_SINGLE);
  glutInitWindowPosition(0,0);
  glutInitWindowSize(WIDTH,HEIGHT);
  int window = glutCreateWindow("Ray Tracer");
  #ifdef __APPLE__
    // This is needed on recent Mac OS X versions to correctly display the window.
    glutReshapeWindow(WIDTH - 1, HEIGHT - 1);
  #endif
  glutDisplayFunc(display);
  glutIdleFunc(idle);
  init();
  glutMainLoop();
}

