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

#include "scene_view.hpp"

struct TemplateView : public Gtk::ScrolledWindow
{
    sigc::signal<void()> signal_selected;

    Glib::RefPtr<Gtk::FlowBox> template_list;
    Glib::RefPtr<Gtk::Button> template_1;
    Glib::RefPtr<Gtk::Button> template_2;
    Glib::RefPtr<Gtk::Button> template_3;
    Glib::RefPtr<Gtk::Button> template_4;

    TemplateView(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::ScrolledWindow(cobject)
        , template_list(builder->get_widget<Gtk::FlowBox>("template_list"))
        , template_1(builder->get_widget<Gtk::Button>("template_1"))
        , template_2(builder->get_widget<Gtk::Button>("template_2"))
        , template_3(builder->get_widget<Gtk::Button>("template_3"))
        , template_4(builder->get_widget<Gtk::Button>("template_4"))
    {
        template_1->signal_clicked().connect(sigc::mem_fun(signal_selected,&sigc::signal<void()>::emit));
        template_2->signal_clicked().connect(sigc::mem_fun(signal_selected,&sigc::signal<void()>::emit));
        template_3->signal_clicked().connect(sigc::mem_fun(signal_selected,&sigc::signal<void()>::emit));
        template_4->signal_clicked().connect(sigc::mem_fun(signal_selected,&sigc::signal<void()>::emit));
    }
};

struct SceneView : public Gtk::Overlay 
{
    SceneView(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::Overlay(cobject)
    {
        area = builder->get_widget<Gtk::DrawingArea>("simulation");
        right_side_pannel = builder->get_widget<Gtk::ScrolledWindow>("right_side_pannel");
        properties = Gtk::Builder::get_widget_derived<ObjectProperties>(builder,"properties");
    }

    ~SceneView()
    {
        delete workflow;
        delete editor;
        delete scene;
    }

    void open() {
        cout << "11111111111111111" << endl;
        // auto dialog = make_shared<Gtk::FileChooserDialog>("Please choose a file",Gtk::FileChooser::Action::OPEN);
        // dialog->add_button("_Open", Gtk::ResponseType::OK);
        // // dialog->set_transient_for();
        // dialog->set_modal();
        // dialog->set_visible();
        // auto filter_text = Gtk::FileFilter::create();
        // filter_text->set_name("*.json");
        // filter_text->add_pattern("*.json");
        // dialog->add_filter(filter_text);

        // dialog->signal_response().connect([this,dialog](int response_id) {
        //     dialog->close();

        //     if(response_id != Gtk::ResponseType::OK) return;
        //     auto scene_path = dialog->get_file()->get_path();
        //     cout << response_id << ' ' << dialog->get_file()->get_path() << endl;
            
        //     scene = new Scene(800,640,scene_path);
        //     editor = new Editor(scene);
        //     workflow = new Workflow(scene);
        //     scene_view->area->set_draw_func(sigc::mem_fun(*scene_view, &SceneView::area_paint_event));
        //     // Robot* robot = dynamic_cast<Robot*>(scene->get_active_objs()["robot"]);

        //     // Camera3D* camera = dynamic_cast<Camera3D*>(scene->get_active_objs()["camera"]);
        //     // camera->set_rtt_func([](vector<unsigned char> rgba_pixels,vector<float> depth_pixels,int width,int height) {
        //     //     cv::Mat rgba = cv::Mat::zeros(cv::Size(width,height),CV_8UC4);
        //     //     memcpy(rgba.ptr<unsigned char>(),rgba_pixels.data(),rgba_pixels.size());
        //     //     cv::cvtColor(rgba,rgba,cv::COLOR_RGBA2BGRA);

        //     //     cv::Mat depth = cv::Mat::zeros(cv::Size(width,height),CV_32FC1);
        //     //     memcpy(depth.ptr<unsigned char>(),depth_pixels.data(),depth_pixels.size() * 4);
        //     //     cv::Mat gray = cv::Mat::zeros(cv::Size(width,height),CV_8UC1);
        //     //     depth.convertTo(gray, CV_8U, 255.0);
        //     //     cv::imwrite("rgba.png",rgba);
        //     //     cv::imwrite("depth.png",gray);
        //     // });
            
        //     // Camera3DReal* camera = dynamic_cast<Camera3DReal*>(scene->get_active_objs()["camera"]);
        //     // camera->set_calibration("[[3507.176621132752,0.0,1218.55397352377],[0.0,3506.5191735910958,1038.1997907533078],[0.0,0.0,1.0]]",
        //     //                         "[[0.03775400213484963,0.9992836506766881,-0.002611668637494993,0.18975821360828188],[0.9946931066717141,-0.03783062740787925,-0.09567897976933247,-0.8562761592934359],[-0.09570924126005752,0.0010144556158444772,-0.995408816525767,1.1566388127904739],[0,0,0,1]]");

        //     // camera->set_rtt_func([](vector<unsigned char>& rgb_pixels,vector<float>& depth_pixels,int& width,int& height) {
        //     //     width = 1920,height = 1200;

        //     //     std::string sRGBFilePath = "/home/truman/Downloads/20230322110752009.png";
        //     //     cv::Mat rgbMat = cv::imread(sRGBFilePath);
        //     //     rgb_pixels = rgbMat.reshape(1,1);
                
        //     //     std::string sDepthFilePath = "/home/truman/Downloads/20230322110752009.tiff";
        //     //     cv::Mat depthMat = cv::imread(sDepthFilePath, cv::IMREAD_ANYDEPTH);
        //     //     depth_pixels = depthMat.reshape(1,1);

        //     //     width = rgbMat.cols;
        //     //     height = rgbMat.rows;
        //     //     return true;
        //     // });

        //     // workflow->add_active_obj_node("Vision","PickLight","detect",[](){
        //     //     return "[[[-0.6109328215852812,-0.770627694607955,-0.18136690351319718,-0.16305102293176799],[-0.7829483866689572,0.6220597556471429,-0.0057755018884309605,-0.03409599941935853],[0.11727181739493445,0.13847249629497058,-0.9833984376293907,1.2207300122066735],[0.0,0.0,0.0,1.0]]]";
        //     // });
        
        //     // camera->rtt();

        //     auto controller = Gtk::EventControllerLegacy::create();
        //     controller->signal_event().connect(sigc::mem_fun(*scene_view,&SceneView::area_drag_event),false);
        //     scene_view->add_controller(controller);
        // });

    }


    // int handled = 0;
    // Glib::RefPtr<const Gdk::Event> prev_ev;
    // bool area_drag_event(const Glib::RefPtr<const Gdk::Event>& ev)
    // { 
    //     auto type = ev->get_event_type();
    //     auto button = ev->get_button();

    //     switch (type) {
    //         case Gdk::Event::Type::SCROLL:
    //             scene->zoom(ev->get_direction() == Gdk::ScrollDirection::UP ? 0.8 : 1.2);
    //             break;
    //         case Gdk::Event::Type::BUTTON_PRESS:
    //             if(handled != 0 && handled != button) break;
    //             handled = button;
    //             prev_ev = ev;

    //             if(handled == 1) {
    //                 // 计算出鼠标位置相对于图片位置的坐标
    //                 double x,y; ev->get_position(x,y);
    //                 x -= 14,y -= get_titlebar()->get_height(); //补偿x,y值，这是gtk框架bug
    //                 if(right_side_pannel->get_allocation().contains_point(x,y)) break;

    //                 auto x_norm = (x / area_zoom_factor - img_x) / img->get_width();
    //                 auto y_norm = (y / area_zoom_factor - img_y) / img->get_height();
    //                 auto hit = editor->ray(x_norm,y_norm);
                    
    //                 if(!hit.name.empty()) {
    //                     auto active_obj = editor->select(hit.name);
    //                     properties->parse(active_obj);
    //                 }
                    
    //                 right_side_pannel->set_visible(!hit.name.empty());
    //             }

    //             break;
    //         case Gdk::Event::Type::MOTION_NOTIFY: {
    //             if(handled == 0) break;
    //             double x,y; ev->get_position(x,y);
    //             x -= 14,y -= get_titlebar()->get_height(); //补偿x,y值，这是gtk框架bug
    //             x =  x / area_zoom_factor - img_x, y = y / area_zoom_factor - img_y;
                
    //             // 计算出上一次鼠标位置相对于图片位置的坐标
    //             double x_prev,y_prev; prev_ev->get_position(x_prev,y_prev);
    //             x_prev -= 14,y_prev -= get_titlebar()->get_height(); //补偿x,y值，这是gtk框架bug
    //             x_prev = x_prev / area_zoom_factor - img_x, y_prev = y_prev / area_zoom_factor - img_y;

    //             // 将两次鼠标位置的差值转化为百分比
    //             auto x_offset_norm = (x - x_prev) / img->get_width();
    //             auto y_offset_norm = (y - y_prev) / img->get_height();

    //             switch(handled) {
    //                 case 2:
    //                     scene->rotate(x_offset_norm,y_offset_norm);
    //                     break;
    //                 case 3:
    //                     scene->pan(x_offset_norm,y_offset_norm);
    //                     break;
    //             }
                
    //             prev_ev = ev;
    //             break;
    //         }
    //         case Gdk::Event::Type::BUTTON_RELEASE:
    //             handled = 0;
    //             break;
    //     }

    //     return false;
    // }

    // void area_paint_event(const Cairo::RefPtr<Cairo::Context>& cr, int area_w, int area_h)
    // {
    //     Glib::signal_timeout().connect_once([this]() { area->queue_draw(); }, 1000 / 24);
        
    //     cr->set_source_rgb(40 / 255.,40 / 255.,40 / 255.);
    //     cr->paint();

    //     auto texture = scene->rtt();
    //     if(texture.rgba_pixels == nullptr) return;

    //     auto aspect_ratio = 1. * texture.width / texture.height;
    //     auto aspect_ratio2 = 1. * area_w / area_h;
    //     auto factor = 1.0;
    //     if (aspect_ratio > aspect_ratio2) factor = 1. * area_w / texture.width;
    //     else factor = 1. * area_h / texture.height;
    //     cr->scale(factor,factor);
 
    //     img_x = (area_w / factor - texture.width) / 2;
    //     img_y = (area_h / factor - texture.height) / 2;

    //     if(!img) img = Gdk::Pixbuf::create_from_data(texture.rgba_pixels,Gdk::Colorspace::RGB,true,8,texture.width,texture.height,texture.width*4);
    //     Gdk::Cairo::set_source_pixbuf(cr, img,img_x,img_y);
    //     cr->paint();

    //     area_zoom_factor = factor;
    // }

    ObjectProperties* properties;
    Gtk::ScrolledWindow* right_side_pannel;
    Gtk::DrawingArea* area;
    Glib::RefPtr<Gdk::Pixbuf> img;
    double area_zoom_factor = 1.0,img_x,img_y;

    digitaltwin::Scene* scene;
    digitaltwin::Editor* editor;
    digitaltwin::Workflow* workflow;
};

class AppWindow : public Gtk::ApplicationWindow
{
public:
    AppWindow(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::ApplicationWindow(cobject)
        , template_view(Gtk::Builder::get_widget_derived<TemplateView>(builder,"template_view"))
        , scene_view(Gtk::Builder::get_widget_derived<SceneView>(builder,"scene_view"))
        , views(builder->get_widget<Gtk::Stack>("views"))
    {
        auto button_open = builder->get_widget<Gtk::Button>("open");
        button_open->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_open_clicked));
        auto button_start = builder->get_widget<Gtk::Button>("start");
        button_start->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_start_clicked));
        auto button_stop = builder->get_widget<Gtk::Button>("stop");
        button_stop->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_stop_clicked));
        auto button_workflow = builder->get_widget<Gtk::Button>("workflow");
        button_workflow->signal_clicked().connect(sigc::mem_fun(*this,&AppWindow::on_button_workflow_clicked));

        template_view->signal_selected.connect(sigc::mem_fun(*scene_view,&SceneView::open));
    }

    ~AppWindow()
    {

    }

    void on_button_open_clicked()
    {
        
    }

    void on_button_start_clicked()
    {
        // workflow->start();
        // builder->get_widget<Gtk::Button>("start")->set_visible(false);
        // builder->get_widget<Gtk::Button>("stop")->set_visible(true);
    }

    void on_button_stop_clicked()
    {
        // workflow->stop();
        // builder->get_widget<Gtk::Button>("start")->set_visible(true);
        // builder->get_widget<Gtk::Button>("stop")->set_visible(false);
    }

    void on_button_workflow_clicked()
    {

    }

    Glib::RefPtr<Gtk::Builder> builder;
    TemplateView* template_view;
    SceneView* scene_view;
    Gtk::Stack* views;
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
