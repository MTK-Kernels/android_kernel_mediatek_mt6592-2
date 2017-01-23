// Copyright (C) 2008-2009 Guillaume Saupin <guillaume.saupin@cea.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SKYLINEMATRIXBASE_H
#define EIGEN_SKYLINEMATRIXBASE_H

#include "SkylineUtil.h"

namespace Eigen { 

template<typename Derived> class SkylineMatrixBase : public EigenBase<Derived> {
public:

    typedef typename internal::traits<Derived>::Scalar Scalar;
    typedef typename internal::traits<Derived>::StorageKind StorageKind;
    typedef typename internal::index<StorageKind>::type Index;

    enum {
        RowsAtCompileTime = internal::traits<Derived>::RowsAtCompileTime,

        ColsAtCompileTime = internal::traits<Derived>::ColsAtCompileTime,


        SizeAtCompileTime = (internal::size_at_compile_time<internal::traits<Derived>::RowsAtCompileTime,
        internal::traits<Derived>::ColsAtCompileTime>::ret),

        MaxRowsAtCompileTime = RowsAtCompileTime,
        MaxColsAtCompileTime = ColsAtCompileTime,

        MaxSizeAtCompileTime = (internal::size_at_compile_time<MaxRowsAtCompileTime,
        MaxColsAtCompileTime>::ret),

        IsVectorAtCompileTime = RowsAtCompileTime == 1 || ColsAtCompileTime == 1,

        Flags = internal::traits<Derived>::Flags,

        CoeffReadCost = internal::traits<Derived>::CoeffReadCost,

        IsRowMajor = Flags & RowMajorBit ? 1 : 0
    };

#ifndef EIGEN_PARSED_BY_DOXYGEN
    typedef typename NumTraits<Scalar>::Real RealScalar;

    
    typedef Matrix<Scalar, EIGEN_SIZE_MAX(RowsAtCompileTime, ColsAtCompileTime),
                           EIGEN_SIZE_MAX(RowsAtCompileTime, ColsAtCompileTime) > SquareMatrixType;

    inline const Derived& derived() const {
        return *static_cast<const Derived*> (this);
    }

    inline Derived& derived() {
        return *static_cast<Derived*> (this);
    }

    inline Derived& const_cast_derived() const {
        return *static_cast<Derived*> (const_cast<SkylineMatrixBase*> (this));
    }
#endif 

    
    inline Index rows() const {
        return derived().rows();
    }

    
    inline Index cols() const {
        return derived().cols();
    }

    inline Index size() const {
        return rows() * cols();
    }

    inline Index nonZeros() const {
        return derived().nonZeros();
    }

    Index outerSize() const {
        return (int(Flags) & RowMajorBit) ? this->rows() : this->cols();
    }

    Index innerSize() const {
        return (int(Flags) & RowMajorBit) ? this->cols() : this->rows();
    }

    bool isRValue() const {
        return m_isRValue;
    }

    Derived& markAsRValue() {
        m_isRValue = true;
        return derived();
    }

    SkylineMatrixBase() : m_isRValue(false) {
        
    }

    inline Derived & operator=(const Derived& other) {
        this->operator=<Derived > (other);
        return derived();
    }

    template<typename OtherDerived>
    inline void assignGeneric(const OtherDerived& other) {
        derived().resize(other.rows(), other.cols());
        for (Index row = 0; row < rows(); row++)
            for (Index col = 0; col < cols(); col++) {
                if (other.coeff(row, col) != Scalar(0))
                    derived().insert(row, col) = other.coeff(row, col);
            }
        derived().finalize();
    }

    template<typename OtherDerived>
            inline Derived & operator=(const SkylineMatrixBase<OtherDerived>& other) {
        
    }

    template<typename Lhs, typename Rhs>
            inline Derived & operator=(const SkylineProduct<Lhs, Rhs, SkylineTimeSkylineProduct>& product);

    friend std::ostream & operator <<(std::ostream & s, const SkylineMatrixBase& m) {
        s << m.derived();
        return s;
    }

    template<typename OtherDerived>
    const typename SkylineProductReturnType<Derived, OtherDerived>::Type
    operator*(const MatrixBase<OtherDerived> &other) const;

    
    template<typename DenseDerived>
    void evalTo(MatrixBase<DenseDerived>& dst) const {
        dst.setZero();
        for (Index i = 0; i < rows(); i++)
            for (Index j = 0; j < rows(); j++)
                dst(i, j) = derived().coeff(i, j);
    }

    Matrix<Scalar, RowsAtCompileTime, ColsAtCompileTime> toDense() const {
        return derived();
    }

    EIGEN_STRONG_INLINE const typename internal::eval<Derived, IsSkyline>::type eval() const {
        return typename internal::eval<Derived>::type(derived());
    }

protected:
    bool m_isRValue;
};

} 

#endif 
