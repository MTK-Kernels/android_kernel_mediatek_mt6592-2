namespace Eigen { 

namespace internal {

template <typename Scalar>
void lmpar(
        Matrix< Scalar, Dynamic, Dynamic > &r,
        const VectorXi &ipvt,
        const Matrix< Scalar, Dynamic, 1 >  &diag,
        const Matrix< Scalar, Dynamic, 1 >  &qtb,
        Scalar delta,
        Scalar &par,
        Matrix< Scalar, Dynamic, 1 >  &x)
{
    using std::abs;
    using std::sqrt;
    typedef DenseIndex Index;

    
    Index i, j, l;
    Scalar fp;
    Scalar parc, parl;
    Index iter;
    Scalar temp, paru;
    Scalar gnorm;
    Scalar dxnorm;


    
    const Scalar dwarf = (std::numeric_limits<Scalar>::min)();
    const Index n = r.cols();
    eigen_assert(n==diag.size());
    eigen_assert(n==qtb.size());
    eigen_assert(n==x.size());

    Matrix< Scalar, Dynamic, 1 >  wa1, wa2;

    
    
    Index nsing = n-1;
    wa1 = qtb;
    for (j = 0; j < n; ++j) {
        if (r(j,j) == 0. && nsing == n-1)
            nsing = j - 1;
        if (nsing < n-1)
            wa1[j] = 0.;
    }
    for (j = nsing; j>=0; --j) {
        wa1[j] /= r(j,j);
        temp = wa1[j];
        for (i = 0; i < j ; ++i)
            wa1[i] -= r(i,j) * temp;
    }

    for (j = 0; j < n; ++j)
        x[ipvt[j]] = wa1[j];

    
    
    
    iter = 0;
    wa2 = diag.cwiseProduct(x);
    dxnorm = wa2.blueNorm();
    fp = dxnorm - delta;
    if (fp <= Scalar(0.1) * delta) {
        par = 0;
        return;
    }

    
    
    
    parl = 0.;
    if (nsing >= n-1) {
        for (j = 0; j < n; ++j) {
            l = ipvt[j];
            wa1[j] = diag[l] * (wa2[l] / dxnorm);
        }
        
        
        for (j = 0; j < n; ++j) {
            Scalar sum = 0.;
            for (i = 0; i < j; ++i)
                sum += r(i,j) * wa1[i];
            wa1[j] = (wa1[j] - sum) / r(j,j);
        }
        temp = wa1.blueNorm();
        parl = fp / delta / temp / temp;
    }

    
    for (j = 0; j < n; ++j)
        wa1[j] = r.col(j).head(j+1).dot(qtb.head(j+1)) / diag[ipvt[j]];

    gnorm = wa1.stableNorm();
    paru = gnorm / delta;
    if (paru == 0.)
        paru = dwarf / (std::min)(delta,Scalar(0.1));

    
    
    par = (std::max)(par,parl);
    par = (std::min)(par,paru);
    if (par == 0.)
        par = gnorm / dxnorm;

    
    while (true) {
        ++iter;

        
        if (par == 0.)
            par = (std::max)(dwarf,Scalar(.001) * paru); 
        wa1 = sqrt(par)* diag;

        Matrix< Scalar, Dynamic, 1 > sdiag(n);
        qrsolv<Scalar>(r, ipvt, wa1, qtb, x, sdiag);

        wa2 = diag.cwiseProduct(x);
        dxnorm = wa2.blueNorm();
        temp = fp;
        fp = dxnorm - delta;

        
        
        
        if (abs(fp) <= Scalar(0.1) * delta || (parl == 0. && fp <= temp && temp < 0.) || iter == 10)
            break;

        
        for (j = 0; j < n; ++j) {
            l = ipvt[j];
            wa1[j] = diag[l] * (wa2[l] / dxnorm);
        }
        for (j = 0; j < n; ++j) {
            wa1[j] /= sdiag[j];
            temp = wa1[j];
            for (i = j+1; i < n; ++i)
                wa1[i] -= r(i,j) * temp;
        }
        temp = wa1.blueNorm();
        parc = fp / delta / temp / temp;

        
        if (fp > 0.)
            parl = (std::max)(parl,par);
        if (fp < 0.)
            paru = (std::min)(paru,par);

        
        
        par = (std::max)(parl,par+parc);

        
    }

    
    if (iter == 0)
        par = 0.;
    return;
}

template <typename Scalar>
void lmpar2(
        const ColPivHouseholderQR<Matrix< Scalar, Dynamic, Dynamic> > &qr,
        const Matrix< Scalar, Dynamic, 1 >  &diag,
        const Matrix< Scalar, Dynamic, 1 >  &qtb,
        Scalar delta,
        Scalar &par,
        Matrix< Scalar, Dynamic, 1 >  &x)

{
    using std::sqrt;
    using std::abs;
    typedef DenseIndex Index;

    
    Index j;
    Scalar fp;
    Scalar parc, parl;
    Index iter;
    Scalar temp, paru;
    Scalar gnorm;
    Scalar dxnorm;


    
    const Scalar dwarf = (std::numeric_limits<Scalar>::min)();
    const Index n = qr.matrixQR().cols();
    eigen_assert(n==diag.size());
    eigen_assert(n==qtb.size());

    Matrix< Scalar, Dynamic, 1 >  wa1, wa2;

    
    

    const Index rank = qr.rank(); 
    wa1 = qtb;
    wa1.tail(n-rank).setZero();
    qr.matrixQR().topLeftCorner(rank, rank).template triangularView<Upper>().solveInPlace(wa1.head(rank));

    x = qr.colsPermutation()*wa1;

    
    
    
    iter = 0;
    wa2 = diag.cwiseProduct(x);
    dxnorm = wa2.blueNorm();
    fp = dxnorm - delta;
    if (fp <= Scalar(0.1) * delta) {
        par = 0;
        return;
    }

    
    
    
    parl = 0.;
    if (rank==n) {
        wa1 = qr.colsPermutation().inverse() *  diag.cwiseProduct(wa2)/dxnorm;
        qr.matrixQR().topLeftCorner(n, n).transpose().template triangularView<Lower>().solveInPlace(wa1);
        temp = wa1.blueNorm();
        parl = fp / delta / temp / temp;
    }

    
    for (j = 0; j < n; ++j)
        wa1[j] = qr.matrixQR().col(j).head(j+1).dot(qtb.head(j+1)) / diag[qr.colsPermutation().indices()(j)];

    gnorm = wa1.stableNorm();
    paru = gnorm / delta;
    if (paru == 0.)
        paru = dwarf / (std::min)(delta,Scalar(0.1));

    
    
    par = (std::max)(par,parl);
    par = (std::min)(par,paru);
    if (par == 0.)
        par = gnorm / dxnorm;

    
    Matrix< Scalar, Dynamic, Dynamic > s = qr.matrixQR();
    while (true) {
        ++iter;

        
        if (par == 0.)
            par = (std::max)(dwarf,Scalar(.001) * paru); 
        wa1 = sqrt(par)* diag;

        Matrix< Scalar, Dynamic, 1 > sdiag(n);
        qrsolv<Scalar>(s, qr.colsPermutation().indices(), wa1, qtb, x, sdiag);

        wa2 = diag.cwiseProduct(x);
        dxnorm = wa2.blueNorm();
        temp = fp;
        fp = dxnorm - delta;

        
        
        
        if (abs(fp) <= Scalar(0.1) * delta || (parl == 0. && fp <= temp && temp < 0.) || iter == 10)
            break;

        
        wa1 = qr.colsPermutation().inverse() * diag.cwiseProduct(wa2/dxnorm);
        
        
        for (j = 0; j < n; ++j) {
            wa1[j] /= sdiag[j];
            temp = wa1[j];
            for (Index i = j+1; i < n; ++i)
                wa1[i] -= s(i,j) * temp;
        }
        temp = wa1.blueNorm();
        parc = fp / delta / temp / temp;

        
        if (fp > 0.)
            parl = (std::max)(parl,par);
        if (fp < 0.)
            paru = (std::min)(paru,par);

        
        par = (std::max)(parl,par+parc);
    }
    if (iter == 0)
        par = 0.;
    return;
}

} 

} 
