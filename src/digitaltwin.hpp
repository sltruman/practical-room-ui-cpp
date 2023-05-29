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

namespace digitaltwin
{
using namespace std;

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
    Scene(int width,int height,string backend_path="./digitaltwin",string data_dir_path="./data");
    ~Scene();
    string get_data_dir_path();
    string get_backend_path();
    int load(string scene_path); //0：正常 1：后端启动失败;
    int save();
    int rtt(Texture& t);
    int play(bool run);
    int rotate(double x,double y);  
    int rotete_left();
    int rotete_right();
    int rotete_top();
    int rotete_front();
    int rotete_back();
    int pan(double x,double y);
    int zoom(double factor);
    int draw_origin(bool enable);
    int draw_collision(bool enable);
    int set_ground_z(float z);
    int set_ground_texture(string image_path);
    float get_ground_z();
    string get_ground_texture();
    map<string,ActiveObject*> get_active_objs();
    void set_log_func(std::function<void(char,string)> slot);
    int set_user_data(string value);
    string get_user_data();


private:
    void sync_profile();
    void sync_active_objs();

protected:
    struct Plugin;
    Plugin* md;
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
    virtual ~Editor();
    int ray(double x,double y,RayInfo& info);
    int select(string name);
    int add(string kind,string base,Vec3 pos,Vec3 rot,Vec3 scale,string& name);
    int add_box(Vec3 pos,Vec3 rot,Vec3 size,int thickness,string& name);
    int add_cube(Vec3 pos,Vec3 rot,Vec3 size,string& name);
    int add_cylinder(Vec3 pos,Vec3 rot,int radius,int length,string& name);
    int remove(string name);
    int set_relation(string parent,string child);
    list<Relation> get_relations();

protected:
    Scene* scene;

    struct Plugin;
    Plugin* md;
};

struct ActiveObject
{
    ActiveObject(Scene* sp,string properties);
    virtual ~ActiveObject();
    Scene* get_own_scene();
    int set_name(string name);
    string get_name();
    int set_base(string path);
    string get_base();
    int set_pos(Vec3 pos);
    Vec3 get_pos();
    int set_rot(Vec3 rot);
    Vec3 get_rot();
    int set_scale(Vec3 scale);
    Vec3 get_scale();
    int set_transparence(float value);
    float get_transparence();
    string get_kind();
    int set_user_data(string value);
    string get_user_data();

    struct Plugin;
    Plugin* md;

    Scene* scene;
    string name,kind,base;
    Vec3 pos,rot,scale;
    string user_data;
};

struct Robot : public ActiveObject
{
    Robot(Scene* sp,string properties);
    virtual ~Robot() {}

    int set_end_effector(string path);
    string get_end_effector();
    int digital_output(bool pickup);
    bool is_picking();
    vector<float> get_joints();
    int set_joints(vector<float> vals);
    int get_joints_num();
    int set_joint_position(int joint_index,float value);
    float get_joint_position(int joint_index);
    int set_end_effector_pos(Vec3 pos);
    Vec3 get_end_effector_pos();
    int set_end_effector_rot(Vec3 rot);
    Vec3 get_end_effector_rot();
    int set_speed(float value);
    float get_speed();
    void set_home();
    int home();
    int track(bool enable);

    int end_effector_id;
    string end_effector;
    float speed;
    vector<float> current_joint_poses,home_joint_poses;
    Vec3 end_effector_pos,end_effector_rot;
};

struct Camera3D : public ActiveObject
{
    Camera3D(Scene* sp,string properties);
    virtual ~Camera3D();
    int rtt(Texture&);
    void set_rtt_func(std::function<void(vector<unsigned char>,vector<float>,int,int)> slot);  //获取虚拟相机数据，RGB，Depth
    std::function<void(vector<unsigned char>,vector<float>,int,int)> slot_rtt;
    int clear();
    void draw_fov(bool show=true);
    void draw_roi(bool show=true);
    int set_roi(Vec3 pos,Vec3 rot,Vec3 size);
    void get_roi(Vec3& pos,Vec3& rot,Vec3& size);
    int set_fov(float fov);
    float get_fov();
    int set_focal(float focal);
    float get_focal();

    string get_intrinsics();
    
    vector<unsigned char> rgba_pixels;
    vector<float> depth_pixels;
    int pixels_w,pixels_h;
    float fov,focal;
    Vec3 roi_pos,roi_rot,roi_size;
    
    thread rtt_proxy;
    bool rtt_proxy_running;
};

struct Camera3DReal : public ActiveObject
{
    Camera3DReal(Scene* sp,string properties);
    virtual ~Camera3DReal();
    int rtt(TextureReal& t);
    void set_rtt_func(std::function<bool(vector<unsigned char>&,vector<float>&,int&,int&)> slot);  //获取真实相机数据，点云，RGB，Depth
    int set_calibration(string projection_transform,string eye_to_hand_transform); //相机内参，手眼标定矩阵
    int clear();

    std::function<bool(vector<unsigned char>&,vector<float>&,int&,int&)> slot_rtt;
    vector<unsigned char> rgb_pixels;
    vector<float> depth_pixels;
    int pixels_w,pixels_h;
    float fov,focal;

    thread rtt_proxy;
    bool rtt_proxy_running;
};

struct Placer : public ActiveObject
{
    Placer(Scene* sp,string properties);
    virtual ~Placer() {}
    string get_workpiece();
    int set_workpiece(string base);
    string get_workpiece_texture();
    int set_workpiece_texture(string texture);
    Vec3 get_center();
    int set_center(Vec3 pos);
    int get_amount() { return 0; }
    int set_amount(int num);
    void get_layout(int& x,int& y,int& z);
    int set_layout(int x,int y,int z);
    float get_interval();
    int set_interval(float seconds);
    void get_scale_factor(float& max,float& min) {}
    int set_scale_factor(float max,float min);
    string get_place_mode() { return ""; }
    int set_place_mode(string place_mode); // random：随机的 tidy：整齐的 layout：整齐的xyz布局
    
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
    
    Vec3 center;
};

class Workflow
{
public:
    Workflow(Scene* sp);
    ~Workflow();
    void add_active_obj_node(string kind,string name,string f,std::function<string()> slot);
    string get_active_obj_nodes();
    int set(string info);
    string get();
    int start();
    int stop();
    
protected:
    Scene* scene;
    list<thread> proxy_nodes;
    bool proxy_nodes_running;
    
    struct Plugin;
    Plugin* md;
};

}

#endif