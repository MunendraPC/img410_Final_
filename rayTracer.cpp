// headers
#include "raycast_lib.h"

#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstdlib>

float dot3(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];

}

void normalize3(float v[3])
{
    float len = std::sqrt(dot3(v, v));
    if (len > 1e-6f)
    {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

uint8_t toByte(float x)
{
    if (x < 0.0f)
        x = 0.0f;
    if (x > 1.0f)
        x = 1.0f;
    int v = (int)std::lround(x * 255.0f);
    if (v < 0)
        v = 0;
    if (v > 255)
        v = 255;
    return (uint8_t)v;
}

// raycast hit sphere
bool hitSphere(const float Ro[3], const float Rd[3], sceneData *s, float &tHit)
{
    float C[3] = {s->position[0], s->position[1], s->position[2]};
    sphere *spherePtr = static_cast<sphere *>(s);
    float r = spherePtr->radius;

    float OC[3] = {Ro[0] - C[0], Ro[1] - C[1], Ro[2] - C[2]};

    float a = dot3(Rd, Rd);
    float b = 2.0f * dot3(OC, Rd);
    float c = dot3(OC, OC) - r * r;

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f)
        return false;

    float sd = std::sqrt(disc);
    float t0 = (-b - sd) / (2.0f * a);
    float t1 = (-b + sd) / (2.0f * a);

    const float EPS = 1e-6f;
    if (t0 > EPS)
    {
        tHit = t0;
        return true;
    }
    if (t1 > EPS)
    {
        tHit = t1;
        return true;
    }
    return false;
}

// raycast hit plane
bool hitPlane(const float Ro[3], const float Rd[3], sceneData *p, float &tHit)
{
    plane *planePtr = static_cast<plane *>(p);
    float Normal[3] = {planePtr->normal[0], planePtr->normal[1], planePtr->normal[2]};
    normalize3(Normal);

    float P0[3] = {p->position[0], p->position[1], p->position[2]};

    float denom = dot3(Normal, Rd);
    const float EPS = 1e-6f;
    if (std::fabs(denom) < EPS)
        return false;

    float P0O[3] = {P0[0] - Ro[0], P0[1] - Ro[1], P0[2] - Ro[2]};
    float t = dot3(Normal, P0O) / denom;
    if (t <= EPS)
        return false;

    tHit = t;
    return true;
}

// skip white space and comments in the PPM file
//PPM comments start with # and go to the end of the line
void skipComments(std::ifstream &image)
{
    image >> std::ws; 
    // if the next character is a '#', skip the entire line
    while (image.peek() == '#')
    {
        std::string comment;
        std::getline(image, comment);
        image >> std::ws; 
    }
}

//read a PPM file and store the pixel data in a 1D array
bool readPPM(const char filename[], uint8_t *&pixmap, metaData &metadata)
{
    std::ifstream image(filename);
    if (!image.is_open())
    {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }

    image >> metadata.type;
    if (metadata.type != "P3")
    {
        std::cerr << "Error: Only P3 PPM format supported\n";
        return false;
    }

    skipComments(image);
    image >> metadata.width;
    skipComments(image);
    image >> metadata.height;
    skipComments(image);
    image >> metadata.maxval;
    skipComments(image);

    pixmap = new uint8_t[metadata.width * metadata.height * 3];

    for (int i = 0; i < metadata.width * metadata.height; i++)
    {
        int r, g, b;

        skipComments(image);
        image >> r;
        skipComments(image);
        image >> g;
        skipComments(image);
        image >> b;

        pixmap[i * 3]     = (uint8_t)r;
        pixmap[i * 3 + 1] = (uint8_t)g;
        pixmap[i * 3 + 2] = (uint8_t)b;
    }

    return true;
}

// read scene
bool readScene(char file[], light **lights, sceneData **Objects, sceneData *camera, int *objCount, int *lightCount)
{
    std::ifstream scene(file);
    if (!scene.is_open())
    {
        std::cerr << "Error: Could not open file " << file << "\n";
        return false;
    }

    std::printf("\nUsing scene %s \n\n", file);

    // defaults for camera
    camera->cam_width = 0.0f;
    camera->cam_height = 0.0f;
    camera->c_diff[0] = camera->c_diff[1] = camera->c_diff[2] = 0.0f;
    camera->position[0] = camera->position[1] = camera->position[2] = 0.0f;
    camera->type = OBJ_CAMERA;

    std::string magic;
    scene >> magic;
    if (magic != "img410scene")
    {
        std::cerr << "Error: Only img410scene format supported\n";
        return false;
    }

    std::string elem;
    while (scene >> elem)
    {
        if (elem == "end")
            break;

        // camera
        if (elem == "camera")
        {
            std::string property;
            while (scene >> property)
            {
                bool endObj = (!property.empty() && property.back() == ';');
                if (!property.empty() && property.back() == ';')
                    property.pop_back();

                if (property == "width:")
                {
                    scene >> camera->cam_width;
                }
                else if (property == "height:")
                {
                    scene >> camera->cam_height;
                }

                if (endObj)
                    break;
            }
        }

        // sphere
        else if (elem == "sphere")
        {
            if (*objCount >= 128)
            {
                std::cerr << "Error: too many objects\n";
                return false;
            }

            sceneData *s = new sphere();
            sphere *spherePtr = static_cast<sphere *>(s);
            // defaults if elements are missing
            spherePtr->reflection = 0.0f;
            spherePtr->c_diff[0] = spherePtr->c_diff[1] = spherePtr->c_diff[2] = 0.0f;
            spherePtr->c_spec[0] = spherePtr->c_spec[1] = spherePtr->c_spec[2] = 1.0f;
            spherePtr->position[0] = spherePtr->position[1] = spherePtr->position[2] = 0.0f;
            spherePtr->radius = 0.0f;
            spherePtr->texture[0] = '\0';
            spherePtr->pixmap = nullptr;
            spherePtr->texWidth = 0;
            spherePtr->texHeight = 0;
            spherePtr->texMax = 255;
            spherePtr->type = OBJ_SPHERE;

            std::string property;
            while (scene >> property)
            {
                bool endObj = (!property.empty() && property.back() == ';');
                if (!property.empty() && property.back() == ';')
                    property.pop_back();

                if(property == "reflection:")
                {
                    scene >> spherePtr->reflection;
                }
                else if (property == "c_diff:")
                {
                    scene >> spherePtr->c_diff[0] >> spherePtr->c_diff[1] >> spherePtr->c_diff[2];
                }
                else if (property == "position:")
                {
                    scene >> spherePtr->position[0] >> spherePtr->position[1] >> spherePtr->position[2];
                }
                else if (property == "radius:")
                {
                    scene >> spherePtr->radius;
                }
                else if (property == "c_spec:")
                {
                    scene >> spherePtr->c_spec[0] >> spherePtr->c_spec[1] >> spherePtr->c_spec[2];
                }
                else if (property == "texture:")
                {
                    std::string tex;
                    scene >> tex;

                    if (tex.size() >= 2 && tex.front() == '"' && tex.back() == '"')
                    {
                        tex = tex.substr(1, tex.size() - 2);
                    }

                    std::strncpy(spherePtr->texture, tex.c_str(), 64);
                    spherePtr->texture[64] = '\0';
                }

                if (endObj)
                    break;
            }
            if (spherePtr->texture[0] != '\0')
            {
                metaData metadata;
                if (readPPM(spherePtr->texture, spherePtr->pixmap, metadata))
                {
                    spherePtr->texWidth = metadata.width;
                    spherePtr->texHeight = metadata.height;
                    spherePtr->texMax = metadata.maxval;
                }
                else
                {
                    std::cerr << "Warning: failed to load texture " << spherePtr->texture << "\n";
                }
            }
            Objects[*objCount] = s;
            (*objCount)++;
        }

        // plane
        else if (elem == "plane")
        {
            if (*objCount >= 128)
            {
                std::cerr << "Error: too many objects\n";
                return false;
            }

            sceneData *p = new plane();
            plane *planePtr = static_cast<plane *>(p);
            // defaults if elements are missing
            planePtr->reflection = 0.0f;
            planePtr->c_diff[0] = planePtr->c_diff[1] = planePtr->c_diff[2] = 0.0f;
            planePtr->c_spec[0] = planePtr->c_spec[1] = planePtr->c_spec[2] = 1.0f;
            planePtr->position[0] = planePtr->position[1] = planePtr->position[2] = 0.0f;
            planePtr->normal[0] = planePtr->normal[1] = planePtr->normal[2] = 0.0f;
            planePtr->type = OBJ_PLANE;

            std::string property;
            while (scene >> property)
            {
                bool endObj = (!property.empty() && property.back() == ';');
                if (!property.empty() && property.back() == ';')
                    property.pop_back();

                if(property == "reflection:")
                {
                    scene >> planePtr->reflection;
                }
                else if (property == "c_diff:")
                {
                    scene >> planePtr->c_diff[0] >> planePtr->c_diff[1] >> planePtr->c_diff[2];
                }
                else if (property == "position:")
                {
                    scene >> planePtr->position[0] >> planePtr->position[1] >> planePtr->position[2];
                }
                else if (property == "normal:")
                {
                    scene >> planePtr->normal[0] >> planePtr->normal[1] >> planePtr->normal[2];
                    normalize3(planePtr->normal);
                }
                else if (property == "c_spec:")
                {
                    scene >> planePtr->c_spec[0] >> planePtr->c_spec[1] >> planePtr->c_spec[2];
                }

                if (endObj)
                    break;
            }

            Objects[*objCount] = p;
            (*objCount)++;
        }

        // light
        else if (elem == "light")
        {
            if (*lightCount >= 128)
            {
                std::cerr << "Error: too many lights\n";
                return false;
            }

            light *lightPtr = new light;
            // defaults if elements are missing
            lightPtr->color[0] = lightPtr->color[1] = lightPtr->color[2] = 1.0f;
            lightPtr->radial_a0 = 0.0f;
            lightPtr->radial_a1 = 0.0f;
            lightPtr->radial_a2 = 0.0f;
            lightPtr->angular_a0 = 0.0f;
            lightPtr->theta = 0.0f;
            lightPtr->direction[0] = lightPtr->direction[1] = lightPtr->direction[2] = 0.0f;

            std::string property;
            while (scene >> property)
            {
                bool endObj = (!property.empty() && property.back() == ';');
                if (!property.empty() && property.back() == ';')
                    property.pop_back();

                if (property == "color:")
                {
                    scene >> lightPtr->color[0] >> lightPtr->color[1] >> lightPtr->color[2];
                }
                else if (property == "radial_a2:")
                {
                    scene >> lightPtr->radial_a2;
                }
                else if (property == "radial_a1:")
                {
                    scene >> lightPtr->radial_a1;
                }
                else if (property == "radial_a0:")
                {
                    scene >> lightPtr->radial_a0;
                }
                else if (property == "position:")
                {
                    scene >> lightPtr->position[0] >> lightPtr->position[1] >> lightPtr->position[2];
                }
                else if (property == "direction:")
                {
                    scene >> lightPtr->direction[0] >> lightPtr->direction[1] >> lightPtr->direction[2];
                }
                else if (property == "angular_a0:")
                {
                    scene >> lightPtr->angular_a0;
                }
                else if (property == "theta:")
                {
                    scene >> lightPtr->theta;
                }

                if (endObj)
                    break;
            }

            lights[*lightCount] = lightPtr;
            (*lightCount)++;
        }

        else
        {
            // do nothing if unrecognized or giberish
        }
    }

    return true;
}

bool writeppm(const char *outFile, int Wid, int Height, const uint8_t *pix)
{
    std::ofstream out(outFile);
    if (!out.is_open())
        return false;

    out << "P3\n"
        << Wid << " " << Height << "\n255\n";
    for (int j = 0; j < Height; j++)
    {
        for (int i = 0; i < Wid; i++)
        {
            int idx = (j * Wid + i) * 3;
            out << (int)pix[idx] << " " << (int)pix[idx + 1] << " " << (int)pix[idx + 2] << "\n";
        }
    }
    return true;
}

void v3sub(float out[3], const float a[3], const float b[3])
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

float v3len(const float a[3])
{
    return std::sqrt(dot3(a, a));
}

bool inShadow(const float P[3], const float N[3], const light *L,
                     sceneData **objects, int objCount)
{
    float RoS[3] = {P[0] + N[0] * 1e-4f, P[1] + N[1] * 1e-4f, P[2] + N[2] * 1e-4f};

    float toL[3];
    v3sub(toL, L->position, P);
    float distToLight = v3len(toL);
    normalize3(toL);

    for (int i = 0; i < objCount; i++)
    {
        float tHit;
        if (objects[i]->type == OBJ_SPHERE)
        {
            if (hitSphere(RoS, toL, objects[i], tHit) && tHit > 1e-6f && tHit < distToLight)
                return true;
        }
        else if (objects[i]->type == OBJ_PLANE)
        {
            if (hitPlane(RoS, toL, objects[i], tHit) && tHit > 1e-6f && tHit < distToLight)
                return true;
        }
    }
    return false;
}

void traceRay(float color[3], const float Ro[3], const float Rd[3], int depth, sceneData **objects, light **lights, int objCount, int lightCount, int raycastLimit) 
{

    color[0] = color[1] = color[2] = 0.0f;
    float min_t = std::numeric_limits<float>::infinity();
            
    // set hit
     sceneData *hitObj = nullptr;
    bool hit = false;

    // intersection by object type
    for (int s = 0; s < objCount; s++)
    {
        float tHit = 0.0f;
        if (objects[s]->type == OBJ_SPHERE)
        {
            if (hitSphere(Ro, Rd, objects[s], tHit) && tHit < min_t) // if sphere at that location
            {
                // set the closest t value 
                min_t = tHit;
                hitObj = objects[s];
                hit = true;
            }
        }
        else if (objects[s]->type == OBJ_PLANE) // same with planes
        {
            if (hitPlane(Ro, Rd, objects[s], tHit) && tHit < min_t)
            {
                min_t = tHit;
                hitObj = objects[s];
                hit = true;
            }
        }
    }

    if (!hit) {
    // No hit: background is black
    return;
    }

    // hit point P
    float P[3] = {Ro[0] + min_t * Rd[0], Ro[1] + min_t * Rd[1], Ro[2] + min_t * Rd[2]};

    // normal N
    float N[3] = {0, 0, 0};
    if (hitObj->type == OBJ_SPHERE)
    {
        float C[3] = {hitObj->position[0], hitObj->position[1], hitObj->position[2]};
        v3sub(N, P, C);
        normalize3(N);
    }
    else
    { // plane
        plane *pp = static_cast<plane *>(hitObj);
        N[0] = pp->normal[0];
        N[1] = pp->normal[1];
        N[2] = pp->normal[2];
        normalize3(N);
    }

    // view dir V (camera at origin)
    float V[3] = {-P[0], -P[1], -P[2]};
    normalize3(V);

    float outColor[3] = {0.0f, 0.0f, 0.0f};

    // for light in lights
    for (int li = 0; li < lightCount; li++)
    {
        // set a temp light to represent the current light
        light *L = lights[li];

        // shadows
        if (inShadow(P, N, L, objects, objCount))
            continue;

        // light direction
        float Ldir[3];
        v3sub(Ldir, L->position, P); // subtract
        float d = v3len(Ldir); // length
        normalize3(Ldir); // normalize

        // radial attenuation
        float denom = (L->radial_a0 + L->radial_a1 * d + L->radial_a2 * d * d);
        float frad = 1.0f;
        if (std::fabs(denom) > 1e-6f)
            frad = 1.0f / denom;

        // angular attenuation (spot lights only)
        float fang = 1.0f;
        if (L->theta > 0.0f)
        {
            // spotlight direction
            float spotDir[3] = {L->direction[0], L->direction[1], L->direction[2]};
            normalize3(spotDir); // normalize

            // cosine
            float cosAngle = dot3(spotDir, Ldir);
            // convert 
            float cosTheta = std::cos(L->theta * (3.14159265f / 180.0f));

            if (cosAngle <= cosTheta)
                fang = 0.0f; // outside of cone
            else
                fang = std::pow(cosAngle, L->angular_a0);
        }

        // diffuse
        float ndotl = dot3(N, Ldir);
        if (ndotl < 0.0f)
            ndotl = 0.0f;

        float scale = frad * fang;

        // color rendered at the location
        if(hitObj->type == OBJ_SPHERE)
        {
            sphere *spherePtr = static_cast<sphere *>(hitObj);
            float diffColor[3] = { hitObj->c_diff[0], hitObj->c_diff[1], hitObj->c_diff[2] };

            // sphere texture mapping
        if (spherePtr->texture[0] != '\0' && spherePtr->pixmap != nullptr)
        {
            float nx = N[0];
            float ny = N[1];
            float nz = N[2];

            const float PI = 3.14159265f;
            float theta = std::atan2(nz, nx);
            if (theta < 0.0f)
                theta += 2.0f * PI;
            float phi = std::acos(ny);

            float u = theta / (2.0f * PI);
            float v = phi / PI;

            if (u < 0.0f) u = 0.0f;
            if (u > 1.0f) u = 1.0f;
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;

            int texX = static_cast<int>(u * (spherePtr->texWidth - 1));
            int texY = static_cast<int>(v * (spherePtr->texHeight - 1));

            if (texX < 0) texX = 0;
            if (texX >= spherePtr->texWidth) texX = spherePtr->texWidth - 1;
            if (texY < 0) texY = 0;
            if (texY >= spherePtr->texHeight) texY = spherePtr->texHeight - 1;

            int idx = (texY * spherePtr->texWidth + texX) * 3;
            float invMax = (spherePtr->texMax > 0) ? (1.0f / spherePtr->texMax) : (1.0f / 255.0f);

            diffColor[0] = (float)spherePtr->pixmap[idx] * invMax;
            diffColor[1] = (float)spherePtr->pixmap[idx + 1] * invMax;
            diffColor[2] = (float)spherePtr->pixmap[idx + 2] * invMax;
        }

            outColor[0] += diffColor[0] * L->color[0] * ndotl * scale;
            outColor[1] += diffColor[1] * L->color[1] * ndotl * scale;
            outColor[2] += diffColor[2] * L->color[2] * ndotl * scale;
        }
        else{
            outColor[0] += hitObj->c_diff[0] * L->color[0] * ndotl * scale;
            outColor[1] += hitObj->c_diff[1] * L->color[1] * ndotl * scale;
            outColor[2] += hitObj->c_diff[2] * L->color[2] * ndotl * scale;
        }

        float minusL[3] = {-Ldir[0], -Ldir[1], -Ldir[2]};
        float dRN = dot3(minusL, N);

        float R[3] = {
            minusL[0] - 2.0f * dRN * N[0],
            minusL[1] - 2.0f * dRN * N[1],
            minusL[2] - 2.0f * dRN * N[2]};
        normalize3(R);

        float rdotv = dot3(R, V);
        if (rdotv < 0.0f)
            rdotv = 0.0f;

        float spec = std::pow(rdotv, 20.0f);

        // specular at that location
        outColor[0] += hitObj->c_spec[0] * L->color[0] * spec * scale;
        outColor[1] += hitObj->c_spec[1] * L->color[1] * spec * scale;
        outColor[2] += hitObj->c_spec[2] * L->color[2] * spec * scale;
    }

    // add color to output
    color[0] += outColor[0];
    color[1] += outColor[1];
    color[2] += outColor[2];

    // Reflections
    if (hitObj->reflection > 0.0f && depth < raycastLimit) 
    {
        // reflected ray direction
        float rdDotN = dot3(Rd, N);
        float reflectedRd[3] = {
            Rd[0] - 2.0f * rdDotN * N[0],
            Rd[1] - 2.0f * rdDotN * N[1],
            Rd[2] - 2.0f * rdDotN * N[2]};
        normalize3(reflectedRd);

        // recurse for reflected color
        float reflectedColor[3];
        float reflectedOrigin[3] = {
        P[0] + N[0] * 1e-4f,
        P[1] + N[1] * 1e-4f,
        P[2] + N[2] * 1e-4f
    };

traceRay(reflectedColor, reflectedOrigin, reflectedRd, depth + 1, objects, lights, objCount, lightCount, raycastLimit);

        // add reflected color 
        color[0] += reflectedColor[0] * hitObj->reflection;
        color[1] += reflectedColor[1] * hitObj->reflection;
        color[2] += reflectedColor[2] * hitObj->reflection;
    }
}

void render(uint8_t *pix, int Wid, int Height, float Ro[3], sceneData **objects, light **lights, int objCount, int lightCount, sceneData camera, int raycastLimit){

    std::printf("Rendering in progress . . .\n\n");
    // render scene
    for (int j = 0; j < Height; j++){ // image height and width
        for (int i = 0; i < Wid; i++){

            // rd 
            float x = (i + 0.5f) / (float)Wid;
            float y = (j + 0.5f) / (float)Height;

            float Rd[3];
            Rd[0] = (-camera.cam_width * 0.5f) + x * camera.cam_width;
            Rd[1] = (camera.cam_height * 0.5f) - y * camera.cam_height;
            Rd[2] = -1.0f;

            normalize3(Rd);

            float color[3];
            traceRay(color, Ro, Rd, 0, objects, lights, objCount, lightCount, raycastLimit);

            

            int idx = (j * Wid + i) * 3;
            pix[idx + 0] = toByte(color[0]);
            pix[idx + 1] = toByte(color[1]);
            pix[idx + 2] = toByte(color[2]);
        }
    }

    std::printf("Scene Rendered\n\n");

}
void makeAnaglyph(uint8_t *out, uint8_t *left, uint8_t *right, int Wid, int Height)
{
    for (int i = 0; i < Wid * Height; i++)
    {
        int idx = i * 3;

        out[idx + 0] = left[idx + 0];     // red from left eye
        out[idx + 1] = right[idx + 1];    // green from right eye
        out[idx + 2] = right[idx + 2];    // blue from right eye
    }
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        std::printf("Usage: raycast width height input.scene output.ppm\n\n");
        return 1;
    }

    int raycastLimit = 1;
    int Wid = std::atoi(argv[1]);
    int Height = std::atoi(argv[2]);

    sceneData **objects = new sceneData *[128];
    light **lights = new light *[128];
    int objCount = 0;
    int lightCount = 0;
    sceneData camera;

    if (!readScene(argv[3], lights, objects, &camera, &objCount, &lightCount))
    {
        return 1;
    }

    if (camera.cam_width == 0.0f)
        camera.cam_width = 1.0f;
    if (camera.cam_height == 0.0f)
        camera.cam_height = 1.0f;

    uint8_t *leftPix = new uint8_t[Wid * Height * 3];
    uint8_t *rightPix = new uint8_t[Wid * Height * 3];
    uint8_t *anaglyphPix = new uint8_t[Wid * Height * 3];

    float eyeSep = 0.08f;

    float leftRo[3] = {-eyeSep / 2.0f, 0.0f, 0.0f};
    float rightRo[3] = {eyeSep / 2.0f, 0.0f, 0.0f};

    render(leftPix, Wid, Height, leftRo, objects, lights, objCount, lightCount, camera, raycastLimit);
    render(rightPix, Wid, Height, rightRo, objects, lights, objCount, lightCount, camera, raycastLimit);

    makeAnaglyph(anaglyphPix, leftPix, rightPix, Wid, Height);

    if (!writeppm(argv[4], Wid, Height, anaglyphPix))
    {
        std::cerr << "Error: Could not write output file " << argv[4] << "\n";
        delete[] leftPix;
        delete[] rightPix;
        delete[] anaglyphPix;
        delete[] objects;
        delete[] lights;
        return 1;
    }

    std::printf("Wrote anaglyph 3D scene to file %s\n\n", argv[4]);

    delete[] leftPix;
    delete[] rightPix;
    delete[] anaglyphPix;

    for (int s = 0; s < objCount; s++)
    {
        if (objects[s]->type == OBJ_SPHERE)
        {
            sphere *spherePtr = static_cast<sphere *>(objects[s]);
            delete[] spherePtr->pixmap;
        }
        delete objects[s];
    }

    delete[] objects;

    for (int i = 0; i < lightCount; i++)
        delete lights[i];

    delete[] lights;

    return 0;
}