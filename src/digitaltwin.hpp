#ifndef DIGITALTWIN_HPP
#define DIGITALTWIN_HPP

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
    float* depth_pixels;
    int width, height;
};

struct TextureReal
{
    unsigned char* rgb_pixels;
    float* depth_pixels;
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
    void save();
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
    ActiveObject* add(string kind,string base,Vec3 pos,Vec3 rot,float scale);
    void remove(string name);
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
    void set_user_data(string value);
    string get_user_data();

    Scene* scene;
    string name,kind,base;
    Vec3 pos,rot;
    string user_data;
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
    float speed;
    vector<float> current_joint_poses,home_joint_poses;
    Vec3 end_effector_pos,end_effector_rot;
};

struct Camera3D : public ActiveObject
{
    Camera3D(Scene* sp,string properties);
    virtual ~Camera3D() {}
    const Texture rtt();
    void set_rtt_func(std::function<void(vector<unsigned char>,vector<float>,int,int)> slot);  //获取虚拟相机数据，RGB，Depth
    std::function<void(vector<unsigned char>,vector<float>,int,int)> slot_rtt;
    void clear();
    
    vector<unsigned char> rgba_pixels;
    vector<float> depth_pixels;
    int image_size[2],fov;
    double forcal;
    
    thread rtt_proxy;
    bool rtt_proxy_running;
};

struct Camera3DReal : public ActiveObject
{
    Camera3DReal(Scene* sp,string properties);
    virtual ~Camera3DReal();
    const TextureReal rtt();
    void set_rtt_func(std::function<bool(vector<unsigned char>&,vector<float>&,int&,int&)> slot);  //获取真实相机数据，点云，RGB，Depth
    void set_calibration(string projection_transform,string eye_to_hand_transform); //相机内参，手眼标定矩阵
    void clear();

    std::function<bool(vector<unsigned char>&,vector<float>&,int&,int&)> slot_rtt;
    vector<unsigned char> rgb_pixels;
    vector<float> depth_pixels;
    int image_size[2];

    thread rtt_proxy;
    bool rtt_proxy_running;
};

struct Placer : public ActiveObject
{
    Placer(Scene* sp,string properties);
    virtual ~Placer() {}
    string get_workpiece() { return ""; }
    void set_workpiece(string base);
    string get_workpiece_texture() { return ""; }
    void set_workpiece_texture(string texture);
    Vec3 get_center();
    void set_center(Vec3 pos);
    int get_amount() { return 0; }
    void set_amount(int num);
    void get_layout(int& x,int& y,int& z);
    void set_layout(int x,int y,int z);
    float get_interval();
    void set_interval(float seconds);
    void get_scale_factor(float& max,float& min) {}
    void set_scale_factor(float max,float min);
    string get_place_mode() { return ""; }
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
    ~Workflow();
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
    
    struct Plugin;
    Plugin* md;
};

}

#endif