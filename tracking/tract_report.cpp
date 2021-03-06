#include <QClipboard>
#include <QFileDialog>
#include <QMessageBox>
#include "tract_report.hpp"
#include "ui_tract_report.h"
#include "ui_tracking_window.h"
#include "tracking_window.h"
#include "tract/tracttablewidget.h"
#include "libs/tracking/fib_data.hpp"
#include "libs/tracking/tract_model.hpp"
tract_report::tract_report(QWidget *parent) :
    QDialog(parent),report_chart(new QChart),report_chart_view(new QChartView(report_chart)),
    cur_tracking_window((tracking_window*)parent),
    ui(new Ui::tract_report)
{
    ui->setupUi(this);
    ui->report_layout->addWidget(report_chart_view);

    std::vector<std::string> index_list;
    cur_tracking_window->handle->get_index_list(index_list);
    for (unsigned int index = 0; index < index_list.size(); ++index)
        ui->report_index->addItem(index_list[index].c_str());

    // report
    {
        connect(ui->report_index,SIGNAL(currentIndexChanged(int)),this,SLOT(refresh_report()));
        connect(ui->profile_dir,SIGNAL(currentIndexChanged(int)),this,SLOT(refresh_report()));
        connect(ui->linewidth,SIGNAL(valueChanged(int)),this,SLOT(refresh_report()));
        connect(ui->CI,SIGNAL(clicked()),this,SLOT(refresh_report()));
        connect(ui->report_bandwidth,SIGNAL(valueChanged(double)),this,SLOT(refresh_report()));
    }
}

tract_report::~tract_report()
{
    delete ui;
}

void tract_report::refresh_report()
{
    if(cur_tracking_window->tractWidget->tract_models.size() > 1 &&
       cur_tracking_window->tractWidget->tract_models[0]->get_tract_color(0) ==
       cur_tracking_window->tractWidget->tract_models[1]->get_tract_color(0))
        cur_tracking_window->tractWidget->assign_colors();

    report_chart->removeAllSeries();


    for(unsigned int index = 0;index < cur_tracking_window->tractWidget->tract_models.size();++index)
    {
        if(cur_tracking_window->tractWidget->item(index,0)->checkState() != Qt::Checked)
            continue;
        std::vector<float> values,data_profile,data_ci1,data_ci2;
        cur_tracking_window->tractWidget->tract_models[index]->get_report(
                    ui->profile_dir->currentIndex(),
                    ui->report_bandwidth->value(),
                    ui->report_index->currentText().toLocal8Bit().begin(),
                    values,data_profile,data_ci1,data_ci2);
        if(data_profile.empty())
            continue;


        QPen pen;
        tipl::rgb color = cur_tracking_window->tractWidget->tract_models[index]->get_tract_color(0);
        pen.setColor(QColor(color.r,color.g,color.b,200));
        pen.setWidth(ui->linewidth->value()+1);

        {
            QLineSeries* series = new QLineSeries;
            for(size_t i = 0; i < data_profile.size(); ++i)
                series->append(double(values[i]),double(data_profile[i]));
            series->setPen(pen);
            series->setName(cur_tracking_window->tractWidget->item(int(index),0)->text());
            report_chart->addSeries(series);
        }

        if(!data_ci1.empty() && ui->CI->isChecked())
        {
            pen.setWidth(ui->linewidth->value());
            QLineSeries* series = new QLineSeries;
            for(size_t i = 0; i < data_ci1.size(); ++i)
                series->append(double(values[i]),double(data_ci1[i]));
            series->setPen(pen);
            series->setName(cur_tracking_window->tractWidget->item(int(index),0)->text()+" CI");
            report_chart->addSeries(series);

        }

        if(!data_ci2.empty() && ui->CI->isChecked())
        {
            QLineSeries* series = new QLineSeries;
            for(size_t i = 0; i < data_ci2.size(); ++i)
                series->append(double(values[i]),double(data_ci2[i]));
            series->setPen(pen);
            series->setName(cur_tracking_window->tractWidget->item(int(index),0)->text()+" CI");
            report_chart->addSeries(series);
        }


    }
    report_chart->createDefaultAxes();
    report_chart->axes(Qt::Horizontal).back()->setGridLineVisible(false);
    report_chart->axes(Qt::Vertical).back()->setGridLineVisible(false);

}

void tract_report::on_save_report_clicked()
{
    QString filename = QFileDialog::getSaveFileName(
                this,"Save report as","report.txt",
                "Report file (*.txt);;All files (*)");
    if(filename.isEmpty())
        return;

    std::ofstream out(filename.toLocal8Bit().begin());
    if(!out)
    {
        QMessageBox::information(this,"Error","Cannot write to file",0);
        return;
    }


    for(unsigned int index = 0;index < cur_tracking_window->tractWidget->tract_models.size();++index)
    {
        if(cur_tracking_window->tractWidget->item(index,0)->checkState() != Qt::Checked)
            continue;
        std::vector<float> values,data_profile,data_ci1,data_ci2;
        cur_tracking_window->tractWidget->tract_models[index]->get_report(
                    ui->profile_dir->currentIndex(),
                    ui->report_bandwidth->value(),
                    ui->report_index->currentText().toLocal8Bit().begin(),
                    values,data_profile,data_ci1,data_ci2);
        if(data_profile.empty())
            continue;

        out << cur_tracking_window->tractWidget->item(index,0)->text().toStdString() << "\t";
        for(unsigned int i = 0;i < values.size();++i)
            out<< values[i] << "\t";
        out << std::endl;

        out << cur_tracking_window->tractWidget->item(index,0)->text().toStdString() << "\t";
        for(unsigned int i = 0;i < data_profile.size();++i)
            out << data_profile[i] << "\t";

        if(!data_ci1.empty())
        {
            out << "CI\t";
            for(unsigned int i = 0;i < data_profile.size();++i)
                out << data_ci1[i] << "\t";
        }
        if(!data_ci2.empty())
        {
            out << "CI\t";
            for(unsigned int i = 0;i < data_profile.size();++i)
                out << data_ci2[i] << "\t";
        }
        out << std::endl;

    }
}


void tract_report::on_save_image_clicked()
{
    QString filename = QFileDialog::getSaveFileName(
                this,"Save report as","report.jpg",
                "JPEC file (*.jpg);;BMP file (*.bmp);;PDF file (*.pdf);;PNG file (*.png);;All files (*)");
    report_chart_view->grab().save(filename);
}
