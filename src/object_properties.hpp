#ifndef OBJECT_PROPERTIES_HPP
#define OBJECT_PROPERTIES_HPP

#include "digitaltwin.hpp"
using namespace digitaltwin;

#include <locale.h>

#include <opencv2/opencv.hpp>
#include <gtkmm.h>
#include <gtkmm/eventcontrollerlegacy.h>

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>
#include <list>
#include <string>
using namespace std;

#include <boost/json.hpp>
#include <boost/filesystem.hpp>
using namespace boost;


struct ObjectProperties : public Gtk::ListBox 
{
    boost::filesystem::path base_dir;
    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::DropDown* dropdown_base;
    Gtk::SpinButton *spin_x,*spin_y,*spin_z,*spin_rx,*spin_ry,*spin_rz;
    sigc::connection sig_base,sig_x,sig_y,sig_z,sig_rx,sig_ry,sig_rz;
    ActiveObject* active_obj;

    ObjectProperties(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::ListBox(cobject),builder(builder)
        , base_dir("./objects")
    {
        dropdown_base = builder->get_widget<Gtk::DropDown>("base");
        dropdown_base->set_data("base_path",new vector<string>());
        spin_x = builder->get_widget<Gtk::SpinButton>("x");
        spin_y = builder->get_widget<Gtk::SpinButton>("y");
        spin_z = builder->get_widget<Gtk::SpinButton>("z");
        spin_rx = builder->get_widget<Gtk::SpinButton>("rx");
        spin_ry = builder->get_widget<Gtk::SpinButton>("ry");
        spin_rz = builder->get_widget<Gtk::SpinButton>("rz");
    }

    virtual ~ObjectProperties() 
    {
        
    }

    virtual void parse(ActiveObject* active_obj)
    {   
        this->active_obj = active_obj;
        
        auto entry_kind = builder->get_widget<Gtk::Label>("kind");
        entry_kind->set_text(active_obj->get_kind());

        sig_x.disconnect();
        sig_y.disconnect();
        sig_z.disconnect();
        sig_rx.disconnect();
        sig_ry.disconnect();
        sig_rz.disconnect();
        
        auto pos = active_obj->get_pos();
        spin_x->set_value(pos[0]);
        spin_y->set_value(pos[1]);
        spin_z->set_value(pos[2]);

        auto rot = active_obj->get_rot();
        spin_rx->set_value(rot[0] * 180 / M_PI);
        spin_ry->set_value(rot[1] * 180 / M_PI);
        spin_rz->set_value(rot[2] * 180 / M_PI);

        sig_x = spin_x->signal_value_changed().connect(sigc::mem_fun(*this, &ObjectProperties::on_pos_changed));
        sig_y = spin_y->signal_value_changed().connect(sigc::mem_fun(*this, &ObjectProperties::on_pos_changed));
        sig_z = spin_z->signal_value_changed().connect(sigc::mem_fun(*this, &ObjectProperties::on_pos_changed));
        sig_rx = spin_rx->signal_value_changed().connect(sigc::mem_fun(*this, &ObjectProperties::on_rot_changed));
        sig_ry = spin_ry->signal_value_changed().connect(sigc::mem_fun(*this, &ObjectProperties::on_rot_changed));
        sig_rz = spin_rz->signal_value_changed().connect(sigc::mem_fun(*this, &ObjectProperties::on_rot_changed));

        sig_base.disconnect();
        auto base = active_obj->get_base();
        auto model = Gtk::StringList::create({});
        dropdown_base->set_model(model);
        dropdown_base->set_selected(-1);
        auto scene = active_obj->get_own_scene();
        
        boost::filesystem::path data_dir = scene->get_data_dir_path();
        for (auto fileitem : boost::filesystem::directory_iterator(data_dir / base_dir)) { 
            auto dirpath = fileitem.path();
            auto dirname = dirpath.filename().string();
            auto filename = dirname + ".urdf";
            auto filepath = dirpath / filename;
            if(!boost::filesystem::exists(filepath)) continue;
            model->append(dirpath.filename().string());
            auto base_path = (base_dir / dirname / filename).string();
            dropdown_base->set_data(dirname.c_str(),new string(base_path),[](gpointer data) {delete reinterpret_cast<string*>(data);});
            if(string::npos != base.find(filename)) dropdown_base->set_selected(model->get_n_items()-1);
        }
        
        sig_base = dropdown_base->property_selected().signal_changed().connect(sigc::mem_fun(*this, &ObjectProperties::on_base_changed));
    }

    void on_base_changed()
    {
        auto so = dynamic_pointer_cast<Gtk::StringObject>(dropdown_base->get_selected_item());
        active_obj->set_base(*reinterpret_cast<string*>(dropdown_base->get_data(so->get_string())));
    }

    void on_pos_changed()
    {
        active_obj->set_pos({spin_x->get_value(),spin_y->get_value(),spin_z->get_value()});
    }

    void on_rot_changed()
    {
        active_obj->set_rot({spin_rx->get_value() / 180 * M_PI,spin_ry->get_value() / 180 * M_PI,spin_rz->get_value() / 180 * M_PI});
    }
    
};

#endif