// Copyright (C) 2008 Guillaume Saupin <guillaume.saupin@cea.fr>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_SKYLINEINPLACELU_H
#define EIGEN_SKYLINEINPLACELU_H

namespace Eigen { 

template<typename MatrixType>
class SkylineInplaceLU {
protected:
    typedef typename MatrixType::Scalar Scalar;
    typedef typename MatrixType::Index Index;
    
    typedef typename NumTraits<typename MatrixType::Scalar>::Real RealScalar;

public:

    SkylineInplaceLU(MatrixType& matrix, int flags = 0)
    :  m_flags(flags), m_status(0), m_lu(matrix) {
        m_precision = RealScalar(0.1) * Eigen::dummy_precision<RealScalar > ();
        m_lu.IsRowMajor ? computeRowMajor() : compute();
    }

    void setPrecision(RealScalar v) {
        m_precision = v;
    }

    RealScalar precision() const {
        return m_precision;
    }

    void setFlags(int f) {
        m_flags = f;
    }

    
    int flags() const {
        return m_flags;
    }

    void setOrderingMethod(int m) {
        m_flags = m;
    }

    int orderingMethod() const {
        return m_flags;
    }

    
    void compute();
    void computeRowMajor();

    
    

    
    

    template<typename BDerived, typename XDerived>
    bool solve(const MatrixBase<BDerived> &b, MatrixBase<XDerived>* x,
            const int transposed = 0) const;

    
    inline bool succeeded(void) const {
        return m_succeeded;
    }

protected:
    RealScalar m_precision;
    int m_flags;
    mutable int m_status;
    bool m_succeeded;
    MatrixType& m_lu;
};

template<typename MatrixType>
void SkylineInplaceLU<MatrixType>::compute() {
    const size_t rows = m_lu.rows();
    const size_t cols = m_lu.cols();

    eigen_assert(rows == cols && "We do not (yet) support rectangular LU.");
    eigen_assert(!m_lu.IsRowMajor && "LU decomposition does not work with rowMajor Storage");

    for (Index row = 0; row < rows; row++) {
        const double pivot = m_lu.coeffDiag(row);

        
        const Index& col = row;
        for (typename MatrixType::InnerLowerIterator lIt(m_lu, col); lIt; ++lIt) {
            lIt.valueRef() /= pivot;
        }

        
        typename MatrixType::InnerLowerIterator lIt(m_lu, col);
        for (Index rrow = row + 1; rrow < m_lu.rows(); rrow++) {
            typename MatrixType::InnerUpperIterator uItPivot(m_lu, row);
            typename MatrixType::InnerUpperIterator uIt(m_lu, rrow);
            const double coef = lIt.value();

            uItPivot += (rrow - row - 1);

            
            for (++uItPivot; uIt && uItPivot;) {
                uIt.valueRef() -= uItPivot.value() * coef;

                ++uIt;
                ++uItPivot;
            }
            ++lIt;
        }

        
        typename MatrixType::InnerLowerIterator lIt3(m_lu, col);
        for (Index rrow = row + 1; rrow < m_lu.rows(); rrow++) {
            typename MatrixType::InnerUpperIterator uItPivot(m_lu, row);
            const double coef = lIt3.value();

            
            for (Index i = 0; i < rrow - row - 1; i++) {
                m_lu.coeffRefLower(rrow, row + i + 1) -= uItPivot.value() * coef;
                ++uItPivot;
            }
            ++lIt3;
        }
        
        typename MatrixType::InnerLowerIterator lIt2(m_lu, col);
        for (Index rrow = row + 1; rrow < m_lu.rows(); rrow++) {

            typename MatrixType::InnerUpperIterator uItPivot(m_lu, row);
            typename MatrixType::InnerUpperIterator uIt(m_lu, rrow);
            const double coef = lIt2.value();

            uItPivot += (rrow - row - 1);
            m_lu.coeffRefDiag(rrow) -= uItPivot.value() * coef;
            ++lIt2;
        }
    }
}

template<typename MatrixType>
void SkylineInplaceLU<MatrixType>::computeRowMajor() {
    const size_t rows = m_lu.rows();
    const size_t cols = m_lu.cols();

    eigen_assert(rows == cols && "We do not (yet) support rectangular LU.");
    eigen_assert(m_lu.IsRowMajor && "You're trying to apply rowMajor decomposition on a ColMajor matrix !");

    for (Index row = 0; row < rows; row++) {
        typename MatrixType::InnerLowerIterator llIt(m_lu, row);


        for (Index col = llIt.col(); col < row; col++) {
            if (m_lu.coeffExistLower(row, col)) {
                const double diag = m_lu.coeffDiag(col);

                typename MatrixType::InnerLowerIterator lIt(m_lu, row);
                typename MatrixType::InnerUpperIterator uIt(m_lu, col);


                const Index offset = lIt.col() - uIt.row();


                Index stop = offset > 0 ? col - lIt.col() : col - uIt.row();

                
#ifdef VECTORIZE
                Map<VectorXd > rowVal(lIt.valuePtr() + (offset > 0 ? 0 : -offset), stop);
                Map<VectorXd > colVal(uIt.valuePtr() + (offset > 0 ? offset : 0), stop);


                Scalar newCoeff = m_lu.coeffLower(row, col) - rowVal.dot(colVal);
#else
                if (offset > 0) 
                    uIt += offset;
                else 
                    lIt += -offset;
                Scalar newCoeff = m_lu.coeffLower(row, col);

                for (Index k = 0; k < stop; ++k) {
                    const Scalar tmp = newCoeff;
                    newCoeff = tmp - lIt.value() * uIt.value();
                    ++lIt;
                    ++uIt;
                }
#endif

                m_lu.coeffRefLower(row, col) = newCoeff / diag;
            }
        }

        
        const Index col = row;
        typename MatrixType::InnerUpperIterator uuIt(m_lu, col);
        for (Index rrow = uuIt.row(); rrow < col; rrow++) {

            typename MatrixType::InnerLowerIterator lIt(m_lu, rrow);
            typename MatrixType::InnerUpperIterator uIt(m_lu, col);
            const Index offset = lIt.col() - uIt.row();

            Index stop = offset > 0 ? rrow - lIt.col() : rrow - uIt.row();

#ifdef VECTORIZE
            Map<VectorXd > rowVal(lIt.valuePtr() + (offset > 0 ? 0 : -offset), stop);
            Map<VectorXd > colVal(uIt.valuePtr() + (offset > 0 ? offset : 0), stop);

            Scalar newCoeff = m_lu.coeffUpper(rrow, col) - rowVal.dot(colVal);
#else
            if (offset > 0) 
                uIt += offset;
            else 
                lIt += -offset;
            Scalar newCoeff = m_lu.coeffUpper(rrow, col);
            for (Index k = 0; k < stop; ++k) {
                const Scalar tmp = newCoeff;
                newCoeff = tmp - lIt.value() * uIt.value();

                ++lIt;
                ++uIt;
            }
#endif
            m_lu.coeffRefUpper(rrow, col) = newCoeff;
        }


        
        typename MatrixType::InnerLowerIterator lIt(m_lu, row);
        typename MatrixType::InnerUpperIterator uIt(m_lu, row);

        const Index offset = lIt.col() - uIt.row();


        Index stop = offset > 0 ? lIt.size() : uIt.size();
#ifdef VECTORIZE
        Map<VectorXd > rowVal(lIt.valuePtr() + (offset > 0 ? 0 : -offset), stop);
        Map<VectorXd > colVal(uIt.valuePtr() + (offset > 0 ? offset : 0), stop);
        Scalar newCoeff = m_lu.coeffDiag(row) - rowVal.dot(colVal);
#else
        if (offset > 0) 
            uIt += offset;
        else 
            lIt += -offset;
        Scalar newCoeff = m_lu.coeffDiag(row);
        for (Index k = 0; k < stop; ++k) {
            const Scalar tmp = newCoeff;
            newCoeff = tmp - lIt.value() * uIt.value();
            ++lIt;
            ++uIt;
        }
#endif
        m_lu.coeffRefDiag(row) = newCoeff;
    }
}

template<typename MatrixType>
template<typename BDerived, typename XDerived>
bool SkylineInplaceLU<MatrixType>::solve(const MatrixBase<BDerived> &b, MatrixBase<XDerived>* x, const int transposed) const {
    const size_t rows = m_lu.rows();
    const size_t cols = m_lu.cols();


    for (Index row = 0; row < rows; row++) {
        x->coeffRef(row) = b.coeff(row);
        Scalar newVal = x->coeff(row);
        typename MatrixType::InnerLowerIterator lIt(m_lu, row);

        Index col = lIt.col();
        while (lIt.col() < row) {

            newVal -= x->coeff(col++) * lIt.value();
            ++lIt;
        }

        x->coeffRef(row) = newVal;
    }


    for (Index col = rows - 1; col > 0; col--) {
        x->coeffRef(col) = x->coeff(col) / m_lu.coeffDiag(col);

        const Scalar x_col = x->coeff(col);

        typename MatrixType::InnerUpperIterator uIt(m_lu, col);
        uIt += uIt.size()-1;


        while (uIt) {
            x->coeffRef(uIt.row()) -= x_col * uIt.value();
            
            uIt += -1;
        }


    }
    x->coeffRef(0) = x->coeff(0) / m_lu.coeffDiag(0);

    return true;
}

} 

#endif 
