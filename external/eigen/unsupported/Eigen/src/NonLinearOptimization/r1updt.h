namespace Eigen { 

namespace internal {

template <typename Scalar>
void r1updt(
        Matrix< Scalar, Dynamic, Dynamic > &s,
        const Matrix< Scalar, Dynamic, 1> &u,
        std::vector<JacobiRotation<Scalar> > &v_givens,
        std::vector<JacobiRotation<Scalar> > &w_givens,
        Matrix< Scalar, Dynamic, 1> &v,
        Matrix< Scalar, Dynamic, 1> &w,
        bool *sing)
{
    typedef DenseIndex Index;
    const JacobiRotation<Scalar> IdentityRotation = JacobiRotation<Scalar>(1,0);

    
    const Index m = s.rows();
    const Index n = s.cols();
    Index i, j=1;
    Scalar temp;
    JacobiRotation<Scalar> givens;

    
    
    eigen_assert(m==n);
    eigen_assert(u.size()==m);
    eigen_assert(v.size()==n);
    eigen_assert(w.size()==n);

    
    w[n-1] = s(n-1,n-1);

    
    
    for (j=n-2; j>=0; --j) {
        w[j] = 0.;
        if (v[j] != 0.) {
            
            
            givens.makeGivens(-v[n-1], v[j]);

            
            
            v[n-1] = givens.s() * v[j] + givens.c() * v[n-1];
            v_givens[j] = givens;

            
            for (i = j; i < m; ++i) {
                temp = givens.c() * s(j,i) - givens.s() * w[i];
                w[i] = givens.s() * s(j,i) + givens.c() * w[i];
                s(j,i) = temp;
            }
        } else
            v_givens[j] = IdentityRotation;
    }

    
    w += v[n-1] * u;

    
    *sing = false;
    for (j = 0; j < n-1; ++j) {
        if (w[j] != 0.) {
            
            
            givens.makeGivens(-s(j,j), w[j]);

            
            for (i = j; i < m; ++i) {
                temp = givens.c() * s(j,i) + givens.s() * w[i];
                w[i] = -givens.s() * s(j,i) + givens.c() * w[i];
                s(j,i) = temp;
            }

            
            
            w_givens[j] = givens;
        } else
            v_givens[j] = IdentityRotation;

        
        if (s(j,j) == 0.) {
            *sing = true;
        }
    }
    
    s(n-1,n-1) = w[n-1];

    if (s(j,j) == 0.) {
        *sing = true;
    }
    return;
}

} 

} 
