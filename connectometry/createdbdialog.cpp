#include <QFileDialog>
#include <QStringListModel>
#include <QMessageBox>
#include <fstream>
#include "createdbdialog.h"
#include "ui_createdbdialog.h"
#include "fib_data.hpp"
#include "libs/vbc/vbc_database.h"
#include "prog_interface_static_link.h"
#include "dsi_interface_static_link.h"

extern QString fib_template_file_name;
CreateDBDialog::CreateDBDialog(QWidget *parent,bool create_db_) :
    QDialog(parent),
    create_db(create_db_),
    ui(new Ui::CreateDBDialog)
{
    ui->setupUi(this);
    ui->group_list->setModel(new QStringListModel);
    ui->group_list->setSelectionModel(new QItemSelectionModel(ui->group_list->model()));
    ui->skeleton->setText(fib_template_file_name);
    if(!create_db)
    {
        ui->index_of_interest->hide();
        ui->skeleton_widget->hide();
        ui->movedown->hide();
        ui->moveup->hide();
        ui->create_data_base->setText("Create skeleton");
    }


}

CreateDBDialog::~CreateDBDialog()
{
    delete ui;
}

void CreateDBDialog::on_close_clicked()
{
    close();
}


void CreateDBDialog::update_list(void)
{
    QStringList filenames;
    for(unsigned int index = 0;index < group.size();++index)
        filenames << QFileInfo(group[index]).baseName();
    ((QStringListModel*)ui->group_list->model())->setStringList(filenames);
}

void CreateDBDialog::on_group1open_clicked()
{
    QStringList filenames = QFileDialog::getOpenFileNames(
                                     this,
                                     "Open Fib files",
                                     "",
                                     "Fib files (*fib.gz);;All files (*)" );
    if (filenames.isEmpty())
        return;
    if(ui->skeleton->text().isEmpty())
    {
        QDir cur_dir(QApplication::applicationDirPath());
        QStringList file_list = cur_dir.entryList(QStringList("*.fib.gz"),QDir::Files);
        if(!file_list.empty())
        {
        if(filenames[0].contains("1mm"))
            for(unsigned int i = 0;i < file_list.size();++i)
                if(file_list[i].contains("1mm"))
                    ui->skeleton->setText(QApplication::applicationDirPath() + "/"+ file_list[0]);
        if(filenames[0].contains("2mm"))
            for(unsigned int i = 0;i < file_list.size();++i)
                if(file_list[i].contains("2mm"))
                    ui->skeleton->setText(QApplication::applicationDirPath() + "/"+ file_list[0]);
        }
    }
    if(group.empty())
    {
        fib_data fib;
        if(!fib.load_from_file(filenames[0].toLocal8Bit().begin()))
        {
            QMessageBox::information(this,"Error","Invalid FIB file format",0);
            return;
        }
        if(!fib.is_qsdr)
        {
            QMessageBox::information(this,"Error","The FIB file was not reconstructed by QSDR.",0);
            return;
        }
        ui->index_of_interest->clear();
        if(fib.has_odfs())
            ui->index_of_interest->addItem("sdf");
        std::vector<std::string> item_list;
        fib.get_index_list(item_list);
        for(unsigned int i = fib.dir.index_name.size();i < item_list.size();++i)
            ui->index_of_interest->addItem(item_list[i].c_str());
    }
    group << filenames;
    update_list();
    if(create_db)
        ui->output_file_name->setText(QFileInfo(filenames[0]).absolutePath() + "/connectometry." + ui->index_of_interest->currentText() + ".db.fib.gz");
    else
        ui->output_file_name->setText(QFileInfo(filenames[0]).absolutePath() + "/template.fib.gz");
}

void CreateDBDialog::on_group1delete_clicked()
{
    QModelIndexList indexes = ui->group_list->selectionModel()->selectedRows();
    if(!indexes.count())
        return;
    group.erase(group.begin()+indexes.first().row());
    update_list();
}


void CreateDBDialog::on_moveup_clicked()
{
    QModelIndexList indexes = ui->group_list->selectionModel()->selectedRows();
    if(!indexes.count() || indexes.first().row() == 0)
        return;
    group.swap(indexes.first().row(),indexes.first().row()-1);
    update_list();
    ui->group_list->selectionModel()->select(ui->group_list->model()->index(indexes.first().row()-1,0),
                                             QItemSelectionModel::Select);
}

void CreateDBDialog::on_movedown_clicked()
{
    QModelIndexList indexes = ui->group_list->selectionModel()->selectedRows();
    if(!indexes.count() || indexes.first().row() == group.size()-1)
        return;
    group.swap(indexes.first().row(),indexes.first().row()+1);
    update_list();
    ui->group_list->selectionModel()->select(ui->group_list->model()->index(indexes.first().row()+1,0),
                                             QItemSelectionModel::Select);
}

void CreateDBDialog::on_sort_clicked()
{
    if(group.empty())
        return;
    if(QFileInfo(group[0]).baseName().count('_') == 2)
    {
        std::map<QString,QString> sort_map;
        for(unsigned int index = 0;index < group.size();++index)
        {
            QString str = QFileInfo(group[index]).baseName();
            int pos = str.lastIndexOf('_')+1;
            sort_map[pos ? str.right(str.length()-pos):str] = group[index];
        }
        std::vector<std::pair<QString,QString> > sorted_groups(sort_map.begin(),sort_map.end());
        for(unsigned int index = 0;index < sorted_groups.size();++index)
            group[index] = sorted_groups[index].second;
    }
    else
        group.sort();
    update_list();
}


void CreateDBDialog::on_open_list1_clicked()
{
    QString filename = QFileDialog::getOpenFileName(
                                 this,
                                 "Open text file",
                                 "",
                                 "Text files (*.txt);;All files (*)" );
    if(filename.isEmpty())
        return;
    group.clear();
    std::string line;
    std::ifstream in(filename.toLocal8Bit().begin());
    while(std::getline(in,line))
        group << line.c_str();
    update_list();
}

void CreateDBDialog::on_save_list1_clicked()
{
    QString filename = QFileDialog::getSaveFileName(
                                 this,
                                 "Open text file",
                                 "",
                                 "Text files (*.txt);;All files (*)" );
    if(filename.isEmpty())
        return;

    std::ofstream out(filename.toLocal8Bit().begin());
    for(int index = 0;index < group.size();++index)
        out << group[index].toLocal8Bit().begin() <<  std::endl;
}

QStringList search_files(QString dir,QString filter);
void CreateDBDialog::on_open_dir1_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(
                                this,
                                "Open directory",
                                "");
    if(dir.isEmpty())
        return;
    group << search_files(dir,"*.fib.gz");
    update_list();
}

void CreateDBDialog::on_select_output_file_clicked()
{
    QString filename = QFileDialog::getSaveFileName(
                                 this,
                                 "Save file",
                                 "",
                                 "FIB file (*fib.gz);;All files (*)");
    if(filename.isEmpty())
        return;
#ifdef __APPLE__
// fix the Qt double extension bug here
    if(QFileInfo(filename).completeSuffix().contains(".fib.gz"))
        filename = QFileInfo(filename).absolutePath() + "/" + QFileInfo(filename).baseName() + ".fib.gz";
#endif
    ui->output_file_name->setText(filename);
}
void CreateDBDialog::on_open_skeleton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(
                                 this,
                                 "Template file",
                                 "",
                                 "FIB file (*fib.gz);;All files (*)");
    if(filename.isEmpty())
        return;
    ui->skeleton->setText(filename);
}

void CreateDBDialog::on_create_data_base_clicked()
{
    if(ui->output_file_name->text().isEmpty())
    {
        QMessageBox::information(this,"Error","Please assign output file",0);
        return;
    }
    if(group.empty())
    {
        QMessageBox::information(this,"Error","Please assign subject files",0);
        return;
    }

    if(create_db)
    {
        if(ui->skeleton->text().isEmpty())
        {
            QMessageBox::information(this,"error","Please assign skeleton file",0);
            return;
        }

        begin_prog("loading skeleton");
        std::auto_ptr<vbc_database> data(new vbc_database);

        if(!data->create_database(ui->skeleton->text().toLocal8Bit().begin()))
        {
            QMessageBox::information(this,"error in creating database",data->error_msg.c_str(),0);
            return;
        }

        data->handle->db.index_name = ui->index_of_interest->currentText().toLower().toStdString();

        for (unsigned int index = 0;index < group.count();++index)
        {
            begin_prog(QFileInfo(group[index]).baseName().toStdString().c_str());
            if(!data->handle->db.add_subject_file(group[index].toStdString(),QFileInfo(group[index]).baseName().toStdString()))
            {
                QMessageBox::information(this,"error in loading subject fib files",data->handle->error_msg.c_str(),0);
                check_prog(0,0);
                return;
            }
            if(prog_aborted())
            {
                check_prog(0,0);
                return;
            }
        }
        data->handle->db.save_subject_data(ui->output_file_name->text().toStdString().c_str());
        QMessageBox::information(this,"completed","Connectometry database created",0);
    }
    else
    {
        std::vector<std::string> name_list(group.count());
        for (unsigned int index = 0;index < group.count();++index)
            name_list[index] = group[index].toLocal8Bit().begin();
        const char* error_msg = odf_average(ui->output_file_name->text().toLocal8Bit().begin(),name_list);
        if(error_msg)
            QMessageBox::information(this,"error",error_msg,0);
        else
            QMessageBox::information(this,"completed","File created",0);
    }


}




void CreateDBDialog::on_index_of_interest_currentIndexChanged(const QString &arg1)
{
    if(!group.empty())
        ui->output_file_name->setText(QFileInfo(group[0]).absolutePath() + "/connectometry." + arg1.toLower() + ".db.fib.gz");
}