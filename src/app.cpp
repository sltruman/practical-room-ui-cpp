#include <locale.h>

#include <opencv2/opencv.hpp>
#include <gtkmm.h>
#include <gtkmm/eventcontrollerlegacy.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <string>
#include <memory>
using namespace std;

#include <boost/json.hpp>
#include <boost/filesystem.hpp>
using namespace boost;

#include "digitaltwin.hpp"
using namespace digitaltwin;

#include "scene_view.hpp"

struct TemplateView : public Gtk::ScrolledWindow
{
    sigc::signal<void(string)> signal_selected;

    Glib::RefPtr<Gtk::FlowBox> template_list;
    Glib::RefPtr<Gtk::Button> template_0,template_1,template_2,template_3,template_4;

    TemplateView(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::ScrolledWindow(cobject)
        , template_list(builder->get_widget<Gtk::FlowBox>("template_list"))
        , template_0(builder->get_widget<Gtk::Button>("template_0"))
        , template_1(builder->get_widget<Gtk::Button>("template_1"))
        , template_2(builder->get_widget<Gtk::Button>("template_2"))
        , template_3(builder->get_widget<Gtk::Button>("template_3"))
        , template_4(builder->get_widget<Gtk::Button>("template_4"))
    {
        template_0->signal_clicked().connect(sigc::bind(sigc::mem_fun(signal_selected,&sigc::signal<void(string)>::emit),"data/scenes/空.json"));
        template_1->signal_clicked().connect(sigc::bind(sigc::mem_fun(signal_selected,&sigc::signal<void(string)>::emit),"data/scenes/深度图.json"));
        template_2->signal_clicked().connect(sigc::bind(sigc::mem_fun(signal_selected,&sigc::signal<void(string)>::emit),"data/scenes/姿态估计.json"));
        template_3->signal_clicked().connect(sigc::bind(sigc::mem_fun(signal_selected,&sigc::signal<void(string)>::emit),"data/scenes/混合拆垛.json"));
        template_4->signal_clicked().connect(sigc::bind(sigc::mem_fun(signal_selected,&sigc::signal<void(string)>::emit),"data/scenes/无序抓取.json"));
    };
};

struct SceneView : public Gtk::Overlay 
{
    SceneView(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::Overlay(cobject)
    {
        area = builder->get_widget<Gtk::DrawingArea>("simulation");

        right_side_pannel = builder->get_widget<Gtk::ScrolledWindow>("right_side_pannel");
        add_overlay(*right_side_pannel);

        properties = Gtk::Builder::get_widget_derived<ObjectProperties>(builder,"properties");
        
        auto controller = Gtk::EventControllerLegacy::create();
        controller->signal_event().connect(sigc::mem_fun(*this,&SceneView::area_drag_event),false);
        add_controller(controller);
    }

    ~SceneView()
    {
        
    }

    void open(string scene_path) {
        img.reset();
        workflow.reset();
        editor.reset();
        scene.reset();

        scene = make_shared<Scene>(800,640,scene_path);
        editor = make_shared<Editor>(scene.get());
        workflow = make_shared<Workflow>(scene.get());
        scene->set_log_func([](char l,string s){
            cout << l << ' ' << s << endl;
        });

        area->set_draw_func(sigc::mem_fun(*this, &SceneView::area_paint_event));

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
                    x -= 14,y -= 46; //补偿x,y值，这是gtk框架bug
                    if(right_side_pannel->get_allocation().contains_point(x,y)) break;
                    auto x_norm = (x / area_zoom_factor - img_x) / img->get_width();
                    auto y_norm = (y / area_zoom_factor - img_y) / img->get_height();

                    auto hit = editor->ray(x_norm,y_norm);
                    
                    if(!hit.name.empty()) {
                        auto active_obj = editor->select(hit.name);
                        properties->parse(active_obj);
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

    ObjectProperties* properties;
    Gtk::ScrolledWindow* right_side_pannel;
    Gtk::DrawingArea* area;
    double area_zoom_factor = 1.0,img_x,img_y;
    std::shared_ptr<Gdk::Pixbuf> img;

    std::shared_ptr<digitaltwin::Scene> scene;
    std::shared_ptr<digitaltwin::Editor> editor;
    std::shared_ptr<digitaltwin::Workflow> workflow;
};

class AppWindow : public Gtk::ApplicationWindow
{
public:
    AppWindow(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::ApplicationWindow(cobject)
        , template_view(Gtk::Builder::get_widget_derived<TemplateView>(builder,"template_view"))
        , scene_view(Gtk::Builder::get_widget_derived<SceneView>(builder,"scene_view"))
        , views(builder->get_widget<Gtk::Stack>("views"))
        , view_switcher(builder->get_widget<Gtk::StackSwitcher>("view_switcher"))
        , scene_page(dynamic_cast<Gtk::StackPage*>(builder->get_object("scene_page").get()))
        , button_anchor(builder->get_widget<Gtk::Button>("anchor"))
        , button_start(builder->get_widget<Gtk::Button>("start"))
        , button_stop(builder->get_widget<Gtk::Button>("stop"))
        , button_workflow(builder->get_widget<Gtk::Button>("workflow"))

    {
        button_anchor->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_anchor_clicked));
        button_start->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_start_clicked));
        button_stop->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_stop_clicked));
        button_workflow->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_workflow_clicked));
        template_view->signal_selected.connect([this](string) { views->set_visible_child("scene_page"); });
        template_view->signal_selected.connect(sigc::mem_fun(*scene_view,&SceneView::open));
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
        scene_view->workflow->get_active_obj_nodes();

        auto obj = scene_view->editor->add("Robot","data/robots/franka_panda/franka_panda.urdf",{2,0,0},{0,0,0},1);
        auto pos = obj->get_pos();
        cout << pos[0] << ' ' << pos[1] << ' ' << pos[2] << endl;
    }

    Glib::RefPtr<Gtk::Builder> builder;
    TemplateView* template_view;
    SceneView* scene_view;
    Gtk::Stack* views;
    Gtk::StackSwitcher* view_switcher;
    Gtk::StackPage* scene_page;
    Gtk::Button *button_anchor,*button_start,*button_stop,*button_workflow;
};

int main(int argc, char* argv[])
{
    auto app = Gtk::Application::create("xyz.diracloud.DigitalTwin");
    
    setlocale(LC_ALL,"zh");
    bindtextdomain("digitaltwin","po");
    textdomain("digitaltwin");

    app->signal_activate().connect([app]() {
        // auto builder = Gtk::Builder::create_from_resource("/app.glade");
        auto builder = Gtk::Builder::create_from_file("./app.glade");
        builder->add_from_file("./template_view.glade");
        builder->add_from_file("./scene_view.glade");
        app->add_window(*Gtk::Builder::get_widget_derived<AppWindow>(builder,"app_window"));
    });

    return app->run(argc, argv);
}
