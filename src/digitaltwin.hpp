#include <string>
#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <array>
#include <functional>
#include <thread>
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
friend class Camera3DReal;
friend class Workflow;

public:
    Scene(int width,int height,string scene_path);
    ~Scene();
    void load(string scene_path);
    const Texture rtt();
    void play(bool run);
    void rotate(double x,double y);
    void rotete_left();
    void rotete_right();
    void rotete_top();
    void rotete_front();
    void rotete_back();
    void pan(double x,double y);
    void zoom(double factor);
    void draw_origin(bool enable);
    void draw_collision(bool enable);

    map<string,ActiveObject*> get_active_objs();
    void set_logging(std::function<void(string)> log_callback);

protected:
    struct Plugin;
    Plugin* md;
    map<string,ActiveObject*> active_objs_by_name;
};

struct RayInfo { string name; double pos[3];};
struct Relation {
    string name;
    list<Relation> children;
};

class Editor
{
public:
    Editor(Scene* sp);
    RayInfo ray(double x,double y);
    ActiveObject* select(string name);
    ActiveObject* add(string base,Vec3 pos,Vec3 rot,Vec3 scale);
    void remove(string name);
    void save();
    void set_relation(string parent,string child);
    list<Relation> get_relations();

protected:
    struct Plugin;
    Plugin* md;
    Scene* scene;
};

struct ActiveObject
{
    ActiveObject(Scene* sp,string properties);
    virtual ~ActiveObject() {}
    bool set_name(string name);
    string get_name();
    void set_base(string path);
    string get_base();
    void set_pos(Vec3 pos);
    Vec3 get_pos();
    void set_rot(Vec3 rot);
    Vec3 get_rot();
    void set_scale(Vec3 scale);
    Vec3 get_scale();
    void set_transparence(float value);
    float get_transparence();
    string get_kind();

    Scene* scene;
    string name,kind,base;
    Vec3 pos,rot;
};

struct Robot : public ActiveObject
{
    Robot(Scene* sp,string properties);
    virtual ~Robot() {}
    void set_end_effector(string path);
    string get_end_effector();
    void digital_output(bool pickup);
    int get_joints_num();
    void set_joint_position(int joint_index,float value);
    float get_joint_position(int joint_index);
    void set_end_effector_pos(Vec3 pos);
    Vec3 get_end_effector_pos();
    void set_end_effector_rot(Vec3 rot);
    Vec3 get_end_effector_rot();
    void set_speed(float value);
    float get_speed();
    void set_home();
    void home();
    void track(bool enable);

    int end_effector_id;
    string end_effector;
};

struct Camera3D : public ActiveObject
{
    Camera3D(Scene* sp,string properties);
    virtual ~Camera3D() {}
    const Texture rtt();
    
    vector<unsigned char> rgba_pixels,gray_pixels;
    vector<unsigned char> depth_pixels;
    int image_size[2],fov;
    double forcal;
};

struct Camera3DReal : public ActiveObject
{
    Camera3DReal(Scene* sp,string properties);
    virtual ~Camera3DReal() {rtt_proxy_running=false;}
    const Texture rtt();
    void set_rtt_func(std::function<bool(vector<unsigned char>&,vector<float>&,int&,int&)> slot);  //获取真实相机数据，点云，RGB，Depth
    void draw_point_cloud(string ply_path);
    void clear_point_cloud();
    void set_calibration(string projection_transform,string eye_to_hand_transform); //相机内参，手眼标定矩阵

    vector<unsigned char> rgba_pixels,gray_pixels;
    vector<unsigned char> depth_pixels;
    int image_size[2],fov;
    double forcal;
    thread rtt_proxy;
    bool rtt_proxy_running;
};

struct Placer : public ActiveObject
{
    Placer(Scene* sp,string properties);
    virtual ~Placer() {}
    string get_workpiece();
    void set_workpiece(string base);
    string get_workpiece_texture();
    void set_workpiece_texture(string texture);
    Vec3 get_center();
    void set_center(Vec3 pos);
    int get_amount();
    void set_amount(int num);
    void get_layout(int& x,int& y,int& z);
    void set_layout(int x,int y,int z);
    float get_interval();
    void set_interval(float seconds);
    void get_scale_factor(float& max,float& min);
    void set_scale_factor(float max,float min);
    string get_place_mode();
    void set_place_mode(string place_mode); // random：随机的 tidy：整齐的 layout：整齐的xyz布局
    
    string workpiece,workpiece_texture;
    Vec3 center,scale_factor;
    int interval,amount;
    string place_mode;
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
    ~Workflow() {proxy_nodes_running = false;}
    void add_active_obj_node(string kind,string name,string f,std::function<string()> slot);
    string get_active_obj_nodes();
    void set(string info);
    string get();
    void start();
    void stop();
    
protected:
    Scene* scene;
    list<thread> proxy_nodes;
    bool proxy_nodes_running;
};

}