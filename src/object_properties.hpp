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

#include "digitaltwin.hpp"
using namespace digitaltwin;



struct ObjectProperties : public Gtk::ListBox 
{
    const boost::filesystem::path base_dir_path;
    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::DropDown* dropdown_base;
    sigc::connection sig_base;
    ActiveObject* active_obj;

    ObjectProperties(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::ListBox(cobject),builder(builder)
        , base_dir_path("./objects")
    {
        dropdown_base = builder->get_widget<Gtk::DropDown>("base");
    }

    sigc::connection base_signal_changed,end_effector_signal_changed,rtt_signal_click;

    void parse(ActiveObject* active_obj)
    {
        this->active_obj = active_obj;

        auto entry_kind = builder->get_widget<Gtk::Label>("kind");
        
        auto spin_x = builder->get_object<Gtk::SpinButton>("x");
        auto spin_y = builder->get_object<Gtk::SpinButton>("y");
        auto spin_z = builder->get_object<Gtk::SpinButton>("z");
        auto spin_rx = builder->get_object<Gtk::SpinButton>("rx");
        auto spin_ry = builder->get_object<Gtk::SpinButton>("ry");
        auto spin_rz = builder->get_object<Gtk::SpinButton>("rz");

        entry_kind->set_text(active_obj->get_kind());

        auto pos = active_obj->get_pos();
        spin_x->set_value(pos[0]);
        spin_y->set_value(pos[1]);
        spin_z->set_value(pos[2]);

        auto rot = active_obj->get_rot();
        spin_rx->set_value(rot[0] * 180 / M_PI);
        spin_ry->set_value(rot[1] * 180 / M_PI);
        spin_rz->set_value(rot[2] * 180 / M_PI);

        sig_base.disconnect();
        auto base = active_obj->get_base();
        auto model = Gtk::StringList::create({});
        dropdown_base->set_model(model);
        dropdown_base->set_selected(-1);
        auto scene = active_obj->get_own_scene();
        
        boost::filesystem::path data_dir = scene->get_data_dir_path();
        for (auto fileitem : boost::filesystem::directory_iterator(data_dir / base_dir_path)) { 
            auto dirpath = fileitem.path();
            auto dirname = dirpath.filename().string();
            auto filename = dirpath.filename().string() + ".urdf";
            auto filepath = dirpath / filename;
            if(!boost::filesystem::exists(filepath)) continue;
            model->append(dirpath.filename().string());

            auto base_path = base_dir_path / dirname / filename;
            dropdown_base->set_data(dirname.c_str(),new string(base_path.string()));
            if(string::npos != base.find(filename)) dropdown_base->set_selected(model->get_n_items()-1);
        }
        
        sig_base = dropdown_base->property_selected().signal_changed().connect(sigc::mem_fun(*this, &ObjectProperties::on_base_changed));
    }

    void on_base_changed()
    {
        auto so = dynamic_pointer_cast<Gtk::StringObject>(dropdown_base->get_selected_item());
        cout << so->get_string() << endl;
        auto s = reinterpret_cast<string*>(dropdown_base->get_data(so->get_string()));
        active_obj->set_base(*s);
        delete s;
    }

};