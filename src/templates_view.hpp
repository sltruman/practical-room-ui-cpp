#include <locale.h>

#include <opencv2/opencv.hpp>
#include <gtkmm.h>
#include <gtkmm/eventcontrollerlegacy.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <string>
using namespace std;

#include <boost/json.hpp>
#include <boost/filesystem.hpp>
using namespace boost;

#include "digitaltwin.hpp"
using namespace digitaltwin;

static const boost::filesystem::path dir_data = "./data";
static const boost::filesystem::path dir_workpieces = "./data/workpieces";
static const boost::filesystem::path dir_end_effectors = "./data/end_effectors";
static const boost::filesystem::path dir_cameras = "./data/cameras";
static const boost::filesystem::path dir_objects = "./data/objects";
static const boost::filesystem::path dir_static_objects = "./data/static_objects";
static const boost::filesystem::path dir_robots = "./data/robots";
static const boost::filesystem::path dir_scenes = "./data/scenes";

class ObjectProperties : public Gtk::ListBox 
{
public:
    ObjectProperties(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::ListBox(cobject),builder(builder)
    {
        auto dropdown_base = builder->get_widget<Gtk::ComboBox>("base");

        Gtk::TreeModel::ColumnRecord columns;
        columns.add(col_key);
        columns.add(col_value);

        base_model = Gtk::ListStore::create(columns);
        dropdown_base->set_model(base_model);
        dropdown_base->pack_start(col_key);

        auto dropdown_end_effector = builder->get_widget<Gtk::ComboBox>("end_effector");

        end_effector_model = Gtk::ListStore::create(columns);
        dropdown_end_effector->set_model(end_effector_model);
        dropdown_end_effector->pack_start(col_key);
    }

    Gtk::TreeModelColumn<string> col_key,col_value;
    Glib::RefPtr<Gtk::ListStore> base_model,end_effector_model;
    sigc::connection base_signal_changed,end_effector_signal_changed,rtt_signal_click;

    void parse(ActiveObject* active_obj)
    {
        auto entry_kind = builder->get_widget<Gtk::Label>("kind");
        auto dropdown_base = builder->get_widget<Gtk::ComboBox>("base");
        auto spin_x = builder->get_object<Gtk::SpinButton>("x");
        auto spin_y = builder->get_object<Gtk::SpinButton>("y");
        auto spin_z = builder->get_object<Gtk::SpinButton>("z");
        auto spin_roll = builder->get_object<Gtk::SpinButton>("roll");
        auto spin_pitch = builder->get_object<Gtk::SpinButton>("pitch");
        auto spin_yaw = builder->get_object<Gtk::SpinButton>("yaw");
        auto row_end_effector = builder->get_widget<Gtk::ListBoxRow>("row_end_effector");
        auto row_width = builder->get_widget<Gtk::ListBoxRow>("row_width");
        auto row_height = builder->get_widget<Gtk::ListBoxRow>("row_height");
        auto row_rtt = builder->get_widget<Gtk::ListBoxRow>("row_rtt");
        for(auto lbr : vector<Gtk::ListBoxRow*>{ row_end_effector,row_width,row_height,row_rtt }) lbr->set_visible(false);

        spin_x->set_value(active_obj->pos[0]);
        spin_y->set_value(active_obj->pos[1]);
        spin_z->set_value(active_obj->pos[2]);
        spin_roll->set_value(active_obj->rot[0]);
        spin_pitch->set_value(active_obj->rot[1]);
        spin_yaw->set_value(active_obj->rot[2]);
        entry_kind->set_text(active_obj->kind);
        
        base_signal_changed.disconnect();
        base_model->clear();

        auto dir_base = dir_objects;
        if(parseRobot(active_obj)) {
            row_end_effector->set_visible();
            dir_base = dir_robots;
        } else if (parseCamera(active_obj)) {
            row_width->set_visible();
            row_height->set_visible();
            row_rtt->set_visible();
            dir_base = dir_cameras;
        } else if (parsePacker(active_obj)) {
        } else if (parseStacker(active_obj)) {  
        } 

        for (auto e : boost::filesystem::recursive_directory_iterator(dir_base)) { 
            auto filename = e.path().filename().string();
            if (string::npos == filename.find(".urdf")) continue;
            auto row = base_model->append();
            row->set_value(col_key, e.path().filename().string());
            row->set_value(col_value, e.path().string());
            cout << e.path().string() << endl;
            if(active_obj->base == e.path()) dropdown_base->set_active(row);
        }

        base_signal_changed = dropdown_base->signal_changed().connect([active_obj,dropdown_base,this]() {
            auto model = dropdown_base->get_model();
            auto row = dropdown_base->get_active();
            active_obj->set_base(row->get_value(col_value));
        });
    }

    bool parseRobot(ActiveObject* active_obj)
    {
        auto robot = dynamic_cast<Robot*>(active_obj);
        if(robot == nullptr) return false;
        auto dropdown_end_effector = builder->get_widget<Gtk::ComboBox>("end_effector");
        end_effector_signal_changed.disconnect();
        
        end_effector_model->clear();

        for (auto e : boost::filesystem::recursive_directory_iterator(dir_end_effectors)) { 
            auto filename = e.path().filename().string();
            if (string::npos == filename.find(".urdf")) continue;
            auto row = end_effector_model->append();
            row->set_value(col_key, e.path().filename().string());
            row->set_value(col_value, e.path().string());
            cout << e.path().string() << endl;
            if(robot->end_effector == e.path()) dropdown_end_effector->set_active(row);
        }

        end_effector_signal_changed = dropdown_end_effector->signal_changed().connect([robot,dropdown_end_effector,this]() {
            auto model = dropdown_end_effector->get_model();
            auto row = dropdown_end_effector->get_active();
            robot->set_end_effector(row->get_value(col_value));
        });

        return true;
    }

    bool parseCamera(ActiveObject* active_obj)
    {
        auto obj = dynamic_cast<Camera3D*>(active_obj);
        if(obj == nullptr) return false;

        auto spin_width = builder->get_object<Gtk::SpinButton>("fov");
        auto spin_height = builder->get_object<Gtk::SpinButton>("forcal");
        auto btn_rtt = builder->get_object<Gtk::Button>("rtt");
        auto area_texture = builder->get_object<Gtk::Picture>("texture");
        auto area_texture2 = builder->get_object<Gtk::Picture>("texture_depth");
        spin_width->set_value(obj->fov);
        spin_height->set_value(obj->forcal);

        auto rtt = [obj,area_texture,area_texture2]() {
            auto aspect_ratio_viewport = 1. * obj->image_size[0] / obj->image_size[1];
            int area_w = area_texture->get_width();
            int area_h = area_w / aspect_ratio_viewport;
            auto texture = obj->rtt();
            auto img = Gdk::Pixbuf::create_from_data(texture.rgba_pixels,Gdk::Colorspace::RGB,true,8,texture.width,texture.height,texture.width*4);
            area_texture->set_pixbuf(img);
            // auto img2 = Gdk::Pixbuf::create_from_data(texture.depth_pixels,Gdk::Colorspace::RGB,false,8,texture.width,texture.height,texture.width*3);
            // area_texture2->set_pixbuf(img2);
            area_texture->set_size_request(area_w,area_h);
            // area_texture2->set_size_request(area_w,area_h);
        };

        if(!area_texture->get_paintable()) {
            Glib::signal_timeout().connect_once([area_texture,area_texture2,obj]() {
                auto aspect_ratio_viewport = 1. * obj->image_size[0] / obj->image_size[1];
                int area_w = area_texture->get_width();
                int area_h = area_w / aspect_ratio_viewport;
                auto img = Gdk::Pixbuf::create(Gdk::Colorspace::RGB, false, 8, obj->image_size[0],obj->image_size[1]);
                
                area_texture->set_pixbuf(img);
                area_texture2->set_pixbuf(img);
                area_texture->set_size_request(area_w,area_h);
                area_texture2->set_size_request(area_w,area_h);
            },100);
        }
        
        rtt_signal_click.disconnect();
        rtt_signal_click = btn_rtt->signal_clicked().connect(rtt);
        return true;
    }

    bool parseStacker(ActiveObject* active_obj) {
        return true;
    }

    bool parsePacker(ActiveObject* active_obj) {
        return true;
    }

    Glib::RefPtr<Gtk::Builder> builder;
};