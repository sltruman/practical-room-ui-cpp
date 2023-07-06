#ifndef ROBOT_PROPERTIES_HPP
#define ROBOT_PROPERTIES_HPP

#include "object_properties.hpp"
#include <boost/date_time.hpp>
#include <boost/process.hpp>


struct RobotProperties : public ObjectProperties
{
    std::filesystem::path end_effector_dir;
    sigc::connection sig_end_effector,sig_digital_output,sig_joints[7],sig_ee_x,sig_ee_y,sig_ee_z,sig_ee_rx,sig_ee_ry,sig_ee_rz;
    Gtk::SpinButton *spin_ee_x,*spin_ee_y,*spin_ee_z,*spin_ee_rx,*spin_ee_ry,*spin_ee_rz;
    Gtk::Switch *sw_digital_output;
    Gtk::DropDown* dropdown_end_effector;
    Gtk::Scale *slider_joints[7];
    Gtk::Button *btn_set_home,*btn_go_home;

    Robot* robot;

    RobotProperties(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : ObjectProperties(cobject, builder)
        , end_effector_dir("end_effectors")
    {
        base_dir = "robots";
        dropdown_end_effector = builder->get_widget<Gtk::DropDown>("end_effector");
        sw_digital_output = builder->get_widget<Gtk::Switch>("ee_do");
        slider_joints[0] = builder->get_widget<Gtk::Scale>("joint1");
        slider_joints[1] = builder->get_widget<Gtk::Scale>("joint2");
        slider_joints[2] = builder->get_widget<Gtk::Scale>("joint3");
        slider_joints[3] = builder->get_widget<Gtk::Scale>("joint4");
        slider_joints[4] = builder->get_widget<Gtk::Scale>("joint5");
        slider_joints[5] = builder->get_widget<Gtk::Scale>("joint6");
        slider_joints[6] = builder->get_widget<Gtk::Scale>("joint7");
        spin_ee_x = builder->get_widget<Gtk::SpinButton>("ee_x");
        spin_ee_y = builder->get_widget<Gtk::SpinButton>("ee_y");
        spin_ee_z = builder->get_widget<Gtk::SpinButton>("ee_z");
        spin_ee_rx = builder->get_widget<Gtk::SpinButton>("ee_rx");
        spin_ee_ry = builder->get_widget<Gtk::SpinButton>("ee_ry");
        spin_ee_rz = builder->get_widget<Gtk::SpinButton>("ee_rz");
        btn_set_home = builder->get_widget<Gtk::Button>("set_home");
        btn_go_home = builder->get_widget<Gtk::Button>("go_home");
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
        
        std::filesystem::path data_dir = scene->get_data_dir_path();
        for (auto fileitem : std::filesystem::directory_iterator(data_dir / end_effector_dir)) { 
            auto dirpath = fileitem.path();
            auto dirname = dirpath.filename().string();
            auto filename = dirname + ".urdf";
            auto filepath = dirpath / filename;
            if(!std::filesystem::exists(filepath)) continue;
            model->append(dirpath.filename().string());
            auto end_effector_path = (end_effector_dir / dirname / filename).string();
            dropdown_end_effector->set_data(dirname.c_str(),new string(end_effector_path),[](gpointer data) {delete reinterpret_cast<string*>(data);});
            if(string::npos != end_effector.find(filename)) dropdown_end_effector->set_selected(model->get_n_items()-1);
        }
        
        sig_end_effector = dropdown_end_effector->property_selected().signal_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_end_effector_changed));

        sig_digital_output.disconnect();

        sig_digital_output = sw_digital_output->property_active().signal_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_digital_output_changed));

        sync_joints();
        sync_ee_pose();

        btn_set_home->signal_clicked().connect(sigc::mem_fun(*this,&RobotProperties::on_set_home_clicked));
        btn_go_home->signal_clicked().connect(sigc::mem_fun(*this,&RobotProperties::on_go_home_clicked));
    }

    void sync_joints() 
    {
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
    }

    void sync_ee_pose()
    {
        sig_ee_x.disconnect();
        sig_ee_y.disconnect();
        sig_ee_z.disconnect();
        sig_ee_rx.disconnect();
        sig_ee_ry.disconnect();
        sig_ee_rz.disconnect();
        
        auto pos = robot->get_end_effector_pos();
        spin_ee_x->set_value(pos[0]);
        spin_ee_y->set_value(pos[1]);
        spin_ee_z->set_value(pos[2]);

        auto rot = robot->get_end_effector_rot();
        spin_ee_rx->set_value(rot[0] * 180 / M_PI);
        spin_ee_ry->set_value(rot[1] * 180 / M_PI);
        spin_ee_rz->set_value(rot[2] * 180 / M_PI);

        sig_ee_x = spin_ee_x->signal_value_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_ee_pos_changed));
        sig_ee_y = spin_ee_y->signal_value_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_ee_pos_changed));
        sig_ee_z = spin_ee_z->signal_value_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_ee_pos_changed));
        sig_ee_rx = spin_ee_rx->signal_value_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_ee_rot_changed));
        sig_ee_ry = spin_ee_ry->signal_value_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_ee_rot_changed));
        sig_ee_rz = spin_ee_rz->signal_value_changed().connect(sigc::mem_fun(*this, &RobotProperties::on_ee_rot_changed));
    }

    void on_end_effector_changed()
    {
        auto so = dynamic_pointer_cast<Gtk::StringObject>(dropdown_end_effector->get_selected_item());
        robot->set_end_effector(*reinterpret_cast<string*>(dropdown_end_effector->get_data(so->get_string())));
    }

    void  on_digital_output_changed()
    {
        robot->digital_output(sw_digital_output->get_active());
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

        sync_ee_pose();
        
        return false;
    }

    void on_ee_pos_changed()
    {
        robot->set_end_effector_pos({spin_ee_x->get_value(),spin_ee_y->get_value(),spin_ee_z->get_value()});
        sync_joints();
    }

    void on_ee_rot_changed()
    {
        robot->set_end_effector_rot({spin_ee_rx->get_value() / 180 * M_PI,spin_ee_ry->get_value() / 180 * M_PI,spin_ee_rz->get_value() / 180 * M_PI});
        sync_joints();
    }

    void on_set_home_clicked()
    {
        robot->set_home();
    }

    void on_go_home_clicked()
    {
        robot->home();
    }

    void x()
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