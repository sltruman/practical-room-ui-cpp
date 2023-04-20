#include "digitaltwin.hpp"

#include <memory>
#include <fstream>
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
    boost::filesystem::path tmp_path;
    
    int viewport_size[2];
    vector<unsigned char> rgba_pixels;
    std::shared_ptr<process::child> backend;
    process::ipstream backend_out,backend_err;
    std::function<void(char,string)> slot_log;
    json profile;
};

Scene::Scene(int width,int height,string scene_path)
{
    md = new Plugin;
    md->rgba_pixels.resize(width * height * 4);
    md->viewport_size[1] = height,md->viewport_size[0] = width;
    load(scene_path);
}

Scene::~Scene()
{
    md->socket.close();
    for(auto kv : active_objs_by_name) delete kv.second;
    logging.join();

    delete md;
}

void Scene::load(string scene_path) {
    if(scene_path.empty()) return;

    try {
        cout << "Starting digital-twin service...";
        if(md->backend) {
            md->backend->terminate();
            md->backend->wait();
        }

        md->tmp_path = boost::filesystem::temp_directory_path().append("digitaltwin");

        stringstream ss;
        ss << "digitaltwin" << ' '
           << scene_path << ' '
           << md->viewport_size[0] << ' ' 
           << md->viewport_size[1] << ' '
           << md->tmp_path;

        md->backend = make_shared<process::child>(ss.str(),process::std_out > md->backend_out,process::std_err > md->backend_err);
        cout << "succeded" << endl
             << "command:" << ss.str() << endl;

        logging = thread([this](){
            std::string line;
            cout << "Logging has start..." << endl;
            
            while (md->backend->running() && md->backend_out) {
                getline(md->backend_out,line);
                if(!line.empty() && md->slot_log) md->slot_log('D',line);
                md->backend->wait_for(chrono::microseconds(1000));
            }

            cout << "Logging is finished." << endl;
        });
        
        auto socket_name = boost::filesystem::basename(scene_path) + ".json.sock";
        auto socket_path = md->tmp_path / socket_name;
        
        for(int i=0;i<4;i++) {
            cout << "Connecting to digital-twin...";
            md->backend->wait_for(chrono::seconds(1));
            
            system::error_code ec;           
            if (md->socket.connect(asio::local::stream_protocol::endpoint(socket_path.c_str()),ec)) {
                cout << "failed" << endl;
                cerr << ec.message() << endl;
            } else break;

            if (i==3) throw ec;
        }

        cout << "succeded" << endl;

        goto SUCCEDED;
    } catch(system::system_error& e) {
        cout << "failed" << endl;
        cerr << e.what() << endl;
    }

FAILED:
    md->socket.close();
    throw std::runtime_error("Failed to contact digtial-twin backend.");

SUCCEDED:

    stringstream req;
    req << "scene.profile" << endl;
    cout << req.str();
    asio::write(md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(md->socket, res,'\n');
    md->profile = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
}

const Texture Scene::rtt() {
    stringstream req;
    req << "scene.rtt()" << endl;
    asio::write(md->socket,asio::buffer(req.str()));
    asio::read(md->socket,asio::buffer(md->rgba_pixels));
    
    Texture t;
    t.width = md->viewport_size[0];
    t.height = md->viewport_size[1];
    t.rgba_pixels = md->rgba_pixels.data();
    return t;
}

void Scene::play(bool run)
{
    stringstream req;
    req << "scene.play(" << (run ? "True" : "False") << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::rotate(double x,double y) 
{
    stringstream req;
    req << "scene.rotate(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::rotete_left() 
{
    stringstream req;
    req << "scene.rotate_left()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::rotete_right() 
{
    stringstream req;
    req << "scene.rotate_right()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::rotete_top()
{
    stringstream req;
    req << "scene.rotate_top()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::rotete_front()
{
    stringstream req;
    req << "scene.rotate_front()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::rotete_back()
{
    stringstream req;
    req << "scene.rotate_back()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::pan(double x,double y) 
{
    stringstream req;
    req << "scene.pan(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::zoom(double factor)
{
    stringstream req;
    req << "scene.zoom("<< factor <<")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

map<string,ActiveObject*> Scene::get_active_objs()
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
        auto it = active_objs_by_name.find(name);
        if(it != active_objs_by_name.end()) continue;

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

        active_objs_by_name[name] = obj;
    }

    return active_objs_by_name;
}

void Scene::set_ground_z(float z) {
    md->profile["ground_z"] = z;
    stringstream req;
    req << "scene.set_ground_z("<<z<<")" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

float Scene::get_ground_z() {
    return md->profile["ground_z"].get<float>();
}

void Scene::set_ground_texture(string image_path) 
{
    md->profile["ground_texture"] = image_path;
    stringstream req;
    req << "scene.set_ground_texture('"<<image_path<<"')" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

string Scene::get_ground_texture() 
{
    return md->profile["ground_texture"].get<string>();
}

void Scene::set_log_func(std::function<void(char,string)> slot)
{
    md->slot_log = slot;
}

Editor::Editor(Scene* sp) : scene(sp)
{

}

RayInfo Editor::ray(double x,double y) 
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
    return RayInfo {json_res["name"].get<string>(),{pos[0].get<double>(),pos[1].get<double>(),pos[2].get<double>()}};
}

ActiveObject* Editor::select(string name)
{
    stringstream req;
    req << "editor.select('"<<name<<"')" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));

    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    cout << json_res.dump() << endl;
    auto properties = json_res.dump();
    auto kind = json_res["kind"].get<string>();
    return scene->get_active_objs()[name];
}

ActiveObject* Editor::add(string kind,string base,Vec3 pos,Vec3 rot,Vec3 scale) 
{
    stringstream req;
    req << "editor.add('"<<kind<<"','"<<base<<"',[" << pos[0]<<","<<pos[0]<<","<<pos[2]<< "],[" << rot[0]<<","<<rot[1]<<","<<rot[2]<< "],"<<scale[0]<<")" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    cout << json_res.dump() << endl;
    auto name = json_res["name"].get<string>();
    auto objs = scene->get_active_objs();
    return objs[name];
}

void Editor::remove(string name) 
{
    auto p = scene->active_objs_by_name[name];
    scene->active_objs_by_name.erase(name);
    delete p;

    stringstream req;
    req << "editor.remove('"<<name<<"')" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

void Editor::set_relation(string parent,string child)
{

}

list<Relation> Editor::get_relations() 
{
    return {};
}

void Scene::save() 
{
    stringstream req;
    req << "scene.save()" << endl;
    asio::write(md->socket,asio::buffer(req.str()));
}

ActiveObject::ActiveObject(Scene* sp, string properties) : scene(sp)
{
    auto json_properties = json::parse(properties);
    name = json_properties["name"].get<string>().c_str();
    kind = json_properties["kind"].get<string>().c_str();
    base = json_properties["base"].get<string>().c_str();
    pos = json_properties["pos"].get<Vec3>();
    rot = json_properties["rot"].get<Vec3>();
    scale = json_properties["scale"].get<Vec3>();

    if(json_properties.contains("user_data")) {
        user_data = json_properties["user_data"].get<string>().c_str();
    }
}

bool ActiveObject::set_name(string new_name)
{
    auto it = scene->active_objs_by_name.find(new_name);
    if(scene->active_objs_by_name.end() != it) return false;
    
    stringstream req;
    req << "editor.rename('"<<this->name<<"','"<<new_name<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    
    scene->active_objs_by_name.erase(name);
    scene->active_objs_by_name[new_name] = this;
    name = new_name;
    return true;
}

string ActiveObject::get_name()
{
    return name;
}

void ActiveObject::set_base(string path)
{
    base = path;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_base('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

string ActiveObject::get_base() { return this->base; }

void ActiveObject::set_pos(Vec3 pos)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_pos([" << pos[0] << ',' << pos[1] << ',' << pos[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    this->pos = pos;
}

Vec3 ActiveObject::get_pos()
{
    return pos;
}

void ActiveObject::set_rot(Vec3 rot)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_rot([" << rot[0] << ',' << rot[1] << ',' << rot[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    this->rot = rot;
}

Vec3 ActiveObject::get_rot()
{
    return rot;
}

void ActiveObject::set_scale(Vec3 scale)
{
    this->scale = scale;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_scale([" << rot[0] << ',' << rot[1] << ',' << rot[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

Vec3 ActiveObject::get_scale()
{
    return this->scale;
}

void ActiveObject::set_transparence(float value)
{

}

float ActiveObject::get_transparence() 
{
    return 1.0 - 0.0;
}

string ActiveObject::get_kind()
{
    return kind;
}

void ActiveObject::set_user_data(string value)
{
    user_data = value;

    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_user_data('" << value << "')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

string ActiveObject::get_user_data()
{
    return user_data;
}

Robot::Robot(Scene* sp, string properties) : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    end_effector = json_properties["end_effector"].get<string>();

    if(json_properties.contains("speed")) 
        speed = json_properties["speed"].get<float>();
    else 
        speed = 1.0;
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
}

void Robot::set_end_effector(string path)
{
    end_effector = path;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_end_effector('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

string Robot::get_end_effector() { return end_effector; }

void Robot::digital_output(bool pickup) 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].end_effector_obj.do(" << (pickup ? "True" : "False")  << ")"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

int Robot::get_joints_num()
{
    return current_joint_poses.size();
}

void Robot::set_joint_position(int joint_index,float value)
{
    current_joint_poses[joint_index] = value;

    stringstream req,args;
    for (auto i : current_joint_poses) args << i << ",";
    args.seekp(-1,ios::cur);
    args << ' ';
    req << "scene.active_objs_by_name['"<<name<<"'].set_joints(["<<args.str()<<"])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

float Robot::get_joint_position(int joint_index)
{
    return current_joint_poses[joint_index];
}

void Robot::set_end_effector_pos(Vec3 pos)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_end_effector_pos([" << pos[0] << ',' << pos[0] << ',' << pos[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    end_effector_pos = pos;
}

Vec3 Robot::get_end_effector_pos()
{
    return end_effector_pos;
}

void Robot::set_end_effector_rot(Vec3 rot)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_end_effector_rot([" << rot[0] << ',' << rot[0] << ',' << rot[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    end_effector_rot = rot;
}

Vec3 Robot::get_end_effector_rot()
{
    return end_effector_rot;
}

void Robot::set_home()
{
    home_joint_poses = current_joint_poses;
}

void Robot::home()
{
    stringstream req,args;
    for (auto i : home_joint_poses) args << i << ",";
    args.seekp(-1,ios::cur);
    args << ' ';
    req << "scene.active_objs_by_name['"<<name<<"'].set_joints(["<<args.str()<<"])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

void Robot::set_speed(float value)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_speed("<< value <<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    speed = value;
}

float Robot::get_speed()
{
    return speed;
}

void Robot::track(bool enable)
{

}

Camera3D::Camera3D(Scene* sp,string properties)  : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    auto vs = json_properties["image_size"];
    image_size[0] = vs[0].get<long long>();
    image_size[1] = vs[1].get<long long>();
    rgba_pixels.resize(image_size[0]*image_size[1]*4);
    depth_pixels.resize(image_size[0]*image_size[1]);
    fov = json_properties["fov"].get<long long>();
    forcal = json_properties["forcal"].get<double>();
}

Camera3D::~Camera3D()
{
    rtt_proxy_running=false;
    rtt_proxy.join();
}

const Texture Camera3D::rtt() {
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].rtt()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::read(scene->md->socket,asio::buffer(rgba_pixels));
    asio::read(scene->md->socket,asio::buffer(depth_pixels));

    Texture t;
    t.width = image_size[0];
    t.height = image_size[1];
    t.rgba_pixels = rgba_pixels.data();
    t.depth_pixels = depth_pixels.data();
    return t;
}

void Camera3D::set_rtt_func(std::function<void(vector<unsigned char>,vector<float>,int,int)> slot) { //获取虚拟相机数据，RGB，Depth
    slot_rtt = slot;
    rtt_proxy_running = true;
    rtt_proxy = thread([this]() {
        asio::io_context io_context;
        auto sock = (scene->md->tmp_path / ("/" + name + ".sock")).string();
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
            slot_rtt(rgba_pixels,depth_pixels,image_size[0],image_size[1]);
        }

        acceptor.close();
    });
}  

void Camera3D::clear() 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].clear_point_cloud()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

Camera3DReal::Camera3DReal(Scene* sp,string properties)  : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    auto vs = json_properties["image_size"];
    image_size[0] = vs[0].get<long long>();
    image_size[1] = vs[1].get<long long>();
    rgb_pixels.resize(image_size[0]*image_size[1]*3);
    depth_pixels.resize(image_size[0]*image_size[1]);
}

Camera3DReal::~Camera3DReal()
{
    rtt_proxy_running=false;
    rtt_proxy.join();
}

const TextureReal Camera3DReal::rtt() {
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].rtt()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));

    TextureReal t;
    t.width = image_size[0];
    t.height = image_size[1];
    t.rgb_pixels = rgb_pixels.data();
    t.depth_pixels = depth_pixels.data();
    return t;
}

void Camera3DReal::set_rtt_func(std::function<bool(vector<unsigned char>&,vector<float>&,int&,int&)> slot) 
{
    slot_rtt = slot;
    rtt_proxy_running = true;
    
    rtt_proxy = thread([this]() {
        asio::io_context io_context;
        auto sock = (scene->md->tmp_path / ("/" + name + ".sock")).string();
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
            
            if(slot_rtt(rgb_pixels,depth_pixels,image_size[0],image_size[1])) {
                asio::write(socket,asio::buffer(&image_size[0],4));
                asio::write(socket,asio::buffer(&image_size[1],4));
                asio::write(socket,asio::buffer(rgb_pixels));
                asio::write(socket,asio::buffer(depth_pixels));  
            } else {
                cout << "failed rtt" << endl;
            }
        }

        acceptor.close();
    });
}

void Camera3DReal::set_calibration(string projection_transform,string eye_to_hand_transform) 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_calibration("<<projection_transform<<","<<eye_to_hand_transform<<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Camera3DReal::clear() 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].clear_point_cloud()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
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

void Placer::set_workpiece(string base)
{
    workpiece = base;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_workpiece('"<<base<<"')" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_workpiece_texture(string img_path)
{
    workpiece_texture = img_path;
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_workpiece_texture('"<<img_path<<"')" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_center(Vec3 pos)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_center([" << pos[0] << ',' << pos[0] << ',' << pos[2] << "])" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_amount(int num)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_amount("<<num<<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_interval(float seconds)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_interval("<<seconds<<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_scale_factor(float max,float min) 
{

}

void Placer::set_place_mode(string place_mode)
{

}

void Placer::get_layout(int& x,int& y,int& z)
{

}

void Placer::set_layout(int x,int y,int z)
{
    
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
        auto sock = (scene->md->tmp_path / ("/" + name + ".sock")).string();
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

void Workflow::set(string src) {
    stringstream req;
    req << "workflow.set('" << src << "')" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

string Workflow::get() {
    stringstream req;
    req << "workflow.get()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    return string(asio::buffers_begin(res.data()),asio::buffers_end(res.data()));
}

void Workflow::start() {
    stringstream req;
    req << "workflow.start()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Workflow::stop() {
    stringstream req;
    req << "workflow.stop()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

}