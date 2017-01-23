namespace Eigen { 

namespace internal {


template <typename Scalar>
void r1mpyq(DenseIndex m, DenseIndex n, Scalar *a, const std::vector<JacobiRotation<Scalar> > &v_givens, const std::vector<JacobiRotation<Scalar> > &w_givens)
{
    typedef DenseIndex Index;

    
    for (Index j = n-2; j>=0; --j)
        for (Index i = 0; i<m; ++i) {
            Scalar temp = v_givens[j].c() * a[i+m*j] - v_givens[j].s() * a[i+m*(n-1)];
            a[i+m*(n-1)] = v_givens[j].s() * a[i+m*j] + v_givens[j].c() * a[i+m*(n-1)];
            a[i+m*j] = temp;
        }
    
    for (Index j = 0; j<n-1; ++j)
        for (Index i = 0; i<m; ++i) {
            Scalar temp = w_givens[j].c() * a[i+m*j] + w_givens[j].s() * a[i+m*(n-1)];
            a[i+m*(n-1)] = -w_givens[j].s() * a[i+m*j] + w_givens[j].c() * a[i+m*(n-1)];
            a[i+m*j] = temp;
        }
}

} 

} 
