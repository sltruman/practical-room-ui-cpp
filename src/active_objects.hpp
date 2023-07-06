#ifndef ACTIVE_OBJECTS_HPP
#define ACTIVE_OBJECTS_HPP

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
#include <filesystem>
using namespace std;

#include <boost/json.hpp>
using namespace boost;


struct ActiveObjects : public Gtk::ListBox 
{
    std::filesystem::path base_dir;
    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::DropDown* dropdown_base;
    Gtk::SpinButton *spin_x,*spin_y,*spin_z,*spin_rx,*spin_ry,*spin_rz;
    sigc::connection sig_base,sig_x,sig_y,sig_z,sig_rx,sig_ry,sig_rz;
    ActiveObject* active_obj;

    ActiveObjects(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
        : Gtk::ListBox(cobject),builder(builder)
        , base_dir("./objects")
    {
        
    }

    virtual ~ActiveObjects()
    {
        
    }
};

#endif