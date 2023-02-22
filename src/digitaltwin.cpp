#include "digitaltwin.hpp"

#include <memory>
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

        // md->backend = make_shared<process::child>("digitaltwin",scene_path,to_string(md->viewport_size[0]),to_string(md->viewport_size[1]),tmp_path);
        // md->backend->wait_for(chrono::seconds(4));
        cout << "succeded" << endl;
        
        cout << "Connecting to digital-twin...";
        
        auto socket_name = boost::filesystem::basename(scene_path) + ".json.sock";
        auto socket_path = boost::filesystem::temp_directory_path().append("digitaltwin").append(socket_name);
        
        md->socket.connect(asio::local::stream_protocol::endpoint(socket_path.c_str()));
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

void Scene::save() {
    
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
    return RayInfo {(int)json_res["id"].get<long long>(),{pos[0].get<double>(),pos[1].get<double>(),pos[2].get<double>()}};
}

void Editor::move(int id,double pos[3])
{

}

ActiveObject* Editor::select(int id)
{
    stringstream req;
    req << "editor.select(" << id << ")" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));

    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data())));
    cout << json_res.dump() << endl;
    auto properties = json_res.dump();

    auto kind = json_res["kind"].get<string>();
    
    ActiveObject* obj = nullptr;
    
    if(kind == "Robot") {
        obj = new Robot(scene, properties);
    } else if(kind == "Camera3D") {
        obj = new Camera3D(scene, properties);
    } else if(kind == "Packer") {
        obj = new ActiveObject(scene, properties);
    }

    if(active_objs.contains(id))
        *active_objs[id] = *obj;
    else
        active_objs[id] = obj;

    return active_objs[id];
}

ActiveObject::ActiveObject(Scene* sp, string properties) : scene(sp)
{
    auto json_properties = json::parse(properties);

    kind = json_properties["kind"].get<string>().c_str();
    id = json_properties["id"].get<long long>();
    base = json_properties["base"].get<string>().c_str();
    auto pos = json_properties["pos"];
    auto rot = json_properties["rot"];

    x = pos[0].get<double>();
    y = pos[1].get<double>();
    z = pos[2].get<double>();
    roll = rot[1].get<double>();
    pitch = rot[0].get<double>();
    yaw = rot[2].get<double>();
}

void ActiveObject::set_base(string path) 
{
    stringstream req;
    req << "scene.active_objs["<<id<<"].set_base('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
     asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    cout << res.data().data() << endl;
    base = path;
    istream in(&res); in >> id;
}


Robot::Robot(Scene* sp, string properties) : ActiveObject(sp,properties)
{
    auto json_properties = json::parse(properties);
    end_effector = json_properties["end_effector"].get<string>();
}

void Robot::set_end_effector(string path)
{
    stringstream req;
    req << "scene.active_objs["<<id<<"].set_end_effector('"<<path<<"')"<<endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));
    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    cout << res.data().data() << endl;
    end_effector = path;
    
    istream i(&res); i >> end_effector_id;
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
    req << "scene.active_objs["<<id<<"].rtt()" << endl;
    asio::write(scene->md->socket,asio::buffer(req.str()));
    asio::read(scene->md->socket,asio::buffer(rgba_pixels));
    asio::read(scene->md->socket,asio::buffer(depth_pixels));

    Texture t;
    t.width = image_size[0];
    t.height = image_size[1];
    t.rgba_pixels = rgba_pixels.data();
    t.depth_pixels = depth_pixels.data();

    return t;
};

Workflow::Workflow(Scene* sp) : scene(sp)   {

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