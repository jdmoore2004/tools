/* 
 * Copyright 2009-2016 The VOTCA Development Team (http://www.votca.org)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <votca/tools/linalg.h>
#include <boost/numeric/ublas/matrix_proxy.hpp>

#include <gsl/gsl_linalg.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_eigen.h>


namespace votca { namespace tools {

using namespace std;


void linalg_qrsolve(ub::vector<double> &x, ub::matrix<double> &A, ub::vector<double> &b, ub::vector<double> *residual)
{
    // check matrix for zero column
    int nonzero_found = 0;
    for(size_t j=0; j<A.size2(); j++) {
        nonzero_found = 0;
        for(size_t i=0; i<A.size1(); i++) {
            if(fabs(A(i,j))>0) {
                nonzero_found = 1;
            }
        }
        if(nonzero_found==0) {
            throw "qrsolve_zero_column_in_matrix";
        }
    }

    gsl_matrix_view m
        = gsl_matrix_view_array (&A(0,0), A.size1(), A.size2());

    gsl_vector_view gb
        = gsl_vector_view_array (&b(0), b.size());

    gsl_vector *gsl_x = gsl_vector_alloc (x.size());
    gsl_vector *tau = gsl_vector_alloc (x.size());
    gsl_vector *gsl_residual = gsl_vector_alloc (b.size());

    gsl_linalg_QR_decomp (&m.matrix, tau);

    gsl_linalg_QR_lssolve (&m.matrix, tau, &gb.vector, gsl_x, gsl_residual);

    for (size_t i =0 ; i < x.size(); i++)
        x(i) = gsl_vector_get(gsl_x, i);

    if(residual)
        for (size_t i =0 ; i < residual->size(); i++)
            (*residual)(i) = gsl_vector_get(gsl_residual, i);

    gsl_vector_free (gsl_x);
    gsl_vector_free (tau);
    gsl_vector_free (gsl_residual);
}


void linalg_constrained_qrsolve(ub::vector<double> &x, ub::matrix<double> &A, ub::vector<double> &b, ub::matrix<double> &constr)
{
    // check matrix for zero column
    int nonzero_found = 0;
    for(size_t j=0; j<A.size2(); j++) {
        nonzero_found = 0;
        for(size_t i=0; i<A.size1(); i++) {
            if(fabs(A(i,j))>0) {
                nonzero_found = 1;
            }
        }
        if(nonzero_found==0) {
            throw std::runtime_error("constrained_qrsolve_zero_column_in_matrix");
        }
    }

    const int N = b.size();
    const int ngrid = x.size()/2;
    const int ysize = constr.size1(); //number of constraints is number of rows of constr
    //std::cout << "ysize: " << ysize << std::endl;
    
    //std::cout << "constr^T.size1: " << constr.size1() << " , constr^T.size2: " << constr.size2() << std::endl;
    
    // Transpose constr:
    constr = trans(constr);

    // temporary variables
    ub::matrix<double> Q(2*ngrid, 2*ngrid);  
    // Q matrix: QR decomposition of trans(B) (trans(B) has 2*ngrid rows and ysize columns )
    // Q matrix is orthogonal 2*ngrid x 2*ngrid matrix
    ub::matrix<double> Q_k(2*ngrid, 2*ngrid);
    ub::identity_matrix<double> I (2*ngrid);
    ub::vector<double> v(2*ngrid);

    Q = ub::zero_matrix<double>(2*ngrid, 2*ngrid);
    Q_k = ub::zero_matrix<double>(2*ngrid, 2*ngrid);
    v = ub::zero_vector<double>(2*ngrid);

    double *tmp = & constr(0,0);
    gsl_matrix_view gsl_constr
      = gsl_matrix_view_array (tmp, constr.size1(), constr.size2());

    tmp = &b(0);
    gsl_vector_view gsl_b
         = gsl_vector_view_array (tmp, b.size());


    //vector to store Householder coefficients of QR-decomposition of trans(B)
    gsl_vector *tau_qr = gsl_vector_alloc (ysize);

    gsl_linalg_QR_decomp (&gsl_constr.matrix, tau_qr);

    //construct the matrix Q of trans(B)=Q * R
    Q = I;

    //k=1,..,ysize (number of rows of B, meaning number of constraints)
    for (int k = ysize; k > 0 ; k--) {

        for (int icout = 0; icout < k - 1; icout++) {
             v(icout) = 0;
        }
        v(k - 1) = 1.0;

        for (int icout = k; icout < 2*ngrid; icout++) {
             v(icout) = gsl_matrix_get(&gsl_constr.matrix, icout, k - 1 );
        }

        Q_k = I - gsl_vector_get(tau_qr, k - 1 ) * outer_prod ( v, v );
        Q = prec_prod(Q, Q_k);

    }

    //now Q is the one of trans(B)=Q * R
    Q = trans(Q);
    gsl_vector_free (tau_qr);

    // Calculate A * Q and store the result in A
    A = prec_prod(A, Q);


    // A = [A1 A2], so A2 is just a block of A
    // [A1 A2] has N rows. A1 has ysize columns (range(0,ysize) ).
    //A2 has 2*ngrid-ysize columns ( range(ysize,2*ngrid) ).
    ub::matrix<double> A2 = ub::matrix_range<ub::matrix<double> >(A,
            ub::range (0, N), ub::range (ysize, 2*ngrid)
         );

    tmp = &A2(0,0);
    gsl_matrix_view gsl_A2
         = gsl_matrix_view_array (tmp, A2.size1(), A2.size2());
    
    
   
    //now perform QR-decomposition of A2 to solve the least-squares problem A2 * z = b
    //A2 has N rows and (2*ngrid-ysize) columns -> 
    gsl_vector *z = gsl_vector_alloc (2*ngrid-ysize);
    gsl_vector *tau_solve = gsl_vector_alloc (2*ngrid-ysize);  
    gsl_vector *residual = gsl_vector_alloc (N);

    gsl_linalg_QR_decomp (&gsl_A2.matrix, tau_solve);
    gsl_linalg_QR_lssolve (&gsl_A2.matrix, tau_solve, &gsl_b.vector, z, residual);

    // Next two cycles assemble vector from y (which is zero-vector) and z
    // (which we just got by gsl_linalg_QR_lssolve)

    //vector y has ysize components
    for (int i = 0; i < ysize; i++ ) {
           x[i] = 0.0;
    }

    //vector z has (2*ngrid-ysize) components
    for (int i = ysize; i < 2 * ngrid; i++ ) {
           x[i] = gsl_vector_get(z, i - ysize);
    }
    
    /*for (int i = 0; i < 2 * ngrid; i++ ) {
        std::cout << "x[" << i << "]: " << x[i] << std::endl;
    }*/

    // To get the final answer this vector should be multiplied by matrix Q
    // TODO: here i changed the sign, check again! (victor)
    x = -prec_prod( Q, x );
    
    /*for (int i = 0; i < 2 * ngrid; i++ ) {
        std::cout << "x[" << i << "]: " << x[i] << std::endl;
    }*/

    gsl_vector_free (z);
    gsl_vector_free (tau_solve);
    gsl_vector_free (residual);
}

/*void linalg_constrained_qrsolve(ub::vector<double> &x, ub::matrix<double> &A, ub::vector<double> &b, ub::matrix<double> &constr)
{
    // check matrix for zero column
    int nonzero_found = 0;
    for(size_t j=0; j<A.size2(); j++) {
        nonzero_found = 0;
        for(size_t i=0; i<A.size1(); i++) {
            if(fabs(A(i,j))>0) {
                nonzero_found = 1;
            }
        }
        if(nonzero_found==0) {
            throw std::runtime_error("constrained_qrsolve_zero_column_in_matrix");
        }
    }

    // Transpose constr:
    constr = trans(constr);

    const int N = b.size();
    const int ngrid = x.size()/2;

    // temporary variables
    ub::matrix<double> Q(2*ngrid, 2*ngrid);       // Q matrix: QR decomposition of trans(B)
    ub::matrix<double> Q_k(2*ngrid, 2*ngrid);
    ub::identity_matrix<double> I (2*ngrid);
    ub::vector<double> v(2*ngrid);

    Q = ub::zero_matrix<double>(2*ngrid, 2*ngrid);
    Q_k = ub::zero_matrix<double>(2*ngrid, 2*ngrid);
    v = ub::zero_vector<double>(2*ngrid);

    double *tmp = & constr(0,0);
    gsl_matrix_view gsl_constr
      = gsl_matrix_view_array (tmp, constr.size1(), constr.size2());

    tmp = &b(0);
    gsl_vector_view gsl_b
         = gsl_vector_view_array (tmp, b.size());


    gsl_vector *tau_qr = gsl_vector_alloc (ngrid);

    gsl_linalg_QR_decomp (&gsl_constr.matrix, tau_qr);

    Q = I;

    for (int k = ngrid; k > 0 ; k--) {

        for (int icout = 0; icout < k - 1; icout++) {
             v(icout) = 0;
        }
        v(k - 1) = 1.0;

        for (int icout = k; icout < 2*ngrid; icout++) {
             v(icout) = gsl_matrix_get(&gsl_constr.matrix, icout, k - 1 );
        }

        Q_k = I - gsl_vector_get(tau_qr, k - 1 ) * outer_prod ( v, v );
        Q = prec_prod(Q, Q_k);

    }

    Q = trans(Q);
    gsl_vector_free (tau_qr);

    // Calculate A * Q and store the result in A
    A = prec_prod(A, Q);


    // A = [A1 A2], so A2 is just a block of A
    ub::matrix<double> A2 = ub::matrix_range<ub::matrix<double> >(A,
            ub::range (0, N), ub::range (ngrid, 2*ngrid)
         );

    tmp = &A2(0,0);
    gsl_matrix_view gsl_A2
         = gsl_matrix_view_array (tmp, A2.size1(), A2.size2());
   
        
    gsl_vector *z = gsl_vector_alloc (ngrid);
    gsl_vector *tau_solve = gsl_vector_alloc (ngrid);  // already done!
    gsl_vector *residual = gsl_vector_alloc (N);

    gsl_linalg_QR_decomp (&gsl_A2.matrix, tau_solve);
    gsl_linalg_QR_lssolve (&gsl_A2.matrix, tau_solve, &gsl_b.vector, z, residual);

    // Next two cycles assemble vector from y (which is zero-vector) and z
    // (which we just got by gsl_linalg_QR_lssolve)

    for (int i = 0; i < ngrid; i++ ) {
           x[i] = 0.0;
    }

    for (int i = ngrid; i < 2 * ngrid; i++ ) {
           x[i] = gsl_vector_get(z, i - ngrid);
    }

    // To get the final answer this vector should be multiplied by matrix Q
    // TODO: here i changed the sign, check again! (victor)
    x = -prec_prod( Q, x );

    gsl_vector_free (z);
    gsl_vector_free (tau_solve);
    gsl_vector_free (residual);
}*/

}}
