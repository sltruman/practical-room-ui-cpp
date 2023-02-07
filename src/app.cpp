#include <locale.h>

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
        using namespace boost::filesystem;

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
        for(auto lbr : vector<Gtk::ListBoxRow*>{ row_end_effector,row_width,row_height,row_rtt }) 
            lbr->set_visible(false);

        base_signal_changed.disconnect();

        base_model->clear();
        for (auto e : directory_iterator(path(active_obj->base.c_str()).parent_path())) { 
            if(is_directory(e)) continue;
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

        spin_x->set_value(active_obj->x);
        spin_y->set_value(active_obj->y);
        spin_z->set_value(active_obj->z);
        spin_roll->set_value(active_obj->roll);
        spin_pitch->set_value(active_obj->pitch);
        spin_yaw->set_value(active_obj->yaw);
        entry_kind->set_text(active_obj->kind);
        
        if(parseRobot(active_obj)) {
            row_end_effector->set_visible();
        } else if (parseCamera(active_obj)) {
            row_width->set_visible();
            row_height->set_visible();
            row_rtt->set_visible();
        } else if (parsePacker(active_obj)) {
            
        } else if (parseStacker(active_obj)) {

        } else {

        }
    }

    bool parseRobot(ActiveObject* active_obj)
    {
        auto robot = dynamic_cast<Robot*>(active_obj);
        if(robot == nullptr) return false;
        using namespace boost::filesystem;
        auto dropdown_end_effector = builder->get_widget<Gtk::ComboBox>("end_effector");
        end_effector_signal_changed.disconnect();
        
        end_effector_model->clear();
        for (auto e : directory_iterator(path(robot->end_effector.c_str()).parent_path())) { 
            if(is_directory(e)) continue;
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

        using namespace boost::filesystem;
        auto spin_width = builder->get_object<Gtk::SpinButton>("width");
        auto spin_height = builder->get_object<Gtk::SpinButton>("height");
        auto btn_rtt = builder->get_object<Gtk::Button>("rtt");
        auto area_texture = builder->get_object<Gtk::Picture>("texture");
        spin_width->set_value(obj->viewport_size[0]);
        spin_height->set_value(obj->viewport_size[1]);

        rtt_signal_click.disconnect();
        rtt_signal_click =  btn_rtt->signal_clicked().connect([obj,area_texture](){
            auto aspect_ratio_viewport = 1. * obj->viewport_size[0] / obj->viewport_size[1];
            int area_w = area_texture->get_width();
            int area_h = area_w / aspect_ratio_viewport;
            
            auto texture = obj->rtt();
            auto img = Gdk::Pixbuf::create_from_data(texture.rgba_pixels,Gdk::Colorspace::RGB,true,8,texture.width,texture.height,texture.width*4);
            area_texture->set_pixbuf(img);
            area_texture->set_size_request(area_w,area_h);
        });
        
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

class AppWindow : public Gtk::ApplicationWindow
{
public:
    AppWindow(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder) 
        : Gtk::ApplicationWindow(cobject) , builder(builder)
    {
        auto button_open = builder->get_widget<Gtk::Button>("open");
        button_open->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_open_clicked));
        auto button_start = builder->get_widget<Gtk::Button>("start");
        button_start->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_start_clicked));
        auto button_stop = builder->get_widget<Gtk::Button>("stop");
        button_stop->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_stop_clicked));
        overlay = builder->get_widget<Gtk::Overlay>("overlay");
        area = builder->get_widget<Gtk::DrawingArea>("simulation");
        right_side_pannel = builder->get_widget<Gtk::ScrolledWindow>("right_side_pannel");
        properties = builder->get_widget_derived<ObjectProperties>(builder,"properties");
        
    }

    ~AppWindow()
    {
        delete editor;
        delete scene;
    }

    void on_button_open_clicked()
    {
        auto dialog = make_shared<Gtk::FileChooserDialog>("Please choose a file",Gtk::FileChooser::Action::OPEN);
        dialog->add_button("_Open", Gtk::ResponseType::OK);
        dialog->set_transient_for(*this);
        dialog->set_modal();
        dialog->set_visible();
        auto filter_text = Gtk::FileFilter::create();
        filter_text->set_name("*.json");
        filter_text->add_pattern("*.json");
        dialog->add_filter(filter_text);
        dialog->signal_response().connect([this,dialog](int response_id) {
            dialog->close();

            if(response_id != Gtk::ResponseType::OK) return;
            auto scene_path = dialog->get_file()->get_path();
            cout << response_id << ' ' << dialog->get_file()->get_path() << endl;
            
            scene = new Scene(800,640, scene_path);
            editor = new Editor(scene);
            area->set_draw_func(sigc::mem_fun(*this, &AppWindow::area_paint_event));

            auto controller = Gtk::EventControllerLegacy::create();
            controller->signal_event().connect(sigc::mem_fun(*this,&AppWindow::area_drag_event),false);
            overlay->add_controller(controller);
        });
    }

    int handled = 0;
    Glib::RefPtr<const Gdk::Event> prev_ev;
    bool area_drag_event(const Glib::RefPtr<const Gdk::Event>& ev)
    { 
        auto type = ev->get_event_type();
        auto button = ev->get_button();

        switch (type) {
            case Gdk::Event::Type::SCROLL:
                scene->zoom(ev->get_direction() == Gdk::ScrollDirection::UP ? 0.8 : 1.2);
                break;
            case Gdk::Event::Type::BUTTON_PRESS:
                if(handled != 0 && handled != button) break;
                handled = button;
                prev_ev = ev;

                if(handled == 1) {
                    // 计算出鼠标位置相对于图片位置的坐标
                    double x,y; ev->get_position(x,y);
                    x -= 14,y -= get_titlebar()->get_height(); //补偿x,y值，这是gtk框架bug
                    if(right_side_pannel->get_allocation().contains_point(x,y)) break;

                    auto x_norm = (x / area_zoom_factor - img_x) / img->get_width();
                    auto y_norm = (y / area_zoom_factor - img_y) / img->get_height();
                    auto hit = editor->ray(x_norm,y_norm);
                    
                    if(hit.id != -1) {
                        auto active_obj = editor->select(hit.id);
                        properties->parse(active_obj);
                    }

                    right_side_pannel->set_visible(hit.id != -1);
                }

                break;
            case Gdk::Event::Type::MOTION_NOTIFY: {
                if(handled == 0) break;
                double x,y; ev->get_position(x,y);
                x -= 14,y -= get_titlebar()->get_height(); //补偿x,y值，这是gtk框架bug
                x =  x / area_zoom_factor - img_x, y = y / area_zoom_factor - img_y;
                
                // 计算出上一次鼠标位置相对于图片位置的坐标
                double x_prev,y_prev; prev_ev->get_position(x_prev,y_prev);
                x_prev -= 14,y_prev -= get_titlebar()->get_height(); //补偿x,y值，这是gtk框架bug
                x_prev = x_prev / area_zoom_factor - img_x, y_prev = y_prev / area_zoom_factor - img_y;

                // 将两次鼠标位置的差值转化为百分比
                auto x_offset_norm = (x - x_prev) / img->get_width();
                auto y_offset_norm = (y - y_prev) / img->get_height();

                switch(handled) {
                    case 2:
                        scene->rotate(x_offset_norm,y_offset_norm);
                        break;
                    case 3:
                        scene->pan(x_offset_norm,y_offset_norm);
                        break;
                }
                
                prev_ev = ev;
                break;
            }
            case Gdk::Event::Type::BUTTON_RELEASE:
                handled = 0;
                break;
        }

        return false;
    }

    void area_paint_event(const Cairo::RefPtr<Cairo::Context>& cr, int area_w, int area_h) 
    {
        Glib::signal_timeout().connect_once([this]() { area->queue_draw(); }, 1000 / 24);
        
        cr->set_source_rgb(40 / 255.,40 / 255.,40 / 255.);
        cr->paint();

        auto texture = scene->rtt();
        if(texture.rgba_pixels == nullptr) return;

        auto aspect_ratio = 1. * texture.width / texture.height;
        auto aspect_ratio2 = 1. * area_w / area_h;
        auto factor = 1.0;
        if (aspect_ratio > aspect_ratio2) factor = 1. * area_w / texture.width;
        else factor = 1. * area_h / texture.height;
        cr->scale(factor,factor);
 
        img_x = (area_w / factor - texture.width) / 2;
        img_y = (area_h / factor - texture.height) / 2;

        if(!img) img = Gdk::Pixbuf::create_from_data(texture.rgba_pixels,Gdk::Colorspace::RGB,true,8,texture.width,texture.height,texture.width*4);
        Gdk::Cairo::set_source_pixbuf(cr, img,img_x,img_y);
        cr->paint();

        area_zoom_factor = factor;
    }

    void on_button_start_clicked()
    {
        scene->start();
    }

    void on_button_stop_clicked()
    {
        scene->stop();
    }

    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::Overlay* overlay;
    Gtk::DrawingArea* area;
    Glib::RefPtr<Gdk::Pixbuf> img;
    double area_zoom_factor = 1.0,img_x,img_y;
    ObjectProperties* properties;
    Gtk::ScrolledWindow* right_side_pannel;
    
    digitaltwin::Scene* scene;
    digitaltwin::Editor* editor;
};

int main(int argc, char* argv[])
{
    auto app = Gtk::Application::create("xyz.diracloud.DigitalTwin");
    
    setlocale(LC_ALL,"zh");
    bindtextdomain("digitaltwin","po");
    textdomain("digitaltwin");

    app->signal_activate().connect([app]() {
        auto builder = Gtk::Builder::create_from_file("../src/app.glade");
        app->add_window(*Gtk::Builder::get_widget_derived<AppWindow>(builder,"app_window"));
    });

    return app->run(argc, argv);
}
