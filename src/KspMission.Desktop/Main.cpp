#include "Ephemeris.hpp"
#include "RuntimeReader.hpp"
#include "WorkerClient.hpp"
#include "StudyReport.hpp"
#include "EvaluationReport.hpp"
#include "ShootingWorkflow.hpp"
#include "ReportReopen.hpp"
#include "ViewMath.hpp"
#include <epoxy/gl.h>
#include <glibmm/ustring.h>
#include <sigc++/sigc++.h>
#include <gdkmm/texture.h>
// GTK 4.24's final Cursor typedef conflicts with gtkmm 4.20's wrapper alias.
// Preload its dependencies, then omit the redundant wrapper aliases only here.
#if defined(_WIN32)
#define DOXYGEN_SHOULD_SKIP_THIS
#include <gdkmm/cursor.h>
#undef DOXYGEN_SHOULD_SKIP_THIS
#endif
#include <gtkmm/application.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/cssprovider.h>
#include <gtkmm/eventcontrollerscroll.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/gesturedrag.h>
#include <gtkmm/glarea.h>
#include <gtkmm/entry.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <glibmm/main.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stylecontext.h>
#include <gdkmm/display.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace ksp;
namespace {
using json=nlohmann::json;
constexpr double pi=3.14159265358979323846;
std::string fixed(double value,int digits=2){std::ostringstream out;out<<std::fixed<<std::setprecision(digits)<<value;return out.str();}
std::string scientific(double value){std::ostringstream out;out<<std::scientific<<std::setprecision(3)<<value;return out.str();}
Snapshot synthetic_snapshot(){
    constexpr double mu=3.986004418e14;
    Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="synthetic-m5-window-v2";
    s.state_epoch_ut_s=0;s.frame={"synthetic barycenter","X right, Y up, Z out","right",true};
    s.bodies.push_back({"Helion",mu,1.0e6,State{{0,0,0},{0,0,0}},0});
    auto orbit=[&](const std::string& id,double radius,double phase,double inclination_deg,
                   double node_deg,double body_mu,double body_radius){
        const auto state=view::circular_orbit(mu,radius,phase,inclination_deg*pi/180,node_deg*pi/180);
        s.bodies.push_back({id,body_mu,body_radius,state,0});
    };
    orbit("Haven",1.0e7,0,0,0,1,3.1e5);
    orbit("Ares",1.5e7,1.2,18,30,1,2.4e5);
    orbit("Cyra",7.2e6,-1.0,9,-45,1,2.1e5);
    orbit("Neris",2.0e7,0.7,27,55,1,2.2e5);
    return s;
}
Ephemeris synthetic_ephemeris(const Snapshot& snapshot){
    Settings q;q.start_ut_s=0;q.end_ut_s=86400;q.step_s=2;
    q.max_position_fit_error_m=1000;q.max_velocity_fit_error_mps=10;
    return integrate(snapshot,q);
}
Ephemeris runtime_preview(const Snapshot& snapshot){
    Settings q;q.start_ut_s=*snapshot.state_epoch_ut_s;q.end_ut_s=q.start_ut_s+3600;q.step_s=10;
    q.max_position_fit_error_m=1000;q.max_velocity_fit_error_mps=1;
    return integrate(snapshot,q);
}
std::string read_bytes(const std::string& path){
    std::ifstream in(path,std::ios::binary);
    if(!in)throw std::runtime_error("Cannot open snapshot: "+path);
    std::ostringstream bytes;bytes<<in.rdbuf();
    if(in.bad())throw std::runtime_error("Cannot read complete snapshot: "+path);
    return bytes.str();
}
struct PreparedRuntime {RuntimeLoad load;Ephemeris preview;};
PreparedRuntime prepare_runtime(const std::string& path,const std::string& expected_hash){
    if(path.empty()||expected_hash.empty())throw std::runtime_error("path and expected SHA-256 are required");
    auto load=read_runtime_snapshot(read_bytes(path),expected_hash);
    auto preview=runtime_preview(load.snapshot);
    return {std::move(load),std::move(preview)};
}
Gtk::Label* label(const std::string& text,const std::string& css=""){
    auto* item=Gtk::make_managed<Gtk::Label>(text);item->set_xalign(0);item->set_wrap(true);
    if(!css.empty())item->add_css_class(css);return item;
}
void append_field(Gtk::Box& box,const std::string& title,const std::string& value){
    auto* row=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,2);row->add_css_class("field");
    row->append(*label(title,"field-name"));row->append(*label(value,"field-value"));box.append(*row);
}
struct Vertex {float x,y,z,r,g,b,size;};
class OrbitView final:public Gtk::GLArea {
public:
    OrbitView(const Snapshot& snapshot,const Ephemeris& ephemeris,std::function<void(std::size_t)> selection,
              std::function<void(const std::string&)> error)
        :snapshot_(snapshot),ephemeris_(ephemeris),selection_(std::move(selection)),error_(std::move(error)){
        set_hexpand(true);set_vexpand(true);set_has_depth_buffer(true);set_required_version(3,3);
        signal_render().connect(sigc::mem_fun(*this,&OrbitView::render),false);
        signal_unrealize().connect(sigc::mem_fun(*this,&OrbitView::release),false);
        auto orbit_drag=Gtk::GestureDrag::create();orbit_drag->set_button(1);
        orbit_drag->signal_drag_begin().connect([this](double,double){drag_camera_=camera_;});
        orbit_drag->signal_drag_update().connect([this](double dx,double dy){
            camera_.yaw_rad=drag_camera_.yaw_rad+dx*0.004;
            camera_.tilt_rad=view::drag_tilt(drag_camera_.tilt_rad,dy);queue_render();});
        add_controller(orbit_drag);
        auto pan_drag=Gtk::GestureDrag::create();pan_drag->set_button(3);
        pan_drag->signal_drag_begin().connect([this](double,double){drag_camera_=camera_;});
        pan_drag->signal_drag_update().connect([this](double dx,double dy){
            camera_.pan_x=drag_camera_.pan_x-dx*2.0/std::max(1,get_width());
            camera_.pan_y=drag_camera_.pan_y+dy*2.0/std::max(1,get_height());queue_render();});
        add_controller(pan_drag);
        auto scroll=Gtk::EventControllerScroll::create();scroll->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
        scroll->signal_scroll().connect([this](double,double dy){camera_.zoom=std::clamp(camera_.zoom*std::exp(dy*0.12),0.35,5.0);queue_render();return true;},false);
        add_controller(scroll);
        auto click=Gtk::GestureClick::create();click->set_button(1);
        click->signal_pressed().connect([this](int,double x,double y){select_at(x,y);});add_controller(click);
        reload();
    }
    void select(std::size_t index){selected_=index;queue_render();}
    void top_view(){camera_.yaw_rad=0;camera_.tilt_rad=0;camera_.pan_x=0;camera_.pan_y=0;queue_render();}
    void reload(){
        paths_.clear();selected_=snapshot_.bodies.size()>1?1:0;
        const auto& metadata=ephemeris_.metadata;
        double maximum=1;
        for(std::size_t i=1;i<snapshot_.bodies.size();++i){
            std::vector<Vec3> path;
            for(int sample=0;sample<=360;++sample){
                const double ut=metadata.start_ut_s+(metadata.end_ut_s-metadata.start_ut_s)*sample/360.0;
                const auto center=ephemeris_.query(snapshot_.bodies[0].id,ut).position_m;
                auto relative=ephemeris_.query(snapshot_.bodies[i].id,ut).position_m-center;
                maximum=std::max(maximum,norm(relative));path.push_back(relative);
            }
            paths_.push_back(std::move(path));
        }
        scale_=maximum*1.5;queue_render();
    }
private:
    const Snapshot& snapshot_;const Ephemeris& ephemeris_;
    std::function<void(std::size_t)> selection_;std::function<void(const std::string&)> error_;
    std::vector<std::vector<Vec3>> paths_;
    GLuint program_=0,vao_=0,vbo_=0;GLint mode_uniform_=-1;
    view::Camera camera_,drag_camera_;
    double scale_=2.4e7;
    std::size_t selected_=1;
    std::array<std::array<float,3>,5> colors_{{{{0.96f,0.75f,0.35f}},{{0.28f,0.78f,0.96f}},{{0.97f,0.43f,0.42f}},{{0.64f,0.81f,0.58f}},{{0.76f,0.61f,0.94f}}}};
    std::array<float,3> project(Vec3 p) const{
        // All subtraction and camera motion use doubles before conversion to GPU floats.
        const auto pixel=view::project(p,scale_,camera_,get_width(),get_height());
        return {static_cast<float>(pixel.x),static_cast<float>(pixel.y),static_cast<float>(pixel.z)};
    }
    static GLuint shader(GLenum type,const char* source){
        GLuint handle=glCreateShader(type);glShaderSource(handle,1,&source,nullptr);glCompileShader(handle);
        GLint okay=0;glGetShaderiv(handle,GL_COMPILE_STATUS,&okay);if(!okay){glDeleteShader(handle);return 0;}return handle;
    }
    bool setup_gl(){
        static constexpr char vertex[]="#version 330 core\nlayout(location=0) in vec3 position; layout(location=1) in vec3 color; layout(location=2) in float point_size; out vec3 tint; void main(){gl_Position=vec4(position,1.0);tint=color;gl_PointSize=point_size;}";
        static constexpr char fragment[]="#version 330 core\nin vec3 tint; out vec4 pixel; uniform int point_mode; void main(){if(point_mode==1 && length(gl_PointCoord-vec2(0.5))>0.5)discard;pixel=vec4(tint,1.0);}";
        GLuint vs=shader(GL_VERTEX_SHADER,vertex),fs=shader(GL_FRAGMENT_SHADER,fragment);
        if(!vs||!fs){error_("OpenGL shader compilation failed");return false;}
        program_=glCreateProgram();glAttachShader(program_,vs);glAttachShader(program_,fs);glLinkProgram(program_);
        glDeleteShader(vs);glDeleteShader(fs);GLint linked=0;glGetProgramiv(program_,GL_LINK_STATUS,&linked);
        if(!linked){error_("OpenGL program link failed");return false;}
        mode_uniform_=glGetUniformLocation(program_,"point_mode");glGenVertexArrays(1,&vao_);glGenBuffers(1,&vbo_);
        glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)0);glEnableVertexAttribArray(0);
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(3*sizeof(float)));glEnableVertexAttribArray(1);
        glVertexAttribPointer(2,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void*)(6*sizeof(float)));glEnableVertexAttribArray(2);
        glBindVertexArray(0);return true;
    }
    void draw(const std::vector<Vertex>& vertices,GLenum primitive,bool points){
        if(vertices.empty())return;
        glUniform1i(mode_uniform_,points?1:0);glBindBuffer(GL_ARRAY_BUFFER,vbo_);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_DYNAMIC_DRAW);
        glDrawArrays(primitive,0,static_cast<GLsizei>(vertices.size()));
    }
    bool render(const Glib::RefPtr<Gdk::GLContext>&){
        if(!glGetString(GL_VERSION)){error_("OpenGL context unavailable");return true;}
        if(!program_&&!setup_gl())return true;
        const int factor=get_scale_factor();glViewport(0,0,get_width()*factor,get_height()*factor);
        glClearColor(0.055f,0.075f,0.10f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);glEnable(GL_PROGRAM_POINT_SIZE);glUseProgram(program_);glBindVertexArray(vao_);
        for(std::size_t i=0;i<paths_.size();++i){
            std::vector<Vertex> lines;lines.reserve(paths_[i].size());const auto c=colors_[(i+1)%colors_.size()];
            for(auto p:paths_[i]){auto v=project(p);lines.push_back({v[0],v[1],v[2],c[0]*0.50f,c[1]*0.50f,c[2]*0.50f,1});}
            draw(lines,GL_LINE_STRIP,false);
        }
        const auto ut=ephemeris_.metadata.start_ut_s;
        const auto center=ephemeris_.query(snapshot_.bodies[0].id,ut).position_m;
        std::vector<Vertex> markers;
        for(std::size_t i=0;i<snapshot_.bodies.size();++i){
            auto p=project(ephemeris_.query(snapshot_.bodies[i].id,ut).position_m-center);auto c=colors_[i%colors_.size()];
            markers.push_back({p[0],p[1],p[2],c[0],c[1],c[2],static_cast<float>(i==selected_?19:(i==0?16:12))});
        }
        draw(markers,GL_POINTS,true);glBindVertexArray(0);return true;
    }
    void select_at(double x,double y){
        const auto ut=ephemeris_.metadata.start_ut_s;
        const auto center=ephemeris_.query(snapshot_.bodies[0].id,ut).position_m;
        double best=24*24;std::size_t index=snapshot_.bodies.size();
        for(std::size_t i=0;i<snapshot_.bodies.size();++i){
            auto p=project(ephemeris_.query(snapshot_.bodies[i].id,ut).position_m-center);
            double px=(p[0]+1)*get_width()/2,py=(1-p[1])*get_height()/2;
            double d=(px-x)*(px-x)+(py-y)*(py-y);if(d<best){best=d;index=i;}
        }
        if(index<snapshot_.bodies.size())selection_(index);
    }
    void release(){
        make_current();if(vbo_)glDeleteBuffers(1,&vbo_);if(vao_)glDeleteVertexArrays(1,&vao_);if(program_)glDeleteProgram(program_);
        vbo_=vao_=program_=0;
    }
};
class DesktopWindow final:public Gtk::ApplicationWindow {
public:
    DesktopWindow(Snapshot snapshot,Ephemeris ephemeris,std::string import_path,std::string expected_hash,
                  std::string worker_path,std::string command_error):snapshot_(std::move(snapshot)),ephemeris_(std::move(ephemeris)),
        view_(snapshot_,ephemeris_,[this](std::size_t index){select(index);},[this](const std::string& error){status_.set_text(error);}),
        root_(Gtk::Orientation::VERTICAL,0),main_(Gtk::Orientation::HORIZONTAL,0),left_(Gtk::Orientation::VERTICAL,10),
        right_(Gtk::Orientation::VERTICAL,10),center_(Gtk::Orientation::VERTICAL,0),bottom_(Gtk::Orientation::VERTICAL,8),
        selection_("Selected: Haven"),status_("Synthetic visualization ready · camera changes do not affect the trajectory"),
        import_("Import runtime JSON"),search_("Screen runtime mission"),evaluate_("Evaluate fixed trial"),
        shoot_("Shoot nearby trial"),open_("Open saved report"),
        cancel_("Cancel worker"),export_("Save report"),
        worker_path_(std::move(worker_path)){
        set_title("KSP Mission Analysis · Synthetic study");set_default_size(1440,880);
        install_css();set_child(root_);
        auto* top=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,4);top->add_css_class("topbar");
        eyebrow_.set_xalign(0);eyebrow_.add_css_class("eyebrow");top->append(eyebrow_);
        top->append(*label("A dated orbital workspace","title"));
        top_meta_.set_xalign(0);top_meta_.add_css_class("meta");top->append(top_meta_);
        root_.append(*top);
        main_.set_vexpand(true);root_.append(main_);
        build_left();build_center();build_right();build_bottom();root_.append(bottom_);
        path_.set_text(import_path);hash_.set_text(expected_hash);
        refresh_source();select(snapshot_.bodies.size()>1?1:0);
        poll_connection_=Glib::signal_timeout().connect(sigc::mem_fun(*this,&DesktopWindow::poll_worker),100);
        signal_close_request().connect(sigc::mem_fun(*this,&DesktopWindow::close_request),false);
        if(!command_error.empty())show_import_error(command_error);
        else if(!import_path.empty()||!expected_hash.empty())import_runtime();
    }
private:
    Snapshot snapshot_;Ephemeris ephemeris_;OrbitView view_;
    Gtk::Box root_,main_,left_,right_,center_,bottom_;
    Gtk::Box body_rows_{Gtk::Orientation::VERTICAL,7},fields_{Gtk::Orientation::VERTICAL,1};
    Gtk::Box form_{Gtk::Orientation::VERTICAL,5},route_rows_{Gtk::Orientation::VERTICAL,4};
    Gtk::Label selection_,status_,eyebrow_,top_meta_,source_info_,frame_info_,model_info_,scene_time_,import_status_,worker_status_;
    Gtk::Entry path_,hash_;Gtk::Button import_,search_,evaluate_,shoot_,open_,cancel_,export_;
    Gtk::ProgressBar progress_;
    std::map<std::string,Gtk::Entry*> inputs_;
    WorkerClient worker_;
    sigc::connection poll_connection_;
    std::optional<RuntimeLoad> runtime_source_;
    std::optional<json> submitted_request_,terminal_event_;
    std::vector<json> evaluation_events_;
    std::string worker_path_,loaded_path_,loaded_hash_,request_id_;
    std::string shooting_stages_;
    std::size_t request_sequence_=0;
    bool pending_close_=false,terminal_seen_=false,active_evaluation_=false,active_shooting_=false;
    std::string source_details_="Synthetic M2 fixture · five bodies with 0°, 9°, 18° and 27° orbit tilts · no KSP or Principia runtime import";
    bool runtime_loaded_=false;
    void show_import_error(const std::string& detail){
        const std::string message="Import rejected: "+detail+". Retained previous "+(runtime_loaded_?std::string("runtime"):std::string("synthetic"))+" scene.";
        import_status_.set_text(message);status_.set_text(message);
    }
    void import_runtime(){
        try{
            if(worker_.running())throw std::runtime_error("Cancel the active worker before importing another source");
            const auto filename=path_.get_text();const auto expected=hash_.get_text();
            auto prepared=prepare_runtime(filename,expected);
            auto& loaded=prepared.load;
            const std::string details="Runtime capture · "+loaded.exporter_id+" v"+loaded.exporter_version+
                " · KSP "+loaded.game_version+" · save "+loaded.save_id+" · capture UT "+fixed(loaded.capture_ut_s,0)+" s"+
                "\nNo-leap display: "+format_no_leap_ut(loaded,loaded.capture_ut_s)+
                " · day "+fixed(loaded.display_day_duration_s,0)+" SI s · origin UT "+fixed(loaded.display_origin_ut_s,0)+" s"+
                "\nPrincipia state source · observed, not compared to installed game";
            runtime_source_=loaded;snapshot_=loaded.snapshot;ephemeris_=std::move(prepared.preview);
            loaded_path_=filename;loaded_hash_=expected;
            submitted_request_.reset();terminal_event_.reset();evaluation_events_.clear();
            active_evaluation_=false;active_shooting_=false;shooting_stages_.clear();
            export_.set_sensitive(false);open_.set_sensitive(true);
            source_details_=details;runtime_loaded_=true;view_.reload();refresh_source();select(snapshot_.bodies.size()>1?1:0);
            for(const auto& key:{"central","home","mars","venus","leg1_min","leg1_max","leg2_min","leg2_max",
                "leg3_min","leg3_max","ephemeris_end","home_parking","mars_parking","home_capture",
                "venus_max_periapsis","venus_speed_tolerance","max_duration"})inputs_.at(key)->set_text("");
            inputs_.at("launch_start")->set_text(fixed(loaded.capture_ut_s,0));
            inputs_.at("launch_end")->set_text(fixed(loaded.capture_ut_s,0));
            inputs_.at("trial_path")->set_text("");
            search_.set_sensitive(true);evaluate_.set_sensitive(true);shoot_.set_sensitive(true);
            cancel_.set_sensitive(false);progress_.set_fraction(0);
            while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
            route_rows_.append(*label("No ranked screened routes for this source.","small"));
            worker_status_.set_text("Runtime snapshot ready · configure exact roles and SI grids to screen");
            import_status_.set_text("Runtime JSON accepted · exact-byte SHA-256 checked · independent Newtonian preview");
            status_.set_text("Runtime scene loaded from "+filename+" · screened search available");
            set_title("KSP Mission Analysis · Runtime capture (uncompared)");
        }catch(const std::exception& error){show_import_error(error.what());}
    }
    void refresh_source(){
        eyebrow_.set_text(runtime_loaded_?"MISSION ANALYSIS  /  RUNTIME CAPTURE, UNCOMPARED":"MISSION ANALYSIS  /  SYNTHETIC STUDY");
        top_meta_.set_text(snapshot_.confidence+"   ·   SHA-256 "+snapshot_.snapshot_hash+"   ·   "+
            snapshot_.frame.origin+" / "+snapshot_.frame.axes+" / "+snapshot_.frame.handedness+" inertial   ·   SI   ·   UT "+
            fixed(ephemeris_.metadata.start_ut_s,0)+"–"+fixed(ephemeris_.metadata.end_ut_s,0)+" s");
        source_info_.set_text(source_details_);
        frame_info_.set_text("Frame: "+snapshot_.frame.origin+"\nAxes: "+snapshot_.frame.axes+" · "+snapshot_.frame.handedness+
            "-handed inertial\nState epoch: UT "+fixed(*snapshot_.state_epoch_ut_s,0)+" s · SI units");
        model_info_.set_text("Model: "+ephemeris_.metadata.result_label+"\nFit: "+fixed(ephemeris_.metadata.measured_max_position_error_m,2)+
            " m · "+fixed(ephemeris_.metadata.measured_max_velocity_error_mps,3)+" m/s");
        scene_time_.set_text("BARYCENTRIC SCENE  ·  UT "+fixed(ephemeris_.metadata.start_ut_s,0)+" s");
        while(auto* child=body_rows_.get_first_child())body_rows_.remove(*child);
        for(std::size_t i=0;i<snapshot_.bodies.size();++i){
            const auto& b=snapshot_.bodies[i];auto* button=Gtk::make_managed<Gtk::Button>();
            auto* content=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,2);
            content->append(*label(b.id,"field-value"));
            content->append(*label("R "+fixed(b.radius_m/1000,0)+" km   ·   μ "+scientific(b.mu_m3_s2)+" m³/s²","small"));
            button->set_child(*content);button->add_css_class("body-button");
            button->signal_clicked().connect([this,i]{select(i);});body_rows_.append(*button);
        }
        while(auto* child=fields_.get_first_child())fields_.remove(*child);
        append_field(fields_,"SYSTEM MODEL",std::to_string(snapshot_.bodies.size())+
            " bodies shown and integrated; route roles select stops only");
        const auto synthetic_role=[this](std::size_t index){return index<snapshot_.bodies.size()?snapshot_.bodies[index].id+" · synthetic":std::string("Unassigned");};
        const auto suggested_role=[this](const std::string& id){
            const auto found=std::find_if(snapshot_.bodies.begin(),snapshot_.bodies.end(),[&](const Body& body){return body.id==id;});
            return found==snapshot_.bodies.end()?std::string("Unassigned · exact ID not found"):id+" · suggested, unconfirmed";
        };
        append_field(fields_,"HOME BODY",runtime_loaded_?suggested_role("JNSQKerbin"):synthetic_role(1));
        append_field(fields_,"MARS ROLE",runtime_loaded_?suggested_role("JNSQDuna"):synthetic_role(2));
        append_field(fields_,"VENUS ROLE",runtime_loaded_?suggested_role("JNSQEve"):synthetic_role(3));
        append_field(fields_,"CENTRAL BODY",runtime_loaded_?suggested_role("JNSQSun"):synthetic_role(0));
        append_field(fields_,"LAUNCH WINDOW","Edit exact UT grid below");
        append_field(fields_,"FLIGHT TIME","Edit three SI-second grids below");
        append_field(fields_,"PARKING STAY","5,184,000 SI seconds fixed");
        append_field(fields_,"PARKING / CAPTURE ALTITUDE","Editable metres below");
        append_field(fields_,"FLYBY CLEARANCE","Editable safety above source atmosphere below");
        append_field(fields_,"RETURN CONDITION","Home parking-orbit capture only");
    }
    void install_css(){
        auto provider=Gtk::CssProvider::create();provider->load_from_data(R"CSS(
window {background:#111820;color:#d8e3ed;font-family:Sans;}
.topbar {padding:20px 24px 16px;background:#17212b;border-bottom:1px solid #344453;}
.eyebrow {color:#86a6bd;font-size:11px;font-weight:700;letter-spacing:1.3px;}
.title {color:#f1f6f9;font-size:26px;font-weight:700;}
.meta {color:#a8bac7;font-size:12px;}
.side {background:#151f28;padding:18px;border-right:1px solid #344453;}
.right-side {border-right:0;border-left:1px solid #344453;}
.section {color:#e8f2f7;font-size:15px;font-weight:700;margin-bottom:8px;}
.small {color:#9fb3c1;font-size:12px;}
.body-button {background:#1d2b36;color:#d8e3ed;border:1px solid #374e5f;border-radius:4px;padding:6px 8px;}
.body-button:hover {background:#294052;}
.field {padding:2px 0;border-bottom:1px solid #2b3b48;}
.field-name {color:#88a4b6;font-size:11px;}
.field-value {color:#dce9f1;font-size:13px;font-weight:600;}
.scene-head {padding:10px 16px;background:#131e27;border-bottom:1px solid #2d4050;}
.bottom {background:#17222c;border-top:1px solid #344453;padding:14px 22px;}
.disabled-action, .disabled-action:disabled {background:#22313c;color:#91a4b2;border:1px solid #405463;border-radius:4px;}
)CSS");
        Gtk::StyleContext::add_provider_for_display(Gdk::Display::get_default(),provider,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    void build_left(){
        left_.add_css_class("side");left_.set_size_request(270,-1);main_.append(left_);
        left_.append(*label("Source & bodies","section"));
        source_info_.set_xalign(0);source_info_.set_wrap(true);source_info_.add_css_class("small");left_.append(source_info_);
        frame_info_.set_xalign(0);frame_info_.set_wrap(true);frame_info_.add_css_class("small");left_.append(frame_info_);
        path_.set_placeholder_text("Runtime JSON path");left_.append(path_);
        hash_.set_placeholder_text("Expected SHA-256 (64 hex)");left_.append(hash_);
        import_.signal_clicked().connect([this]{import_runtime();});left_.append(import_);
        import_status_.set_xalign(0);import_status_.set_wrap(true);import_status_.add_css_class("small");left_.append(import_status_);
        auto* list=Gtk::make_managed<Gtk::ScrolledWindow>();list->set_vexpand(true);left_.append(*list);
        list->set_child(body_rows_);
        model_info_.set_xalign(0);model_info_.set_wrap(true);model_info_.add_css_class("small");left_.append(model_info_);
    }
    void build_center(){
        center_.set_hexpand(true);center_.set_vexpand(true);main_.append(center_);
        auto* head=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,3);head->add_css_class("scene-head");
        scene_time_.set_xalign(0);scene_time_.add_css_class("eyebrow");head->append(scene_time_);
        selection_.set_xalign(0);head->append(selection_);center_.append(*head);center_.append(view_);
        auto* controls=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL,12);
        controls->set_margin(10);
        auto* hint=label("Drag up: overhead   ·   Drag down: edge-on   ·   Right drag: pan   ·   Wheel: zoom   ·   Click marker: select","small");
        hint->set_hexpand(true);controls->append(*hint);
        auto* overhead=Gtk::make_managed<Gtk::Button>("Top view");
        overhead->add_css_class("body-button");
        overhead->signal_clicked().connect([this]{view_.top_view();});controls->append(*overhead);
        center_.append(*controls);
    }
    void build_right(){
        right_.add_css_class("side");right_.add_css_class("right-side");right_.set_size_request(355,-1);main_.append(right_);
        right_.append(*label("Mission setup","section"));
        auto* scroll=Gtk::make_managed<Gtk::ScrolledWindow>();scroll->set_vexpand(true);right_.append(*scroll);
        auto* contents=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,12);
        contents->append(fields_);contents->append(form_);scroll->set_child(*contents);
        build_mission_form();
        right_.append(*label("Screened routes, fixed-trial evaluation, or bounded nearby-trial shooting · independent Newtonian model · SI / UT","small"));
        search_.set_sensitive(false);evaluate_.set_sensitive(false);shoot_.set_sensitive(false);
        export_.set_sensitive(false);open_.set_sensitive(false);
        cancel_.set_sensitive(false);
        search_.signal_clicked().connect([this]{start_mission();});
        evaluate_.signal_clicked().connect([this]{start_evaluation();});
        shoot_.signal_clicked().connect([this]{start_shooting();});
        open_.signal_clicked().connect([this]{open_report();});
        cancel_.signal_clicked().connect([this]{worker_.cancel();worker_status_.set_text("Cancellation requested · waiting for worker checkpoint");});
        export_.signal_clicked().connect([this]{save_report();});
        right_.append(search_);right_.append(evaluate_);right_.append(shoot_);right_.append(open_);
        right_.append(cancel_);right_.append(export_);
    }
    void build_bottom(){
        bottom_.add_css_class("bottom");bottom_.append(*label("CANDIDATES & WORKER","eyebrow"));
        auto* routes=Gtk::make_managed<Gtk::ScrolledWindow>();routes->set_size_request(-1,110);routes->set_child(route_rows_);
        bottom_.append(*routes);route_rows_.append(*label("No ranked screened routes.","small"));
        progress_.set_fraction(0);bottom_.append(progress_);
        worker_status_.set_xalign(0);worker_status_.set_wrap(true);worker_status_.add_css_class("small");
        worker_status_.set_text("Worker idle · runtime snapshot required");bottom_.append(worker_status_);
        status_.set_xalign(0);status_.add_css_class("small");bottom_.append(status_);
    }
    void add_input(const std::string& key,const std::string& title,const std::string& value=""){
        auto* row=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,2);row->add_css_class("field");
        row->append(*label(title,"field-name"));
        auto* entry=Gtk::make_managed<Gtk::Entry>();entry->set_text(value);row->append(*entry);
        form_.append(*row);inputs_.emplace(key,entry);
    }
    void build_mission_form(){
        form_.append(*label("Runtime mission request · edit exact IDs and SI values","section"));
        add_input("central","Central body ID");add_input("home","Home body ID");
        add_input("mars","Mars-role body ID");add_input("venus","Venus-role body ID");
        add_input("launch_start","Launch start UT (s)");add_input("launch_end","Launch end UT (s)");
        add_input("launch_step","Launch step (s)","86400");
        for(int i=1;i<=3;++i){const auto prefix="leg"+std::to_string(i);
            add_input(prefix+"_min","Leg "+std::to_string(i)+" flight minimum (s)");
            add_input(prefix+"_max","Leg "+std::to_string(i)+" flight maximum (s)");
            add_input(prefix+"_step","Leg "+std::to_string(i)+" flight step (s)","86400");
            add_input(prefix+"_branch","Leg "+std::to_string(i)+" Lambert branch: short / long","short");
            add_input(prefix+"_direction","Leg "+std::to_string(i)+" direction: positive / negative","positive");}
        add_input("home_parking","Home parking altitude (m)");
        add_input("mars_parking","Mars parking altitude (m)");
        add_input("home_capture","Home return capture altitude (m)");
        add_input("venus_safety","Venus flyby safety above atmosphere (m)","0");
        add_input("venus_max_periapsis","Venus maximum periapsis radius (m)");
        add_input("venus_speed_tolerance","Flyby speed matching tolerance (m/s)");
        add_input("max_duration","Maximum total duration (s)");
        add_input("lambert_position","Lambert position residual limit (m)","1000");
        add_input("lambert_velocity","Lambert velocity residual limit (m/s)","0.01");
        add_input("central_safety","Central body safety above atmosphere (m)","0");
        add_input("ephemeris_end","Independent ephemeris end UT (s)");
        add_input("ephemeris_step","Ephemeris fixed step (s)","3600");
        add_input("fit_position","Maximum position fit error (m)","1000");
        add_input("fit_velocity","Maximum velocity fit error (m/s)","1");
        add_input("max_cells","Prospective Lambert cell cap (≤100000)","100000");
        add_input("max_routes","Retained route cap (≤128)","50");
        add_input("timeout","Worker deadline (s)","300");
        add_input("trial_path","Fixed-impulse trial JSON path for evaluation or bounded shooting (advanced)");
        add_input("shoot_fd","Shooting finite-difference impulse (m/s)","0.01");
        add_input("shoot_impulse_cap","Shooting per-impulse cap (m/s) · enter explicitly");
        add_input("shoot_iterations","Shooting iteration cap (≤1000)","6");
        add_input("shoot_probes","Shooting probe cap (≤10000)","80");
        add_input("report_path","Saved report path for Open or Save (JSON)");
        form_.append(*label("Parking-orbit stay: exactly 5,184,000 SI s · return: parking capture","small"));
    }
    std::string field(const std::string& name) const{return inputs_.at(name)->get_text().raw();}
    double number(const std::string& name) const{
        const auto raw=field(name);std::size_t used=0;double value=0;
        try{value=std::stod(raw,&used);}catch(const std::exception&){throw std::runtime_error(name+" needs a finite SI number");}
        if(used!=raw.size()||!std::isfinite(value))throw std::runtime_error(name+" needs a finite SI number");
        return value;
    }
    std::size_t positive_integer(const std::string& name,std::size_t maximum) const{
        const auto raw=field(name);if(raw.empty()||raw.find_first_not_of("0123456789")!=std::string::npos)
            throw std::runtime_error(name+" needs a positive integer");
        std::size_t used=0;unsigned long long value=0;
        try{value=std::stoull(raw,&used);}catch(const std::exception&){throw std::runtime_error(name+" integer invalid");}
        if(used!=raw.size()||value==0||value>maximum)throw std::runtime_error(name+" exceeds its cap");
        return static_cast<std::size_t>(value);
    }
    static std::size_t grid_count(double first,double last,double step,const std::string& name){
        if(!std::isfinite(first)||!std::isfinite(last)||!std::isfinite(step)||step<=0||last<first)
            throw std::runtime_error(name+" range or step invalid");
        const double intervals=(last-first)/step;
        if(!std::isfinite(intervals)||intervals>100000||std::abs(intervals-std::round(intervals))>1e-9)
            throw std::runtime_error(name+" must contain integer steps within cap");
        return static_cast<std::size_t>(std::llround(intervals))+1;
    }
    json mission_request(){
        if(!runtime_loaded_||!runtime_source_)throw std::runtime_error("Import a verified runtime snapshot first");
        if(path_.get_text().raw()!=loaded_path_||hash_.get_text().raw()!=loaded_hash_)
            throw std::runtime_error("Source path or hash changed; import the snapshot again");
        const std::array<std::string,4> roles{field("central"),field("home"),field("mars"),field("venus")};
        for(std::size_t i=0;i<roles.size();++i){
            if(roles[i].empty()||std::none_of(snapshot_.bodies.begin(),snapshot_.bodies.end(),
                [&](const Body& body){return body.id==roles[i];}))throw std::runtime_error("Role ID missing from loaded snapshot: "+roles[i]);
            for(std::size_t j=0;j<i;++j)if(roles[i]==roles[j])throw std::runtime_error("Body roles must have distinct IDs");
        }
        const double launch_start=number("launch_start"),launch_end=number("launch_end"),launch_step=number("launch_step");
        const auto launches=grid_count(launch_start,launch_end,launch_step,"launch");
        const auto max_cells=positive_integer("max_cells",100000),max_routes=positive_integer("max_routes",128);
        json legs=json::array();std::size_t product=launches,total=0;double latest_return=launch_end+5184000;
        for(int i=1;i<=3;++i){const auto prefix="leg"+std::to_string(i);
            const double minimum=number(prefix+"_min"),maximum=number(prefix+"_max"),step=number(prefix+"_step");
            if(minimum<=0)throw std::runtime_error(prefix+" flight time must be positive");
            const auto count=grid_count(minimum,maximum,step,prefix);
            if(product>max_cells/count)throw std::runtime_error("Mission work cap exceeded");
            product*=count;if(total>max_cells-product)throw std::runtime_error("Mission work cap exceeded");total+=product;
            latest_return+=maximum;
            const auto branch=field(prefix+"_branch"),direction=field(prefix+"_direction");
            if((branch!="short"&&branch!="long")||(direction!="positive"&&direction!="negative"))
                throw std::runtime_error(prefix+" branch or direction invalid");
            legs.push_back({{"min_s",minimum},{"max_s",maximum},{"step_s",step},
                {"branch",branch},{"direction",direction}});
        }
        const double end=number("ephemeris_end"),ephemeris_step=number("ephemeris_step");
        const double epoch=*snapshot_.state_epoch_ut_s;
        if(launch_start<epoch||end<latest_return||end<=epoch||grid_count(epoch,end,ephemeris_step,"ephemeris")>100001)
            throw std::runtime_error("Independent ephemeris coverage or step cap invalid");
        if(number("fit_position")<=0||number("fit_velocity")<=0||number("lambert_position")<=0||number("lambert_velocity")<=0)
            throw std::runtime_error("Fit and Lambert residual limits must be positive");
        if(number("max_duration")<=0||number("venus_max_periapsis")<=0||number("venus_speed_tolerance")<0||
           number("venus_safety")<0||number("central_safety")<0)
            throw std::runtime_error("Duration and flyby constraints invalid");
        const auto timeout=positive_integer("timeout",1800);(void)timeout;
        const json mission={{"central_body_id",roles[0]},{"home_body_id",roles[1]},
            {"mars_body_id",roles[2]},{"venus_body_id",roles[3]},
            {"reference_normal",json::array({0,0,1})},{"launch_start_ut_s",launch_start},
            {"launch_end_ut_s",launch_end},{"launch_step_s",launch_step},{"legs",legs},
            {"fixed_stay_s",5184000.0},{"time_tolerance_s",0.0},{"max_total_duration_s",number("max_duration")},
            {"home_parking_altitude_m",number("home_parking")},{"mars_parking_altitude_m",number("mars_parking")},
            {"return_capture_altitude_m",number("home_capture")},{"venus_safety_margin_m",number("venus_safety")},
            {"venus_maximum_periapsis_m",number("venus_max_periapsis")},
            {"venus_speed_tolerance_mps",number("venus_speed_tolerance")},
            {"max_lambert_position_residual_m",number("lambert_position")},
            {"max_lambert_velocity_residual_mps",number("lambert_velocity")},
            {"central_safety_margin_m",number("central_safety")},{"max_cells",max_cells},{"max_routes",max_routes},
            {"return_condition","parking_capture"}};
        return {{"protocol_version",1},{"command","start_mission"},{"request_id",request_id_},
            {"source",{{"mode","runtime_snapshot"},{"path",loaded_path_},{"expected_snapshot_hash",loaded_hash_},
                {"expected_frame_origin",snapshot_.frame.origin},{"expected_frame_axes",snapshot_.frame.axes},
                {"expected_state_epoch_ut_s",epoch}}},
            {"ephemeris",{{"end_ut_s",end},{"step_s",ephemeris_step},
                {"max_position_fit_error_m",number("fit_position")},
                {"max_velocity_fit_error_mps",number("fit_velocity")}}},{"mission",mission}};
    }
    void start_mission(){
        try{
            if(worker_.running())throw std::runtime_error("Worker is already running");
            request_id_="desktop-mission-"+std::to_string(++request_sequence_);
            auto request=mission_request();ClientOptions options;
            options.executable_path=worker_path_;options.request_line=request.dump();options.request_id=request_id_;
            options.expected_snapshot_hash=loaded_hash_;options.expected_source_confidence="runtime_observed_uncompared";
            options.result_kind=WorkerResultKind::screened_route;
            options.max_candidates=positive_integer("max_routes",128);options.max_retained_events=2048;
            options.timeout=std::chrono::seconds(positive_integer("timeout",1800));
            options.cancel_grace=std::chrono::milliseconds(750);
            if(!worker_.start(std::move(options)))throw std::runtime_error("Worker already active");
            submitted_request_=request;terminal_event_.reset();evaluation_events_.clear();
            active_evaluation_=false;active_shooting_=false;shooting_stages_.clear();
            export_.set_sensitive(false);open_.set_sensitive(false);
            terminal_seen_=false;progress_.set_fraction(0);search_.set_sensitive(false);
            evaluate_.set_sensitive(false);shoot_.set_sensitive(false);cancel_.set_sensitive(true);
            import_.set_sensitive(false);worker_status_.set_text("Worker starting · "+request_id_);
            while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
            route_rows_.append(*label("Screening runtime mission · no final routes yet","small"));
        }catch(const std::exception& error){worker_status_.set_text(std::string("Mission request rejected: ")+error.what());}
    }
    void start_evaluation(){
        try{
            if(worker_.running())throw std::runtime_error("Worker is already running");
            if(!runtime_loaded_||!runtime_source_)throw std::runtime_error("Import a runtime snapshot first");
            if(path_.get_text().raw()!=loaded_path_||hash_.get_text().raw()!=loaded_hash_)
                throw std::runtime_error("Source path or hash changed; import the snapshot again");
            const auto trial_path=field("trial_path");
            if(trial_path.empty())throw std::runtime_error("Enter a fixed-impulse trial JSON path");
            std::ifstream file(trial_path,std::ios::binary|std::ios::ate);
            if(!file)throw std::runtime_error("Fixed-impulse trial JSON cannot be opened");
            const auto length=file.tellg();
            if(length<=0||length>1024*1024)throw std::runtime_error("Fixed-impulse trial JSON must be 1 MiB or less");
            std::string bytes(static_cast<std::size_t>(length),'\0');file.seekg(0);
            if(!file.read(bytes.data(),length))throw std::runtime_error("Fixed-impulse trial JSON read incomplete");
            const auto trial=json::parse(bytes);
            if(!trial.is_object()||trial.contains("source")||trial.contains("source_confidence")||
               trial.contains("atmosphere_boundaries"))
                throw std::runtime_error("Trial must be an object without a source or atmosphere override");
            if(!trial.contains("route_seed")||!trial.at("route_seed").is_object()||
               trial.at("route_seed").value("snapshot_hash",std::string{})!=loaded_hash_||
               trial.at("route_seed").value("source_confidence",std::string{})!="runtime_observed_uncompared")
                throw std::runtime_error("Trial route seed must match the loaded runtime source");
            request_id_="desktop-eval-"+std::to_string(++request_sequence_);
            const json request={{"protocol_version",1},{"command","evaluate_route"},{"request_id",request_id_},
                {"source",{{"mode","runtime_snapshot"},{"path",loaded_path_},
                    {"expected_snapshot_hash",loaded_hash_},{"expected_frame_origin",snapshot_.frame.origin},
                    {"expected_frame_axes",snapshot_.frame.axes},
                    {"expected_state_epoch_ut_s",*snapshot_.state_epoch_ut_s}}},{"trial",trial}};
            const auto request_line=request.dump();
            if(request_line.size()>1024*1024)throw std::runtime_error("Fixed-impulse worker request exceeds 1 MiB");
            ClientOptions options;options.executable_path=worker_path_;options.request_line=request_line;
            options.request_id=request_id_;options.expected_snapshot_hash=loaded_hash_;
            options.expected_source_confidence="runtime_observed_uncompared";
            options.result_kind=WorkerResultKind::fixed_impulse_evaluation;
            options.max_retained_events=32;
            options.timeout=std::chrono::seconds(positive_integer("timeout",1800));
            options.cancel_grace=std::chrono::milliseconds(750);
            if(!worker_.start(std::move(options)))throw std::runtime_error("Worker already active");
            active_evaluation_=true;active_shooting_=false;shooting_stages_.clear();
            submitted_request_=request;terminal_event_.reset();
            evaluation_events_.clear();export_.set_sensitive(false);open_.set_sensitive(false);
            terminal_seen_=false;progress_.set_fraction(0);search_.set_sensitive(false);
            evaluate_.set_sensitive(false);shoot_.set_sensitive(false);
            cancel_.set_sensitive(true);import_.set_sensitive(false);
            while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
            route_rows_.append(*label("Evaluating supplied fixed impulses · checkpointed result pending","small"));
            worker_status_.set_text("Fixed-impulse worker starting · "+request_id_);
        }catch(const std::exception& error){worker_status_.set_text(std::string("Fixed trial rejected: ")+error.what()+" · previous result retained");}
    }
    void start_shooting(){
        try{
            if(worker_.running())throw std::runtime_error("Worker is already running");
            if(!runtime_loaded_||!runtime_source_)throw std::runtime_error("Import a runtime snapshot first");
            if(path_.get_text().raw()!=loaded_path_||hash_.get_text().raw()!=loaded_hash_)
                throw std::runtime_error("Source path or hash changed; import the snapshot again");
            const auto trial_path=field("trial_path");
            if(trial_path.empty())throw std::runtime_error("Enter a fixed-date four-impulse trial JSON path");
            std::ifstream file(trial_path,std::ios::binary|std::ios::ate);
            if(!file)throw std::runtime_error("Shooting trial JSON cannot be opened");
            const auto length=file.tellg();
            if(length<=0||length>1024*1024)throw std::runtime_error("Shooting trial JSON must be 1 MiB or less");
            std::string trial_bytes(static_cast<std::size_t>(length),'\0');file.seekg(0);
            if(!file.read(trial_bytes.data(),length))throw std::runtime_error("Shooting trial JSON read incomplete");
            const ShootingSource source{loaded_path_,loaded_hash_,snapshot_.confidence,
                snapshot_.frame.origin,snapshot_.frame.axes,*snapshot_.state_epoch_ut_s};
            const ShootingLimits limits{number("shoot_fd"),number("shoot_impulse_cap"),
                positive_integer("shoot_iterations",1000),positive_integer("shoot_probes",10000)};
            request_id_="desktop-shoot-"+std::to_string(++request_sequence_);
            const auto request=build_shooting_request(source,trial_bytes,limits,request_id_);
            ClientOptions options;options.executable_path=worker_path_;options.request_line=request.dump();
            options.request_id=request_id_;options.expected_snapshot_hash=loaded_hash_;
            options.expected_source_confidence="runtime_observed_uncompared";
            options.result_kind=WorkerResultKind::bounded_shooting;options.max_retained_events=160;
            options.timeout=std::chrono::seconds(positive_integer("timeout",1800));
            options.cancel_grace=std::chrono::milliseconds(750);
            if(!worker_.start(std::move(options)))throw std::runtime_error("Worker already active");
            active_evaluation_=false;active_shooting_=true;shooting_stages_.clear();
            submitted_request_=request;
            terminal_event_.reset();evaluation_events_.clear();export_.set_sensitive(false);open_.set_sensitive(false);
            terminal_seen_=false;progress_.set_fraction(0);search_.set_sensitive(false);
            evaluate_.set_sensitive(false);shoot_.set_sensitive(false);
            cancel_.set_sensitive(true);import_.set_sensitive(false);
            while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
            route_rows_.append(*label("Shooting supplied fixed-date trial · bounded independent n-body probes pending","small"));
            worker_status_.set_text("Shooting worker starting · "+request_id_);
        }catch(const std::exception& error){
            worker_status_.set_text(std::string("Nearby trial rejected: ")+error.what()+
                " · previous result retained");
        }
    }
    void show_ranked(const json& event){
        while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
        const auto& rows=event.at("ranked_routes");
        if(rows.empty()){route_rows_.append(*label("No screened route satisfies this bounded request.","small"));return;}
        for(const auto& row:rows){
            const auto leg_dates=[&](const char* key){const auto& leg=row.at(key);
                return fixed(leg.at("departure_ut_s").get<double>(),0)+"→"+
                    fixed(leg.at("arrival_ut_s").get<double>(),0);};
            const auto text="#"+std::to_string(row.at("rank").get<int>())+"  "+
                fixed(row.at("total_optimistic_delta_v_mps").get<double>(),2)+" m/s optimistic  ·  UT "+
                fixed(row.at("launch_ut_s").get<double>(),0)+" → "+fixed(row.at("return_ut_s").get<double>(),0)+
                " s  ·  Venus periapsis margin "+fixed(row.at("venus_periapsis_margin_m").get<double>(),1)+
                " m\nLeg UT: H→M "+leg_dates("home_mars")+" · M→V "+leg_dates("mars_venus")+
                " · V→H "+leg_dates("venus_home")+
                "\nBurns m/s: injection "+fixed(row.at("home_injection_mps").get<double>(),2)+
                " · Mars capture "+fixed(row.at("mars_capture_mps").get<double>(),2)+
                " · Mars departure "+fixed(row.at("mars_departure_mps").get<double>(),2)+
                " · home capture "+fixed(row.at("home_return_capture_mps").get<double>(),2)+
                "\nC3 m²/s²: departure "+fixed(row.at("departure_c3_m2_s2").get<double>(),2)+
                " · return "+fixed(row.at("return_c3_m2_s2").get<double>(),2)+
                "\n"+row.at("result_label").get<std::string>()+"  ·  "+row.at("route_id").get<std::string>();
            route_rows_.append(*label(text,"small"));
        }
    }
    std::vector<std::string> evaluation_lines(const json& event){
        std::vector<std::string> lines;
        const auto& strict=event.at("strict");
        lines.push_back("Checkpointed fixed-impulse trial · "+event.at("result_label").get<std::string>()+
            "\nCharged total "+fixed(event.at("total_charged_delta_v_mps").get<double>(),3)+
            " m/s · route seed revalidated: false · continuous Mars stay verified: false");
        const auto& burns=strict.at("burns"),&checks=strict.at("checkpoints");
        for(std::size_t i=0;i<4;++i){
            const auto& burn=burns.at(i),&check=checks.at(i);
            lines.push_back(check.at("name").get<std::string>()+" · UT "+
                fixed(burn.at("ut_s").get<double>(),0)+" s · burn "+
                fixed(burn.at("magnitude_mps").get<double>(),3)+" m/s"+
                "\nResiduals: position "+fixed(check.at("position_error_m").get<double>(),3)+
                " m · velocity "+fixed(check.at("velocity_error_mps").get<double>(),6)+
                " m/s · parking radius "+fixed(check.at("parking_radius_error_m").get<double>(),3)+" m");
        }
        const auto& venus=strict.at("venus"),&mars=strict.at("mars_radius");
        const auto& disagreement=event.at("disagreement");
        lines.push_back("Venus event UT "+fixed(venus.at("ut_s").get<double>(),0)+
            " s · distance "+fixed(venus.at("distance_m").get<double>(),2)+
            " m · clearance "+fixed(venus.at("clearance_m").get<double>(),2)+
            " m · safety margin "+fixed(venus.at("safety_margin_m").get<double>(),2)+
            " m\nMars stay radius observed "+fixed(mars.at("observed_minimum_m").get<double>(),2)+
            "–"+fixed(mars.at("observed_maximum_m").get<double>(),2)+" m"+
            " · coarse/strict max checkpoint disagreement "+
            fixed(disagreement.at("maximum_checkpoint_position_m").get<double>(),3)+" m / "+
            fixed(disagreement.at("maximum_checkpoint_velocity_mps").get<double>(),6)+" m/s");
        return lines;
    }
    void show_evaluation(const json& event){
        auto lines=evaluation_lines(event);
        while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
        for(const auto& line:lines)route_rows_.append(*label(line,"small"));
    }
    void show_shooting(const json& event){
        while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
        route_rows_.append(*label(present_shooting_completion(event).text,"small"));
    }
    void open_report(){
        try{
            if(worker_.running())throw std::runtime_error("Wait for the active worker to finish");
            if(!runtime_loaded_||!runtime_source_)throw std::runtime_error("Import a runtime source first");
            const auto path=field("report_path");
            const CurrentReportSource source{loaded_hash_,snapshot_.confidence,snapshot_.frame.origin,
                snapshot_.frame.axes,snapshot_.frame.handedness,*snapshot_.state_epoch_ut_s};
            auto opened=reopen_saved_report(path,source);
            std::vector<std::string> lines{opened.summary};
            if(opened.kind==SavedReportKind::screened_study){
                const auto& result=opened.document.at("result");
                for(const auto& route:result.at("ranked_routes"))
                    lines.push_back(route.at("route_id").get<std::string>()+
                        " · optimistic burn total "+fixed(route.at("total_optimistic_delta_v_mps").get<double>(),3)+
                        " m/s · patched-conic screen only");
            }else if(opened.kind==SavedReportKind::fixed_impulse_evaluation){
                auto detail=evaluation_lines(opened.document.at("events").back());
                lines.insert(lines.end(),detail.begin(),detail.end());
            }else lines.push_back(present_shooting_completion(opened.document.at("events").back()).text);
            // All parsing, validation and presentation above precedes any visible state change.
            while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
            for(const auto& line:lines)route_rows_.append(*label(line,"small"));
            submitted_request_.reset();terminal_event_.reset();evaluation_events_.clear();
            active_evaluation_=false;active_shooting_=false;shooting_stages_.clear();
            export_.set_sensitive(false);progress_.set_fraction(0);
            worker_status_.set_text("Opened saved report · read-only historical result · "+path);
        }catch(const std::exception& error){
            worker_status_.set_text(std::string("Saved report rejected: ")+error.what()+" · previous view retained");
        }
    }
    void save_report(){
        try{
            if(!submitted_request_||!terminal_event_||worker_.running())
                throw std::runtime_error("Wait for a completed runtime mission worker result");
            const auto destination=field("report_path");
            if(destination.empty())throw std::runtime_error("Enter a study report output path");
            if(active_shooting_&&
               (path_.get_text().raw()!=loaded_path_||hash_.get_text().raw()!=loaded_hash_))
                throw std::runtime_error("Source path or hash changed; import again before saving");
            std::ifstream file(loaded_path_,std::ios::binary|std::ios::ate);
            if(!file)throw std::runtime_error("Runtime source cannot be reopened");
            const auto length=file.tellg();
            if(length<0||length>16*1024*1024)throw std::runtime_error("Runtime source size invalid");
            std::string bytes(static_cast<std::size_t>(length),'\0');file.seekg(0);
            if(!file.read(bytes.data(),length))throw std::runtime_error("Runtime source read incomplete");
            if(active_shooting_){
                const auto report=compose_shooting_report(bytes,loaded_hash_,
                    *submitted_request_,evaluation_events_);
                save_shooting_report(report,std::filesystem::path(destination));
                worker_status_.set_text("Saved bounded shooting result · "+destination);
            }else if(active_evaluation_){
                const auto report=compose_fixed_evaluation_report(bytes,loaded_hash_,
                    *submitted_request_,evaluation_events_);
                save_fixed_evaluation_report(report,std::filesystem::path(destination));
                worker_status_.set_text("Saved checkpointed fixed-impulse trial · "+destination);
            }else{
                const auto report=compose_runtime_study_report(bytes,loaded_hash_,
                    *submitted_request_,*terminal_event_);
                save_study_report(report,std::filesystem::path(destination));
                worker_status_.set_text("Saved historical screened study · "+destination);
            }
        }catch(const std::exception& error){worker_status_.set_text(std::string("Report rejected: ")+error.what());}
    }
    bool poll_worker(){
        for(const auto& event:worker_.drain()){
            const auto type=event.value("type",std::string{});
            if((active_evaluation_||active_shooting_)&&type!="client_error")evaluation_events_.push_back(event);
            if(type=="started"){
                if(active_shooting_){
                    std::string stages;
                    if(event.contains("non_interruptible_stages")&&event.at("non_interruptible_stages").is_array())
                        for(const auto& stage:event.at("non_interruptible_stages")){
                            if(!stages.empty())stages+=" · ";
                            const auto name=stage.get<std::string>();
                            if(name=="planetary_ephemeris_integration")stages+="planetary ephemeris integration";
                            else if(name=="single_spacecraft_probe")stages+="one spacecraft probe";
                            else if(name=="strict_coarse_and_fine_repropagation")stages+="coarse and strict repropagation";
                            else stages+=name;
                        }
                    shooting_stages_=std::move(stages);
                    worker_status_.set_text("Nearby trial shooting started · SI / UT · noninterruptible: "+shooting_stages_);
                }else worker_status_.set_text(active_evaluation_?
                    "Fixed-impulse evaluation started · checkpointed independent n-body · SI / UT":
                    "Worker started · "+event.value("source_mode",std::string{})+
                        " · independent "+event.value("force_model",std::string{})+" · SI / UT");
            }
            else if(type=="progress"){
                if(active_shooting_){
                    const auto done=event.at("completed_probes").get<std::size_t>();
                    const auto total=event.at("total_probes").get<std::size_t>();
                    progress_.set_fraction(total?static_cast<double>(done)/total:0);
                    worker_status_.set_text("Nearby trial probes "+std::to_string(done)+" / "+
                        std::to_string(total)+" · noninterruptible: "+shooting_stages_);
                }else if(active_evaluation_){
                    const auto done=event.at("completed_phases").get<std::size_t>();
                    const auto total=event.at("total_phases").get<std::size_t>();
                    progress_.set_fraction(total?static_cast<double>(done)/total:0);
                    worker_status_.set_text("Fixed-impulse evaluation "+std::to_string(done)+" / "+
                        std::to_string(total)+" phase · numerical stage may run until deadline");
                }else{
                    const auto sampled=event.value("sampled_cells",std::size_t{0}),total=event.value("total_cells",std::size_t{1});
                    progress_.set_fraction(total?static_cast<double>(sampled)/total:0);
                    worker_status_.set_text("Screening "+std::to_string(sampled)+" / "+std::to_string(total)+
                        " Lambert cells · "+event.value("phase",std::string("route_screen")));
                }
            }else if(type=="route")worker_status_.set_text("Provisional screened route received · final ranking pending");
            else if(type=="complete"||type=="cancelled"){
                terminal_seen_=true;cancel_.set_sensitive(false);search_.set_sensitive(runtime_loaded_);
                evaluate_.set_sensitive(runtime_loaded_);shoot_.set_sensitive(runtime_loaded_);
                import_.set_sensitive(true);open_.set_sensitive(runtime_loaded_);
                if(active_shooting_){
                    while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
                    if(type=="complete"){
                        show_shooting(event);terminal_event_=event;export_.set_sensitive(true);
                        worker_status_.set_text(event.at("status")=="checkpointed_accepted"?
                            "Completed · checkpointed nearby-trial result shown":
                            "Completed · diagnostic non-success shown, report available");
                    }else{
                        route_rows_.append(*label("Nearby trial cancelled · no completed shooting report","small"));
                        terminal_event_.reset();export_.set_sensitive(false);
                        worker_status_.set_text("Nearby trial cancelled · no report available");
                    }
                }else if(active_evaluation_){
                    while(auto* child=route_rows_.get_first_child())route_rows_.remove(*child);
                    if(type=="complete"){
                        show_evaluation(event);terminal_event_=event;export_.set_sensitive(true);
                        worker_status_.set_text("Completed · checkpointed fixed-impulse trial shown");
                    }else{
                        route_rows_.append(*label("Fixed-impulse trial cancelled · no completed result","small"));
                        terminal_event_.reset();export_.set_sensitive(false);
                        worker_status_.set_text("Fixed-impulse trial cancelled · no report available");
                    }
                }else{
                    show_ranked(event);terminal_event_=event;
                    export_.set_sensitive(event.contains("ephemeris_metadata"));
                    worker_status_.set_text(type=="cancelled"?
                        "Cancelled · accepted partial screened routes shown":"Completed · final ranked screened routes shown");
                }
            }else if(type=="error"||type=="client_error"){
                terminal_seen_=true;cancel_.set_sensitive(false);search_.set_sensitive(runtime_loaded_);
                evaluate_.set_sensitive(runtime_loaded_);shoot_.set_sensitive(runtime_loaded_);
                import_.set_sensitive(true);open_.set_sensitive(runtime_loaded_);
                terminal_event_.reset();export_.set_sensitive(false);
                worker_status_.set_text("Worker error "+event.value("code",std::string("unknown"))+": "+
                    event.value("detail",std::string("no detail")));
            }
        }
        if(pending_close_&&!worker_.running()){pending_close_=false;close();}
        return true;
    }
    bool close_request(){
        if(worker_.running()){pending_close_=true;worker_.cancel();
            worker_status_.set_text("Closing after worker cancellation and reap");return true;}
        poll_connection_.disconnect();return false;
    }
    void select(std::size_t index){
        view_.select(index);const auto& b=snapshot_.bodies[index];
        const auto state=ephemeris_.query(b.id,ephemeris_.metadata.start_ut_s);
        selection_.set_text("Selected: "+b.id+"   ·   r "+fixed(norm(state.position_m)/1000,1)+" km   ·   |v| "+fixed(norm(state.velocity_mps),1)+" m/s");
    }
};
class ErrorWindow final:public Gtk::ApplicationWindow {
public:
    explicit ErrorWindow(std::string message){
        set_title("KSP Mission Analysis · Synthetic scene unavailable");set_default_size(620,230);
        auto* content=Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL,12);content->set_margin(24);
        content->append(*label("Synthetic scene could not be prepared","section"));
        content->append(*label(message,"small"));
        content->append(*label("No analysis-ready scene was opened.","small"));
        set_child(*content);
    }
};
}
int main(int argc,char** argv){
    std::string path,hash,worker_path,command_error;bool validate_only=false;
    for(int i=1;i<argc;++i){
        const std::string option=argv[i];
        if(option=="--snapshot"&&i+1<argc)path=argv[++i];
        else if(option=="--sha256"&&i+1<argc)hash=argv[++i];
        else if(option=="--worker"&&i+1<argc)worker_path=argv[++i];
        else if(option=="--validate-snapshot")validate_only=true;
        else {command_error="Unknown or incomplete option: "+option;break;}
    }
    if(worker_path.empty()){
        auto sibling=std::filesystem::absolute(argv[0]).parent_path()/"ksp_worker";
#ifdef _WIN32
        sibling.replace_extension(".exe");
#endif
        worker_path=sibling.string();
    }
    if(validate_only){
        try{
            if(!command_error.empty())throw std::runtime_error(command_error);
            const auto prepared=prepare_runtime(path,hash);
            std::cout<<"ACCEPTED "<<prepared.load.snapshot.snapshot_hash<<' '<<prepared.load.snapshot.confidence<<' '
                <<prepared.load.snapshot.bodies.size()<<" bodies UT "<<prepared.preview.metadata.start_ut_s<<".."<<prepared.preview.metadata.end_ut_s
                <<" s no-leap "<<format_no_leap_ut(prepared.load,prepared.load.capture_ut_s)<<'\n';
            return 0;
        }catch(const std::exception& error){std::cerr<<"REJECTED "<<error.what()<<'\n';return 1;}
    }
    auto app=Gtk::Application::create("org.kspmission.desktop");
    try{
        auto snapshot=synthetic_snapshot();auto ephemeris=synthetic_ephemeris(snapshot);
        return app->make_window_and_run<DesktopWindow>(1,argv,std::move(snapshot),std::move(ephemeris),path,hash,worker_path,command_error);
    }catch(const std::exception& error){
        g_printerr("KSP Mission synthetic desktop failed: %s\n",error.what());
        return app->make_window_and_run<ErrorWindow>(argc,argv,std::string(error.what()));
    }
}
