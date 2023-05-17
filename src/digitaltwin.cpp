#include "digitaltwin.hpp"

#include <memory>
#include <fstream>
#include <stdexcept>
using namespace std;

#include "json.h"
#include <boost/asio.hpp>
#include <boost/process.hpp>
using namespace boost;
using json = nlohmann::json;

namespace digitaltwin {

struct Scene::Plugin 
{
    Plugin(): socket(io_context)
    {}

    asio::io_context io_context;
    asio::local::stream_protocol::socket socket;
    boost::filesystem::path tmp_dir_path,backend_path,data_dir_path;
    
    int width,height;
    vector<unsigned char> rgba_pixels;
    
    std::shared_ptr<process::child> backend_process;
    process::ipstream backend_out,backend_err;

    std::function<void(char,string)> slot_log;
    thread logging,rendering;

    string scene_path;
    json profile;
    map<string,ActiveObject*> active_objs_by_name;
};

Scene::Scene(int width,int height,string backend_path,string data_dir_path)
{
    md = new Plugin;
    md->rgba_pixels.resize(width * height * 4);
    md->height = height,md->width = width;
    md->backend_path = backend_path;
    md->data_dir_path = data_dir_path;
}

Scene::~Scene()
{
    md->socket.close();
    for(auto kv : md->active_objs_by_name) delete kv.second;

    if(md->backend_process) {
        md->backend_process->terminate();
        md->backend_process->join();
    }

    md->logging.join();
    
    delete md;
}

string Scene::get_data_dir_path()
{
    return md->data_dir_path.string();
}

string Scene::get_backend_path()
{
    return md->backend_path.string();
}

int Scene::load(string scene_path) {
    if(scene_path.empty()) return 1;
    if(scene_path == md->scene_path) return 2;
   
    try {
        cout << "Starting digital-twin service...";
        if(md->backend_process) {
            md->backend_process->terminate();
            md->backend_process->wait();
        }

        md->tmp_dir_path = boost::filesystem::temp_directory_path().append("digitaltwin");
        
        stringstream ss;
        ss << md->backend_path << ' '
           << scene_path << ' '
           << md->width << ' '
           << md->height << ' '
           << md->data_dir_path << ' '
           << md->tmp_dir_path;

        cout << "succeded" << endl
             << "command:" << ss.str() << endl;

        auto socket_name = boost::filesystem::basename(scene_path) + ".json.sock";
        auto socket_path = md->tmp_dir_path / socket_name;
        
        auto backend_out = process::std_out > md->backend_out;
        md->backend_process = make_shared<process::child>(ss.str(),backend_out);

        for(int i=0;;i++) {
            cout << "Connecting to digital-twin...";
            md->backend_process->wait_for(chrono::milliseconds(500));
           
            system::error_code ec;
            if (md->socket.connect(asio::local::stream_protocol::endpoint(socket_path.c_str()),ec)) {
                cerr << "failed" << endl;
                cerr << ec.message() << endl;
            } else break;
        }

        cout << "succeded" << endl;
        goto SUCCEDED;
    } catch(system::system_error& e) {
        cerr << "failed" << endl;
        cerr << e.what() << endl;
    }
    
FAILED:
    md->socket.close();
    return -3;

SUCCEDED:
    md->logging = thread([this]() {
        cout << "Logging has start..." << endl;
        
        while (md->backend_process->running() && md->backend_out) {
            string line;
            getline(md->backend_out,line);
            if(!line.empty() && md->slot_log) md->slot_log('D',line);
            else cout << line << endl;
        }

        cout << "Logging is finished." << endl;
    });

    sync_profile();
    return 0;
}

int Scene::rtt(Texture& t) {
    stringstream req;
    req << "scene.rtt()" << endl;
    asio::write(md->socket,asio::buffer(req.str()));
    asio::read(md->socket,asio::buffer(md->rgba_pixels));

    t.width = md->width;
    t.height = md->height;
    t.rgba_pixels = md->rgba_pixels.data();
    return 0;
}

int Scene::play(bool run)
{
    stringstream req;
    req << "scene.play(" << (run ? "True" : "False") << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

int Scene::rotate(double x,double y) 
{
    stringstream req;
    req << "scene.rotate(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

int Scene::rotete_left() 
{
    stringstream req;
    req << "scene.rotate_left()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

int Scene::rotete_right() 
{
    stringstream req;
    req << "scene.rotate_right()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

int Scene::rotete_top()
{
    stringstream req;
    req << "scene.rotate_top()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

int Scene::rotete_front()
{
    stringstream req;
    req << "scene.rotate_front()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

int Scene::rotete_back()
{
    stringstream req;
    req << "scene.rotate_back()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

int Scene::pan(double x,double y)
{
    stringstream req;
    req << "scene.pan(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

int Scene::zoom(double factor)
{
    stringstream req;
    req << "scene.zoom("<< factor <<")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

void Scene::sync_profile()
{
    stringstream req;
    req << "scene.get_profile()" << endl;
    cout << req.str();

    asio::write(md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(md->socket, res,'\n');
    md->profile = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
}

void Scene::sync_active_objs()
{
    stringstream req;
    req << "scene.get_active_obj_properties()" << endl;
    cout << req.str() << endl;
    asio::write(md->socket,asio::buffer(req.str()));
    
    asio::streambuf res;
    asio::read_until(md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    cout << json_res.dump() << endl;

    for (auto i : json_res.items()) {
        auto name = i.key();
        auto properties = i.value();
        auto description = properties.dump();
        
        ActiveObject* obj = nullptr;
        auto it = md->active_objs_by_name.find(name);
        if(it != md->active_objs_by_name.end()) continue;

        auto kind = properties["kind"].get<string>();
        if(kind == "Robot") {
            obj = new Robot(this, description);
        } else if(kind == "Camera3D") {
            obj = new Camera3D(this, description);
        }  else if(kind == "Camera3DReal") {
            obj = new Camera3DReal(this, description);
        } else if(kind == "Placer") {
            obj = new Placer(this, description);
        } else if(kind == "Stacker") {
            obj = new Stacker(this, description);
        } else {
            obj = new ActiveObject(this, description);
        }

        md->active_objs_by_name[name] = obj;
    }
}

map<string,ActiveObject*> Scene::get_active_objs()
{
    if(md->active_objs_by_name.empty()) sync_active_objs();
    return md->active_objs_by_name;
}

int Scene::set_ground_z(float z) {
    md->profile["ground_z"] = z;
    stringstream req;
    req << "scene.set_ground_z("<<z<<")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

float Scene::get_ground_z() {
    return md->profile["ground_z"].get<float>();
}

int Scene::set_ground_texture(string image_path) 
{
    md->profile["ground_texture"] = image_path;
    stringstream req;
    req << "scene.set_ground_texture('"<<image_path<<"')" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
    return 0;
}

string Scene::get_ground_texture() 
{
    return md->profile["ground_texture"].get<string>();
}

void Scene::set_log_func(std::function<void(char,string)> slot)
{
    md->slot_log = slot;
}

struct Editor::Plugin {


};

Editor::Editor(Scene* sp) : scene(sp)
{
    md = new Plugin;
}

Editor::~Editor() 
{
    delete md;
}

int Editor::ray(double x,double y,RayInfo& info)
{
    stringstream req;
    req << "editor.ray(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));

    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    cout << json_res.dump() << endl;
    
    auto pos = json_res["pos"];
    info = RayInfo {json_res["name"].get<string>(),{pos[0].get<double>(),pos[1].get<double>(),pos[2].get<double>()}};

    return 0;
}

int Editor::select(string name)
{
    stringstream req;
    req << "editor.select('"<<name<<"')" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    return 0;
}

int Editor::add(string kind,string base,Vec3 pos,Vec3 rot,Vec3 scale,string& name)
{
    stringstream req;
    req << "editor.add('"<<kind<<"','"<<base<<"',[" << pos[0]<<","<<pos[0]<<","<<pos[2]<< "],[" << rot[0]<<","<<rot[1]<<","<<rot[2]<< "],"<<scale[0]<<")" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    cout << json_res.dump() << endl;
    name = json_res["name"].get<string>();
    
    scene->sync_active_objs();
    return 0;
}

int Editor::remove(string name) 
{
    auto p = scene->md->active_objs_by_name[name];
    scene->md->active_objs_by_name.erase(name);
    delete p;

    stringstream req;
    req << "editor.remove('"<<name<<"')" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    return 0;
}

int Editor::set_relation(string parent,string child)
{
    return 0;
}

list<Relation> Editor::get_relations() 
{
    return {};
}

int Scene::save() 
{
    stringstream req;
    req << "scene.save()" << endl;
    asio::write(md->socket,asio::buffer(req.str()));
    return 0;
}

struct ActiveObject::Plugin 
{
    json profile;
};

ActiveObject::ActiveObject(Scene* sp, string properties) : scene(sp)
{
    md = new Plugin;

    auto json_properties = json::parse(properties);
    md->profile = json_properties;

    kind = json_properties["kind"].get<string>().c_str();
    name = json_properties["name"].get<string>().c_str();
    base = json_properties["base"].get<string>().c_str();
    pos = json_properties["pos"].get<Vec3>();
    rot = json_properties["rot"].get<Vec3>();
    scale = json_properties["scale"].get<Vec3>();

    if(json_properties.contains("user_data")) {
        user_data = json_properties["user_data"].get<string>().c_str();
    }
}

ActiveObject::~ActiveObject() {
    delete md;
}

Scene* ActiveObject::get_own_scene()
{
    return scene;
}

int ActiveObject::set_name(string new_name)
{
    auto it = scene->md->active_objs_by_name.find(new_name);
    if(scene->md->active_objs_by_name.end() != it) return 1;
    
    stringstream req;
    req << "editor.rename('"<<this->name<<"','"<<new_name<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    
    scene->md->active_objs_by_name.erase(name);
    scene->md->active_objs_by_name[new_name] = this;
    name = new_name;
    return 0;
}

string ActiveObject::get_name()
{
    return name;
}

int ActiveObject::set_base(string path)
{
    base = path;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_base('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    return 0;
}

string ActiveObject::get_base() { return this->base; }

int ActiveObject::set_pos(Vec3 pos)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_pos([" << pos[0] << ',' << pos[1] << ',' << pos[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    this->pos = pos;
    return 0;
}

Vec3 ActiveObject::get_pos()
{
    return pos;
}

int ActiveObject::set_rot(Vec3 rot)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_rot([" << rot[0] << ',' << rot[1] << ',' << rot[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    this->rot = rot;
    return 0;
}

Vec3 ActiveObject::get_rot()
{
    return rot;
}

int ActiveObject::set_scale(Vec3 scale)
{
    this->scale = scale;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_scale([" << rot[0] << ',' << rot[1] << ',' << rot[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    return 0;
}

Vec3 ActiveObject::get_scale()
{
    return this->scale;
}

int ActiveObject::set_transparence(float value)
{
    return 0;
}

float ActiveObject::get_transparence() 
{
    return 1.0 - 0.0;
}

string ActiveObject::get_kind()
{
    return kind;
}

int ActiveObject::set_user_data(string value)
{
    user_data = value;

    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_user_data('" << value << "')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    return 0;
}

string ActiveObject::get_user_data()
{
    return user_data;
}

Robot::Robot(Scene* sp, string properties) : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    end_effector = json_properties["end_effector"].get<string>();
    speed = json_properties["speed"].get<float>();
    end_effector_pos = {0,0,0};
    end_effector_rot = {0,0,0};
    
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].get_joints()"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    current_joint_poses.clear();
    for(auto& item : json_res) current_joint_poses.push_back(item);
    home_joint_poses = current_joint_poses;
}

int Robot::set_end_effector(string path)
{
    end_effector = path;
    stringstream req,req2;
    req << "scene.active_objs_by_name['"<<name<<"'].set_end_effector('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));

    req2 << "scene.active_objs_by_name['"<<name<<"'].get_joints()"<<endl;
    cout << req2.str();
    asio::write(scene->md->socket,  asio::buffer(req2.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    current_joint_poses.clear();
    for(auto& item : json_res) current_joint_poses.push_back(item);
    home_joint_poses = current_joint_poses;
    return 0;
}

string Robot::get_end_effector() { return end_effector; }

int Robot::digital_output(bool pickup) 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].end_effector_obj.do(" << (pickup ? "True" : "False")  << ")"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    return 0;
}

int Robot::get_joints_num()
{
    return current_joint_poses.size();
}

int Robot::set_joint_position(int joint_index,float value)
{
    current_joint_poses[joint_index] = value;

    stringstream req,args;
    for (auto i : current_joint_poses) args << i << ",";
    args.seekp(-1,ios::cur);
    args << ' ';
    req << "scene.active_objs_by_name['"<<name<<"'].set_joints(["<<args.str()<<"])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    return 0;
}

float Robot::get_joint_position(int joint_index)
{
    return current_joint_poses[joint_index];
}

int Robot::set_end_effector_pos(Vec3 pos)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_end_effector_pos([" << pos[0] << ',' << pos[0] << ',' << pos[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    end_effector_pos = pos;
    return 0;
}

Vec3 Robot::get_end_effector_pos()
{
    return end_effector_pos;
}

int Robot::set_end_effector_rot(Vec3 rot)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_end_effector_rot([" << rot[0] << ',' << rot[0] << ',' << rot[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    end_effector_rot = rot;
    return 0;
}

Vec3 Robot::get_end_effector_rot()
{
    return end_effector_rot;
}

void Robot::set_home()
{
    home_joint_poses = current_joint_poses;
}

int Robot::home()
{
    stringstream req,args;
    for (auto i : home_joint_poses) args << i << ",";
    args.seekp(-1,ios::cur);
    args << ' ';
    req << "scene.active_objs_by_name['"<<name<<"'].set_joints(["<<args.str()<<"])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    return 0;
}

int Robot::set_speed(float value)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_speed("<< value <<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    speed = value;
    return 0;
}

float Robot::get_speed()
{
    return speed;
}

int Robot::track(bool enable)
{
    return 0;
}

Camera3D::Camera3D(Scene* sp,string properties)  : ActiveObject(sp,properties)
    , rtt_proxy_running(false)
{
    auto json_properties = json::parse(properties);
    pixels_w = json_properties["pixels_w"].get<long long>();
    pixels_h = json_properties["pixels_h"].get<long long>();
    rgba_pixels.resize(pixels_w*pixels_h*4);
    depth_pixels.resize(pixels_w*pixels_h);
    fov = json_properties["fov"].get<float>();
    focal = json_properties["focal"].get<float>();
}

Camera3D::~Camera3D()
{
    if (rtt_proxy_running) {
        rtt_proxy_running=false;
        rtt_proxy.join();
    }
}

int Camera3D::rtt(Texture& t) {
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].rtt()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::read(scene->md->socket,asio::buffer(rgba_pixels));
    asio::read(scene->md->socket,asio::buffer(depth_pixels));

    t.width = pixels_w;
    t.height = pixels_h;
    t.rgba_pixels = rgba_pixels.data();
    t.depth_pixels = depth_pixels.data();
    return 0;
}

void Camera3D::set_rtt_func(std::function<void(vector<unsigned char>,vector<float>,int,int)> slot) { //获取虚拟相机数据，RGB，Depth
    slot_rtt = slot;
    rtt_proxy_running = true;
    rtt_proxy = thread([this]() {
        asio::io_context io_context;
        auto sock = (scene->md->tmp_dir_path / ("/" + name + ".sock")).string();
        cout << sock << endl;
        unlink(sock.c_str());
        asio::local::stream_protocol::endpoint ep(sock);
        asio::local::stream_protocol::acceptor acceptor(io_context, ep);
        asio::local::stream_protocol::socket socket(io_context);
        acceptor.non_blocking(true);

        while(rtt_proxy_running) {
            asio::local::stream_protocol::socket socket(io_context);
            system::error_code ec;
            acceptor.accept(socket,ec);
            
            if(ec == asio::error::would_block) {
                this_thread::sleep_for(chrono::seconds(1));
                continue;
            }

            asio::read(socket,asio::buffer(rgba_pixels));
            asio::read(socket,asio::buffer(depth_pixels));
            slot_rtt(rgba_pixels,depth_pixels,pixels_w,pixels_h);
        }

        acceptor.close();
    });
}  

int Camera3D::clear() 
{
    return 0;
}

int Camera3D::set_roi(Vec3 pos,Vec3 rot,Vec3 size)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_roi("
        << pos[0] << ',' << pos[1] << ',' << pos[2] << ',' 
        << rot[0] << ',' << rot[1] << ',' << rot[2] << ',' 
        << size[0] << ',' << size[1] << ',' << size[2] <<  ")" 
        << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

void Camera3D::get_roi(Vec3& pos,Vec3& rot,Vec3& size) 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].get_roi()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto roi = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    roi_pos = {roi[0].get<float>(),roi[1].get<float>(),roi[2].get<float>()};
    roi_rot = {roi[3].get<float>(),roi[4].get<float>(),roi[5].get<float>()};
    roi_size = {roi[6].get<float>(),roi[7].get<float>(),roi[8].get<float>()};
}

string Camera3D::get_intrinsics()
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].get_intrinsics()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    return string(asio::buffers_begin(res.data()),asio::buffers_end(res.data()));
}

Camera3DReal::Camera3DReal(Scene* sp,string properties)  : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    pixels_w = json_properties["pixels_w"].get<long long>();
    pixels_h = json_properties["pixels_h"].get<long long>();
    rgb_pixels.resize(pixels_w*pixels_h*3);
    depth_pixels.resize(pixels_w*pixels_h);
}

Camera3DReal::~Camera3DReal()
{
    rtt_proxy_running=false;
    rtt_proxy.join();
}

int Camera3DReal::rtt(TextureReal& t) {
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].rtt()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));

    t.width = pixels_w;
    t.height = pixels_h;
    t.rgb_pixels = rgb_pixels.data();
    t.depth_pixels = depth_pixels.data();
    return 0;
}

void Camera3DReal::set_rtt_func(std::function<bool(vector<unsigned char>&,vector<float>&,int&,int&)> slot) 
{
    slot_rtt = slot;
    rtt_proxy_running = true;
    
    rtt_proxy = thread([this]() {
        asio::io_context io_context;
        auto sock = (scene->md->tmp_dir_path / ("/" + name + ".sock")).string();
        cout << sock << endl;
        unlink(sock.c_str());
        asio::local::stream_protocol::endpoint ep(sock);
        asio::local::stream_protocol::acceptor acceptor(io_context, ep);
        asio::local::stream_protocol::socket socket(io_context);
        acceptor.non_blocking(true);

        while(rtt_proxy_running) {
            asio::local::stream_protocol::socket socket(io_context);
            system::error_code ec;
            acceptor.accept(socket,ec);
            
            if(ec == asio::error::would_block) {
                this_thread::sleep_for(chrono::seconds(1));
                continue;
            }
            
            if(slot_rtt(rgb_pixels,depth_pixels,pixels_w,pixels_h)) {
                asio::write(socket,asio::buffer(&pixels_w,4));
                asio::write(socket,asio::buffer(&pixels_h,4));
                asio::write(socket,asio::buffer(rgb_pixels));
                asio::write(socket,asio::buffer(depth_pixels));  
            } else {
                cout << "failed rtt" << endl;
            }
        }

        acceptor.close();
    });
}

int Camera3DReal::set_calibration(string projection_transform,string eye_to_hand_transform) 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_calibration("<<projection_transform<<","<<eye_to_hand_transform<<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

int Camera3DReal::clear() 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].clear_point_cloud()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

Placer::Placer(Scene* sp,string properties) : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    auto pos = json_properties["center"];
    copy(pos.begin(),pos.end(),center.begin());
    interval = json_properties["interval"].get<int>();
    amount = json_properties["amount"].get<int>();
    workpiece = json_properties["workpiece"].get<string>();
}

int Placer::set_workpiece(string base)
{
    workpiece = base;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_workpiece('"<<base<<"')" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

int Placer::set_workpiece_texture(string img_path)
{
    workpiece_texture = img_path;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_workpiece_texture('"<<img_path<<"')" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

int Placer::set_center(Vec3 pos)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_center([" << pos[0] << ',' << pos[0] << ',' << pos[2] << "])" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

int Placer::set_amount(int num)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_amount("<<num<<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

int Placer::set_interval(float seconds)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_interval("<<seconds<<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

int Placer::set_scale_factor(float max,float min) 
{
    return 0;
}

int Placer::set_place_mode(string place_mode)
{
return 0;
}

void Placer::get_layout(int& x,int& y,int& z)
{

}

int Placer::set_layout(int x,int y,int z)
{
return 0;    
}

Stacker::Stacker(Scene* sp,string properties) : ActiveObject(sp,properties) {
    auto json_properties = json::parse(properties);
    auto pos = json_properties["center"];
    copy(pos.begin(),pos.end(),center.begin());
}

Workflow::Workflow(Scene* sp) : scene(sp) {
    proxy_nodes_running = true;
}

Workflow::~Workflow() {
    proxy_nodes_running = false;
    for(auto& t : proxy_nodes) {
        t.join();
    }
}

void Workflow::add_active_obj_node(string kind,string name,string f,std::function<string()> slot) 
{
    proxy_nodes.emplace_back([this,kind,name,f,slot]() {
        auto sock = (scene->md->tmp_dir_path / ("/" + name + ".sock")).string();
        cout << sock << endl;
        unlink(sock.c_str());

        asio::io_context io_context;
        asio::local::stream_protocol::endpoint ep(sock);
        asio::local::stream_protocol::acceptor acceptor(io_context, ep);
        
        acceptor.non_blocking(true);

        while(proxy_nodes_running) {
            asio::local::stream_protocol::socket socket(io_context);
            system::error_code ec;
            acceptor.accept(socket,ec);

            if(ec == asio::error::would_block) {
                this_thread::sleep_for(chrono::seconds(1));
                continue;
            }
            
            auto pickposes = slot();
            if(pickposes.empty()) {
                cout << "empty pickposes" << endl;
            } else {
                stringstream req;
                req << pickposes << endl;
                asio::write(socket,asio::buffer(req.str()));
            }

            socket.close();
        }

        acceptor.close();
    });
}

string Workflow::get_active_obj_nodes() {
    stringstream req;
    req << "workflow.get_active_obj_nodes()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    return string(asio::buffers_begin(res.data()),asio::buffers_end(res.data()));
}

int Workflow::set(string src) {
    stringstream req;
    req << "workflow.set('" << src << "')" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

string Workflow::get() {
    stringstream req;
    req << "workflow.get()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    return string(asio::buffers_begin(res.data()),asio::buffers_end(res.data()));
}

int Workflow::start() {
    stringstream req;
    req << "workflow.start()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

int Workflow::stop() {
    stringstream req;
    req << "workflow.stop()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    return 0;
}

}