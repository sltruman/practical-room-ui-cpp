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
        md->backend = make_shared<process::child>("digitaltwin",scene_path,to_string(md->viewport_size[0]),to_string(md->viewport_size[1]),md->tmp_path);
        cout << "succeded" << endl;

        cout << "command:" << "digitaltwin" << ' '
             << scene_path << ' '
             << to_string(md->viewport_size[0]) << ' '
             << to_string(md->viewport_size[1]) << ' '
             << md->tmp_path << endl;

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
    ;
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

}

void Scene::rotete_right() 
{

}

void Scene::rotete_top()
{

}

void Scene::rotete_front()
{

}

void Scene::rotete_back() 
{

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
        } else if(kind == "Packer") {
            obj = new Placer(this, description);
        } else {
            obj = new ActiveObject(this, description);
        }

        active_objs_by_name[name] = obj;
    }

    return active_objs_by_name;
}

void Scene::set_logging(std::function<void(string)> log_callback)
{

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

ActiveObject* Editor::add(string base,Vec3 pos,Vec3 rot,Vec3 scale) 
{
    return nullptr;
}

void Editor::remove(string name) 
{

}

void Editor::set_relation(string parent,string child)
{   

}

list<Relation> Editor::get_relations() 
{
    return {};
}

void Editor::save() 
{
    stringstream req;
    req << "editor.save()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

ActiveObject::ActiveObject(Scene* sp, string properties) : scene(sp)
{
    auto json_properties = json::parse(properties);
    name = json_properties["name"].get<string>().c_str();
    kind = json_properties["kind"].get<string>().c_str();
    base = json_properties["base"].get<string>().c_str();
    auto pos = json_properties["pos"];
    auto rot = json_properties["rot"];
    copy(pos.begin(),pos.begin(),this->pos.begin());
    copy(rot.begin(),rot.begin(),this->rot.begin());
}
bool ActiveObject::set_name(string name)
{
    return false;
}

string ActiveObject::get_name()
{
    return name;
}

void ActiveObject::set_base(string path)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_base('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    cout << res.data().data() << endl;
    base = path;
}

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
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_scale([" << rot[0] << ',' << rot[1] << ',' << rot[2] << "])"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    this->rot = rot;
}

Vec3 ActiveObject::get_scale()
{
    return rot;
}

void ActiveObject::set_transparence(float value)
{

}

float ActiveObject::get_transparence() 
{
    return 0.;
}

string ActiveObject::get_kind()
{
    return kind;
}

Robot::Robot(Scene* sp, string properties) : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    end_effector = json_properties["end_effector"].get<string>();
}

void Robot::set_end_effector(string path)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_end_effector('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    cout << res.data().data() << endl;
    end_effector = path;
    
    istream i(&res); i >> end_effector_id;
}

void Robot::digital_output(bool pickup) 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].end_effector_obj.do(" << (pickup ? "True" : "False")  << ")"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
}

int Robot::get_joints_num()
{
    return 0;
}

void Robot::set_joint_position(int joint_index,float value)
{

}

float Robot::get_joint_position(int joint_index)
{
    return 0.0;
}

void Robot::set_end_effector_pos(Vec3 pos)
{

}

Vec3 Robot::get_end_effector_pos()
{
    return Vec3();
}

void Robot::set_end_effector_rot(Vec3 rot)
{

}

Vec3 Robot::get_end_effector_rot()
{
    return Vec3();
}

void Robot::set_home()
{

}

void Robot::home()
{

}

void Robot::set_speed(float value)
{

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
    depth_pixels.resize(image_size[0]*image_size[1]*3);
    fov = json_properties["fov"].get<long long>();
    forcal = json_properties["forcal"].get<double>();
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

Camera3DReal::Camera3DReal(Scene* sp,string properties)  : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    auto vs = json_properties["image_size"];
    image_size[0] = vs[0].get<long long>();
    image_size[1] = vs[1].get<long long>();
    rgba_pixels.resize(image_size[0]*image_size[1]*4);
    depth_pixels.resize(image_size[0]*image_size[1]*3);
}

const Texture Camera3DReal::rtt() {
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

void Camera3DReal::set_rtt_func(std::function<bool(vector<unsigned char>&,vector<float>&,int&,int&)> slot) 
{
    rtt_proxy_running = true;
    rtt_proxy = thread([this,slot]() {
        while(rtt_proxy_running) {
            asio::io_context io_context;
            auto sock = (scene->md->tmp_path / ("/" + name + ".sock")).string();
            cout << sock << endl;
            unlink(sock.c_str());
            asio::local::stream_protocol::endpoint ep(sock);
            asio::local::stream_protocol::acceptor acceptor(io_context, ep);
            asio::local::stream_protocol::socket socket(io_context);
            acceptor.accept(socket);
            vector<unsigned char> rgb_pixels;
            vector<float> depth_pixels;
            int width,height;
            if(slot(rgb_pixels,depth_pixels,width,height)) {
                asio::write(socket,asio::buffer(&width,4));
                asio::write(socket,asio::buffer(&height,4));
                asio::write(socket,asio::buffer(rgb_pixels));
                asio::write(socket,asio::buffer(depth_pixels));  
            } else {
                cout << "failed rtt" << endl;
            }

            acceptor.close();
        }
    });
}

void Camera3DReal::set_calibration(string projection_transform,string eye_to_hand_transform) 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].set_calibration("<<projection_transform<<","<<eye_to_hand_transform<<")" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Camera3DReal::draw_point_cloud(string ply_path) 
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].draw_point_cloud('"<<ply_path<<"')" << endl;
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
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].workpiece = " << base << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_workpiece_texture(string img_path)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].workpiece_texture = " << img_path << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_center(Vec3 pos)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].center = [" << pos[0] << ',' << pos[0] << ',' << pos[2] << ']' << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_amount(int num)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].amount = " << num << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
}

void Placer::set_interval(float seconds)
{
    stringstream req;
    req << "scene.active_objs_by_name['"<<name<<"'].interval = " << seconds << endl;
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

Workflow::Workflow(Scene* sp) : scene(sp) {
    proxy_nodes_running = true;
}

void Workflow::add_active_obj_node(string kind,string name,string f,std::function<string()> slot) 
{
    proxy_nodes.emplace_back([this,kind,name,f,slot](){
        while(proxy_nodes_running) {
            asio::io_context io_context;
            auto sock = (scene->md->tmp_path / ("/" + name + ".sock")).string();
            cout << sock << endl;
            unlink(sock.c_str());
            asio::local::stream_protocol::endpoint ep(sock);
            asio::local::stream_protocol::acceptor acceptor(io_context, ep);
            asio::local::stream_protocol::socket socket(io_context);
            acceptor.accept(socket);
            
            auto pickposes = slot();
            if(pickposes.empty()) {
                cout << "failed rtt" << endl;
            } else {
                stringstream req;
                req << pickposes << endl;
                asio::write(socket,asio::buffer(req.str()));
            }

            acceptor.close();
        }
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