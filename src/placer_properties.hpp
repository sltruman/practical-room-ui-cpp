#ifndef PLACER_PROPERTIES_HPP
#define PLACER_PROPERTIES_HPP

#include "object_properties.hpp"
#include <boost/date_time.hpp>
#include <boost/process.hpp>


struct PlacerProperties : public ObjectProperties
{
    std::filesystem::path workpieces_dir,workpiece_textures_dir;
    sigc::connection sig_workpiece,sig_workpiece_texture;
    Gtk::DropDown *dropdown_workpiece,*dropdown_workpiece_texture;
    
    Placer* placer;

    PlacerProperties(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : ObjectProperties(cobject, builder)
        , workpieces_dir("workpieces")
        , workpiece_textures_dir("workpiece_textures")
    {
        base_dir = "containers";
        dropdown_workpiece = builder->get_widget<Gtk::DropDown>("workpiece");
        dropdown_workpiece_texture = builder->get_widget<Gtk::DropDown>("workpiece_texture");
    }

    virtual ~PlacerProperties()
    {
        
    }

    virtual void parse(ActiveObject* active_obj)
    {
        ObjectProperties::parse(active_obj);
        placer = dynamic_cast<Placer*>(active_obj);
        
        sig_workpiece.disconnect();
        auto workpiece = placer->get_workpiece();
        auto workpiece_texture = placer->get_workpiece_texture();

        auto model = Gtk::StringList::create({});
        dropdown_workpiece->set_model(model);
        dropdown_workpiece->set_selected(-1);
        auto scene = active_obj->get_own_scene();
        
        std::filesystem::path data_dir = scene->get_data_dir_path();
        for (auto fileitem : std::filesystem::directory_iterator(data_dir / workpieces_dir)) { 
            auto dirpath = fileitem.path();
            auto dirname = dirpath.filename().string();
            auto filename = dirname + ".urdf";
            auto filepath = dirpath / filename;
            if(!std::filesystem::exists(filepath)) continue;
            model->append(dirpath.filename().string());
            auto workpieces_path = (workpieces_dir / dirname / filename).string();
            dropdown_workpiece->set_data(dirname.c_str(),new string(workpieces_path),[](gpointer data) {delete reinterpret_cast<string*>(data);});
            if(string::npos != workpiece.find(filename)) dropdown_workpiece->set_selected(model->get_n_items()-1);
        }
        
        sig_workpiece = dropdown_workpiece->property_selected().signal_changed().connect(sigc::mem_fun(*this, &PlacerProperties::on_workpiece_changed));
    }

    void on_workpiece_changed()
    {
        auto so = dynamic_pointer_cast<Gtk::StringObject>(dropdown_workpiece->get_selected_item());
        placer->set_workpiece(*reinterpret_cast<string*>(dropdown_workpiece->get_data(so->get_string())));
    }
};

#endif