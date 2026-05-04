#ifndef RAYCAST_LIB_H
#define RAYCAST_LIB_H

#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define MAX_OBJECTS 128
#define EPSILON 1e-6f

// objects
enum ObjectType {
    OBJ_CAMERA,
    OBJ_SPHERE,
    OBJ_PLANE, 
};

typedef struct metaData{
	
	std::string type;  
	int width;
	int height;
	int maxval; 
	
} metaData;

typedef struct sceneData {
    ObjectType type;

    float reflection;
    float c_diff[3];   
    float c_spec[3];
    float position[3]; 

    // camera properties
    float cam_width;
    float cam_height;

} sceneData;


typedef struct sphere : sceneData {
    char texture[65];
    float radius;

    uint8_t *pixmap;
    int texWidth;
    int texHeight;
    int texMax;
} sphere;

typedef struct plane : sceneData {
    float normal[3];
} plane;

typedef struct light {
    float position[3];
    float color[3];
    float radial_a0;
    float radial_a1;
    float radial_a2;
    float theta;
    float angular_a0;
    float direction[3];


} light;

// function prototypes

float dot3(const float a[3], const float b[3]);

void normalize3(float v[3]);

uint8_t toByte(float x);

bool hitSphere(const float Ro[3], const float Rd[3], sceneData* s, float &tHit);

bool hitPlane(const float Ro[3], const float Rd[3], sceneData* p, float &tHit);

bool readScene(char file[], light **lights, sceneData **Objects, sceneData *camera, int *objCount, int *lightCount);

bool writeppm(const char* outFile, int Wid, int Height, const uint8_t* pix);

bool inShadow(const float P[3], const float N[3], const light *L, sceneData **objects, int objCount);

void render(uint8_t *pix, int Wid, int Height, float Ro[3], sceneData **objects, light **lights, int objCount, int lightCount, sceneData camera, int raycastLimit);

void make3D(uint8_t *out, uint8_t *left, uint8_t *right, int Wid, int Height);

void traceRay(float color[3], const float Ro[3], const float Rd[3], int depth, sceneData **objects, light **lights, int objCount, int lightCount, int raycastLimit);

float v3len(const float a[3]);

void v3sub(float out[3], const float a[3], const float b[3]);

#endif // RAYCAST_LIB_H
