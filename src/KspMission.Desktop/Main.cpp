#include "Ephemeris.hpp"
#include "RuntimeReader.hpp"
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
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/stylecontext.h>
#include <gdkmm/display.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace ksp;
namespace {
constexpr double pi=3.14159265358979323846;
std::string fixed(double value,int digits=2){std::ostringstream out;out<<std::fixed<<std::setprecision(digits)<<value;return out.str();}
std::string scientific(double value){std::ostringstream out;out<<std::scientific<<std::setprecision(3)<<value;return out.str();}
Snapshot synthetic_snapshot(){
    constexpr double mu=3.986004418e14;
    Snapshot s;s.analysis_ready=true;s.confidence="synthetic_fixture";s.snapshot_hash="synthetic-m5-window-v1";
    s.state_epoch_ut_s=0;s.frame={"synthetic barycenter","X right, Y up, Z out","right",true};
    s.bodies.push_back({"Helion",mu,1.0e6,State{{0,0,0},{0,0,0}},0});
    auto orbit=[&](const std::string& id,double radius,double phase,double body_mu,double body_radius){
        const double speed=std::sqrt(mu/radius);
        s.bodies.push_back({id,body_mu,body_radius,State{{radius*std::cos(phase),radius*std::sin(phase),0},
            {-speed*std::sin(phase),speed*std::cos(phase),0}},0});
    };
    orbit("Haven",1.0e7,0,1,3.1e5);
    orbit("Ares",1.5e7,1.2,1,2.4e5);
    orbit("Cyra",7.2e6,-1.0,1,2.1e5);
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
        orbit_drag->signal_drag_begin().connect([this](double,double){drag_yaw_=yaw_;drag_pitch_=pitch_;});
        orbit_drag->signal_drag_update().connect([this](double dx,double dy){yaw_=drag_yaw_+dx*0.006;pitch_=std::clamp(drag_pitch_+dy*0.006,-1.35,1.35);queue_render();});
        add_controller(orbit_drag);
        auto pan_drag=Gtk::GestureDrag::create();pan_drag->set_button(3);
        pan_drag->signal_drag_begin().connect([this](double,double){drag_pan_x_=pan_x_;drag_pan_y_=pan_y_;});
        pan_drag->signal_drag_update().connect([this](double dx,double dy){
            pan_x_=drag_pan_x_-dx*2.0/get_width();pan_y_=drag_pan_y_+dy*2.0/get_height();queue_render();});
        add_controller(pan_drag);
        auto scroll=Gtk::EventControllerScroll::create();scroll->set_flags(Gtk::EventControllerScroll::Flags::VERTICAL);
        scroll->signal_scroll().connect([this](double,double dy){zoom_=std::clamp(zoom_*std::exp(dy*0.12),0.35,5.0);queue_render();return true;},false);
        add_controller(scroll);
        auto click=Gtk::GestureClick::create();click->set_button(1);
        click->signal_pressed().connect([this](int,double x,double y){select_at(x,y);});add_controller(click);
        reload();
    }
    void select(std::size_t index){selected_=index;queue_render();}
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
    double yaw_=0.18,pitch_=0.28,zoom_=1,pan_x_=0,pan_y_=0;
    double scale_=2.4e7;
    double drag_yaw_=0,drag_pitch_=0,drag_pan_x_=0,drag_pan_y_=0;
    std::size_t selected_=1;
    std::array<std::array<float,3>,4> colors_{{{{0.96f,0.75f,0.35f}},{{0.28f,0.78f,0.96f}},{{0.97f,0.43f,0.42f}},{{0.64f,0.81f,0.58f}}}};
    std::array<float,3> project(Vec3 p) const{
        // All subtraction and camera motion use doubles before conversion to GPU floats.
        const double x=(p.x/scale_-pan_x_)/zoom_,y=(p.y/scale_-pan_y_)/zoom_,z=p.z/scale_/zoom_;
        const double cy=std::cos(yaw_),sy=std::sin(yaw_),cp=std::cos(pitch_),sp=std::sin(pitch_);
        const double xr=cy*x-sy*y,yr=sy*x+cy*y;
        return {static_cast<float>(xr),static_cast<float>(cp*yr-sp*z),static_cast<float>(sp*yr+cp*z)};
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
    DesktopWindow(Snapshot snapshot,Ephemeris ephemeris,std::string import_path,std::string expected_hash,std::string command_error):snapshot_(std::move(snapshot)),ephemeris_(std::move(ephemeris)),
        view_(snapshot_,ephemeris_,[this](std::size_t index){select(index);},[this](const std::string& error){status_.set_text(error);}),
        root_(Gtk::Orientation::VERTICAL,0),main_(Gtk::Orientation::HORIZONTAL,0),left_(Gtk::Orientation::VERTICAL,10),
        right_(Gtk::Orientation::VERTICAL,10),center_(Gtk::Orientation::VERTICAL,0),bottom_(Gtk::Orientation::VERTICAL,8),
        selection_("Selected: Haven"),status_("Synthetic visualization ready · camera changes do not affect the trajectory"),
        import_("Import runtime JSON"),search_("Search unavailable"),export_("Export unavailable"){
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
        if(!command_error.empty())show_import_error(command_error);
        else if(!import_path.empty()||!expected_hash.empty())import_runtime();
    }
private:
    Snapshot snapshot_;Ephemeris ephemeris_;OrbitView view_;
    Gtk::Box root_,main_,left_,right_,center_,bottom_;
    Gtk::Box body_rows_{Gtk::Orientation::VERTICAL,7},fields_{Gtk::Orientation::VERTICAL,1};
    Gtk::Label selection_,status_,eyebrow_,top_meta_,source_info_,frame_info_,model_info_,scene_time_,import_status_;
    Gtk::Entry path_,hash_;Gtk::Button import_,search_,export_;
    std::string source_details_="Synthetic M2 fixture · no KSP or Principia runtime import";
    bool runtime_loaded_=false;
    void show_import_error(const std::string& detail){
        const std::string message="Import rejected: "+detail+". Retained previous "+(runtime_loaded_?std::string("runtime"):std::string("synthetic"))+" scene.";
        import_status_.set_text(message);status_.set_text(message);
    }
    void import_runtime(){
        try{
            const auto filename=path_.get_text();const auto expected=hash_.get_text();
            auto prepared=prepare_runtime(filename,expected);
            auto& loaded=prepared.load;
            const std::string details="Runtime capture · "+loaded.exporter_id+" v"+loaded.exporter_version+
                " · KSP "+loaded.game_version+" · save "+loaded.save_id+" · capture UT "+fixed(loaded.capture_ut_s,0)+" s"+
                "\nNo-leap display: "+format_no_leap_ut(loaded,loaded.capture_ut_s)+
                " · day "+fixed(loaded.display_day_duration_s,0)+" SI s · origin UT "+fixed(loaded.display_origin_ut_s,0)+" s"+
                "\nPrincipia state source · observed, not compared to installed game";
            snapshot_=std::move(loaded.snapshot);ephemeris_=std::move(prepared.preview);
            source_details_=details;runtime_loaded_=true;view_.reload();refresh_source();select(snapshot_.bodies.size()>1?1:0);
            import_status_.set_text("Runtime JSON accepted · exact-byte SHA-256 checked · independent Newtonian preview");
            status_.set_text("Runtime scene loaded from "+filename+" · search/export unavailable");
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
        const auto synthetic_role=[this](std::size_t index){return index<snapshot_.bodies.size()?snapshot_.bodies[index].id+" · synthetic":std::string("Unassigned");};
        const auto suggested_role=[this](const std::string& id){
            const auto found=std::find_if(snapshot_.bodies.begin(),snapshot_.bodies.end(),[&](const Body& body){return body.id==id;});
            return found==snapshot_.bodies.end()?std::string("Unassigned · exact ID not found"):id+" · suggested, unconfirmed";
        };
        append_field(fields_,"HOME BODY",runtime_loaded_?suggested_role("JNSQKerbin"):synthetic_role(1));
        append_field(fields_,"MARS ROLE",runtime_loaded_?suggested_role("JNSQDuna"):synthetic_role(2));
        append_field(fields_,"VENUS ROLE",runtime_loaded_?suggested_role("JNSQEve"):synthetic_role(3));
        append_field(fields_,"CENTRAL BODY",runtime_loaded_?suggested_role("JNSQSun"):synthetic_role(0));
        append_field(fields_,"LAUNCH WINDOW","UT "+fixed(ephemeris_.metadata.start_ut_s,0)+"–"+fixed(ephemeris_.metadata.end_ut_s,0)+" s · preview only");
        append_field(fields_,"FLIGHT TIME","Not configured · SI seconds");
        append_field(fields_,"PARKING STAY","5,184,000 SI seconds fixed");
        append_field(fields_,"PARKING / CAPTURE ALTITUDE","Not configured · metres");
        append_field(fields_,"FLYBY CLEARANCE","Not configured · metres above surface + atmosphere");
        append_field(fields_,"RETURN CONDITION","Home rendezvous · not configured");
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
        auto* controls=label("Drag: orbit camera   ·   Right drag: pan   ·   Wheel: zoom   ·   Click marker: select", "small");
        controls->set_margin(10);center_.append(*controls);
    }
    void build_right(){
        right_.add_css_class("side");right_.add_css_class("right-side");right_.set_size_request(315,-1);main_.append(right_);
        right_.append(*label("Mission setup","section"));
        auto* scroll=Gtk::make_managed<Gtk::ScrolledWindow>();scroll->set_vexpand(true);right_.append(*scroll);
        scroll->set_child(fields_);
        right_.append(*label("Worker integration and mission validation are required before search.","small"));
        search_.set_sensitive(false);export_.set_sensitive(false);search_.add_css_class("disabled-action");export_.add_css_class("disabled-action");
        right_.append(search_);right_.append(export_);
    }
    void build_bottom(){
        bottom_.add_css_class("bottom");bottom_.append(*label("CANDIDATES & WORKER","eyebrow"));
        bottom_.append(*label("No screened seeds or refined routes. Worker integration is not available in this window.","small"));
        auto* bar=Gtk::make_managed<Gtk::ProgressBar>();bar->set_fraction(0);bottom_.append(*bar);
        status_.set_xalign(0);status_.add_css_class("small");bottom_.append(status_);
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
    std::string path,hash,command_error;bool validate_only=false;
    for(int i=1;i<argc;++i){
        const std::string option=argv[i];
        if(option=="--snapshot"&&i+1<argc)path=argv[++i];
        else if(option=="--sha256"&&i+1<argc)hash=argv[++i];
        else if(option=="--validate-snapshot")validate_only=true;
        else {command_error="Unknown or incomplete option: "+option;break;}
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
        return app->make_window_and_run<DesktopWindow>(1,argv,std::move(snapshot),std::move(ephemeris),path,hash,command_error);
    }catch(const std::exception& error){
        g_printerr("KSP Mission synthetic desktop failed: %s\n",error.what());
        return app->make_window_and_run<ErrorWindow>(argc,argv,std::string(error.what()));
    }
}
