// Copyright (C) 2009, 2010 Jitse Niesen <jitse@maths.leeds.ac.uk>
// Copyright (C) 2011 Chen-Pang He <jdh8@ms63.hinet.net>
// Public License v. 2.0. If a copy of the MPL was not distributed

#ifndef EIGEN_MATRIX_EXPONENTIAL
#define EIGEN_MATRIX_EXPONENTIAL

#include "StemFunction.h"

namespace Eigen {

template <typename MatrixType>
class MatrixExponential {

  public:

    MatrixExponential(const MatrixType &M);

    template <typename ResultType> 
    void compute(ResultType &result);

  private:

    
    MatrixExponential(const MatrixExponential&);
    MatrixExponential& operator=(const MatrixExponential&);

    void pade3(const MatrixType &A);

    void pade5(const MatrixType &A);

    void pade7(const MatrixType &A);

    void pade9(const MatrixType &A);

    void pade13(const MatrixType &A);

    void pade17(const MatrixType &A);

    void computeUV(double);

    void computeUV(float);
    
    void computeUV(long double);

    typedef typename internal::traits<MatrixType>::Scalar Scalar;
    typedef typename NumTraits<Scalar>::Real RealScalar;
    typedef typename std::complex<RealScalar> ComplexScalar;

    
    typename internal::nested<MatrixType>::type m_M;

    
    MatrixType m_U;

    
    MatrixType m_V;

    
    MatrixType m_tmp1;

    
    MatrixType m_tmp2;

    
    MatrixType m_Id;

    
    int m_squarings;

    
    RealScalar m_l1norm;
};

template <typename MatrixType>
MatrixExponential<MatrixType>::MatrixExponential(const MatrixType &M) :
  m_M(M),
  m_U(M.rows(),M.cols()),
  m_V(M.rows(),M.cols()),
  m_tmp1(M.rows(),M.cols()),
  m_tmp2(M.rows(),M.cols()),
  m_Id(MatrixType::Identity(M.rows(), M.cols())),
  m_squarings(0),
  m_l1norm(M.cwiseAbs().colwise().sum().maxCoeff())
{
  
}

template <typename MatrixType>
template <typename ResultType> 
void MatrixExponential<MatrixType>::compute(ResultType &result)
{
#if LDBL_MANT_DIG > 112 
  if(sizeof(RealScalar) > 14) {
    result = m_M.matrixFunction(StdStemFunctions<ComplexScalar>::exp);
    return;
  }
#endif
  computeUV(RealScalar());
  m_tmp1 = m_U + m_V;   
  m_tmp2 = -m_U + m_V;  
  result = m_tmp2.partialPivLu().solve(m_tmp1);
  for (int i=0; i<m_squarings; i++)
    result *= result;   
}

template <typename MatrixType>
EIGEN_STRONG_INLINE void MatrixExponential<MatrixType>::pade3(const MatrixType &A)
{
  const RealScalar b[] = {120., 60., 12., 1.};
  m_tmp1.noalias() = A * A;
  m_tmp2 = b[3]*m_tmp1 + b[1]*m_Id;
  m_U.noalias() = A * m_tmp2;
  m_V = b[2]*m_tmp1 + b[0]*m_Id;
}

template <typename MatrixType>
EIGEN_STRONG_INLINE void MatrixExponential<MatrixType>::pade5(const MatrixType &A)
{
  const RealScalar b[] = {30240., 15120., 3360., 420., 30., 1.};
  MatrixType A2 = A * A;
  m_tmp1.noalias() = A2 * A2;
  m_tmp2 = b[5]*m_tmp1 + b[3]*A2 + b[1]*m_Id;
  m_U.noalias() = A * m_tmp2;
  m_V = b[4]*m_tmp1 + b[2]*A2 + b[0]*m_Id;
}

template <typename MatrixType>
EIGEN_STRONG_INLINE void MatrixExponential<MatrixType>::pade7(const MatrixType &A)
{
  const RealScalar b[] = {17297280., 8648640., 1995840., 277200., 25200., 1512., 56., 1.};
  MatrixType A2 = A * A;
  MatrixType A4 = A2 * A2;
  m_tmp1.noalias() = A4 * A2;
  m_tmp2 = b[7]*m_tmp1 + b[5]*A4 + b[3]*A2 + b[1]*m_Id;
  m_U.noalias() = A * m_tmp2;
  m_V = b[6]*m_tmp1 + b[4]*A4 + b[2]*A2 + b[0]*m_Id;
}

template <typename MatrixType>
EIGEN_STRONG_INLINE void MatrixExponential<MatrixType>::pade9(const MatrixType &A)
{
  const RealScalar b[] = {17643225600., 8821612800., 2075673600., 302702400., 30270240.,
		      2162160., 110880., 3960., 90., 1.};
  MatrixType A2 = A * A;
  MatrixType A4 = A2 * A2;
  MatrixType A6 = A4 * A2;
  m_tmp1.noalias() = A6 * A2;
  m_tmp2 = b[9]*m_tmp1 + b[7]*A6 + b[5]*A4 + b[3]*A2 + b[1]*m_Id;
  m_U.noalias() = A * m_tmp2;
  m_V = b[8]*m_tmp1 + b[6]*A6 + b[4]*A4 + b[2]*A2 + b[0]*m_Id;
}

template <typename MatrixType>
EIGEN_STRONG_INLINE void MatrixExponential<MatrixType>::pade13(const MatrixType &A)
{
  const RealScalar b[] = {64764752532480000., 32382376266240000., 7771770303897600.,
		      1187353796428800., 129060195264000., 10559470521600., 670442572800.,
		      33522128640., 1323241920., 40840800., 960960., 16380., 182., 1.};
  MatrixType A2 = A * A;
  MatrixType A4 = A2 * A2;
  m_tmp1.noalias() = A4 * A2;
  m_V = b[13]*m_tmp1 + b[11]*A4 + b[9]*A2; 
  m_tmp2.noalias() = m_tmp1 * m_V;
  m_tmp2 += b[7]*m_tmp1 + b[5]*A4 + b[3]*A2 + b[1]*m_Id;
  m_U.noalias() = A * m_tmp2;
  m_tmp2 = b[12]*m_tmp1 + b[10]*A4 + b[8]*A2;
  m_V.noalias() = m_tmp1 * m_tmp2;
  m_V += b[6]*m_tmp1 + b[4]*A4 + b[2]*A2 + b[0]*m_Id;
}

#if LDBL_MANT_DIG > 64
template <typename MatrixType>
EIGEN_STRONG_INLINE void MatrixExponential<MatrixType>::pade17(const MatrixType &A)
{
  const RealScalar b[] = {830034394580628357120000.L, 415017197290314178560000.L,
		      100610229646136770560000.L, 15720348382208870400000.L,
		      1774878043152614400000.L, 153822763739893248000.L, 10608466464820224000.L,
		      595373117923584000.L, 27563570274240000.L, 1060137318240000.L,
		      33924394183680.L, 899510451840.L, 19554575040.L, 341863200.L, 4651200.L,
		      46512.L, 306.L, 1.L};
  MatrixType A2 = A * A;
  MatrixType A4 = A2 * A2;
  MatrixType A6 = A4 * A2;
  m_tmp1.noalias() = A4 * A4;
  m_V = b[17]*m_tmp1 + b[15]*A6 + b[13]*A4 + b[11]*A2; 
  m_tmp2.noalias() = m_tmp1 * m_V;
  m_tmp2 += b[9]*m_tmp1 + b[7]*A6 + b[5]*A4 + b[3]*A2 + b[1]*m_Id;
  m_U.noalias() = A * m_tmp2;
  m_tmp2 = b[16]*m_tmp1 + b[14]*A6 + b[12]*A4 + b[10]*A2;
  m_V.noalias() = m_tmp1 * m_tmp2;
  m_V += b[8]*m_tmp1 + b[6]*A6 + b[4]*A4 + b[2]*A2 + b[0]*m_Id;
}
#endif

template <typename MatrixType>
void MatrixExponential<MatrixType>::computeUV(float)
{
  using std::frexp;
  using std::pow;
  if (m_l1norm < 4.258730016922831e-001) {
    pade3(m_M);
  } else if (m_l1norm < 1.880152677804762e+000) {
    pade5(m_M);
  } else {
    const float maxnorm = 3.925724783138660f;
    frexp(m_l1norm / maxnorm, &m_squarings);
    if (m_squarings < 0) m_squarings = 0;
    MatrixType A = m_M / pow(Scalar(2), m_squarings);
    pade7(A);
  }
}

template <typename MatrixType>
void MatrixExponential<MatrixType>::computeUV(double)
{
  using std::frexp;
  using std::pow;
  if (m_l1norm < 1.495585217958292e-002) {
    pade3(m_M);
  } else if (m_l1norm < 2.539398330063230e-001) {
    pade5(m_M);
  } else if (m_l1norm < 9.504178996162932e-001) {
    pade7(m_M);
  } else if (m_l1norm < 2.097847961257068e+000) {
    pade9(m_M);
  } else {
    const double maxnorm = 5.371920351148152;
    frexp(m_l1norm / maxnorm, &m_squarings);
    if (m_squarings < 0) m_squarings = 0;
    MatrixType A = m_M / pow(Scalar(2), m_squarings);
    pade13(A);
  }
}

template <typename MatrixType>
void MatrixExponential<MatrixType>::computeUV(long double)
{
  using std::frexp;
  using std::pow;
#if   LDBL_MANT_DIG == 53   
  computeUV(double());
#elif LDBL_MANT_DIG <= 64   
  if (m_l1norm < 4.1968497232266989671e-003L) {
    pade3(m_M);
  } else if (m_l1norm < 1.1848116734693823091e-001L) {
    pade5(m_M);
  } else if (m_l1norm < 5.5170388480686700274e-001L) {
    pade7(m_M);
  } else if (m_l1norm < 1.3759868875587845383e+000L) {
    pade9(m_M);
  } else {
    const long double maxnorm = 4.0246098906697353063L;
    frexp(m_l1norm / maxnorm, &m_squarings);
    if (m_squarings < 0) m_squarings = 0;
    MatrixType A = m_M / pow(Scalar(2), m_squarings);
    pade13(A);
  }
#elif LDBL_MANT_DIG <= 106  
  if (m_l1norm < 3.2787892205607026992947488108213e-005L) {
    pade3(m_M);
  } else if (m_l1norm < 6.4467025060072760084130906076332e-003L) {
    pade5(m_M);
  } else if (m_l1norm < 6.8988028496595374751374122881143e-002L) {
    pade7(m_M);
  } else if (m_l1norm < 2.7339737518502231741495857201670e-001L) {
    pade9(m_M);
  } else if (m_l1norm < 1.3203382096514474905666448850278e+000L) {
    pade13(m_M);
  } else {
    const long double maxnorm = 3.2579440895405400856599663723517L;
    frexp(m_l1norm / maxnorm, &m_squarings);
    if (m_squarings < 0) m_squarings = 0;
    MatrixType A = m_M / pow(Scalar(2), m_squarings);
    pade17(A);
  }
#elif LDBL_MANT_DIG <= 112  
  if (m_l1norm < 1.639394610288918690547467954466970e-005L) {
    pade3(m_M);
  } else if (m_l1norm < 4.253237712165275566025884344433009e-003L) {
    pade5(m_M);
  } else if (m_l1norm < 5.125804063165764409885122032933142e-002L) {
    pade7(m_M);
  } else if (m_l1norm < 2.170000765161155195453205651889853e-001L) {
    pade9(m_M);
  } else if (m_l1norm < 1.125358383453143065081397882891878e+000L) {
    pade13(m_M);
  } else {
    const long double maxnorm = 2.884233277829519311757165057717815L;
    frexp(m_l1norm / maxnorm, &m_squarings);
    if (m_squarings < 0) m_squarings = 0;
    MatrixType A = m_M / pow(Scalar(2), m_squarings);
    pade17(A);
  }
#else
  
  eigen_assert(false && "Bug in MatrixExponential"); 
#endif  
}

template<typename Derived> struct MatrixExponentialReturnValue
: public ReturnByValue<MatrixExponentialReturnValue<Derived> >
{
    typedef typename Derived::Index Index;
  public:
    MatrixExponentialReturnValue(const Derived& src) : m_src(src) { }

    template <typename ResultType>
    inline void evalTo(ResultType& result) const
    {
      const typename Derived::PlainObject srcEvaluated = m_src.eval();
      MatrixExponential<typename Derived::PlainObject> me(srcEvaluated);
      me.compute(result);
    }

    Index rows() const { return m_src.rows(); }
    Index cols() const { return m_src.cols(); }

  protected:
    const Derived& m_src;
  private:
    MatrixExponentialReturnValue& operator=(const MatrixExponentialReturnValue&);
};

namespace internal {
template<typename Derived>
struct traits<MatrixExponentialReturnValue<Derived> >
{
  typedef typename Derived::PlainObject ReturnType;
};
}

template <typename Derived>
const MatrixExponentialReturnValue<Derived> MatrixBase<Derived>::exp() const
{
  eigen_assert(rows() == cols());
  return MatrixExponentialReturnValue<Derived>(derived());
}

} 

#endif 
