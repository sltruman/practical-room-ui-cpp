#ifndef ROBOT_PROPERTIES_HPP
#define ROBOT_PROPERTIES_HPP

#include "object_properties.hpp"
#include <boost/date_time.hpp>
#include <boost/process.hpp>


struct RobotProperties : public ObjectProperties
{
    boost::filesystem::path end_effector_dir;
    sigc::connection sig_end_effector,sig_joints[8];
    Gtk::DropDown* dropdown_end_effector;
    Gtk::Scale *slider_joints[7];
    Gtk::Button *btn_file;
    Gtk::Entry *entry_route;

    Robot* robot;

    RobotProperties(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : ObjectProperties(cobject, builder)
        , end_effector_dir("end_effectors")
    {
        base_dir = "robots";
        dropdown_end_effector = builder->get_widget<Gtk::DropDown>("end_effector");
        slider_joints[0] = builder->get_widget<Gtk::Scale>("joint1");
        slider_joints[1] = builder->get_widget<Gtk::Scale>("joint2");
        slider_joints[2] = builder->get_widget<Gtk::Scale>("joint3");
        slider_joints[3] = builder->get_widget<Gtk::Scale>("joint4");
        slider_joints[4] = builder->get_widget<Gtk::Scale>("joint5");
        slider_joints[5] = builder->get_widget<Gtk::Scale>("joint6");
        slider_joints[6] = builder->get_widget<Gtk::Scale>("joint7");
        btn_file = builder->get_widget<Gtk::Button>("file");
        entry_route = builder->get_widget<Gtk::Entry>("route_joint_positions");
    }

    virtual ~RobotProperties()
    {
        
    }

    virtual void parse(ActiveObject* active_obj)
    {
        ObjectProperties::parse(active_obj);
        robot = dynamic_cast<Robot*>(active_obj);
        
        sig_end_effector.disconnect();
        auto end_effector = robot->get_end_effector();
        auto model = Gtk::StringList::create({});
        dropdown_end_effector->set_model(model);
        dropdown_end_effector->set_selected(-1);
        auto scene = active_obj->get_own_scene();
        
        boost::filesystem::path data_dir = scene->get_data_dir_path();
        for (auto fileitem : boost::filesystem::directory_iterator(data_dir / end_effector_dir)) { 
            auto dirpath = fileitem.path();
            auto dirname = dirpath.filename().string();
            auto filename = dirname + ".urdf";
            auto filepath = dirpath / filename;
            if(!boost::filesystem::exists(filepath)) continue;
            model->append(dirpath.filename().string());
            auto end_effector_path = (end_effector_dir / dirname / filename).string();
            dropdown_end_effector->set_data(dirname.c_str(),new string(end_effector_path),[](gpointer data) {delete reinterpret_cast<string*>(data);});
            if(string::npos != end_effector.find(filename)) dropdown_end_effector->set_selected(model->get_n_items()-1);
        }
        
        sig_end_effector = dropdown_end_effector->property_selected().signal_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_end_effector_changed));

        auto joints = robot->get_joints();
        for(int i = 0;i < sizeof(slider_joints) / sizeof(Gtk::Scale*);i++) {
            sig_joints[i].disconnect();
            auto row_joint = builder->get_widget<Gtk::ListBoxRow>("row_joint"+to_string(i+1));
            if (i >= joints.size()) {
                row_joint->set_visible(false);
                continue;
            }
            
            slider_joints[i]->set_value(joints[i] * 180 / M_PI);
            row_joint->set_visible(true);
            sig_joints[i] = slider_joints[i]->signal_change_value().connect(sigc::mem_fun(*this, &RobotProperties::on_joint_changed),true);
        }

        // btn_file->signal_clicked().connect(sigc::mem_fun(*this,&RobotProperties::on_file_clicked));
    }

    void on_end_effector_changed()
    {
        auto so = dynamic_pointer_cast<Gtk::StringObject>(dropdown_end_effector->get_selected_item());
        robot->set_end_effector(*reinterpret_cast<string*>(dropdown_end_effector->get_data(so->get_string())));
    }

    bool on_joint_changed(Gtk::ScrollType, double v)
    {
        robot->set_joints({
                slider_joints[0]->get_value() / 180 * M_PI,
                slider_joints[1]->get_value() / 180 * M_PI,
                slider_joints[2]->get_value() / 180 * M_PI,
                slider_joints[3]->get_value() / 180 * M_PI,
                slider_joints[4]->get_value() / 180 * M_PI,
                slider_joints[5]->get_value() / 180 * M_PI,
                slider_joints[6]->get_value() / 180 * M_PI
            });
        return false;
    }

    void on_file_clicked() 
    {
        // auto dialog = make_shared<Gtk::FileChooserDialog>("Please choose a route file of joint positions.",Gtk::FileChooser::Action::OPEN);
        // dialog->add_button("_Open", Gtk::ResponseType::OK);
        // dialog->set_modal();
        // dialog->set_visible();

        // auto filter_text = Gtk::FileFilter::create();
        // filter_text->set_name("*.json");
        // filter_text->add_pattern("*.json");
        
        // dialog->add_filter(filter_text);
        // dialog->signal_response().connect([this,dialog](int response_id) {
        //     dialog->close();

        //     if(response_id != Gtk::ResponseType::OK) return;
        //     auto route_path = dialog->get_file()->get_path();
        //     cout << response_id << ' ' << dialog->get_file()->get_path() << endl;
        //     entry_route->set_text(route_path);
        // });

    }
};

#endif