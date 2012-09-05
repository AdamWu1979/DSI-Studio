#ifndef SH_PROCESS_HPP
#define SH_PROCESS_HPP
#include <vector>
#include <cmath>
#include <boost/math/special_functions/spherical_harmonic.hpp>
#include <boost/math/special_functions/legendre.hpp>
#include "math/matrix_op.hpp"
#define _USE_MATH_DEFINES


template<unsigned int max_l>
struct SHDecomposition : public BaseProcess
{

    std::vector<float> UPiB;
    unsigned int half_odf_size;
        std::vector<unsigned int> b0_index;

    static float Yj(int l,int m,float theta,float phi)
    {
        if (m == 0)
            return boost::math::spherical_harmonic_r(l,m,theta,phi);
        if (m < 0)
            return M_SQRT2*boost::math::spherical_harmonic_r(l,m,theta,phi);
        else
            return M_SQRT2*boost::math::spherical_harmonic_i(l,m,theta,phi);
    }

    static int getJ(int m,int n)
    {
        return (n*n+n)/2+m;
    }
public:
    virtual void init(Voxel& voxel)
    {
        process_position = __FUNCTION__;

		b0_index.clear();
                for(unsigned int index = 0;index < voxel.bvalues.size();++index)
			if(voxel.bvalues[index] == 0)
			    b0_index.push_back(index);


        half_odf_size = ti_vertices_count()/2;
        float lambda = voxel.param[0];
        const unsigned int R = ((max_l+1)*(max_l+2)/2);
        std::vector<std::pair<int,int> > j_map(R);
        for (int k = 0; k <= max_l; k += 2)
            for (int m = -k; m <= k; ++m)
                j_map[getJ(m,k)] = std::make_pair(m,k);

        std::vector<float> Bt(R*voxel.bvectors.size());
        for (unsigned int j = 0,index = 0; j < R; ++j)
            for (unsigned int n = 0; n < voxel.bvectors.size(); ++n,++index)
            {
                float atan2_xy = std::atan2(voxel.bvectors[n][1],voxel.bvectors[n][0]);
                if (atan2_xy < 0.0)
                    atan2_xy += 2.0*M_PI;
                Bt[index] = Yj(j_map[j].second,j_map[j].first,std::acos(voxel.bvectors[n][2]),atan2_xy);
            }
        std::vector<float> UP(half_odf_size*R);
        {
            std::vector<float> U(half_odf_size*R);
            for (unsigned int n = 0,index = 0; n < half_odf_size; ++n)
                for (unsigned int j = 0; j < R; ++j,++index)
                {
                    float atan2_xy = std::atan2(ti_vertices(n)[1],ti_vertices(n)[0]);
                    if (atan2_xy < 0.0)
                        atan2_xy += 2.0*M_PI;
                    U[index] = Yj(j_map[j].second,j_map[j].first,std::acos(ti_vertices(n)[2]),atan2_xy);
                }
            std::vector<float> P(R*R);
            for (unsigned int i = 0,index = 0; i < R; ++i,index += R+1)
                P[index] = boost::math::legendre_p(j_map[i].second,0.0)*2.0*M_PI;

            math::matrix_product(U.begin(),P.begin(),UP.begin(),math::dyndim(half_odf_size,R),math::dyndim(R,R));
        }

        std::vector<float> iB(Bt.size());
        {
            std::vector<float> BtB(R*R); // BtB = Bt * trans(Bt);
            math::matrix_square(Bt.begin(),BtB.begin(),math::dyndim(R,voxel.bvectors.size()));
            for (unsigned int i = 0,index = 0; i < R; ++i,index += R+1)
            {
                float l = j_map[i].second;
                BtB[index] += l*l*(l+1.0)*(l+1.0)*lambda;
            }
            std::vector<unsigned int> pivot(R);
            math::matrix_lu_decomposition(BtB.begin(),pivot.begin(),math::dyndim(R,R));

            //iB = inv(BtB)*Bt;
            math::matrix_lu_solve(BtB.begin(),pivot.begin(),Bt.begin(),iB.begin(),math::dyndim(R,R),math::dyndim(R,voxel.bvectors.size()));
        }


        UPiB.resize(half_odf_size*voxel.bvectors.size());
        math::matrix_product(UP.begin(),iB.begin(),UPiB.begin(),math::dyndim(half_odf_size,R),math::dyndim(R,voxel.bvectors.size()));




    }
public:
    virtual void run(Voxel& voxel, VoxelData& data)
    {
        process_position = __FUNCTION__;
		// remove the b0 signal
                for(unsigned int index = 0;index < b0_index.size();++index)
			data.space[b0_index[index]] = 0;
        
		math::matrix_vector_product(&*UPiB.begin(),&*data.space.begin(),&*data.odf.begin(),math::dyndim(half_odf_size,voxel.bvectors.size()));
        for (unsigned int index = 0; index < data.odf.size(); ++index)
            if (data.odf[index] < 0.0)
                data.odf[index] = 0.0;
    }
};

#endif//SH_PROCESS_HPP
