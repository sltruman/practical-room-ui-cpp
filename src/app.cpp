#include "object_properties.hpp"
#include "camera3d_properties.hpp"
#include "robot_properties.hpp"
#include "placer_properties.hpp"
#include "stacker_properties.hpp"
#include <locale.h>

#include <opencv2/opencv.hpp>
#include <gtkmm.h>
#include <gtkmm/eventcontrollerlegacy.h>

#include <map>
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <string>
#include <memory>
#include <filesystem>
using namespace std;

#include <boost/json.hpp>
using namespace boost;

#include "digitaltwin.hpp"
using namespace digitaltwin;


struct TemplateView : public Gtk::ScrolledWindow
{
    std::filesystem::path scene_dir = "./digitaltwin_data/scenes";
    sigc::signal<void(string)> signal_selected;

    Gtk::FlowBox* template_list;
    list<std::shared_ptr<Gtk::Button>> templates;

    TemplateView(BaseObjectType* cobject, const std::shared_ptr<Gtk::Builder>& builder)
        : Gtk::ScrolledWindow(cobject)  
        , template_list(builder->get_widget<Gtk::FlowBox>("template_list"))
    {
        refresh();
    }

    void refresh()
    {
        for(auto t : templates) template_list->remove(*t);
        templates.clear();

        for (auto fileitem : std::filesystem::recursive_directory_iterator(scene_dir)) { 
            auto filepath = fileitem.path();
            auto ext = filepath.extension();
            if (".json" != ext) continue;
            auto img_path = filepath; img_path += ".png";
            auto scene_name = filepath.filename().string();
            
            auto t = templates.emplace_back(make_shared<Gtk::Button>());
            Gtk::Box box(Gtk::Orientation::VERTICAL);
            Gtk::Image img; img.set_expand();
            if (std::filesystem::exists(img_path)) {
                img.set_pixel_size(150);
                img.property_file().set_value(img_path.string());
            } else { 
                img.set_pixel_size(100);
                img.set_from_icon_name("application-x-appliance-symbolic");
            }
            
            Gtk::Label name(scene_name.erase(scene_name.find(".json")));
            box.append(img);
            box.append(name);
            
            t->set_child(box);
            t->signal_clicked().connect(sigc::bind(sigc::mem_fun(signal_selected,&sigc::signal<void(string)>::emit),filepath.string()));
            template_list->append(*t);
        }
    }
};

struct RightSidePannel : public Gtk::ScrolledWindow
{
    Gtk::Viewport* content;
    std::map<string,ObjectProperties*> contents;

    RightSidePannel(BaseObjectType* cobject, const std::shared_ptr<Gtk::Builder>& builder)
        : Gtk::ScrolledWindow(cobject)
        , content(builder->get_widget<Gtk::Viewport>("right_side_pannel_content"))
    {
        contents["ActiveObject"] = Gtk::Builder::get_widget_derived<ObjectProperties>(Gtk::Builder::create_from_file("./object_properties.glade"), "object_properties");
        contents["Robot"] = Gtk::Builder::get_widget_derived<RobotProperties>(Gtk::Builder::create_from_file("./robot_properties.glade"), "robot_properties");
        contents["Camera3DReal"] = contents["Camera3D"] = Gtk::Builder::get_widget_derived<Camera3DProperties>(Gtk::Builder::create_from_file("./camera3d_properties.glade"), "camera3d_properties");
        contents["Placer"] = Gtk::Builder::get_widget_derived<PlacerProperties>(Gtk::Builder::create_from_file("./placer_properties.glade"), "placer_properties");
        contents["Stacker"] = Gtk::Builder::get_widget_derived<StackerProperties>(Gtk::Builder::create_from_file("./stacker_properties.glade"), "stacker_properties");
    }

    void parse(ActiveObject* obj) 
    {
        auto kind = obj->get_kind();
        auto properties = contents[kind];
        properties->parse(obj);
        content->set_child(*properties);
    }
};


struct BottomSidePannel : public Gtk::ScrolledWindow
{
    Gtk::Viewport* content;

    BottomSidePannel(BaseObjectType* cobject, const std::shared_ptr<Gtk::Builder>& builder)
        : Gtk::ScrolledWindow(cobject)
        , content(builder->get_widget<Gtk::Viewport>("bottom_side_pannel_content"))
    {
        // content = Gtk::Builder::get_widget_derived<ActiveObjects>(Gtk::Builder::create_from_file("./active_objects.glade"), "active_objects");
    }

    void parse(ActiveObject* obj) 
    {

    }
};



struct SceneView : public Gtk::Overlay 
{
    SceneView(BaseObjectType* cobject, const std::shared_ptr<Gtk::Builder>& builder)
        : Gtk::Overlay(cobject)
    {
        area = builder->get_widget<Gtk::DrawingArea>("simulation");
        right_side_pannel = Gtk::Builder::get_widget_derived<RightSidePannel>(builder,"right_side_pannel");
        bottom_side_pannel = Gtk::Builder::get_widget_derived<BottomSidePannel>(builder,"bottom_side_pannel");
    }

    ~SceneView()
    {
        
    }

    void open(string scene_path)
    {
        close();

        scene = make_shared<Scene>(800,640,"./digitaltwin_data");
        editor = make_shared<Editor>(scene.get());
        workflow = make_shared<Workflow>(scene.get());
        scene->load(scene_path);
        
        Glib::signal_timeout().connect_once([this,scene_path]() {
            Texture texture;
            scene->rtt(texture);

            cv::Mat rgba(cv::Size(texture.width,texture.height),CV_8UC4);
            memcpy(rgba.ptr<unsigned char>(), texture.rgba_pixels, texture.width * texture.height *4);
            cv::cvtColor(rgba,rgba,cv::COLOR_RGBA2BGRA);
            cv::imwrite(scene_path + ".png",rgba);
        },100);
        
        area->set_draw_func(sigc::mem_fun(*this, &SceneView::area_paint_event));
        controller = Gtk::EventControllerLegacy::create();
        controller->signal_event().connect(sigc::mem_fun(*this,&SceneView::area_drag_event),false);
        add_controller(controller);
    }

    void close()
    {
        remove_controller(controller);
        area->set_draw_func([this](const Cairo::RefPtr<Cairo::Context>&,int,int){});
        img.reset();
        workflow.reset();
        editor.reset();
        scene.reset();
    }

    void area_paint_event(const Cairo::RefPtr<Cairo::Context>& cr, int area_w, int area_h)
    {
        cr->set_source_rgb(40 / 255.,40 / 255.,40 / 255.);
        cr->paint();

        Texture texture;
        scene->rtt(texture);

        if(texture.rgba_pixels == nullptr) return;
        Glib::signal_timeout().connect_once([this]() { if (scene) area->queue_draw(); }, 1000 / 24 );

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

    int handled = 0;
    std::shared_ptr<const Gdk::Event> prev_ev;
    bool area_drag_event(const std::shared_ptr<const Gdk::Event>& ev)
    {
        if(!img) return false;
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
                    x -= 14,y -= 45; //补偿x,y值，这是gtk框架bug
                    if(right_side_pannel->get_allocation().contains_point(x,y)) break;
                    auto x_norm = (x / area_zoom_factor - img_x) / img->get_width();
                    auto y_norm = (y / area_zoom_factor - img_y) / img->get_height();
                    cout << "ray x:" << x_norm << " y:" << y_norm << endl;

                    RayInfo hit;
                    editor->ray(x_norm,y_norm,hit);
                    
                    if(!hit.name.empty()) {
                        auto objs = scene->get_active_objs();
                        right_side_pannel->parse(objs[hit.name]);
                    }
                    
                    right_side_pannel->set_visible(!hit.name.empty());
                }

                break;
            case Gdk::Event::Type::MOTION_NOTIFY: {
                if(handled == 0) break;
                double x,y; ev->get_position(x,y);
                x -= 14,y -= 46; //补偿x,y值，这是gtk框架bug
                x =  x / area_zoom_factor - img_x, y = y / area_zoom_factor - img_y;

                // 计算出上一次鼠标位置相对于图片位置的坐标
                double x_prev,y_prev; prev_ev->get_position(x_prev,y_prev);
                x_prev -= 14,y_prev -= 46; //补偿x,y值，这是gtk框架bug
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

    RightSidePannel* right_side_pannel;
    BottomSidePannel* bottom_side_pannel;
    Gtk::DrawingArea* area;
    double area_zoom_factor = 1.0,img_x,img_y;
    std::shared_ptr<Gdk::Pixbuf> img;

    std::shared_ptr<digitaltwin::Scene> scene;
    std::shared_ptr<digitaltwin::Editor> editor;
    std::shared_ptr<digitaltwin::Workflow> workflow;
    std::shared_ptr<Gtk::EventControllerLegacy> controller;
};

class AppWindow : public Gtk::ApplicationWindow
{
public:
    AppWindow(BaseObjectType* cobject, const std::shared_ptr<Gtk::Builder>& builder)
        : Gtk::ApplicationWindow(cobject)
        , template_view(Gtk::Builder::get_widget_derived<TemplateView>(builder,"template_view"))
        , scene_view(Gtk::Builder::get_widget_derived<SceneView>(builder,"scene_view"))
        , views(builder->get_widget<Gtk::Stack>("views"))
        , view_switcher(builder->get_widget<Gtk::StackSwitcher>("view_switcher"))
        , template_page(dynamic_cast<Gtk::StackPage*>(builder->get_object("template_page").get()))
        , scene_page(dynamic_cast<Gtk::StackPage*>(builder->get_object("scene_page").get()))
        , button_anchor(builder->get_widget<Gtk::Button>("anchor"))
        , button_start(builder->get_widget<Gtk::Button>("start"))
        , button_stop(builder->get_widget<Gtk::Button>("stop"))
        , button_workflow(builder->get_widget<Gtk::Button>("workflow"))
        , button_edit(builder->get_widget<Gtk::ToggleButton>("edit"))
    {
        signal_close_request().connect([this](){ scene_view->close(); return false; },true);
        
        button_anchor->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_anchor_clicked));
        button_start->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_start_clicked));
        button_stop->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_stop_clicked));
        button_workflow->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_workflow_clicked));
        button_edit->signal_toggled().connect(sigc::mem_fun(*this,&AppWindow::on_button_edit_clicked));
        
        template_view->signal_selected.connect([this](string scene_path) { 
            views->set_visible_child("scene_page");
            button_edit->set_visible();
            button_workflow->set_visible();
            button_start->set_visible();
            button_anchor->set_visible();
            scene_view->open(scene_path);
        });

        auto controller = Gtk::GestureClick::create();
        controller->signal_pressed().connect([this](int,double,double) {
            button_edit->set_visible(false);
            button_workflow->set_visible(false);
            button_start->set_visible(false);
            button_stop->set_visible(false);
            button_anchor->set_visible(false);
            scene_view->close();
            template_view->refresh();
        });
        view_switcher->add_controller(controller);
    }

    ~AppWindow()
    {

    }

    void on_button_anchor_clicked()
    {

    }

    void on_button_start_clicked()
    {
        scene_view->workflow->start();
        button_start->set_visible(false);
        button_stop->set_visible(true);
    }

    void on_button_stop_clicked()
    {
        scene_view->workflow->stop();
        button_start->set_visible(true);
        button_stop->set_visible(false);
    }

    void on_button_workflow_clicked()
    {

    }

    void on_button_edit_clicked()
    {
        if(button_edit->get_active()) {
            cout << "edit" << endl;
        } else {
            cout << "view" << endl;
        }
    }

    std::shared_ptr<Gtk::Builder> builder;
    TemplateView* template_view;
    SceneView* scene_view;
    Gtk::Stack* views;
    Gtk::StackSwitcher* view_switcher;
    Gtk::StackPage *template_page,*scene_page;
    Gtk::Button *button_anchor,*button_start,*button_stop,*button_workflow;
    Gtk::ToggleButton *button_edit;
};

int main(int argc, char* argv[])
{
    auto app = Gtk::Application::create("xyz.diracloud.DigitalTwin");
    
    // setlocale(LC_ALL,"zh");
    // bindtextdomain("digitaltwin","po");
    // textdomain("digitaltwin");

    app->signal_activate().connect([app]() {
        // auto builder = Gtk::Builder::create_from_resource("/app.glade");
        auto builder = Gtk::Builder::create_from_file("./app.glade");
        app->add_window(*Gtk::Builder::get_widget_derived<AppWindow>(builder,"app_window"));
    });

    app->signal_shutdown().connect([]{
        
    });

    return app->run(argc, argv);
}
