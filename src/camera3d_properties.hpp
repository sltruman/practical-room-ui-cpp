#ifndef CAMERA3D_PROPERTIES_HPP
#define CAMERA3D_PROPERTIES_HPP

#include "object_properties.hpp"
#include <boost/date_time.hpp>
#include <boost/process.hpp>
#include <opencv2/opencv.hpp>

struct  Camera3DProperties : public ObjectProperties
{
    Camera3D* camera;
    Gtk::SpinButton *spin_fov,*spin_focal;
    Gtk::Button *btn_rtt;
    Gtk::Label *label_save_path;
    Gtk::Picture *area_texture,*area_texture2;
    sigc::connection rtt_signal_click,sig_fov,sig_focal;
    
    Camera3DProperties(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : ObjectProperties(cobject, builder)
    {
        base_dir = "cameras";
        spin_focal = builder->get_widget<Gtk::SpinButton>("focal");

        btn_rtt = builder->get_widget<Gtk::Button>("capture");
        label_save_path = builder->get_widget<Gtk::Label>("save");
        area_texture = builder->get_widget<Gtk::Picture>("texture");
        area_texture2 = builder->get_widget<Gtk::Picture>("texture_depth");
        rtt_signal_click = btn_rtt->signal_clicked().connect(sigc::mem_fun(*this, &Camera3DProperties::on_capture_clicked));
    }

    virtual ~Camera3DProperties()
    {
        
    }

    Texture texture;
    
    cv::Mat rgba,depth,gray,rgb;
    virtual void parse(ActiveObject* active_obj)
    {
        ObjectProperties::parse(active_obj);
        camera = dynamic_cast<Camera3D*>(active_obj);
        camera->draw_roi();

        sig_focal.disconnect();
        spin_focal->set_value(camera->focal);
        sig_focal = spin_focal->signal_value_changed().connect(sigc::mem_fun(*this, &Camera3DProperties::on_focal_changed));

        Glib::signal_timeout().connect_once([this]() {
            auto aspect_ratio_viewport = 1. * camera->pixels_w / camera->pixels_h;
            int area_w = area_texture->get_width();
            int area_h = area_w / aspect_ratio_viewport;

            camera->rtt(texture);

            rgba = cv::Mat(cv::Size(texture.width,texture.height),CV_8UC4);
            depth = cv::Mat(cv::Size(texture.width,texture.height),CV_32FC1);

            memcpy(rgba.ptr<unsigned char>(), texture.rgba_pixels, texture.width * texture.height *4);
            cv::cvtColor(rgba,rgba,cv::COLOR_RGBA2BGRA);

            memcpy(depth.ptr<unsigned char>(), texture.depth_pixels, texture.width * texture.height *4);
            
            double min,max;
            cv::minMaxIdx(depth, &min, &max);
            cv::convertScaleAbs(depth, gray, 255 / max);
            cv::cvtColor(gray,rgb,cv::COLOR_GRAY2RGB);

            auto img = Gdk::Pixbuf::create_from_data(texture.rgba_pixels,Gdk::Colorspace::RGB,true,8,texture.width,texture.height,texture.width*4);
            area_texture->set_pixbuf(img);
            area_texture->set_size_request(area_w,area_h);
            
            auto img2 = Gdk::Pixbuf::create_from_data(rgb.ptr<unsigned char>(),Gdk::Colorspace::RGB,false,8,texture.width,texture.height,texture.width*3);
            area_texture2->set_pixbuf(img2);
            area_texture2->set_size_request(area_w,area_h);
        },500); 
    }

    void on_focal_changed() 
    {
        camera->set_focal(spin_focal->get_value());
        camera->draw_roi();
    }

    void on_capture_clicked() 
    {
        camera->rtt(texture);

        rgba = cv::Mat(cv::Size(texture.width,texture.height),CV_8UC4);
        depth = cv::Mat(cv::Size(texture.width,texture.height),CV_32FC1);

        memcpy(rgba.ptr<unsigned char>(), texture.rgba_pixels, texture.width * texture.height *4);
        cv::cvtColor(rgba,rgba,cv::COLOR_RGBA2BGRA);

        memcpy(depth.ptr<unsigned char>(), texture.depth_pixels, texture.width * texture.height *4);
        
        double min,max;
        cv::minMaxIdx(depth, &min, &max);
        cv::convertScaleAbs(depth, gray, 255 / max);
        cv::cvtColor(gray,rgb,cv::COLOR_GRAY2RGB);

        auto env = this_process::environment();
        auto name = camera->get_name();
        std::filesystem::path dir = env["HOME"].to_string();
        auto pictures_dir = dir / "Pictures" / name;
        std::filesystem::create_directories(pictures_dir);
        label_save_path->set_label("Images have been saved to '" + pictures_dir.string() + "'");

        auto filename = boost::posix_time::to_iso_string(boost::posix_time::second_clock::local_time());
        auto rgba_path = pictures_dir / (filename + ".png");
        auto depth_path = pictures_dir / (filename + ".tiff");

        auto aspect_ratio_viewport = 1. * camera->pixels_w / camera->pixels_h;
        int area_w = area_texture->get_width();
        int area_h = area_w / aspect_ratio_viewport;

        auto img = Gdk::Pixbuf::create_from_data(texture.rgba_pixels,Gdk::Colorspace::RGB,true,8,texture.width,texture.height,texture.width*4);
        area_texture->set_pixbuf(img);
        area_texture->set_size_request(area_w,area_h);
        
        auto img2 = Gdk::Pixbuf::create_from_data(rgb.ptr<unsigned char>(),Gdk::Colorspace::RGB,false,8,texture.width,texture.height,texture.width*3);
        area_texture2->set_pixbuf(img2);
        area_texture2->set_size_request(area_w,area_h);

        Glib::signal_timeout().connect_seconds_once([this,rgba_path](){ cv::imwrite(rgba_path.string(),rgba); },1);
        Glib::signal_timeout().connect_seconds_once([this,depth_path](){ cv::imwrite(depth_path.string(),depth); },2);
        Glib::signal_timeout().connect_seconds_once([this](){ label_save_path->set_label(""); },3);
    }
};

#endif