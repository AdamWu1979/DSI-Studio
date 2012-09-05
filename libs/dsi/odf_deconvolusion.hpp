#ifndef ODF_DECONVOLUSION_HPP
#define ODF_DECONVOLUSION_HPP
#include "odf_decomposition.hpp"
#include "odf_process.hpp"


/*

struct EstimateResponseFunction : public BaseProcess
{
    SearchLocalMaximum lm;
	ConvolutedOdfComponent decomposition;
	double max_value;
public:
    virtual void init(Voxel& voxel)
    {
        process_position = __FUNCTION__;
		lm.init();
		decomposition.icosa_components.resize(ti_vertices_count() >> 1);
                for(unsigned int index = 0;index < ti_vertices_count() >> 1;++index)
			decomposition.icosa_components[index].initialize(index);

		voxel.response_function.resize(FiberDistribution::discrete_count+2);
		std::fill(voxel.response_function.begin(),voxel.response_function.end(),0);
		max_value = 0;

    }
    virtual void run(Voxel& voxel, VoxelData& data)
    {
		process_position = __FUNCTION__;
        lm.search(data.odf);
        if(lm.max_table.size() > 1)
			return;
		std::vector<float> function;
		double min_value = RemoveIsotropicPart()(data.odf);
		double cur_max_value = *std::max_element(data.odf.begin(),data.odf.end());
		if(cur_max_value < max_value)
			return;
		max_value = cur_max_value;
		decomposition.get_response_function(data.odf,function);
		std::for_each(function.begin(),function.end(),boost::lambda::_1 += min_value);
		voxel.response_function = function;
    }
};
*/

struct EstimateResponseFunction : public BaseProcess
{
    float max_value;
    boost::mutex mutex;
public:
    virtual void init(Voxel& voxel)
    {
        process_position = __FUNCTION__;
        voxel.response_function.resize(ti_vertices_count()/2);
        voxel.reponse_function_scaling = 0;
        max_value = 0;
        std::fill(voxel.response_function.begin(),voxel.response_function.end(),1.0);
    }
    virtual void run(Voxel& voxel, VoxelData& data)
    {
        boost::mutex::scoped_lock lock(mutex);
        process_position = __FUNCTION__;
        float max_diffusion_value = std::accumulate(data.odf.begin(),data.odf.end(),0.0)/data.odf.size();
        if (max_diffusion_value > voxel.reponse_function_scaling)
		{
			voxel.reponse_function_scaling = max_diffusion_value;
			voxel.free_water_diffusion = data.odf;
		}
		float cur_value = data.fa[0]-data.fa[1]-data.fa[2];
        if (cur_value < max_value)
            return;
        voxel.response_function = data.odf;
        max_value = cur_value;
    }
};

class ODFDeconvolusion  : public BaseProcess
{
protected:
    std::vector<float> A,Rt;
    std::vector<unsigned int> pv;
    float sensitivity_error_percentage;
	float specificity_error_percentage;
	// for iterative deconvolution
    std::vector<float> AA;
    unsigned int iteration;


    unsigned int half_odf_size;

    double inner_angle(double cos_value)
    {
        double abs_cos = std::abs(cos_value);
        if (abs_cos > 1.0)
            abs_cos = 1.0;
        if (abs_cos < 0.0)
            abs_cos = 0.0;
        return std::acos(abs_cos)*2.0/M_PI;
    }
    double kernel_regression(const std::vector<float>& fiber_profile,
                             const std::vector<double>& inner_angles,
                             double cur_angle,double sigma)
    {
        sigma *= sigma;
        std::vector<double> weighting(inner_angles.size());
        for (unsigned int index = 0; index < inner_angles.size(); ++index)
        {
            double dx = cur_angle-inner_angles[index];
            weighting[index] = std::exp(-dx*dx/2.0/sigma);
        }
        double result = 0.0;
        for (unsigned int index = 0; index < fiber_profile.size(); ++index)
            result += fiber_profile[index]*weighting[index];
        result /= std::accumulate(weighting.begin(),weighting.end(),0.0);
        return result;
    }
    void estimate_Rt(Voxel& voxel)
    {
        std::vector<double> inner_angles(half_odf_size);
        {
            unsigned int max_index = std::max_element(voxel.response_function.begin(),voxel.response_function.end())-voxel.response_function.begin();
            for (unsigned int index = 0; index < inner_angles.size(); ++index)
                inner_angles[index] = inner_angle(::ti_vertices_cos(index,max_index));
        }


        Rt.resize(half_odf_size*half_odf_size);
        for (unsigned int i = 0,index = 0; i < half_odf_size; ++i)
            for (unsigned int j = 0; j < half_odf_size; ++j,++index)
                Rt[index] = kernel_regression(voxel.response_function,inner_angles,inner_angle(::ti_vertices_cos(i,j)),9.0/180.0*M_PI);
    }

	void deconvolution(std::vector<float>& odf)
	{
		std::vector<float> tmp(half_odf_size);
		math::matrix_vector_product(&*Rt.begin(),&*odf.begin(),&*tmp.begin(),math::dyndim(half_odf_size,half_odf_size));
        math::matrix_lu_solve(&*A.begin(),&*pv.begin(),&*tmp.begin(),&*odf.begin(),math::dyndim(half_odf_size,half_odf_size));
	}
	void remove_isotropic(std::vector<float>& odf)
	{
		float min_value = *std::min_element(odf.begin(),odf.end());
        if (min_value > 0)
            std::for_each(odf.begin(),odf.end(),boost::lambda::_1 -= min_value);
        else
            for (unsigned int index = 0; index < half_odf_size; ++index)
                if (odf[index] < 0.0)
                    odf[index] = 0.0;
	}
	float dif_ratio(const std::vector<float>& odf)
	{
		SearchLocalMaximum local_max;
		local_max.init();
        local_max.search(odf);
        if (local_max.max_table.size() < 2)
            return 0.0;
        float first_value = local_max.max_table.begin()->first;
        float second_value = (++(local_max.max_table.begin()))->first;
        return second_value/first_value;
    }

    void get_error_percentage(std::vector<float> single_fiber_odf,std::vector<float> free_water_odf)
    {
        deconvolution(single_fiber_odf);
		remove_isotropic(single_fiber_odf);
		sensitivity_error_percentage = dif_ratio(single_fiber_odf);

		deconvolution(free_water_odf);
		remove_isotropic(free_water_odf);
                specificity_error_percentage = image::mean(free_water_odf.begin(),free_water_odf.end())/
							(*std::max_element(single_fiber_odf.begin(),single_fiber_odf.end()));

		
	}

public:
    virtual void init(Voxel& voxel)
    {
        process_position = __FUNCTION__;
        if (!voxel.odf_deconvolusion)
            return;

        std::for_each(voxel.response_function.begin(),voxel.response_function.end(),
                      boost::lambda::_1 /= (std::accumulate(voxel.response_function.begin(),voxel.response_function.end(),0.0)
                                            /((double)voxel.response_function.size())));
		// scale the free water diffusion to 1
		std::for_each(voxel.free_water_diffusion.begin(),voxel.free_water_diffusion.end(),(boost::lambda::_1 /= voxel.reponse_function_scaling));
        

        half_odf_size = ti_vertices_count()/2;
        iteration = std::floor(voxel.param[3]+0.5);
        estimate_Rt(voxel);

        A.resize(half_odf_size*half_odf_size);
        pv.resize(half_odf_size);
        math::matrix_product_transpose(Rt.begin(),Rt.begin(),A.begin(),
                                       math::dyndim(half_odf_size,half_odf_size),math::dyndim(half_odf_size,half_odf_size));

        AA = A;
        for (unsigned int i = 0,index = 0; i < half_odf_size; ++i,index += half_odf_size + 1)
            A[index] += voxel.param[2];
        math::matrix_lu_decomposition(A.begin(),pv.begin(),math::dyndim(half_odf_size,half_odf_size));

		get_error_percentage(voxel.response_function,voxel.free_water_diffusion);
		
    }

    virtual void run(Voxel& voxel, VoxelData& data)
    {
        using namespace math;
        // scale the dODF using the reference to free water diffusion
        if (!voxel.odf_deconvolusion)
            return;
        std::for_each(data.odf.begin(),data.odf.end(),(boost::lambda::_1 /= voxel.reponse_function_scaling));
        
		deconvolution(data.odf);
        
        if (voxel.odf_deconvolusion_iterative)
        {	/*
            for (unsigned int index = 0;index < iteration;++index)
            {
                for (unsigned int index = 0; index < data.odf.size(); ++index)
                    if (data.odf[index] < 0)
                        data.odf[index] = 0;

                std::vector<float> A_(AA);
                std::vector<unsigned int> pv_(pv.size());

                float scale = std::accumulate(data.odf.begin(),data.odf.end(),0.0);
                scale /= half_odf_size;
                for (unsigned int i = 0,index = 0; i < half_odf_size; ++i,index += half_odf_size + 1)
                    A_[index] += voxel.param[2]/(0.00001+data.odf[i]/scale)*(0.00001+data.odf[i]/scale);
                math::matrix_lu_decomposition(&*A_.begin(),&*pv_.begin(),math::dyndim(half_odf_size,half_odf_size));
                math::matrix_lu_solve(&*A_.begin(),&*pv_.begin(),&*original_odf.begin(),&*data.odf.begin(),math::dyndim(data.odf.size(),data.odf.size()));
            }*/
        }

		remove_isotropic(data.odf);
    }

    virtual void end(Voxel& voxel,MatFile& mat_writer)
    {
        if (!voxel.odf_deconvolusion)
            return;
        mat_writer.add_matrix("deconvolution_kernel",&*voxel.response_function.begin(),1,voxel.response_function.size());
        mat_writer.add_matrix("free_water_diffusion",&*voxel.free_water_diffusion.begin(),1,voxel.free_water_diffusion.size());
        mat_writer.add_matrix("sensitivity_error_percentage",&sensitivity_error_percentage,1,1);
        mat_writer.add_matrix("specificity_error_percentage",&specificity_error_percentage,1,1);
    }

};

/*
class ODFCompressSensing : public ODFDeconvolusion
{
    std::vector<float> R;
    double lambda;
private:
    void gradient_fun(const std::vector<double>& dODF,const std::vector<double>& fODF,std::vector<double>& g)
    {
        using namespace math;
        std::vector<double> mm(half_odf_size);
        math::matrix_vector_product(&*R.begin(),&*fODF.begin(),&*mm.begin(),math::dyndim(half_odf_size,half_odf_size));
        mm -= dODF;
        math::matrix_vector_product(&*Rt.begin(),&*mm.begin(),&*g.begin(),math::dyndim(half_odf_size,half_odf_size));
        g *= (double)2.0;
        double n0 = std::max((double)0.0,*std::min_element(fODF.begin(),fODF.end()));
        for (unsigned int index = 0;index < fODF.size();++index)
        {
            double x = fODF[index]-n0;
            //g[index] += lambda*x/sqrt(x*x+0.00001);
            if (std::abs(x) < 0.001)
                continue;
            g[index] += lambda/std::abs(x);
        }
    }
    double fun(const std::vector<double>& dODF,const std::vector<double>& fODF)
    {
        using namespace math;
        std::vector<double> mm(half_odf_size);
        math::matrix_vector_product(&*R.begin(),&*fODF.begin(),&*mm.begin(),math::dyndim(half_odf_size,half_odf_size));
        mm -= dODF;

        double cost = math::vector_op_dot(&*mm.begin(),&*mm.begin()+mm.size(),&*mm.begin());
        double n0 = std::max((double)0.0,*std::min_element(fODF.begin(),fODF.end()));
        for (unsigned int index = 0;index < fODF.size();++index)
        {
            double x = fODF[index]-n0;
            //if(x > 0.0)
            //	cost += lambda*x;
            //else
            //	cost -= lambda*x;
            cost += lambda*std::log(std::abs(x)+0.1);
        }
        return cost;
    }

public:
    virtual void init(Voxel& voxel)
    {
        if (!voxel.odf_compress_sensing)
            return;

        process_position = __FUNCTION__;
        std::for_each(voxel.response_function.begin(),voxel.response_function.end(),boost::lambda::_1 /= voxel.reponse_function_scaling);
        half_odf_size = ti_vertices_count()/2;
        estimate_Rt(voxel);

        lambda = voxel.param[3];
        R.resize(half_odf_size*half_odf_size);
        math::matrix_transpose(Rt.begin(),R.begin(),math::dyndim(half_odf_size,half_odf_size));

    }


    struct line_search
    {
        std::vector<double> fODF;
        std::vector<double> dfODFk;
        std::vector<double> dODF;
        ODFCompressSensing* pointer;

        double operator()(double t)
        {
            std::vector<double> next_fODF(fODF);
            math::vector_op_axpy(&*next_fODF.begin(),&*next_fODF.begin()+next_fODF.size(),t,&*dfODFk.begin());
            return pointer->fun(dODF,next_fODF);
        }
    };

    virtual void run(Voxel& voxel, VoxelData& data)
    {
        using namespace math;
        if (!voxel.odf_compress_sensing)
            return;
        std::for_each(data.odf.begin(),data.odf.end(),(boost::lambda::_1 /= voxel.reponse_function_scaling));
        std::vector<double> dODF(data.odf.begin(),data.odf.end());
        std::vector<double> fODF(data.odf.size());

        double beta = 5.0;
        std::vector<double> g_k(fODF.size());
        gradient_fun(dODF,fODF,g_k);
        std::vector<double> dfODFk(g_k);
        std::for_each(dfODFk.begin(),dfODFk.end(),boost::lambda::_1 *= -1);
        double length2_gk = math::vector_op_norm2(g_k.begin(),g_k.end());
        double fun_mk = fun(dODF,fODF);

        math::brent_method<double,double> backsearch;
        backsearch.min = 0;
        backsearch.max = 1000.0;
        backsearch.param = 1;
        line_search line;
        line.dODF = dODF;
        line.pointer = this;


        for (unsigned int k = 0,j;k < 40;++k)
        {
            if (length2_gk < 0.001)
                break;

            line.fODF = fODF;
            line.dfODFk.swap(dfODFk);
            backsearch.argmin(line,0.001);
            if (backsearch.value > fun_mk)
                break;
            line.dfODFk.swap(dfODFk);
            fun_mk = backsearch.value;
            math::vector_op_axpy(&*fODF.begin(),&*fODF.begin()+fODF.size(),backsearch.param,&*dfODFk.begin());

            gradient_fun(dODF,fODF,g_k);
            double length2_gk_1 = math::vector_op_norm2(&*g_k.begin(),&*g_k.begin()+g_k.size());
            double gamma = length2_gk_1/length2_gk;
            length2_gk = length2_gk_1;
            gamma *= gamma;
            dfODFk *= gamma;
            dfODFk -= g_k;
        }

        for (unsigned int index = 0; index < data.odf.size(); ++index)
            if (fODF[index] < 0.0)
                data.odf[index] = 0.0;
            else
                data.odf[index] = fODF[index];
    }
};

*/

#endif//ODF_DECONVOLUSION_HPP
