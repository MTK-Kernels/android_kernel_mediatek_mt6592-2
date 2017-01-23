
// Copyright (C) 2009 Thomas Capricelli <orzel@freehackers.org>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_NUMERICAL_DIFF_H
#define EIGEN_NUMERICAL_DIFF_H

namespace Eigen { 

enum NumericalDiffMode {
    Forward,
    Central
};


template<typename _Functor, NumericalDiffMode mode=Forward>
class NumericalDiff : public _Functor
{
public:
    typedef _Functor Functor;
    typedef typename Functor::Scalar Scalar;
    typedef typename Functor::InputType InputType;
    typedef typename Functor::ValueType ValueType;
    typedef typename Functor::JacobianType JacobianType;

    NumericalDiff(Scalar _epsfcn=0.) : Functor(), epsfcn(_epsfcn) {}
    NumericalDiff(const Functor& f, Scalar _epsfcn=0.) : Functor(f), epsfcn(_epsfcn) {}

    
    template<typename T0>
        NumericalDiff(const T0& a0) : Functor(a0), epsfcn(0) {}
    template<typename T0, typename T1>
        NumericalDiff(const T0& a0, const T1& a1) : Functor(a0, a1), epsfcn(0) {}
    template<typename T0, typename T1, typename T2>
        NumericalDiff(const T0& a0, const T1& a1, const T2& a2) : Functor(a0, a1, a2), epsfcn(0) {}

    enum {
        InputsAtCompileTime = Functor::InputsAtCompileTime,
        ValuesAtCompileTime = Functor::ValuesAtCompileTime
    };

    int df(const InputType& _x, JacobianType &jac) const
    {
        using std::sqrt;
        using std::abs;
        
        Scalar h;
        int nfev=0;
        const typename InputType::Index n = _x.size();
        const Scalar eps = sqrt(((std::max)(epsfcn,NumTraits<Scalar>::epsilon() )));
        ValueType val1, val2;
        InputType x = _x;
        
        val1.resize(Functor::values());
        val2.resize(Functor::values());

        
        switch(mode) {
            case Forward:
                
                Functor::operator()(x, val1); nfev++;
                break;
            case Central:
                
                break;
            default:
                eigen_assert(false);
        };

        
        for (int j = 0; j < n; ++j) {
            h = eps * abs(x[j]);
            if (h == 0.) {
                h = eps;
            }
            switch(mode) {
                case Forward:
                    x[j] += h;
                    Functor::operator()(x, val2);
                    nfev++;
                    x[j] = _x[j];
                    jac.col(j) = (val2-val1)/h;
                    break;
                case Central:
                    x[j] += h;
                    Functor::operator()(x, val2); nfev++;
                    x[j] -= 2*h;
                    Functor::operator()(x, val1); nfev++;
                    x[j] = _x[j];
                    jac.col(j) = (val2-val1)/(2*h);
                    break;
                default:
                    eigen_assert(false);
            };
        }
        return nfev;
    }
private:
    Scalar epsfcn;

    NumericalDiff& operator=(const NumericalDiff&);
};

} 

#endif 

