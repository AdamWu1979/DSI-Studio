#include "manual_alignment.h"
#include "ui_manual_alignment.h"
#include "tracking/tracking_window.h"
#include "fa_template.hpp"

unsigned int mcc_thread_count = 4;
struct thread_count_functor{
    unsigned int operator()(void)
    {
        return mcc_thread_count;
    }
};

void run_reg(const image::basic_image<float,3>& from,
             const image::vector<3>& from_vs,
             const image::basic_image<float,3>& to,
             const image::vector<3>& to_vs,
             reg_data& data,
             unsigned int thread_count,
             unsigned int cost_function)
{
    data.progress = 0;
    mcc_thread_count = thread_count;
    if(cost_function == 1) // mutual information
    {
        image::reg::linear(from,from_vs,to,to_vs,data.arg,data.reg_type,image::reg::mutual_information(),data.terminated);
        image::reg::linear(from,from_vs,to,to_vs,data.arg,data.reg_type,image::reg::mutual_information(),data.terminated);
    }
    else
    {
        image::reg::linear(from,from_vs,to,to_vs,data.arg,data.reg_type,image::reg::mt_correlation<image::basic_image<float,3>,
                       image::transformation_matrix<float>,thread_count_functor>(0),data.terminated);
        image::reg::linear(from,from_vs,to,to_vs,data.arg,data.reg_type,image::reg::mt_correlation<image::basic_image<float,3>,
                       image::transformation_matrix<float>,thread_count_functor>(0),data.terminated);
    }
    if(data.terminated)
        return;
    if(data.reg_type == image::reg::rigid_body)
    {
        data.progress = 2;
        return;
    }
    data.T = image::transformation_matrix<float>(data.arg,from.geometry(),from_vs,to.geometry(),to_vs);
    data.iT = data.T;
    data.iT.inverse();

    data.progress = 1;
    image::basic_image<float,3> new_from(to.geometry());
    image::resample(from,new_from,data.iT,image::linear);
    if(thread_count == 1)
        image::reg::bfnorm(new_from,to,data.bnorm_data,data.terminated);
    else
        multi_thread_reg(data.bnorm_data,new_from,to,thread_count,data.bn_progress,data.terminated);
    data.bn_progress = 16;
    if(!(data.terminated))
        data.progress = 2;
}

manual_alignment::manual_alignment(QWidget *parent,
                                   image::basic_image<float,3> from_,
                                   const image::vector<3>& from_vs_,
                                   image::basic_image<float,3> to_,
                                   const image::vector<3>& to_vs_,
                                   int reg_type_,int cost_function) :
    QDialog(parent),ui(new Ui::manual_alignment),from_vs(from_vs_),to_vs(to_vs_),data(to_.geometry(),reg_type_),timer(0)
{
    data.cost_function = cost_function;
    from.swap(from_);
    to.swap(to_);
    image::normalize(from,1.0);
    image::normalize(to,1.0);
    if(!reg_type_) // manuall rotation
    {
        image::reg::get_bound(from,to,data.arg,b_upper,b_lower,image::reg::rigid_body);
        b_upper.rotation[0] *= 2;
        b_upper.rotation[1] *= 2;
        b_upper.rotation[2] *= 2;
        b_lower.rotation[0] *= 2;
        b_lower.rotation[1] *= 2;
        b_lower.rotation[2] *= 2;
    }
    else
    {
        image::reg::get_bound(from,to,data.arg,b_upper,b_lower,reg_type_);
        reg_thread.reset(new std::future<void>(std::async(std::launch::async,
            [this,cost_function](){run_reg(from,from_vs,to,to_vs,data,std::thread::hardware_concurrency(),cost_function);})));
    }
    ui->setupUi(this);
    if(reg_type_ == image::reg::rigid_body)
    {
        ui->scaling_group->hide();
        ui->tilting_group->hide();
        ui->nl_group_box->hide();
    }
    if(!reg_type_)
    {
        ui->blend_pos->setValue(0);
        ui->scaling_group->hide();
        ui->tilting_group->hide();
        ui->rerun->hide();
        ui->blend_pos->hide();
        ui->switch_view->hide();
        ui->label_13->hide();
    }
    ui->sag_view->setScene(&scene[0]);
    ui->cor_view->setScene(&scene[1]);
    ui->axi_view->setScene(&scene[2]);



    load_param();

    ui->sag_slice_pos->setMaximum(to.geometry()[0]-1);
    ui->sag_slice_pos->setMinimum(0);
    ui->sag_slice_pos->setValue(to.geometry()[0] >> 1);
    ui->cor_slice_pos->setMaximum(to.geometry()[1]-1);
    ui->cor_slice_pos->setMinimum(0);
    ui->cor_slice_pos->setValue(to.geometry()[1] >> 1);
    ui->axi_slice_pos->setMaximum(to.geometry()[2]-1);
    ui->axi_slice_pos->setMinimum(0);
    ui->axi_slice_pos->setValue(to.geometry()[2] >> 1);

    connect_arg_update();
    connect(ui->sag_slice_pos,SIGNAL(valueChanged(int)),this,SLOT(slice_pos_moved()));
    connect(ui->cor_slice_pos,SIGNAL(valueChanged(int)),this,SLOT(slice_pos_moved()));
    connect(ui->axi_slice_pos,SIGNAL(valueChanged(int)),this,SLOT(slice_pos_moved()));
    connect(ui->blend_pos,SIGNAL(valueChanged(int)),this,SLOT(slice_pos_moved()));

    if(reg_type_)
    {
        timer = new QTimer(this);
        timer->setInterval(1000);
        connect(timer, SIGNAL(timeout()), this, SLOT(check_reg()));
    }
    else
    {
        slice_pos_moved();
        update_image();
    }
}


void manual_alignment::connect_arg_update()
{
    connect(ui->tx,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->ty,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->tz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->sx,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->sy,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->sz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->rx,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->ry,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->rz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->xy,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->xz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    connect(ui->yz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
}

void manual_alignment::disconnect_arg_update()
{
    disconnect(ui->tx,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->ty,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->tz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->sx,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->sy,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->sz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->rx,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->ry,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->rz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->xy,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->xz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
    disconnect(ui->yz,SIGNAL(valueChanged(double)),this,SLOT(param_changed()));
}

manual_alignment::~manual_alignment()
{
    if(timer)
    {
        timer->stop();
        if(reg_thread.get())
        {
            timer->stop();
            data.terminated = 1;
            reg_thread->wait();
        }
    }
    delete ui;
}
void manual_alignment::load_param(void)
{


    // translocation
    ui->tx->setMaximum(b_upper.translocation[0]);
    ui->tx->setMinimum(b_lower.translocation[0]);
    ui->tx->setValue(data.arg.translocation[0]);
    ui->ty->setMaximum(b_upper.translocation[1]);
    ui->ty->setMinimum(b_lower.translocation[1]);
    ui->ty->setValue(data.arg.translocation[1]);
    ui->tz->setMaximum(b_upper.translocation[2]);
    ui->tz->setMinimum(b_lower.translocation[2]);
    ui->tz->setValue(data.arg.translocation[2]);
    // rotation
    ui->rx->setMaximum(b_upper.rotation[0]);
    ui->rx->setMinimum(b_lower.rotation[0]);
    ui->rx->setValue(data.arg.rotation[0]);
    ui->ry->setMaximum(b_upper.rotation[1]);
    ui->ry->setMinimum(b_lower.rotation[1]);
    ui->ry->setValue(data.arg.rotation[1]);
    ui->rz->setMaximum(b_upper.rotation[2]);
    ui->rz->setMinimum(b_lower.rotation[2]);
    ui->rz->setValue(data.arg.rotation[2]);
    //scaling
    ui->sx->setMaximum(b_upper.scaling[0]);
    ui->sx->setMinimum(b_lower.scaling[0]);
    ui->sx->setValue(data.arg.scaling[0]);
    ui->sy->setMaximum(b_upper.scaling[1]);
    ui->sy->setMinimum(b_lower.scaling[1]);
    ui->sy->setValue(data.arg.scaling[1]);
    ui->sz->setMaximum(b_upper.scaling[2]);
    ui->sz->setMinimum(b_lower.scaling[2]);
    ui->sz->setValue(data.arg.scaling[2]);
    //tilting
    ui->xy->setMaximum(b_upper.affine[0]);
    ui->xy->setMinimum(b_lower.affine[0]);
    ui->xy->setValue(data.arg.affine[0]);
    ui->xz->setMaximum(b_upper.affine[1]);
    ui->xz->setMinimum(b_lower.affine[1]);
    ui->xz->setValue(data.arg.affine[1]);
    ui->yz->setMaximum(b_upper.affine[2]);
    ui->yz->setMinimum(b_lower.affine[2]);
    ui->yz->setValue(data.arg.affine[2]);

}
void manual_alignment::update_affine(void)
{
    data.T = image::transformation_matrix<float>(data.arg,from.geometry(),from_vs,to.geometry(),to_vs);
    data.iT = data.T;
    data.iT.inverse();
}

void manual_alignment::update_image(void)
{
    update_affine();
    warped_from.clear();
    warped_from.resize(to.geometry());
    image::resample(from,warped_from,data.iT,image::linear);
}
void manual_alignment::param_changed()
{
    data.arg.translocation[0] = ui->tx->value();
    data.arg.translocation[1] = ui->ty->value();
    data.arg.translocation[2] = ui->tz->value();

    data.arg.rotation[0] = ui->rx->value();
    data.arg.rotation[1] = ui->ry->value();
    data.arg.rotation[2] = ui->rz->value();

    data.arg.scaling[0] = ui->sx->value();
    data.arg.scaling[1] = ui->sy->value();
    data.arg.scaling[2] = ui->sz->value();

    data.arg.affine[0] = ui->xy->value();
    data.arg.affine[1] = ui->xz->value();
    data.arg.affine[2] = ui->yz->value();

    update_image();
    slice_pos_moved();
}



void manual_alignment::slice_pos_moved()
{
    if(warped_from.empty() || to.empty())
        return;
    int slice_pos[3];
    slice_pos[0] = ui->sag_slice_pos->value();
    slice_pos[1] = ui->cor_slice_pos->value();
    slice_pos[2] = ui->axi_slice_pos->value();
    double ratio =
        std::min((double)(ui->axi_view->width()-10)/(double)warped_from.width(),
                 (double)(ui->axi_view->height()-10)/(double)warped_from.height());
    float w1 = ui->blend_pos->value()/10.0;
    float w2 = 1.0-w1;
    w1*= 255.0;
    w2 *= 255.0;
    for(unsigned char dim = 0;dim < 3;++dim)
    {
        image::basic_image<float,2> slice,slice2;
        image::reslicing(warped_from,slice,dim,slice_pos[dim]);
        image::reslicing(to,slice2,dim,slice_pos[dim]);
        buffer[dim].resize(slice.geometry());
        for (unsigned int index = 0; index < slice.size(); ++index)
        {
            float value = slice[index]*w2+slice2[index]*w1;
            buffer[dim][index] = image::rgb_color(value,value,value);
        }
        scene[dim].setSceneRect(0, 0, buffer[dim].width()*ratio,buffer[dim].height()*ratio);
        slice_image[dim] = QImage((unsigned char*)&*buffer[dim].begin(),buffer[dim].width(),buffer[dim].height(),QImage::Format_RGB32).
                        scaled(buffer[dim].width()*ratio,buffer[dim].height()*ratio);
        if(dim != 2)
            slice_image[dim] = slice_image[dim].mirrored();
        scene[dim].clear();
        scene[dim].addRect(0, 0, buffer[dim].width()*ratio,buffer[dim].height()*ratio,QPen(),slice_image[dim]);
    }
}

void manual_alignment::check_reg()
{
    if(reg_thread.get())
    {
        disconnect_arg_update();
        ui->tx->setValue(data.arg.translocation[0]);
        ui->ty->setValue(data.arg.translocation[1]);
        ui->tz->setValue(data.arg.translocation[2]);
        ui->rx->setValue(data.arg.rotation[0]);
        ui->ry->setValue(data.arg.rotation[1]);
        ui->rz->setValue(data.arg.rotation[2]);
        ui->sx->setValue(data.arg.scaling[0]);
        ui->sy->setValue(data.arg.scaling[1]);
        ui->sz->setValue(data.arg.scaling[2]);
        ui->xy->setValue(data.arg.affine[0]);
        ui->xz->setValue(data.arg.affine[1]);
        ui->yz->setValue(data.arg.affine[2]);
        connect_arg_update();
        ui->nl_progress_bar->setValue(data.bn_progress);
        update_image();
    }
    slice_pos_moved();
}



void manual_alignment::on_buttonBox_accepted()
{
    if(timer)
        timer->stop();
    update_image(); // to update the affine matrix
}

void manual_alignment::on_buttonBox_rejected()
{
    if(timer)
        timer->stop();
}

void manual_alignment::on_rerun_clicked()
{
    if(reg_thread.get())
    {
        data.terminated = 1;
        reg_thread->wait();
    }
    data.terminated = 0;
    reg_thread.reset(new std::future<void>(std::async(std::launch::async,
            [this](){run_reg(from,from_vs,to,to_vs,data,std::thread::hardware_concurrency(),data.cost_function);})));
    if(timer)
        timer->start();

}

void manual_alignment::on_switch_view_clicked()
{
    ui->blend_pos->setValue(ui->blend_pos->value() > ui->blend_pos->maximum()/2 ? 0:ui->blend_pos->maximum());
}
