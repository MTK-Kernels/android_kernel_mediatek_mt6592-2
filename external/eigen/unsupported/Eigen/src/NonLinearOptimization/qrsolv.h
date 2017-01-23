namespace Eigen { 

namespace internal {

template <typename Scalar>
void qrsolv(
        Matrix< Scalar, Dynamic, Dynamic > &s,
        
        const VectorXi &ipvt,
        const Matrix< Scalar, Dynamic, 1 >  &diag,
        const Matrix< Scalar, Dynamic, 1 >  &qtb,
        Matrix< Scalar, Dynamic, 1 >  &x,
        Matrix< Scalar, Dynamic, 1 >  &sdiag)

{
    typedef DenseIndex Index;

    
    Index i, j, k, l;
    Scalar temp;
    Index n = s.cols();
    Matrix< Scalar, Dynamic, 1 >  wa(n);
    JacobiRotation<Scalar> givens;

    
    
    

    
    
    x = s.diagonal();
    wa = qtb;

    s.topLeftCorner(n,n).template triangularView<StrictlyLower>() = s.topLeftCorner(n,n).transpose();

    
    for (j = 0; j < n; ++j) {

        
        
        l = ipvt[j];
        if (diag[l] == 0.)
            break;
        sdiag.tail(n-j).setZero();
        sdiag[j] = diag[l];

        
        
        
        Scalar qtbpj = 0.;
        for (k = j; k < n; ++k) {
            
            
            givens.makeGivens(-s(k,k), sdiag[k]);

            
            
            s(k,k) = givens.c() * s(k,k) + givens.s() * sdiag[k];
            temp = givens.c() * wa[k] + givens.s() * qtbpj;
            qtbpj = -givens.s() * wa[k] + givens.c() * qtbpj;
            wa[k] = temp;

            
            for (i = k+1; i<n; ++i) {
                temp = givens.c() * s(i,k) + givens.s() * sdiag[i];
                sdiag[i] = -givens.s() * s(i,k) + givens.c() * sdiag[i];
                s(i,k) = temp;
            }
        }
    }

    
    
    Index nsing;
    for(nsing=0; nsing<n && sdiag[nsing]!=0; nsing++) {}

    wa.tail(n-nsing).setZero();
    s.topLeftCorner(nsing, nsing).transpose().template triangularView<Upper>().solveInPlace(wa.head(nsing));

    
    sdiag = s.diagonal();
    s.diagonal() = x;

    
    for (j = 0; j < n; ++j) x[ipvt[j]] = wa[j];
}

} 

} 
