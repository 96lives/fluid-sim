//
// Created by lasagnaphil on 9/14/18.
//

#ifndef FLUID_SIM_ARRAY3D_H
#define FLUID_SIM_ARRAY3D_H

#include <cstddef>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <math_utils.h>
#include <vec3.h>

template <typename T>
struct Array3D {
    T* data;
    int NX, NY, NZ;

    static Array3D create(int nx, int ny, int nz) {
        return {new T[nx*ny*nz], nx, ny, nz};
    }

    void free() {
        delete[] data;
    }

    void copyFrom(const Array3D& arr) {
        memcpy(data, arr.data, sizeof(T)*NX*NY*NZ);
    }

    T& operator()(int i, int j, int k) {
        return data[k*NY*NX + j*NX + i];
    }
    const T& operator()(int i, int j, int k) const {
        return data[k*NY*NX + j*NX + i];
    }

    Array3D& operator+=(const Array3D& rhs) {
        for (int k = 0; k < NZ; k++)
            for (int j = 0; j < NY + 1; j++)
                for (int i = 0; i < NX; i++)
                    (*this)(i,j,k) += rhs(i,j,k);
        return *this;
    }

    // TODO: FMA this
    void setMultiplyAdd(Array3D& a, T b, const Array3D& c) {
        for (int k = 0; k < NZ; k++)
            for (int j = 0; j < NY + 1; j++)
                for (int i = 0; i < NX; i++)
                    (*this)(i,j,k) = a(i,j,k) + b * c(i,j,k);
    }

    T innerProduct(const Array3D& rhs) const {
        T result = {};
        for (int k = 0; k < NZ; k++)
            for (int j = 0; j < NY + 1; j++)
                for (int i = 0; i < NX; i++)
                    result += (*this)(i,j,k)*rhs(i,j,k);
        return result;
    }

    T infiniteNorm() const {
        T result = {};
        for (int k = 0; k < NZ; k++)
            for (int j = 0; j < NY + 1; j++)
                for (int i = 0; i < NX; i++) {
                    if (abs((*this)(i,j,k)) > result) result = abs((*this)(i,j,k));
                }
        return result;
    }

    // from http://www.realtimerendering.com/resources/GraphicsGems/gemsv/ch3-3/tricubic.c
    T triCubic(vec3<T> p)
    {
        int x = (int) p.x, y = (int) p.y, z = (int) p.z;
        if (x < 0 || x >= NX || y < 0 || y >= NY || z < 0 || z >= NZ)
            return (0);

        T dx = p.x - (T) x, dy = p.y - (T) y, dz = p.z - (T) z;

    #define CUBE(x)   ((x) * (x) * (x))
    #define SQR(x)    ((x) * (x))

        /* factors for Catmull-Rom interpolation */
        T u[4], v[4], w[4];
        T r[4], q[4];
        T vox = 0;

        u[0] = -0.5 * CUBE (dx) + SQR (dx) - 0.5 * dx;
        u[1] =  1.5 * CUBE (dx) - 2.5 * SQR (dx) + 1;
        u[2] = -1.5 * CUBE (dx) + 2 * SQR (dx) + 0.5f * dx;
        u[3] =  0.5 * CUBE (dx) - 0.5 * SQR (dx);

        v[0] = -0.5 * CUBE (dy) + SQR (dy) - 0.5 * dy;
        v[1] =  1.5 * CUBE (dy) - 2.5 * SQR (dy) + 1;
        v[2] = -1.5 * CUBE (dy) + 2 * SQR (dy) + 0.5 * dy;
        v[3] =  0.5 * CUBE (dy) - 0.5 * SQR (dy);

        w[0] = -0.5 * CUBE (dz) + SQR (dz) - 0.5 * dz;
        w[1] =  1.5 * CUBE (dz) - 2.5 * SQR (dz) + 1;
        w[2] = -1.5 * CUBE (dz) + 2 * SQR (dz) + 0.5 * dz;
        w[3] =  0.5 * CUBE (dz) - 0.5 * SQR (dz);

        for (int k = 0; k < 4; k++)
        {
            int zp = aml::clamp<int>(z+k-1, 0, NZ-1);
            q[k] = 0;
            for (int j = 0; j < 4; j++)
            {
                int yp = aml::clamp<int>(y+j-1, 0, NY-1);
                r[j] = 0;
                for (int i = 0; i < 4; i++)
                {
                    int xp = aml::clamp<int>(x+i-1, 0, NX-1);
                    r[j] += u[i] * (*this)(zp,yp,xp);
                }
                q[k] += v[j] * r[j];
            }
            vox += w[k] * q[k];
        }
        return (T)(vox < 0 ? 0.0 : vox);

    #undef CUBE
    #undef SQR
    }
};

#endif //FLUID_SIM_ARRAY3D_H
