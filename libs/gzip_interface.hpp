#ifndef GZIP_INTERFACE_HPP
#define GZIP_INTERFACE_HPP
#ifdef WIN32
#include "QtZlib/zlib.h"
#else
#include "zlib.h"
#endif
#include "tipl/tipl.hpp"
#include "prog_interface_static_link.h"
extern bool prog_aborted_;
class gz_istream{
    size_t size_;
    std::ifstream in;
    gzFile handle;
    bool is_gz(const char* file_name)
    {
        std::string filename = file_name;
        if (filename.length() > 3 &&
                filename[filename.length()-3] == '.' &&
                filename[filename.length()-2] == 'g' &&
                filename[filename.length()-1] == 'z')
            return true;
        return false;
    }
public:
    gz_istream(void):size_(0),handle(nullptr){}
    ~gz_istream(void)
    {
        close();
    }

    template<class char_type>
    bool open(const char_type* file_name)
    {
        prog_aborted_ = false;
        in.open(file_name,std::ios::binary);
        unsigned int gz_size = 0;
        if(in)
        {
            in.seekg(-4,std::ios::end);
            size_ = size_t(in.tellg())+4;
            in.read(reinterpret_cast<char*>(&gz_size),4);
            in.seekg(0,std::ios::beg);
        }
        if(is_gz(file_name))
        {
            in.close();
            if(size_ > gz_size) // size > 4G
                size_ = size_*2;
            else
                size_ = gz_size;
            handle = gzopen(file_name, "rb");
            return handle;
        }
        return in.good();
    }
    bool read(void* buf_,size_t buf_size)
    {
        char* buf = reinterpret_cast<char*>(buf_);
        if(prog_aborted())
            return false;
        if(cur() < size())
            check_prog(100*cur()/size(),100);
        else
            check_prog(99,100);
        if(handle)
        {
            const size_t block_size = 104857600;// 100mb
            while(buf_size > block_size)
            {
                if(gzread(handle,buf,block_size) <= 0)
                {
                    close();
                    return false;
                }
                buf_size -= block_size;
                buf = buf + block_size;
            }
            if (gzread(handle,buf,uint32_t(buf_size)) <= 0)
            {
                close();
                return false;
            }
            return true;
        }
        else
            if(in)
            {
                in.read(buf,uint32_t(buf_size));
                return in.good();
            }
        return false;
    }
    void seek(long pos)
    {
        if(handle)
        {
            if(gzseek(handle,pos,SEEK_SET) == -1)
                close();
        }
        else
            if(in)
                in.seekg(pos,std::ios::beg);
    }
    void close(void)
    {
        if(handle)
        {
            gzclose(handle);
            handle = nullptr;
        }
        if(in)
            in.close();
        check_prog(0,0);
    }
    size_t cur(void)
    {
        return handle ? size_t(gztell(handle)):size_t(in.tellg());
    }
    size_t size(void)
    {
        return size_;
    }
    bool good(void) const {return handle ? !gzeof(handle):in.good();}
    operator bool() const	{return good();}
    bool operator!() const	{return !good();}
};

class gz_ostream{
    std::ofstream out;
    gzFile handle;
    bool is_gz(const char* file_name)
    {
        std::string filename = file_name;
        if (filename.length() > 3 &&
                filename[filename.length()-3] == '.' &&
                filename[filename.length()-2] == 'g' &&
                filename[filename.length()-1] == 'z')
            return true;
        return false;
    }
public:
    gz_ostream(void):handle(nullptr){}
    ~gz_ostream(void)
    {
        close();
    }
public:
    template<class char_type>
    bool open(const char_type* file_name)
    {
        if(is_gz(file_name))
        {
            handle = gzopen(file_name, "wb");
            return handle;
        }
        out.open(file_name,std::ios::binary);
        return out.good();
    }
    void write(const void* buf_,size_t size)
    {        
        const char* buf = reinterpret_cast<const char*>(buf_);
        if(handle)
        {
            const size_t block_size = 104857600;// 500mb
            while(size > block_size)
            {
                if(gzwrite(handle,buf,block_size) <= 0)
                {
                    close();
                    throw std::runtime_error("Cannot output gz file");
                }
                size -= block_size;
                buf = buf + block_size;
            }
            if(gzwrite(handle,buf,uint32_t(size)) <= 0)
                close();
        }
        else
            if(out)
                out.write(buf,uint32_t(size));
    }
    void close(void)
    {
        if(handle)
        {
            gzclose(handle);
            handle = nullptr;
        }
        if(out)
            out.close();
    }
    bool good(void) const {return handle ? !gzeof(handle):out.good();}
    operator bool() const	{return good();}
    bool operator!() const	{return !good();}

};


typedef tipl::io::nifti_base<gz_istream,gz_ostream> gz_nifti;
typedef tipl::io::mat_write_base<gz_ostream> gz_mat_write;
typedef tipl::io::mat_read_base<gz_istream> gz_mat_read;

#endif // GZIP_INTERFACE_HPP
