#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <list>
using namespace std;

namespace digitaltwin
{

struct Texture
{
    unsigned char* rgba_pixels;
    int width, height, size;
};

class Scene
{
public:
    Scene(int width,int height,string scene_path);
    ~Scene();
    void load(string scene_path);
    void save();
    const Texture rtt();
    void start();
    void stop();
    void rotate(double x,double y);
    void pan(double x,double y);
    void zoom(double factor);
protected:
    struct Plugin;
    Plugin* md;

friend class Editor;
friend class ActiveObject;
friend class Robot;
friend class Camera3D;

};

class ActiveObject
{
public:
    ActiveObject(Scene* sp,string properties);
    virtual ~ActiveObject() {}
    virtual void set_base(string path);

    int id;
    string kind,base;
    double x,y,z,roll,pitch,yaw;

protected:
    Scene* scene;
};

class Robot : public ActiveObject
{
public:
    Robot(Scene* sp,string properties);
    virtual ~Robot() {}

    int end_effector_id;
    string end_effector;
    void set_end_effector(string path);
};

class Camera3D : public ActiveObject
{
public:
    Camera3D(Scene* sp,string properties);
    virtual ~Camera3D() {}
    const Texture rtt();

    vector<unsigned char> rgba_pixels;
    int viewport_size[2];
};

class Packer : public ActiveObject
{
public:
    Packer(Scene* sp,string properties);
    virtual ~Packer() {}
    
};

struct RayInfo { int id; double pos[3];};
class Editor
{
public:
    Editor(Scene* sp);
    RayInfo ray(double x,double y);
    void move(int id,double pos[3]);
    ActiveObject* select(int id);

public:
    Scene* scene;

protected:
    struct Plugin;
    Plugin* md;
    map<int,ActiveObject*> active_objs;
};


}