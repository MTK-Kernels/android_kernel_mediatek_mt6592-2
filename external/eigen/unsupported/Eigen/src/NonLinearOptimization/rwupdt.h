namespace Eigen { 

namespace internal {

template <typename Scalar>
void rwupdt(
        Matrix< Scalar, Dynamic, Dynamic >  &r,
        const Matrix< Scalar, Dynamic, 1>  &w,
        Matrix< Scalar, Dynamic, 1>  &b,
        Scalar alpha)
{
    typedef DenseIndex Index;

    const Index n = r.cols();
    eigen_assert(r.rows()>=n);
    std::vector<JacobiRotation<Scalar> > givens(n);

    
    Scalar temp, rowj;

    
    for (Index j = 0; j < n; ++j) {
        rowj = w[j];

        
        
        for (Index i = 0; i < j; ++i) {
            temp = givens[i].c() * r(i,j) + givens[i].s() * rowj;
            rowj = -givens[i].s() * r(i,j) + givens[i].c() * rowj;
            r(i,j) = temp;
        }

        
        givens[j].makeGivens(-r(j,j), rowj);

        if (rowj == 0.)
            continue; 

        
        r(j,j) = givens[j].c() * r(j,j) + givens[j].s() * rowj;
        temp = givens[j].c() * b[j] + givens[j].s() * alpha;
        alpha = -givens[j].s() * b[j] + givens[j].c() * alpha;
        b[j] = temp;
    }
}

} 

} 
