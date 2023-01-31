#include "digitaltwin.hpp"

#include <memory>
using namespace std;


namespace digitaltwin {

#include <boost/json/src.hpp>
#include <boost/asio.hpp>
#include <boost/process.hpp>
using namespace boost;

struct Scene::Plugin {
    Plugin(): socket(io_context) {

    }

    asio::io_context io_context;
    asio::local::stream_protocol::socket socket;
    
    int scene_width,scene_height;
    vector<unsigned char> rgba_pixels;
    std::shared_ptr<process::child> backend;
};

Scene::Scene(int width,int height,string scene_path)
{
    md = new Plugin;
    md->rgba_pixels.resize(width * height * 4);
    md->scene_height = height,md->scene_width = width;
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
 
        md->backend = make_shared<process::child>("digitaltwin",scene_path,to_string(md->scene_width),to_string(md->scene_height));
        md->backend->wait_for(chrono::seconds(4));
        cout << "succeded" << endl;

        cout << "Connecting to digital-twin...";
        auto socket_path = scene_path + ".sock";
        
        md->socket.connect(asio::local::stream_protocol::endpoint(socket_path));
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
    t.width = md->scene_width;
    t.height = md->scene_height;
    t.rgba_pixels = md->rgba_pixels.data();
    t.size = md->rgba_pixels.size();

    return t;
}

void Scene::start() {
    stringstream req;
    req << "scene.start()" << endl;
    cout << req.str();
    asio::write(md->socket,  asio::buffer(req.str()));
}

void Scene::stop() {
    stringstream req;
    req << "scene.stop()" << endl;
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

Editor::Editor(Scene* sp) : scene(sp) {

}

RayInfo Editor::ray(double x,double y) 
{
    stringstream req;
    req << "editor.ray(" << x << "," << y << ")" << endl;
    cout << req.str();
    asio::write(scene->md->socket,  asio::buffer(req.str()));

    asio::streambuf res;
    asio::read_until(scene->md->socket, res,'\n');
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data()))).as_object();
    cout << json::serialize(json_res) << endl;
    
    auto pos = json_res["pos"].as_array();
    return RayInfo {(int)json_res["id"].as_int64(),{pos[0].as_double(),pos[1].as_double(),pos[2].as_double()}};
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
    auto json_res = json::parse(string(asio::buffers_begin(res.data()),asio::buffers_end(res.data()))).as_object();
    cout << json::serialize(json_res) << endl;
    auto properties = json::serialize(json_res);

    auto kind = json_res["kind"].as_string();
    ActiveObject* obj = nullptr;

    if(kind == "Robot") {
        obj = new Robot(scene, properties);
    } else if(kind == "Camera3D") {

    } else if(kind == "Packer") {

    }

    active_objs.insert(obj);

    return obj;
}

ActiveObject::ActiveObject(Scene* sp, string properties) : scene(sp)
{
    auto json_properties = json::parse(properties).as_object();

    kind = json_properties["kind"].as_string().c_str();
    id = json_properties["id"].as_int64();
    base = json_properties["base"].as_string().c_str();
    auto pos = json_properties["pos"].as_array();
    auto rpy = json_properties["rpy"].as_array();

    x = pos[0].as_double();
    y = pos[1].as_double();
    z = pos[2].as_double();
    roll = rpy[0].as_double();
    pitch = rpy[1].as_double();
    yaw = rpy[2].as_double();
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
    auto json_properties = json::parse(properties).as_object();
    set_end_effector(json_properties["end_effector"].as_string().c_str());
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


}