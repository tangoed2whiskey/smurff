#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <SmurffCpp/Utils/MatrixUtils.h>
#include <SmurffCpp/Utils/Error.h>
#include <SmurffCpp/Utils/counters.h>

#include <SmurffCpp/SideInfo/SparseSideInfo.h>

namespace smurff { namespace linop {

template<typename T>
int  solve_blockcg(Eigen::MatrixXd & X, T & t, double reg, Eigen::MatrixXd & B, double tol, const int blocksize, const int excess, bool throw_on_cholesky_error = false);
template<typename T>
int  solve_blockcg(Eigen::MatrixXd & X, T & t, double reg, Eigen::MatrixXd & B, double tol, bool throw_on_cholesky_error = false);

void At_mul_A(Eigen::MatrixXd & out, Eigen::MatrixXd & A);

template<typename T>
void compute_uhat(Eigen::MatrixXd & uhat, T & feat, Eigen::MatrixXd & beta);
template<typename T>
void AtA_mul_B(Eigen::MatrixXd & out, T & A, double reg, Eigen::MatrixXd & B, Eigen::MatrixXd & tmp);

template<>
void AtA_mul_B(Eigen::MatrixXd & out, Eigen::MatrixXd & A, double reg, Eigen::MatrixXd & B, Eigen::MatrixXd & tmp);

// compile-time optimized versions (N - number of RHSs)
template<typename T>
inline void AtA_mul_B_switch(Eigen::MatrixXd & out, T & A, double reg, Eigen::MatrixXd & B, Eigen::MatrixXd & tmp);
inline void AtA_mul_B_switch(Eigen::MatrixXd & out, Eigen::MatrixXd & A, double reg, Eigen::MatrixXd & B, Eigen::MatrixXd & tmp);

template<int N>
void AtA_mul_Bx(Eigen::MatrixXd & out, SparseSideInfo & A, double reg, Eigen::MatrixXd & B, Eigen::MatrixXd & tmp);

template<int N>
void A_mul_Bx(Eigen::MatrixXd & out, Eigen::SparseMatrix<double, Eigen::RowMajor> & A, Eigen::MatrixXd & B);


void At_mul_B_blas(Eigen::MatrixXd & Y, Eigen::MatrixXd & A, Eigen::MatrixXd & B);
void At_mul_A_blas(Eigen::MatrixXd & A, double* AtA);
void A_mul_At_blas(Eigen::MatrixXd & A, double* AAt);
void A_mul_B_blas(Eigen::MatrixXd & Y, Eigen::MatrixXd & A, Eigen::MatrixXd & B);
void A_mul_Bt_blas(Eigen::MatrixXd & Y, Eigen::MatrixXd & A, Eigen::MatrixXd & B);

inline void A_mul_B_omp(double alpha, Eigen::MatrixXd & out, double beta, Eigen::MatrixXd & A, Eigen::MatrixXd & B);

void A_mul_At_combo(Eigen::MatrixXd & out, Eigen::MatrixXd & A);
void A_mul_At_omp(Eigen::MatrixXd & out, Eigen::MatrixXd & A);
Eigen::MatrixXd A_mul_At_combo(Eigen::MatrixXd & A);
void A_mul_Bt_omp_sym(Eigen::MatrixXd & out, Eigen::MatrixXd & A, Eigen::MatrixXd & B);

// util functions:
void A_mul_B(  Eigen::VectorXd & out, Eigen::MatrixXd & m, Eigen::VectorXd & b);
void A_mul_Bt( Eigen::MatrixXd & out, Eigen::MatrixXd & m, Eigen::MatrixXd & B);

Eigen::MatrixXd A_mul_B(Eigen::MatrixXd & A, Eigen::MatrixXd & B);

void makeSymmetric(Eigen::MatrixXd & A);

// Y = beta * Y + alpha * A * B (where B is symmetric)
void Asym_mul_B_left(double beta, Eigen::MatrixXd & Y, double alpha, Eigen::MatrixXd & A, Eigen::MatrixXd & B);
void Asym_mul_B_right(double beta, Eigen::MatrixXd & Y, double alpha, Eigen::MatrixXd & A, Eigen::MatrixXd & B);


inline void At_mul_Bt(Eigen::VectorXd & Y, Eigen::MatrixXd & X, const int col, Eigen::MatrixXd & B) 
{
   Y.setZero();

   for (int row = 0; row < B.rows(); row++)
   {
      Y(row) = X.col(col).dot(B.row(row));
   }
}

//
// computes Z += A[:,col] * b', where a and b are vectors
inline void add_Acol_mul_bt(Eigen::MatrixXd & Z, Eigen::MatrixXd & A, const int col, Eigen::VectorXd & b) 
{
   for (int row = 0; row < b.size(); row++)
   {
      Z.row(row) += (A.col(col) * b(row)).transpose();
   }
}

///////////////////////////////////
//     Template functions
///////////////////////////////////

//// for Sparse
/** computes uhat = denseFeat * beta, where beta and uhat are row ordered */
template<> inline void compute_uhat(Eigen::MatrixXd & uhat, Eigen::MatrixXd & denseFeat, Eigen::MatrixXd & beta) {
  A_mul_Bt_blas(uhat, beta, denseFeat);
}

/** good values for solve_blockcg are blocksize=32 an excess=8 */
template<typename T>
inline int solve_blockcg(Eigen::MatrixXd & X, T & K, double reg, Eigen::MatrixXd & B, double tol, const int blocksize, const int excess, bool throw_on_cholesky_error) {
  if (B.rows() <= excess + blocksize) {
    return solve_blockcg(X, K, reg, B, tol, throw_on_cholesky_error);
  }
  // split B into blocks of size <blocksize> (+ excess if needed)
  Eigen::MatrixXd Xblock, Bblock;
  int max_iter = 0;
  for (int i = 0; i < B.rows(); i += blocksize) {
    int nrows = blocksize;
    if (i + blocksize + excess >= B.rows()) {
      nrows = B.rows() - i;
    }
    Bblock.resize(nrows, B.cols());
    Xblock.resize(nrows, X.cols());

    Bblock = B.block(i, 0, nrows, B.cols());
    int niter = solve_blockcg(Xblock, K, reg, Bblock, tol, throw_on_cholesky_error);
    max_iter = std::max(niter, max_iter);
    X.block(i, 0, nrows, X.cols()) = Xblock;
  }

  return max_iter;
}

//
//-- Solves the system (K' * K + reg * I) * X = B for X for m right-hand sides
//   K = d x n matrix
//   I = n x n identity
//   X = n x m matrix
//   B = n x m matrix
//
template<typename T>
inline int solve_blockcg(Eigen::MatrixXd & X, T & K, double reg, Eigen::MatrixXd & B, double tol, bool throw_on_cholesky_error) {
  // initialize
  const int nfeat = B.cols();
  const int nrhs  = B.rows();
  double tolsq = tol*tol;

  if (nfeat != K.cols()) {THROWERROR("B.cols() must equal K.cols()");}

  Eigen::VectorXd norms(nrhs), inorms(nrhs); 
  norms.setZero();
  inorms.setZero();
  #pragma omp parallel for schedule(static)
  for (int rhs = 0; rhs < nrhs; rhs++) 
  {
    double sumsq = 0.0;
    for (int feat = 0; feat < nfeat; feat++) 
    {
      sumsq += B(rhs, feat) * B(rhs, feat);
    }
    norms(rhs)  = std::sqrt(sumsq);
    inorms(rhs) = 1.0 / norms(rhs);
  }
  Eigen::MatrixXd R(nrhs, nfeat);
  Eigen::MatrixXd P(nrhs, nfeat);
  Eigen::MatrixXd Ptmp(nrhs, nfeat);
  X.setZero();
  // normalize R and P:
  #pragma omp parallel for schedule(static) collapse(2)
  for (int feat = 0; feat < nfeat; feat++) 
  {
    for (int rhs = 0; rhs < nrhs; rhs++) 
    {
      R(rhs, feat) = B(rhs, feat) * inorms(rhs);
      P(rhs, feat) = R(rhs, feat);
    }
  }
  Eigen::MatrixXd* RtR = new Eigen::MatrixXd(nrhs, nrhs);
  Eigen::MatrixXd* RtR2 = new Eigen::MatrixXd(nrhs, nrhs);

  Eigen::MatrixXd KP(nrhs, nfeat);
  Eigen::MatrixXd KPtmp(nrhs, K.rows());
  Eigen::MatrixXd PtKP(nrhs, nrhs);
  //Eigen::Matrix<double, N, N> A;
  //Eigen::Matrix<double, N, N> Psi;
  Eigen::MatrixXd A;
  Eigen::MatrixXd Psi;

  A_mul_At_combo(*RtR, R);
  makeSymmetric(*RtR);

  const int nblocks = (int)ceil(nfeat / 64.0);

  // CG iteration:
  int iter = 0;
  for (iter = 0; iter < 1000; iter++) {
    // KP = K * P
    ////double t1 = tick();
    AtA_mul_B_switch(KP, K, reg, P, KPtmp);
    ////double t2 = tick();

    //A_mul_Bt_blas(PtKP, P, KP); // TODO: use KPtmp with dsyrk two save 2x time
    A_mul_Bt_omp_sym(PtKP, P, KP);

    auto chol_PtKP = PtKP.llt();
    THROWERROR_ASSERT_MSG(!throw_on_cholesky_error || chol_PtKP.info() != Eigen::NumericalIssue, "Cholesky Decomposition failed! (Numerical Issue)");
    THROWERROR_ASSERT_MSG(!throw_on_cholesky_error || chol_PtKP.info() != Eigen::InvalidInput, "Cholesky Decomposition failed! (Invalid Input)");
    A = chol_PtKP.solve(*RtR);

    A.transposeInPlace();
    ////double t3 = tick();

    
    #pragma omp parallel for schedule(guided)
    for (int block = 0; block < nblocks; block++) 
    {
      int col = block * 64;
      int bcols = std::min(64, nfeat - col);
      // X += A' * P
      X.block(0, col, nrhs, bcols).noalias() += A *  P.block(0, col, nrhs, bcols);
      // R -= A' * KP
      R.block(0, col, nrhs, bcols).noalias() -= A * KP.block(0, col, nrhs, bcols);
    }
    ////double t4 = tick();

    // convergence check:
    A_mul_At_combo(*RtR2, R);
    makeSymmetric(*RtR2);

    Eigen::VectorXd d = RtR2->diagonal();
    //std::cout << "[ iter " << iter << "] " << std::scientific << d.transpose() << " (max: " << d.maxCoeff() << " > " << tolsq << ")" << std::endl;
    //std::cout << iter << ":" << std::scientific << d.transpose() << std::endl;
    if ( (d.array() < tolsq).all()) {
      break;
    } 

    // Psi = (R R') \ R2 R2'
    auto chol_RtR = RtR->llt();
    THROWERROR_ASSERT_MSG(!throw_on_cholesky_error || chol_RtR.info() != Eigen::NumericalIssue, "Cholesky Decomposition failed! (Numerical Issue)");
    THROWERROR_ASSERT_MSG(!throw_on_cholesky_error || chol_RtR.info() != Eigen::InvalidInput, "Cholesky Decomposition failed! (Invalid Input)");
    Psi  = chol_RtR.solve(*RtR2);
    Psi.transposeInPlace();
    ////double t5 = tick();

    // P = R + Psi' * P (P and R are already transposed)
    #pragma omp parallel for schedule(guided)
    for (int block = 0; block < nblocks; block++) 
    {
      int col = block * 64;
      int bcols = std::min(64, nfeat - col);
      Eigen::MatrixXd xtmp(nrhs, bcols);
      xtmp = Psi *  P.block(0, col, nrhs, bcols);
      P.block(0, col, nrhs, bcols) = R.block(0, col, nrhs, bcols) + xtmp;
    }

    // R R' = R2 R2'
    std::swap(RtR, RtR2);
    ////double t6 = tick();
    ////printf("t2-t1 = %.3f, t3-t2 = %.3f, t4-t3 = %.3f, t5-t4 = %.3f, t6-t5 = %.3f\n", t2-t1, t3-t2, t4-t3, t5-t4, t6-t5);
  }
  
  if (iter == 1000)
  {
    Eigen::VectorXd d = RtR2->diagonal().cwiseSqrt();
    std::cerr << "warning: block_cg: could not find a solution in 1000 iterations; residual: ["
              << d.transpose() << " ].all() > " << tol << std::endl;
  }


  // unnormalizing X:
  #pragma omp parallel for schedule(static) collapse(2)
  for (int feat = 0; feat < nfeat; feat++) 
  {
    for (int rhs = 0; rhs < nrhs; rhs++) 
    {
      X(rhs, feat) *= norms(rhs);
    }
  }
  delete RtR;
  delete RtR2;
  return iter;
}

template<int N>
void A_mul_Bx(Eigen::MatrixXd & out, Eigen::SparseMatrix<double, Eigen::RowMajor> & A, Eigen::MatrixXd & B) {
   THROWERROR_ASSERT(N == out.rows());
   THROWERROR_ASSERT(N == B.rows());
   THROWERROR_ASSERT(A.cols() == B.cols());
   THROWERROR_ASSERT(A.rows() == out.cols());

  int* row_ptr   = A.outerIndexPtr();
  int* cols      = A.innerIndexPtr();
  double* vals   = A.valuePtr();
  const int nrow = A.rows();
  double* Y = out.data();
  double* X = B.data();
  #pragma omp parallel for schedule(guided)
  for (int row = 0; row < nrow; row++) 
  {
    double tmp[N] = { 0 };
    const int end = row_ptr[row + 1];
    for (int i = row_ptr[row]; i < end; i++) 
    {
      int col = cols[i] * N;
      double val = vals[i];
      for (int j = 0; j < N; j++) 
      {
         tmp[j] += X[col + j] * val;
      }
    }
    int r = row * N;
    for (int j = 0; j < N; j++) 
    {
      Y[r + j] = tmp[j];
    }
  }
}

template<typename T>
inline void AtA_mul_B_switch(Eigen::MatrixXd & out, T & A, double reg, Eigen::MatrixXd & B, Eigen::MatrixXd & tmp)
{
  switch(B.rows()) {
    case 1: return AtA_mul_Bx<1>(out, A, reg, B, tmp);
    case 2: return AtA_mul_Bx<2>(out, A, reg, B, tmp);
    case 3: return AtA_mul_Bx<3>(out, A, reg, B, tmp);
    case 4: return AtA_mul_Bx<4>(out, A, reg, B, tmp);
    case 5: return AtA_mul_Bx<5>(out, A, reg, B, tmp);

    case 6: return AtA_mul_Bx<6>(out, A, reg, B, tmp);
    case 7: return AtA_mul_Bx<7>(out, A, reg, B, tmp);
    case 8: return AtA_mul_Bx<8>(out, A, reg, B, tmp);
    case 9: return AtA_mul_Bx<9>(out, A, reg, B, tmp);
    case 10: return AtA_mul_Bx<10>(out, A, reg, B, tmp);

    case 11: return AtA_mul_Bx<11>(out, A, reg, B, tmp);
    case 12: return AtA_mul_Bx<12>(out, A, reg, B, tmp);
    case 13: return AtA_mul_Bx<13>(out, A, reg, B, tmp);
    case 14: return AtA_mul_Bx<14>(out, A, reg, B, tmp);
    case 15: return AtA_mul_Bx<15>(out, A, reg, B, tmp);

    case 16: return AtA_mul_Bx<16>(out, A, reg, B, tmp);
    case 17: return AtA_mul_Bx<17>(out, A, reg, B, tmp);
    case 18: return AtA_mul_Bx<18>(out, A, reg, B, tmp);
    case 19: return AtA_mul_Bx<19>(out, A, reg, B, tmp);
    case 20: return AtA_mul_Bx<20>(out, A, reg, B, tmp);

    case 21: return AtA_mul_Bx<21>(out, A, reg, B, tmp);
    case 22: return AtA_mul_Bx<22>(out, A, reg, B, tmp);
    case 23: return AtA_mul_Bx<23>(out, A, reg, B, tmp);
    case 24: return AtA_mul_Bx<24>(out, A, reg, B, tmp);
    case 25: return AtA_mul_Bx<25>(out, A, reg, B, tmp);

    case 26: return AtA_mul_Bx<26>(out, A, reg, B, tmp);
    case 27: return AtA_mul_Bx<27>(out, A, reg, B, tmp);
    case 28: return AtA_mul_Bx<28>(out, A, reg, B, tmp);
    case 29: return AtA_mul_Bx<29>(out, A, reg, B, tmp);
    case 30: return AtA_mul_Bx<30>(out, A, reg, B, tmp);

    case 31: return AtA_mul_Bx<31>(out, A, reg, B, tmp);
    case 32: return AtA_mul_Bx<32>(out, A, reg, B, tmp);
    case 33: return AtA_mul_Bx<33>(out, A, reg, B, tmp);
    case 34: return AtA_mul_Bx<34>(out, A, reg, B, tmp);
    case 35: return AtA_mul_Bx<35>(out, A, reg, B, tmp);

    case 36: return AtA_mul_Bx<36>(out, A, reg, B, tmp);
    case 37: return AtA_mul_Bx<37>(out, A, reg, B, tmp);
    case 38: return AtA_mul_Bx<38>(out, A, reg, B, tmp);
    case 39: return AtA_mul_Bx<39>(out, A, reg, B, tmp);
    case 40: return AtA_mul_Bx<40>(out, A, reg, B, tmp);
    default: 
    {
       THROWERROR("BlockCG only available for up to 40 RHSs.");
    }
  }
}

inline void AtA_mul_B_switch(
		   Eigen::MatrixXd & out,
		   Eigen::MatrixXd & A,
			 double reg,
			 Eigen::MatrixXd & B,
			 Eigen::MatrixXd & tmp) {
	out.noalias() = (A.transpose() * (A * B.transpose())).transpose() + reg * B;
}

template <int N>
void AtA_mul_Bx(Eigen::MatrixXd& out, SparseSideInfo& A, double reg, Eigen::MatrixXd& B, Eigen::MatrixXd& inner) {
    THROWERROR_ASSERT(N == out.rows());
    THROWERROR_ASSERT(N == B.rows());
    THROWERROR_ASSERT(A.cols() == B.cols());
    THROWERROR_ASSERT(A.cols() == out.cols());
    THROWERROR_ASSERT(A.rows() == inner.cols());

    Eigen::SparseMatrix<double, Eigen::RowMajor>* M = A.matrix_ptr;
    Eigen::SparseMatrix<double, Eigen::RowMajor>* Mt = A.matrix_trans_ptr;

    A_mul_Bx<N>(inner, *M, B);

    int* row_ptr   = Mt->outerIndexPtr();
    int* cols      = Mt->innerIndexPtr();
    double* vals   = Mt->valuePtr();
    const int nrow = Mt->rows();
    double* Y      = out.data();
    double* X      = inner.data();
    double* Braw   = B.data();
    #pragma omp parallel for schedule(guided)
    for (int row = 0; row < nrow; row++) 
    {
        double tmp[N] = { 0 };
        const int end = row_ptr[row + 1];
        for (int i = row_ptr[row]; i < end; i++) 
        {
        int col = cols[i] * N;
        double val = vals[i];
        for (int j = 0; j < N; j++) 
        {
            tmp[j] += X[col + j] * val;
        }
        }
        int r = row * N;
        for (int j = 0; j < N; j++) 
        {
        Y[r + j] = tmp[j] + reg * Braw[r + j];
        }
    }
}

// computes out = alpha * out + beta * A * B
inline void A_mul_B_omp(
    double alpha,
    Eigen::MatrixXd & out,
    double beta,
    Eigen::MatrixXd & A,
    Eigen::MatrixXd & B)
{
   THROWERROR_ASSERT(out.cols() == B.cols());

  const int nblocks = (int)ceil(out.cols() / 64.0);
  const int nrow = out.rows();
  const int ncol = out.cols();
  #pragma omp parallel for schedule(guided)
  for (int block = 0; block < nblocks; block++) 
  {
    int col = block * 64;
    int bcols = std::min(64, ncol - col);
    out.block(0, col, nrow, bcols).noalias() = alpha * out.block(0, col, nrow, bcols) + beta * A * B.block(0, col, nrow, bcols);
  }
}

}}