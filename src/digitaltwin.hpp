#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <array>
using namespace std;

namespace digitaltwin
{

typedef std::array<double,3> Vec3;

struct Texture
{
    unsigned char* rgba_pixels;
    unsigned char* depth_pixels;
    int width, height;
};

class ActiveObject;

class Scene
{
friend class Editor;
friend class ActiveObject;
friend class Placer;
friend class Robot;
friend class Camera3D;
friend class Workflow;

public:
    Scene(int width,int height,string scene_path);
    ~Scene();
    void load(string scene_path);
    const Texture rtt();
    void play(bool run);
    void rotate(double x,double y);
    void pan(double x,double y);
    void zoom(double factor);
    map<string,ActiveObject*> get_active_objs();
protected:
    struct Plugin;
    Plugin* md;
    map<string,ActiveObject*> active_objs_by_name;
};

struct RayInfo { string name; double pos[3];};
class Editor
{
public:
    Editor(Scene* sp);
    RayInfo ray(double x,double y);
    void move(string name,Vec3 pos);
    ActiveObject* select(string name);
    void add(string base,Vec3 pos,Vec3 rot,Vec3 scale);
    void remove(string name);
    void set_parent(string parent_name,string child_name);
    void transparentize(string name,float value);
    void save();

public:
    Scene* scene;

protected:
    struct Plugin;
    Plugin* md;
};

class ActiveObject
{
public:
    ActiveObject(Scene* sp,string properties);
    virtual ~ActiveObject() {}
    bool set_name(string name);
    virtual void set_base(string path);
    string name,kind,base;
    Vec3 pos,rot;

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
    void digital_output(bool pickup);
};

class Camera3D : public ActiveObject
{
public:
    Camera3D(Scene* sp,string properties);
    virtual ~Camera3D() {}
    const Texture rtt();

    vector<unsigned char> rgba_pixels;
    vector<unsigned char> depth_pixels;
    int image_size[2],fov;
    double forcal;
};

class Placer : public ActiveObject
{
public:
    Placer(Scene* sp,string properties);
    virtual ~Placer() {}
    void set_workpiece(string base);
    void set_workpiece_texture(string texture);
    void set_center(Vec3 pos);
    void set_amount(int num);
    void set_interval(float seconds);

    string workpiece;
    Vec3 center;
    int interval,amount;
};

class Stacker : public ActiveObject
{
public:
    Stacker(Scene* sp,string properties);
    virtual ~Stacker() {}
};

class Workflow
{
public:
    Workflow(Scene* sp);
    string get_active_obj_nodes();
    void set(string info);
    string get();
    void start();
    void stop();

public:
    Scene* scene;
};

}